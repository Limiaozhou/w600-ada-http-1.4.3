/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include <ayla/utypes.h>
#include <ada/libada.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ada/dnss.h>
#include <net/net.h>
#include <ada/server_req.h>
#include <adw/wifi.h>
#include <adw/wifi_conf.h>

#include <FreeRTOS.h>
#include <task.h>
//#include <httpd/httpd.h>
#include "httpd.h"

#include "conf_wifi.h"
#include "demo.h"
#include "httpd.h"
#include "conf.h"
#include "wm_wifi.h"
#include "wm_netif.h"
#include "wm_osal.h"
#include "wm_fwup.h"

static OS_STK IdleTaskStk[DEMO_APP_STACKSZ];

/*
 * Event handler.
 * This is called by the Wi-Fi subsystem on connects and disconnects
 * and similar events.
 * This allows the application to start or stop services on those events,
 * and to implement status LEDs, for example.
 */
 extern u8 *wpa_supplicant_get_mac(void);

static void demo_wifi_event_handler(enum adw_wifi_event_id id, void *arg)  //演示WiFi事件处理
{
	static u8 is_cloud_started;

	switch (id) {
	case ADW_EVID_AP_START:
		break;

	case ADW_EVID_AP_UP:
		server_up();  //服务器启动，agent层接口
		dnss_up();  //dns启动，agent层接口
		break;

	case ADW_EVID_AP_DOWN:
		dnss_down();  //dns关闭，agent层接口
		break;

	case ADW_EVID_STA_UP:
		log_put(LOG_DEBUG "Wifi associated with a AP!");
		break;

	case ADW_EVID_STA_DHCP_UP:
		ada_client_up();  //客户端启动，agent层接口
		server_up();  //服务器启动，agent层接口
		if (!is_cloud_started) {  //创建演示空闲任务
			if (xTaskCreateExt(demo_idle_enter, "A_LedEvb", (portSTACK_TYPE *)IdleTaskStk,
		    DEMO_APP_STACKSZ, NULL, DEMO_APP_PRIO, NULL) != pdPASS) {
			AYLA_ASSERT_NOTREACHED();
    			}
			is_cloud_started = 1;
		}
		break;

	case ADW_EVID_STA_DOWN:
		ada_client_down();  //客户端关闭，agent层接口
		break;

	case ADW_EVID_SETUP_START:
	case ADW_EVID_SETUP_STOP:
	case ADW_EVID_ENABLE:
	case ADW_EVID_DISABLE:
		break;

	case ADW_EVID_RESTART_FAILED:
		log_put(LOG_WARN "resetting due to Wi-Fi failure");
		/* sys_msleep(400);
		arch_reboot(); */
		break;

	default:
		break;
	}
}

void demo_wifi_enable(void)  //演示WiFi使能
{
	char *argv[] = { "wifi", "enable" };

	adw_wifi_cli(2, argv);  //调用命令接口配置WiFi使能，agent层接口
}

void demo_wifi_init(void)   //演示WiFi初始化
{
	struct ada_conf *cf = &ada_conf;
	int enable_redirect = 0;
	char ssid[32];

	adw_wifi_event_register(demo_wifi_event_handler, NULL); //WiFi事件demo_wifi_event_handler注册，agent层接口
	adw_wifi_init();  //wifi初始化，agent层接口
	adw_wifi_page_init(enable_redirect);  //WiFi页初始化为0，agent层接口

	/*
	 * Set the network name for AP mode, for use during Wi-Fi setup.
	 */
	//cf->mac_addr = LwIP_GetMAC(&xnetif[0]);
	cf->mac_addr = wpa_supplicant_get_mac();  //获取mac地址，agent层接口
	snprintf(ssid, sizeof(ssid),
	    OEM_AP_SSID_PREFIX "-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
	    cf->mac_addr[0], cf->mac_addr[1], cf->mac_addr[2],
	    cf->mac_addr[3], cf->mac_addr[4], cf->mac_addr[5]); //将mac地址格式化到ssid字符数组
	adw_wifi_ap_ssid_set(ssid);  //设置WiFi AP模式的ssid，agent层接口
	adw_wifi_ios_setup_app_set(OEM_IOS_APP);  //设置能操作的app oem，agent层接口
	adw_wifi_ap_conditional_set(1);/* 设置有条件的AP模式，agent层接口，don't enable AP mode if no profiles */

}
