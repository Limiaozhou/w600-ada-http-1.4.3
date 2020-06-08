/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */

/*
 * Ayla device agent demo of a simple lights and buttons evaluation board
 * using the "simple" property manager.
 *
 * The property names are chosen to be compatible with the Ayla Control
 * App.  E.g., the LED property is Blue_LED even though the color is yellow.
 * Button1 sends the Blue_button property, even though the button is white.
 */
#define HAVE_UTYPES
#include "lwip/ip_addr.h"

#include <ayla/utypes.h>
#include <ayla/log.h>
#include <sys/types.h>
#include <ada/libada.h>
#include <ada/sprop.h>
#include <ada/task_label.h>
#include <ayla/wifi_error.h>
#include <ada/server_req.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/clock.h>
#include <ayla/parse.h>
#include <ayla/tlv.h>
#include <ayla/parse.h>
#include <ayla/timer.h>

#include <ada/err.h>
#include <net/net.h>
#include <ada/client.h>
#include <net/stream.h>
#include <ada/server_req.h>
#include <net/http_client.h>

#include "jsmn.h"
#include "conf.h"
#include "demo.h"
#include "led_key.h"
#include "wm_io.h"
#include "wm_gpio.h"
#include "wm_wifi.h"
#include "wm_include.h"

#include "device.h"
#include "hal/pin.h"
#include "bsp/drv_pin.h"

#define BUILD_PID			 "SN0-0000000"
#define BUILD_PROGNAME "ayla_ledevb_demo"
#define BUILD_VERSION  "v1.0.0"
#define BUILD_STRING	  BUILD_VERSION " "  __DATE__ " " __TIME__

//#define D1_LED0         WM_IO_PB_10  //network status indicate, network config indicate
//#define CF_RELAY        WM_IO_PB_09
//#define D2_LED0         WM_IO_PB_08  //relay status indicate
//#define S1_BUTTON       WM_IO_PB_11
#define DETECT_INTERVAL (HZ/10)
#define CONFIG_TIMEOUT  60*5      //seconds
#define WATCH_DOG_EN    1
#define RELAY_TST_EN    0

#define PIN_LED_BLUE 28
#define PIN_LED_RED  29
#define PIN_PLUG     30
#define PIN_KEY      27

//enum lws_state{
//    ADA_CLIENT_UP,
//    CHECK_NET_STATUS,
//    HANDLE_KEY_AND_BUTTON,
//    ADA_CLIENT_DOWN
//};

//enum ledState {
//    LED_ALWAYS_OFF,
//    LED_ALWAYS_ON,
//    LED_BLINK_1_LEVEL,
//    LED_BLINK_2_LEVEL,
//};

//const char mod_sw_build[] = BUILD_STRING;
//const char mod_sw_version[] = BUILD_PROGNAME " " BUILD_STRING;

/*
 * The oem and oem_model strings determine the host name for the
 * Ayla device service and the device template on the service.
 *
 * If these are changed, the encrypted OEM secret must be re-encrypted
 * unless the oem_model was "*" (wild-card) when the oem_key was encrypted.
 */
char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_LEDEVB_MODEL;

//static unsigned blue_button;
//static u8 blue_led;
//static u8 red_led;
static u8 plug;
//static int input;
// int output;
//static int decimal_in;
// int decimal_out;
#ifdef JV_CTRL
static char jv_ctrl[100];
#endif
static char version[] = BUILD_PID " "BUILD_PROGNAME " " BUILD_STRING;
//static char cmd_buf[TLV_MAX_STR_LEN + 1];
static char demo_host_version[] = "1.0-rtk";	/* property template version */

static enum ada_err demo_plug_set(struct ada_sprop *, const void *, size_t);
//static enum ada_err demo_int_set(struct ada_sprop *, const void *, size_t);
//static enum ada_err demo_cmd_set(struct ada_sprop *, const void *, size_t);
#ifdef JV_CTRL
static enum ada_err jv_ctrl_set(struct ada_sprop *, const void *, size_t);
#endif

void pin_init(void);
void pin_write(u16 pin, u16 status);
u16 pin_read(u16 pin);

