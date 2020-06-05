/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#define HAVE_UTYPES
#include <sys/types.h>
#include <stdint.h>
//#include <main.h>

#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <netif/etharp.h>

#include <FreeRTOS.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/timer.h>
#include <ayla/conf.h>
#include <ayla/conf_token.h>
#include <ayla/parse.h>
#include <ayla/nameval.h>
#include <ayla/tlv.h>
#include <ayla/mod_log.h>
#include <net/net.h>
#include <adw/wifi.h>
#include <adw/wifi_conf.h>


#include "wm_wifi.h"
#include "tls_wireless.h"


#undef LIST_HEAD
//#include "wifi_constants.h"
//#include "wifi_structures.h"
//#include "wireless.h"
#include "wifi_int.h"
#include "wifi_timer.h"
#include <FreeRTOS.h>
#include <task.h>
#include "semphr.h"

#ifndef min
#define min(a, b) ((a < b) ? (a) : (b))
#endif

typedef void * QueueHandle_t;

PREPACKED struct adw_wmi_ssid_scan_result{
	u8 len;
	u8 bssid[6];
	s32 rssi;
	u8 sec;
	u8 wps_pass_id;
	u8 channel;
	char ssid[1];
} PACKED;

struct adw_wmi_state {
	u8 wifi_mode;
	void (*scan_cb)(struct adw_scan *);
};

static struct adw_wmi_state adw_wmi_state;
static QueueHandle_t adw_sema;
u8 adw_locked;

void adw_lock(void)
{
	xSemaphoreTake(adw_sema, portMAX_DELAY);
	AYLA_ASSERT(!adw_locked);
	adw_locked = 1;
}

void adw_unlock(void)
{
	AYLA_ASSERT(adw_locked);
	adw_locked = 0;
	xSemaphoreGive(adw_sema);
}

#if 0
static const char *adw_wmi_err_str(int err_code)
{
	switch (err_code) {
	case NETIF_WIFI_JOIN_SUCCESS:
		return "none";
	case RTW_BADARG:
		return "param";
	case RTW_NOMEM:
		return "nomem";
	case RTW_NOTAP:
		return "not AP";
	case RTW_NOTSTA:
		return "not station";
	case RTW_BADCHAN:
		return "Bad channnel";
	case RTW_UNSUPPORTED:
		return "unsupported";
	case RTW_NOTREADY:
		return "not_ready";
	}
	return "";
}

static void adw_wmi_log_err(const char *msg, int rc)
{
	char *sign = "";

	if (rc < 0) {
		sign = "-";
		rc = -rc;
	}
	adw_log(LOG_ERR "wmi: %s failed rc %s0x%x %s",
	    msg, sign, rc, adw_wmi_err_str(rc));
}

#endif

/*wifi security import*/
enum conf_token adw_wmi_sec_import(u32 wmi_sec)
{
	return wmi_sec;

#if 0	
	switch (wmi_sec) {
	case IEEE80211_ENCRYT_WEP104:
	case IEEE80211_ENCRYT_WEP40:
		return CT_WEP;
	case IEEE80211_ENCRYT_TKIP_WPA:
	case IEEE80211_ENCRYT_CCMP_WPA:
	case IEEE80211_ENCRYT_AUTO_WPA:
		return CT_WPA;
	case IEEE80211_ENCRYT_TKIP_WPA2:
	case IEEE80211_ENCRYT_CCMP_WPA2:

	case IEEE80211_ENCRYT_AUTO_WPA2:
		return CT_WPA2_Personal;
	default:
	case IEEE80211_ENCRYT_NONE:
		return CT_none;
	}
#endif
}

u32 adw_wmi_sec_export(enum conf_token sec)
{
#if 0
	switch (sec) {
	case CT_WEP:
		return IEEE80211_ENCRYT_WEP40;
	case CT_WPA:
		return IEEE80211_ENCRYT_CCMP_WPA;
	case CT_WPA2_Personal:
		return IEEE80211_ENCRYT_CCMP_WPA2;
	case CT_none:
	default:
		break;
	}
	return IEEE80211_ENCRYT_NONE;
#endif
}

const char *adw_wmi_sec_string(u32 wmi_sec)
{
//	rtw_security_t wlan_sec;
//	wlan_sec = wmi_sec;
	const char *sec;

	switch (wmi_sec) {
	case CT_none:
		sec = "None";
		break;
	case CT_WEP:
		sec = "WEP";
		break;
	case CT_WPA:
		sec = "WPA";
		break;
	case CT_WPA2_Personal:
		sec = "WPA2 Personal";
		break;
	default:
		sec = "Unknown";
		break;
	}
	return sec;
}

