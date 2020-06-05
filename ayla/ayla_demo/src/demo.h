/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#ifndef __AYLA_DEMO_H__
#define __AYLA_DEMO_H__

//#include <lwip_netconf.h>

#define DEMO_APP_STACKSZ	(1*1024/sizeof(portSTACK_TYPE))
#define DEMO_APP_PRIO		(tskIDLE_PRIORITY+1)

#define BUTTON_DETEC_STACKSZ	(1*1024/sizeof(portSTACK_TYPE))
#define BUTTON_DETEC_PRIO		(tskIDLE_PRIORITY+1)

/*
 * Demo platform setup_enable key.
 * If this is set, it allows setup mode to be enabled when this key is entered
 * on the CLI "setup_mode enable <key>".
 * #define DEMO_SETUP_ENABLE_KEY "secretkey"
 */



/*
 * Start demo main entry.
 */
void demo_start(void);

/*
 * Initialize demo.
 */
void demo_init(void);

/*
 * Application thread idle-loop function.
 */
void demo_idle(void);
void demo_idle_enter(void *arg);

/*
 * Application thread idle-loop function.
 */
void button_detect_task(void);
void button_detect_enter(void *arg);

/*
 * CLI command to reset the module, optionally to the factory configuration.
 */
void demo_reset_cmd(int argc, char **argv);

/*
 * CLI setup_mode command
 */
void demo_setup_mode_cmd(int argc, char **argv);

void demo_time_cmd(int argc, char **argv);
void demo_save_cmd(int argc, char **argv);
void demo_show_cmd(int argc, char **argv);
void demo_fact_log_cmd(int argc, char **argv);
void demo_client_cmd(int argc, char **argv);
extern const char mod_sw_build[];
extern const char mod_sw_version[];

/*
  * Handler to be called when WIFI is initialized
  */
extern int (*p_wlan_init_done_callback)(void);


/*
  * Set DHCP bound event notification handler
  */
void dhcp_set_bound_handler(void (*handler)(ip_addr_t *));

void ayla_demo_init(void);
extern u8 *wpa_supplicant_get_mac(void);



#endif /* __AYLA_DEMO_H__ */