rt_device_t device_pin_t = NULL;

static struct ada_sprop demo_props[] = { //演示简单属性表，自己实现各属性内容
	/*
	 * version properties
	 */
	{ "oem_host_version", ATLV_UTF8,
	    demo_host_version, sizeof(demo_host_version),
	    ada_sprop_get_string, NULL},
	{ "version", ATLV_UTF8, &version[0], sizeof(version),
	    ada_sprop_get_string, NULL},
	/*
	 * boolean properties
	 */
	{ "Plug", ATLV_BOOL, &plug, sizeof(plug),
	    ada_sprop_get_bool, demo_plug_set },
	/*
	 * string properties
	 */
//	{ "cmd", ATLV_UTF8, &cmd_buf[0], sizeof(cmd_buf),
//	    ada_sprop_get_string, demo_cmd_set },
//	{ "log", ATLV_UTF8, &cmd_buf[0], sizeof(cmd_buf),
//	    ada_sprop_get_string, NULL },
	/*
	 * integer properties
	 */
//	{ "input", ATLV_INT, &input, sizeof(input),
//	    ada_sprop_get_int, demo_int_set },
//	{ "output", ATLV_INT, &output, sizeof(output),
//	    ada_sprop_get_int, NULL },
	/*
	 * decimal properties
	 */
//	{ "decimal_in", ATLV_CENTS, &decimal_in, sizeof(decimal_in),
//	    ada_sprop_get_int, demo_int_set },
//	{ "decimal_out", ATLV_CENTS, &decimal_out, sizeof(decimal_out),
//	    ada_sprop_get_int, NULL },
#ifdef JV_CTRL
	{ "jv_ctrl", ATLV_UTF8, &jv_ctrl[0], sizeof(jv_ctrl),
	    ada_sprop_get_string, jv_ctrl_set},
#endif	    
};


//static 演示发送对应名称的属性
enum ada_err prop_send_by_name(const char *name)
{
	enum ada_err err;

	err = ada_sprop_send_by_name(name);  //发送对应名称的属性，agent层接口
	if (err) {
		log_put(LOG_INFO "demo: %s: send of %s: err %d",
		    __func__, name, err);
	}
    return err;
}

//static void init_led_key(void)
//{
//	tls_gpio_cfg(S1_BUTTON, WM_GPIO_DIR_INPUT, WM_GPIO_ATTR_FLOATING);
//    tls_gpio_cfg(D1_LED0, WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_PULLHIGH);
//    tls_gpio_cfg(D2_LED0, WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_PULLHIGH);
//    tls_gpio_cfg(CF_RELAY, WM_GPIO_DIR_OUTPUT, WM_GPIO_ATTR_PULLHIGH);
//}

//static int get_gpio_value(enum tls_io_name io_num)  //演示读gpio
//{
//	u16 ret;

//	ret = tls_gpio_read(io_num);
//	return ret;
//}
//static void set_gpio_value(enum tls_io_name io_num, bool io_state)
//{
//	//printcli("set_led %d %d\n", led, on);
//	tls_gpio_write(io_num, io_state);
//}

/*
 * Demo set function for bool properties.
 */
static enum ada_err demo_plug_set(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	int ret = 0;

	ret = ada_sprop_set_bool(sprop, buf, len);
	if (ret) {
		return ret;
	}
	if (sprop->val == &plug) {
        pin_write(PIN_LED_RED, plug);
		pin_write(PIN_PLUG, plug);
	}
	log_put(LOG_INFO "%s: %s set to %u",
	    __func__, sprop->name, *(u8 *)sprop->val);
	return AE_OK;
}

/*
 * Demo set function for integer and decimal properties.
 */
//static enum ada_err demo_int_set(struct ada_sprop *sprop,
//				const void *buf, size_t len)
//{
//	int ret;

//	ret = ada_sprop_set_int(sprop, buf, len);
//	if (ret) {
//		return ret;
//	}

