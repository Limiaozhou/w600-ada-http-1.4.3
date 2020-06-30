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

#include "adw/wifi.h"
#include "wifi_int.h"

#include "device.h"
#include "hal/pin.h"
#include "bsp/drv_pin.h"
#include "devices/multi_button.h"

#define BUILD_PID			 "SN0-8888888"
#define BUILD_PROGNAME "ayla_ledevb_demo"
#define BUILD_VERSION  "v1.0.2"
#define BUILD_STRING	  BUILD_VERSION " "  __DATE__ " " __TIME__

//#define D1_LED0         WM_IO_PB_10  //network status indicate, network config indicate
//#define CF_RELAY        WM_IO_PB_09
//#define D2_LED0         WM_IO_PB_08  //relay status indicate
//#define S1_BUTTON       WM_IO_PB_11
#define DETECT_INTERVAL (HZ/10)
#define CONFIG_TIMEOUT  60*5      //seconds
#define WATCH_DOG_EN    1
#define RELAY_TST_EN    0

#define USER_TASK_STK_SIZE 1024
#define PIN_LED_BLUE 23
#define PIN_LED_RED  22
#define PIN_PLUG     21
#define PIN_KEY      27
#define PRODUCT_WIFI_SSID  "sa_produce_01"  //product wifi name

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
static u8 Switch_Control;
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

rt_device_t device_pin_t = NULL;
struct button btn;
tls_os_timer_t *tiemr_led_handle = NULL;
u8 wifi_configured = 0, wifi_connected = 0; //配网、连网标志，有配置网络为1，连网成功为1
u8 produce_check = 0; //产测模式

enum
{
	MODULE_STANDBY 				= 0x00,	//待机状态
	MODULE_CONFIGURE_WIFI 		= 0x01,	//配网状态
	MODULE_CONNECTING_WIFI   	= 0x02,	//配网中
	MODULE_CONNECTED_WIFI   	= 0x03,	//配网结束
	MODULE_CONNECTED_CLOUD  	= 0x04,	//连云状态
};

extern void adw_wifi_get_state( enum adw_wifi_conn_state *state_s,
		enum adw_scan_state *scan_state_s );

static enum ada_err demo_plug_set(struct ada_sprop *, const void *, size_t);
//static enum ada_err demo_int_set(struct ada_sprop *, const void *, size_t);
//static enum ada_err demo_cmd_set(struct ada_sprop *, const void *, size_t);
#ifdef JV_CTRL
static enum ada_err jv_ctrl_set(struct ada_sprop *, const void *, size_t);
#endif