void adw_wmi_powersave_set(enum adw_wifi_powersave_mode mode)
{
}

int adw_wmi_get_rssi(int *rssip)
{
	s8 rssi;
	struct tls_curr_bss_t bss;

	tls_wifi_get_current_bss(&bss);
	rssi = 0xFF - bss.rssi;

	*rssip = (int)rssi;
	return 0;
}

int adw_wmi_sel_ant(int ant)
{
	if (ant == 0) {
		return 0;
	}
	return -1;
}

int adw_wmi_get_txant(int *antp)
{
	*antp = 0;
	return 0;
}

int adw_wmi_get_rxant(int *antp)
{
	*antp = 0;
	return 0;
}

int adw_wmi_set_tx_power(u8 tx_power)
{
	return 0;
}

int adw_wmi_get_tx_power(u8 *tx_powerp)
{
	*tx_powerp = 0;
	return 0;
}

void adw_wmi_set_scan_result(const char *ssid, u8 ssid_len, u8 *bssid,
	u8 channel, u8 bss_type, s16 rssi, u32 security)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;
	struct adw_scan scan;

	if (ssid == NULL) {
		wmi->scan_cb(NULL);
		return;
	}

	if (ssid_len == 0) {
		return;
	}

	memset(&scan, 0, sizeof(scan));
	ASSERT(ssid_len <= sizeof(scan.ssid.id));
	memcpy(scan.ssid.id, ssid, ssid_len);
	scan.ssid.len = ssid_len;
	memcpy(scan.bssid, bssid, sizeof(scan.bssid));
	scan.channel = channel;
	scan.type = bss_type;
	scan.rssi = rssi;
	scan.wmi_sec = security;
	wmi->scan_cb(&scan);
}

int adw_wmi_scan_with_ssid_handler(void)
{
    char *buf;
    u32 buflen;
	int i;
	int err;
	u32 security;
	struct tls_scan_bss_t *wsr;
    struct tls_bss_info_t *bss_info;
	
    buflen = 2000;
    buf = tls_mem_alloc(buflen);
    if (!buf)
    {
		goto end;
	}
    memset(buf, 0, buflen);     
    err = tls_wifi_get_scan_rslt((u8 *)buf, buflen);
    if (err) 
	{
        goto end;
    }

	wsr = (struct tls_scan_bss_t *)buf;
	bss_info = (struct tls_bss_info_t *)(buf + 8);

	for(i=0; i<wsr->count; i++)
	{
	    //printf("sec:%d\r\n", bss_info->privacy);
        switch(bss_info->privacy)
        {
            case WM_WIFI_AUTH_MODE_OPEN:
                security = CT_none;
                break;
            case WM_WIFI_AUTH_MODE_WEP_AUTO:
                security = CT_WEP;      
                break;
            case WM_WIFI_AUTH_MODE_WPA_PSK_TKIP:
            case WM_WIFI_AUTH_MODE_WPA_PSK_CCMP:
            case WM_WIFI_AUTH_MODE_WPA_PSK_AUTO:
                security = CT_WPA;
                break;
            case WM_WIFI_AUTH_MODE_WPA2_PSK_TKIP:   
            case WM_WIFI_AUTH_MODE_WPA2_PSK_CCMP:
            case WM_WIFI_AUTH_MODE_WPA2_PSK_AUTO:
            case WM_WIFI_AUTH_MODE_WPA_WPA2_PSK_TKIP:
            case WM_WIFI_AUTH_MODE_WPA_WPA2_PSK_CCMP:
            case WM_WIFI_AUTH_MODE_WPA_WPA2_PSK_AUTO:
                security = CT_WPA2_Personal;
                break;
            default:
                security = CT_none;
                break;          
        }
 
	    bss_info->ssid[bss_info->ssid_len] = '\0';
		adw_wmi_set_scan_result(bss_info->ssid, bss_info->ssid_len,
		    bss_info->bssid, bss_info->channel, BT_INFRASTRUCTURE,
		    bss_info->rssi, security);	
		bss_info ++;
	}
	
	adw_wmi_set_scan_result(NULL, 0, NULL, 0, 0, 0, 0);

end:
	if(buf)
	{
		tls_mem_free(buf);
	}
	return 0;	
}

int adw_wmi_scan(struct adw_ssid *spec_ssid,
		void (*callback)(struct adw_scan *))
{
	struct adw_wmi_state *wmi = &adw_wmi_state;

	wmi->scan_cb = callback;

	tls_wifi_scan_result_cb_register(adw_wmi_scan_with_ssid_handler);
	tls_wifi_scan();
	
	return WIFI_ERR_NONE;
}