//	if (sprop->val == &input) {
//		log_put(LOG_INFO "%s: %s set to %d",
//		    __func__, sprop->name, input);
//		output = input;
//		prop_send_by_name("output");
//	} else if (sprop->val == &decimal_in) {
//		log_put(LOG_INFO "%s: %s set to %d",
//		    __func__, sprop->name, decimal_in);
//		decimal_out = decimal_in;
//		prop_send_by_name("decimal_out");
//	} else {
//		return AE_NOT_FOUND;
//	}
//	return AE_OK;
//}

/*
 * Demo set function for string properties.
 */
//static enum ada_err demo_cmd_set(struct ada_sprop *sprop,
//				const void *buf, size_t len)
//{
//	int ret;

//	ret = ada_sprop_set_string(sprop, buf, len);
//	if (ret) {
//		return ret;
//	}

//	prop_send_by_name("log");
//	log_put(LOG_INFO "%s: cloud set %s to \"%s\"",
//	    __func__, sprop->name, cmd_buf);
//	return AE_OK;
//}

#ifdef JV_CTRL
static enum ada_err jv_ctrl_set(struct ada_sprop *sprop, const void * buf, size_t len)
{
	int ret =0;
	jsmn_parser parser;
	jsmntok_t tokens[20];
	jsmnerr_t err;
	long utc;
	long cmd;

	ret = ada_sprop_set_string(sprop,buf,len);
	if(ret)
		return ret;

	log_put("%s: %s set to %s\r\n",
		__func__, sprop->name, (u8 *)sprop->val);

	jsmn_init_parser(&parser, sprop->val, tokens, 20);
	err = jsmn_parse(&parser);

	if (err != JSMN_SUCCESS) 
	{
		log_put("%s jsmn err %d\n", __func__, err);
		goto inval;
	}

	if (jsmn_get_long(&parser, NULL, "utc", &utc)) 
	{
		goto inval;
	}

	if (jsmn_get_long(&parser, NULL, "cmd", &cmd)) 
	{
		goto inval;
	}

	log_put("jsmn utc %d,cmd %d\n", utc, cmd);

	if(cmd==2)	//
	{
		u32 now;
		s32 diff;

		now = clock_get(NULL);
		diff = utc - now;
		if(diff > 10 || diff < -10)
		{
			log_put("cloud reset factory timeout - %d\n",diff);
		}
		else
		{
			char *argv[] = { "reset" , "factory"};	
			demo_reset_cmd(2,argv);
		}
	}

	return AE_OK;
	
inval:
	log_put("jsmn inval\n");
	return AE_OK;

}
#endif

/*
 * Initialize property manager.
 */
void demo_init(void)  //演示主用户实现
{
	ada_sprop_mgr_register("ledevb", demo_props, ARRAY_LEN(demo_props));  //简单属性表demo_props注册，agent层接口
}

void demo_idle(void)
{
//	int oldRealyStatus = 1, nowRealyStatus;
	u8 key_status = 0, key_flag = 0;

	log_thread_id_set(TASK_LABEL_DEMO);  //设置当前线程的日志名称，打印日志时的线程名，agent层接口
	pin_init();
#if RELAY_TST_EN
	init_led_key();  //演示led和key初始化
#endif

#if WATCH_DOG_EN
    tls_watchdog_init(30*1000*1000);  //看门狗初始化，agent层接口
#endif
	prop_send_by_name("oem_host_version");  //演示发送对应名称的属性
	prop_send_by_name("version");
#ifdef JV_CTRL
	sprintf(jv_ctrl,"{\"cmd\":1,\"utc\":%d}",clock_utc());  //打印utc时间，获取utc为agent层接口
	log_put("jv_ctrl :%s\n",jv_ctrl);
#endif
	while (1) 
    {
		if(pin_read(PIN_KEY))
		{
			tls_os_time_delay(5);
			if(pin_read(PIN_KEY))
			{
				if(key_flag)
				{
					plug = !plug;
					key_flag = 0;
					
					pin_write(PIN_LED_RED, plug);
					pin_write(PIN_PLUG, plug);
//					ada_sprop_get_bool(&demo_props[2], &key_status, sizeof(key_status));
					prop_send_by_name("Plug");
				}
			}
			else
				key_flag = 1;
		}
		else
			key_flag = 1;
		
#if RELAY_TST_EN    
        nowRealyStatus = get_gpio_value(CF_RELAY);  //演示读gpio
        if( nowRealyStatus != oldRealyStatus) 
        {
            green_led = nowRealyStatus;
            if( 0 == prop_send_by_name("Green_LED") ) {
                oldRealyStatus = nowRealyStatus;
            }
        }
#endif

#if WATCH_DOG_EN
        tls_watchdog_clr();  //清0看门狗，agent层接口
#endif
		tls_os_time_delay(5);  //任务延时，agent层接口
	}
}

