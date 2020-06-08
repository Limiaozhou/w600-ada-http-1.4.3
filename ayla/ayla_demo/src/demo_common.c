/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#define HAVE_UTYPES
#include "lwip/ip_addr.h"
//#include <wifi_constants.h>

#include <FreeRTOS.h>
#include <task.h>
//#include <httpd/httpd.h>
#include "httpd.h"

#include <ayla/utypes.h>
#include <sys/types.h>
#include <ada/libada.h>
#include <ada/sched.h>
#include <ayla/nameval.h>
#include <ayla/log.h>
#ifdef AYLA_WIFI_SUPPORT
#include <adw/wifi.h>
#endif
#include <ayla/conf_flash.h>

#include "conf.h"
#include "conf_wifi.h"
#include "demo.h"
#include "wm_wifi.h"
#include "wm_netif.h"
#include "wm_osal.h"
#include "wm_fwup.h"

/*
 * CLI command to reset the module, optionally to the factory configuration.
 */
void demo_reset_cmd(int argc, char **argv) //演示恢复复位命令
{
#ifdef AYLA_FreeRTOS

#endif
	if (argc == 2 && !strcmp(argv[1], "factory")) {
		printf("factory success\r\n");
		ada_conf_reset(1); //恢复工厂配置，agent层函数
	}
#ifdef AYLA_FreeRTOS    
    if (argc == 2 && !strcmp(argv[1], "erase_all")) {
		conf_erase_all();  //擦除模块，agent层函数
	}
#endif
	adap_conf_reset(0);  //配置重启，agent层函数
}

/*
 * setup_mode command.
 * setup_mode enable|disable|show [<key>]
 */
void demo_setup_mode_cmd(int argc, char **argv)  //演示设置模式命令
{
	if (argc == 2 && !strcmp(argv[1], "show")) {
		printcli("setup_mode %sabled", conf_setup_mode ? "en" : "dis");
		return;
	}
#ifdef DEMO_SETUP_ENABLE_KEY
	if (argc == 3 && !strcmp(argv[1], "enable")) {
		if (strcmp(argv[2], DEMO_SETUP_ENABLE_KEY)) {
			printcli("para set err\r\n");
			printcli("wrong key");
			return;
		}
		ada_conf_setup_mode(1); /* agent设置模式1，agent层函数，saves if not in setup mode */
		printcli("para set ok\r\n");
		return;
	}
#endif /* SETUP_ENABLE_KEY */
	if (argc == 2 && !strcmp(argv[1], "disable")) {
		ada_conf_setup_mode(0);	/* agent设置模式0，agent层函数，saves if clearing setup mode */
		printcli("para set ok\r\n");
		return;
	}
	printcli("para set err\r\n");	
	printcli("usage error");
}

void demo_time_cmd(int argc, char **argv)  //演示时钟命令
{
	char buf[40];
	unsigned long sec;
	unsigned long usec;
	u32 t;

	if (argc == 1) {
		clock_fmt(buf, sizeof(buf), clock_utc()); //转换时间格式，agent层
		printcli("%s  %lu ms since boot", buf, clock_ms());
		return;
	}
	if (argc != 2) {
usage:
		printcli("para set err\r\n");
		printcli("usage: time YYYY-MM-DDTHH:MM:SS");
		return;
	}
	t = clock_parse(argv[1]);  //时钟解析，agent层
	if (!t || t < CLOCK_START) {
		printcli("time setting invalid");
		goto usage;
	}
	if (clock_set(t, CS_LOCAL)) { //设置新的时间，agent层
		printcli("time setting failed");
		printcli("para set err\r\n");
		return;
	}
	printcli("para set ok\r\n");
	printcli("time set\n");
	printcli("time cmd disabled\n");
	return;
}

void demo_client_cmd(int argc, char **argv)  //演示客户端配置
{
	if (argc == 2 && !strcmp(argv[1], "test")) {
		ada_conf.test_connect = 1;
		return;
	} else if (argc == 4 && strcmp(argv[1], "server") == 0 &&
	    strcmp(argv[2], "region") == 0) {
		if (!mfg_or_setup_mode_ok()) { //判断是否为设置模式，agent层函数
			printcli("para set err\r\n");
			return;
		}
		if (client_set_region(argv[3])) {  //设置服务器域，成功返回0，agent层函数
			printcli("para set err\r\n");
			printcli("unknown region code %s\n", argv[3]);
		} else {
			conf_save_config();  //保存配置
			printcli("para set ok\r\n");
			return;
		}
	}
	/* other usage is reserved and should match ADA client_cli() */
	printcli("usage: client <test>|server region <CN|US>");
	return;
}