enum wifi_error adw_wmi_join(struct adw_state *wifi, struct adw_profile *prof)
{
	int rc;
	u8 wmode = 0;
	struct adw_scan *scan = prof->scan;
	struct adw_wmi_state *wmi = &adw_wmi_state;

	AYLA_ASSERT(scan);

	//printf("adw_wmi_join: ");
	
	if (wmi->wifi_mode == IEEE80211_MODE_AP) {
		tls_wifi_softap_destroy();
		tls_os_time_delay(HZ/10);
	}
	else if (wmi->wifi_mode == IEEE80211_MODE_INFRA) {
		tls_wifi_disconnect();
		tls_os_time_delay(HZ/10);
	}
	else
	{
		return WIFI_ERR_NET_UNSUP;
	}
	wmi->wifi_mode = IEEE80211_MODE_INFRA;

	tls_param_get(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
	if(wmi->wifi_mode != wmode)
	{
		wmode = wmi->wifi_mode;
		tls_param_set(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
	}	

	printf("ssid:%s,%d, key:%s,%d\n", prof->ssid.id, prof->ssid.len, prof->key, prof->key_len);

	netif_set_up(wifi->nif_sta);
	netif_set_default(wifi->nif_sta);

	tls_wifi_connect(prof->ssid.id, prof->ssid.len, prof->key, prof->key_len);	

	return WIFI_ERR_NONE;
}

int adw_wmi_leave(struct adw_state *wifi)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;

	if (WM_WIFI_JOINED == tls_wifi_get_state())
	{
		tls_wifi_disconnect();
	}

	return WIFI_ERR_NONE;
}

enum wifi_error adw_wmi_conn_status(int ap_mode)
{
	struct adw_state *wifi = &adw_state;
	struct adw_wmi_state *wmi = &adw_wmi_state;
	enum wifi_error err = WIFI_ERR_NONE;
	int sta_state;

	if (ap_mode) {
		if (WM_WIFI_JOINED == tls_wifi_softap_get_state()) {
			if (!(wifi->nif_ap->flags & NETIF_FLAG_LINK_UP)) {
				adw_log(LOG_DEBUG
				    "wmi_conn_status: AP link up");
			}
			netif_set_link_up(wifi->nif_ap);
		} else {
			if (wifi->nif_ap->flags & NETIF_FLAG_LINK_UP) {
				adw_log(LOG_DEBUG
				    "wmi_conn_status: AP link down");
			}
			netif_set_link_down(wifi->nif_ap);
			err = WIFI_ERR_MEM;	/* XXX what's right? */
		}
	} else {
		if (WM_WIFI_JOINED == tls_wifi_get_state()) {
			if (!(wifi->nif_sta->flags & NETIF_FLAG_LINK_UP)) {
				adw_log(LOG_DEBUG
				    "wmi_conn_status: sta link up");
			}
			netif_set_link_up(wifi->nif_sta);
		} else {
			if (wifi->nif_sta->flags & NETIF_FLAG_LINK_UP) {
				adw_log(LOG_DEBUG
				    "wmi_conn_status: sta state %u", sta_state);
				adw_log(LOG_DEBUG
				    "wmi_conn_status: sta link down");
			}
			netif_set_link_down(wifi->nif_sta);
			if (err == WIFI_ERR_NONE) {
				err = WIFI_ERR_IN_PROGRESS;
			}
		}
	}
	return err;
}

/*
 * Start DHCP client on station interface.
 */
void adw_wmi_ipconfig(struct adw_state *wifi)
{
	tls_dhcp_start();
}

int adw_wmi_dhcp_poll(struct adw_state *wifi, struct adw_wifi_history *hist)
{
	int i;
	ip_addr_t *dns;

	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if ((!netif_is_up(wifi->nif_sta))||(0 == wifi->nif_sta->ip_addr.addr)) {
		return -1;
	}
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	hist->ip_addr = wifi->nif_sta->ip_addr;
	hist->netmask = wifi->nif_sta->netmask;
	hist->def_route = wifi->nif_sta->gw;
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	for (i = 0; i < min(DNS_MAX_SERVERS, WIFI_HIST_DNS_SERVERS); i++) {
		dns = dns_getserver(i);
		hist->dns_servers[i].addr = dns->addr;
	}
	printf("%X,%X,%X,%X\n", hist->ip_addr.addr, hist->netmask.addr, hist->def_route.addr, hist->dns_servers[i].addr);

	return 0;
}

/*
 * Start AP mode.
 * Only handles open AP mode for now - no Wi-Fi security.
 */
int adw_wmi_start_ap(struct adw_profile *prof, int chan)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;
	struct adw_state *wifi = &adw_state;