//static int gpioStateReverse(enum tls_io_name ledNum)
//{    
//    int status = get_gpio_value(ledNum);
//    (status==1)?(status = 0):(status = 1);
//    set_gpio_value(ledNum, status);
//    return status;
//}

//static void ledStateControl(enum tls_io_name ledNum, enum ledState state, u8 baseCycle)
//{
//    static u8 blink_1_HZ = 0;
//    static u8 blink_2_HZ = 0;
//    
//    switch(state)
//    {
//        case LED_ALWAYS_OFF:
//            set_gpio_value(ledNum, 0);
//            break;
//        case LED_ALWAYS_ON:
//            set_gpio_value(ledNum, 1);
//            break;
//        case LED_BLINK_1_LEVEL:
//            {
//                blink_1_HZ ++;
//                if( blink_1_HZ > HZ/baseCycle/2 ) {
//                    gpioStateReverse(ledNum);
//                    blink_1_HZ = 0;
//                }
//            }
//            break;
//        case LED_BLINK_2_LEVEL:
//            {
//                blink_2_HZ ++;
//                if( blink_2_HZ > HZ/2/baseCycle/8 ) {
//                    gpioStateReverse(ledNum);
//                    blink_2_HZ = 0;
//                }
//            }
//            break;
//        default:
//            break;
//    }
//}

void pin_init(void)
{
	struct rt_device_pin_mode pin_mode = {0};
	struct rt_device_pin_status pin_status = {0};
	
	wm_hw_pin_init();
	
	device_pin_t = rt_device_find("pin");
	if(!device_pin_t)
		printf("pin device find failed, in file %s, line %d, func %s", __FILE__, __LINE__, __func__);
	rt_device_open(device_pin_t, RT_DEVICE_OFLAG_RDWR);
	
	pin_mode.pin = PIN_LED_BLUE;
	pin_mode.mode = PIN_MODE_OUTPUT;
	pin_status.pin = PIN_LED_BLUE;
	pin_status.status = 0;
	rt_device_control(device_pin_t, 0, &pin_mode);
	rt_device_write(device_pin_t, 0, &pin_status, sizeof(pin_status));
	
	pin_mode.pin = PIN_LED_RED;
	pin_mode.mode = PIN_MODE_OUTPUT;
	pin_status.pin = PIN_LED_RED;
	rt_device_control(device_pin_t, 0, &pin_mode);
	rt_device_write(device_pin_t, 0, &pin_status, sizeof(pin_status));
	
	pin_mode.pin = PIN_PLUG;
	pin_mode.mode = PIN_MODE_OUTPUT;
	pin_status.pin = PIN_PLUG;
	rt_device_control(device_pin_t, 0, &pin_mode);
	rt_device_write(device_pin_t, 0, &pin_status, sizeof(pin_status));
	
	pin_mode.pin = PIN_KEY;
	pin_mode.mode = PIN_MODE_INPUT_PULLUP;
	rt_device_control(device_pin_t, 0, &pin_mode);
}

void pin_write(u16 pin, u16 status)
{
	struct rt_device_pin_status pin_status = {0};
	
	pin_status.pin = pin;
	pin_status.status = status;
	rt_device_write(device_pin_t, 0, &pin_status, sizeof(pin_status));
}

u16 pin_read(u16 pin)
{
	struct rt_device_pin_status pin_status = {0};
	
	pin_status.pin = pin;
	rt_device_read(device_pin_t, 0, &pin_status, sizeof(pin_status));
	return pin_status.status;
}