static void pin_init(void);
static void pin_write(u16 pin, u16 status);
static u16 pin_read(u16 pin);
static uint8_t button_read_pin(void);
static void user_task(void *arg);
static void timer_led_callback(void *ptmr, void *parg);
static void button_callback(void *parg);
static u8 get_device_wifi_state(void);
static int demo_wifi_product_check(void);
static void uesr_wifi_setup_mode_set(u8 mode);

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
	{ "Switch_Control", ATLV_BOOL, &Switch_Control, sizeof(Switch_Control),
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
	if (sprop->val == &Switch_Control) {
        pin_write(PIN_LED_RED, Switch_Control);
		pin_write(PIN_PLUG, Switch_Control);
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

	log_thread_id_set(TASK_LABEL_DEMO);  //设置当前线程的日志名称，打印日志时的线程名，agent层接口
#if RELAY_TST_EN
	init_led_key();  //演示led和key初始化
#endif

#if WATCH_DOG_EN
    tls_watchdog_init(30*1000*1000);  //看门狗初始化，agent层接口
#endif
	prop_send_by_name("oem_host_version");  //演示发送对应名称的属性
	prop_send_by_name("version");
	prop_send_by_name("Switch_Control");
#ifdef JV_CTRL
	sprintf(jv_ctrl,"{\"cmd\":1,\"utc\":%d}",clock_utc());  //打印utc时间，获取utc为agent层接口
	log_put("jv_ctrl :%s\n",jv_ctrl);
#endif
	while (1) 
    {
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
		tls_os_time_delay(HZ);  //任务延时，agent层接口
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

void user_init(void)
{
	tls_os_status_t err;
	
	uesr_wifi_setup_mode_set(1); //启动设置AirKiss配网模式，1为AirKiss，0为AP，2为等待
	
	pin_init(); //按键、继电器、指示灯的普通IO口初始化
	button_init(&btn, button_read_pin, PIN_LOW); //按键初始化
	
	button_attach(&btn, SINGLE_CLICK, button_callback);
	button_attach(&btn, LONG_RRESS_START, button_callback);
	button_attach(&btn, LONG_PRESS_HOLD, button_callback);
	button_start (&btn);
	
	tls_os_task_create(NULL, "user_task",
	                    user_task,
	                    (void *)0,
	                    (void *)0,
	                    USER_TASK_STK_SIZE * sizeof(u32),
	                    configMAX_PRIORITIES / 2,
	                    0); //创建应用任务user_task
	
	err = tls_os_timer_create(&tiemr_led_handle, timer_led_callback, &produce_check, 75, 1, (u8*)"timer_led"); //创建led定时器timer_led_callback
	if (!err)
		tls_os_timer_start(tiemr_led_handle);
}

static void pin_init(void)
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

static void pin_write(u16 pin, u16 status)
{
	struct rt_device_pin_status pin_status = {0};
	
	pin_status.pin = pin;
	pin_status.status = status;
	rt_device_write(device_pin_t, 0, &pin_status, sizeof(pin_status));
}

static u16 pin_read(u16 pin)
{
	struct rt_device_pin_status pin_status = {0};
	
	pin_status.pin = pin;
	rt_device_read(device_pin_t, 0, &pin_status, sizeof(pin_status));
	return pin_status.status;
}

static uint8_t button_read_pin(void)
{
	return (uint8_t)pin_read(PIN_KEY);
}

static void user_task(void *arg)
{
	while (1)
	{
		if ((!wifi_configured) && (adw_wifi_curr_wifi_get(adw_wifi_curr_profile_get()) != 0)) //有配置过网络
			wifi_configured = 1;
		
		if ((!wifi_connected) && (get_device_wifi_state() == MODULE_CONNECTED_WIFI)) //连网成功，熄蓝灯
		{
			tls_os_timer_stop(tiemr_led_handle);
			pin_write(PIN_LED_BLUE, 0);
			wifi_connected = 1;
		}
		
		if (conf_was_reset && demo_wifi_product_check()) //恢复出厂设置下，判断产测模式
		{
			if(!produce_check) //开始启动一次慢闪
			{
				tls_os_timer_change(tiemr_led_handle, 350);
				tls_os_timer_start(tiemr_led_handle);
				printf("User produce slowly blink\r\n");
			}
			produce_check = 1; //设置为产测模式
		}
		else
			produce_check = 0;
		
		button_ticks();
		
		tls_os_time_delay(2);//由于定时周期为2ms，实际延时时间为2倍
	}
}

static void timer_led_callback(void *ptmr, void *parg)
{
	static u8 led_blue_status = 0;
	
	pin_write(PIN_LED_BLUE, led_blue_status);
	led_blue_status = !led_blue_status;
	
	if(*(u8*)parg) //传入参数为产测模式标志，不为0就是产测模式
	{
		Switch_Control = led_blue_status;
		pin_write(PIN_LED_RED, Switch_Control);
		pin_write(PIN_PLUG, Switch_Control);
	}
}

static void button_callback(void *btn)
{
	uint32_t btn_event_val;
	static uint16_t long_hold_ticks = 0;
	static uint8_t long_hold_flag = 1;
	
	btn_event_val = get_button_event((struct button *)btn);
	switch (btn_event_val)
	{
		case SINGLE_CLICK :
			if(produce_check)
			{
				tls_os_timer_change(tiemr_led_handle, 75);
				tls_os_timer_start(tiemr_led_handle);
			}
			else
			{
				Switch_Control = !Switch_Control;
				pin_write(PIN_LED_RED, Switch_Control);
				pin_write(PIN_PLUG, Switch_Control);
				prop_send_by_name("Switch_Control");
			}
			break;
			
		case LONG_RRESS_START :
			long_hold_ticks = 0;
			long_hold_flag = 1;
			if (!wifi_configured) //没有配置过网络才能切换配网方式
			{
				if(!adw_wifi_setup_mode_get()) //获取是否为AP配网，是为0
				{
					uesr_wifi_setup_mode_set(1);
					tls_os_timer_change(tiemr_led_handle, 75);
					printf("User key set wifi to airkiss\r\n");
				}
				else
				{
					uesr_wifi_setup_mode_set(0);
					tls_os_timer_change(tiemr_led_handle, 350);
					printf("User key set wifi to ap\r\n");
				}
				tls_os_timer_start(tiemr_led_handle);
			}
			break;
			
		case LONG_PRESS_HOLD :
			if(long_hold_flag && ((++long_hold_ticks) > LONG_TICKS)) //长按之后再继续按5s，即长按达到10s
			{
				if (wifi_configured) //有配置过网络才能恢复出厂
				{
					printf("User key set to factory\r\n");
					tls_os_timer_change(tiemr_led_handle, 75);
					tls_os_timer_start(tiemr_led_handle);
					ada_conf_reset(1);
				}
				long_hold_flag = 0;
			}
			break;
	}
}

static u8 get_device_wifi_state(void)
{
	enum adw_wifi_conn_state state_g;
	enum adw_scan_state scan_state_g;	 
	u8 device_state=0xff;

	adw_wifi_get_state(&state_g,&scan_state_g);
	switch(state_g)
	{
	case	 WS_UP_AP:
		if(adw_wifi_setup_mode_get() == 2)
			device_state= MODULE_STANDBY;
		else
			device_state = MODULE_CONFIGURE_WIFI;
		break;
	
	case	 WS_JOIN:			 
		device_state = MODULE_CONNECTING_WIFI;
		break;
	
	case	 WS_DHCP:
	case	 WS_WAIT_CLIENT:
		device_state = MODULE_CONNECTED_WIFI;
		break;
	
	default :
		break;
	}
	
	if(ada_sprop_dest_mask & NODES_ADS)
	{
		device_state= MODULE_CONNECTED_CLOUD;
	}
	
	return   device_state;
}

static int demo_wifi_product_check(void)
{
	struct adw_state *wifi = &adw_state;
	struct adw_profile *prof;
	struct adw_scan *scan;
	char ssid_buf[33];
	
	for (scan = wifi->scan; scan < &wifi->scan[ADW_WIFI_SCAN_CT]; scan++) 
	{
		if (scan->ssid.len == 0)
			break;
		
		adw_format_ssid(&scan->ssid, ssid_buf, sizeof(ssid_buf));
		if(strcmp(ssid_buf, PRODUCT_WIFI_SSID) == 0)
			return TRUE;
	}
	return FALSE;
}

static void uesr_wifi_setup_mode_set(u8 mode)
{
	demo_wifi_disable();
	tls_os_time_delay(10); //之间要有一定延时才能设置生效
	adw_wifi_setup_mode_set(mode);
	tls_os_time_delay(10);
	demo_wifi_enable();
}