	u8 wmode = 0;
	struct tls_softap_info_t* apinfo;
	struct tls_ip_info_t* ipinfo;


	if (wmi->wifi_mode != IEEE80211_MODE_AP) 
	{
		wmi->wifi_mode = IEEE80211_MODE_AP;
		tls_wifi_disconnect();
		tls_os_time_delay(HZ/10);
	}

	tls_param_get(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
	if(wmi->wifi_mode != wmode)
	{
		wmode = wmi->wifi_mode;
		tls_param_set(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
	}
	
	apinfo = tls_mem_alloc(sizeof(struct tls_softap_info_t));
	if(apinfo == NULL)
	{
		return WIFI_ERR_MEM;
	}
	memset(apinfo, 0, sizeof(struct tls_softap_info_t));
	
	ipinfo = tls_mem_alloc(sizeof(struct tls_ip_info_t));
	if(ipinfo == NULL){
		tls_mem_free(apinfo);
		return WIFI_ERR_MEM;
	}
	memset(ipinfo, 0, sizeof(struct tls_ip_info_t));
	MEMCPY(apinfo->ssid, prof->ssid.id, prof->ssid.len);
	apinfo->ssid[prof->ssid.len] = '\0';

	apinfo->encrypt = IEEE80211_ENCRYT_NONE;
	apinfo->channel = chan;

	memcpy(ipinfo->ip_addr, &wifi->ap_mode_addr, 4);
	memcpy(ipinfo->netmask, &wifi->ap_mode_netmask, 4);

	tls_wifi_softap_create(apinfo, ipinfo);

	tls_mem_free(apinfo);
	tls_mem_free(ipinfo);
	
	return WIFI_ERR_NONE;
}

void adw_wmi_stop_ap(void)
{
	u8 wmode = 0;
	
	struct adw_wmi_state *wmi = &adw_wmi_state;

	if (wmi->wifi_mode == IEEE80211_MODE_AP) {
		wmi->wifi_mode = IEEE80211_MODE_INFRA;
		wmode = wmi->wifi_mode;
		tls_param_set(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
		tls_wifi_softap_destroy();
	}
}

extern u8 *hostapd_get_mac(void);
extern u8 *wpa_supplicant_get_mac(void);

int adw_wmi_get_mac_addr(int ap, u8 *mac)
{
	struct adw_state *wifi = &adw_state;
	u8 *getmac = NULL;

	if(ap)
	{
		getmac[0] = hostapd_get_mac();	
	}
	else
	{
		getmac = wpa_supplicant_get_mac();
	}

	memcpy(mac, getmac, 6);
	
	return 0;
}

/*
 * Allocate station and AP mode interfaces after first Wi-Fi start.
 */
static void adw_wmi_init_if(void)
{
	struct adw_state *wifi = &adw_state;

	if (wifi->init_if_done) {
		return;
	}
	wifi->init_if_done = 1;

	wifi->ap_mode_addr.addr = htonl(ADW_WIFI_AP_IP);
	wifi->ap_mode_netmask.addr = htonl(ADW_WIFI_AP_NETMASK);
	memcpy(conf_sys_mac_addr, wpa_supplicant_get_mac(), sizeof(conf_sys_mac_addr));
}

int adw_wmi_on(void)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;
	struct adw_state *wifi = &adw_state;
	u8 wmode = 0;

	wifi->nif_sta = tls_get_netif();
	wifi->nif_ap = wifi->nif_sta->next;
	adw_wmi_init_if();
	if (wmi->wifi_mode != IEEE80211_MODE_INFRA) {
		wmi->wifi_mode = IEEE80211_MODE_INFRA;
		
		tls_param_get(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
		if(wmi->wifi_mode != wmode)
		{
			wmode = wmi->wifi_mode;
			tls_param_set(TLS_PARAM_ID_WPROTOCOL, (void* )&wmode, TRUE);
		}
	}

	return 0;
}

void adw_wmi_off(void)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;

	if (WM_WIFI_JOINED == tls_wifi_get_state())
	{
		tls_wifi_disconnect();
	}

	if (WM_WIFI_JOINED == tls_wifi_softap_get_state())
	{
		tls_wifi_softap_destroy();
	}
}

void adw_wmi_init(void)
{
	struct adw_wmi_state *wmi = &adw_wmi_state;

	adw_sema = xSemaphoreCreateMutex();

	wmi->wifi_mode = IEEE80211_MODE_INFRA;
}