void demo_save_cmd(int argc, char **argv)  //演示保存命令
{
	char *args[] = { "conf", "save", NULL};

	conf_cli(2, args); //配置命令行接口，agent层函数
}

void demo_show_cmd(int argc, char **argv)  //演示显示命令
{
	char *args[] = { "conf", "show", NULL};

	if (argc != 2) {
		goto usage;
	}

	if (!strcmp(argv[1], "conf")) {
		conf_cli(2, args);  //配置命令行接口，agent层函数
		return;
	}
	if (!strcmp(argv[1], "version")) {
		printcli("%s\n", adap_conf_sw_version());  //演示显示版本
		return;
	}
#ifdef AYLA_WIFI_SUPPORT
	if (!strcmp(argv[1], "wifi")) {
		adw_wifi_show();  //WiFi显示，agent层函数
		return;
	}
#endif

usage:
	printcli("usage: show [conf|version|wifi]");
	return;
}

void demo_fact_log_cmd(int argc, char **argv) //演示工厂日志命令
{
	struct ada_conf *cf = &ada_conf;
	u32 now;
	struct clock_info ci;
	const u8 *mac = cf->mac_addr;
	const int conf_connected = 0;

	if (argc != 1) {
		printcli("para set err\r\n");
		printcli("factory-log: invalid usage");
		return;
	}
	if (clock_source() <= CS_DEF) { //判断时钟源，agent层函数
		printcli("factory-log: clock not set");
		printcli("para set err\r\n");
		return;
	}
	now = clock_utc();  //获取utc时间
	clock_fill_details(&ci, now);  //填充时钟信息
	printcli("factory-log line:\r\n");
	printcli("3,%lu,%2.2lu/%2.2u/%2.2u %2.2u:%2.2u:%2.2u UTC,label,0,"
	    "%s,%s,%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x,%s,"
	    "%s,%s,,%s,%s,%u,%s\r\n",
	    now, ci.year, ci.month, ci.days, ci.hour, ci.min, ci.sec,
	    conf_sys_model, conf_sys_dev_id,
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
	    conf_sys_mfg_model,
	    conf_sys_mfg_serial, cf->hw_id, oem, oem_model, conf_connected,
	    ODM_NAME/*,
	    demo_version*/);
	printcli("para set ok\r\n");
}


void demo_idle_enter(void *arg)  //演示空闲任务入口实现
{

	demo_idle();  //演示空闲任务
	AYLA_ASSERT_NOTREACHED();
}

#if 0
void button_detect_enter(void *arg)
{

	button_detect_task();
	AYLA_ASSERT_NOTREACHED();
}
#endif

void ayla_demo_init(void)    //演示实现
{
	int rc;

	demo_init();  //演示主用户功能，注册简单属性
    
#ifdef AYLA_WIFI_SUPPORT
        demo_wifi_init();  //演示WiFi初始化
#endif

	client_conf_init();  //演示客户端配置初始化
	rc = ada_init();  //获取agent初始化结果，agent层接口
	if (rc) {
		log_put(LOG_ERR "ADA init failed");
		return;
	}

	demo_ota_init();  //演示ota初始化
    
#ifdef AYLA_WIFI_SUPPORT
        demo_wifi_enable();  //演示wifi使能
#endif

    fwup_update_autoflag();  //设置wifi自动重连，w600 sdk层接口

#if 0
    if (xTaskCreateExt(demo_idle_enter, "A_LedEvb", (portSTACK_TYPE *)IdleTaskStk,
		    DEMO_APP_STACKSZ, NULL, DEMO_APP_PRIO, NULL) != pdPASS) {
			AYLA_ASSERT_NOTREACHED();
    }
    
	
    if (xTaskCreateExt(button_detect_enter, "button_detect", (portSTACK_TYPE *)ButtTaskStk,
		    BUTTON_DETEC_STACKSZ, NULL, BUTTON_DETEC_PRIO, NULL) != pdPASS) {
			AYLA_ASSERT_NOTREACHED();
    }
#endif    

    //for upgrade firmware for now
    //tls_webserver_init();
}


