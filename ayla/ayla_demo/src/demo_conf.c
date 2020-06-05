/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <ayla/utypes.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>
#include <ayla/cmd.h>
#include <ada/libada.h>
#include <net/base64.h>
#include <net/net.h>
//#include <lwip_netconf.h>
#include "conf.h"
#include "conf_wifi.h"
#ifdef AYLA_WIFI_SUPPORT
#include <adw/wifi.h>
#endif
#include "demo.h"

#include "wm_uart.h"
#include "wm_uart_task.h"

struct list_head {
	struct list_head *next, *prev;
};

typedef void (*log_act_t)(void*);
typedef struct _at_command_item_{
	char *log_cmd;
	log_act_t at_act;
	struct list_head node;
}log_item_t;


/*
 * Set platform ADA client configuration items.
 */
void client_conf_init(void)    //演示客户端初始化
{
	struct ada_conf *cf = &ada_conf;
	static u8 *mac;
	int32_t	ret = -1;

	//mac = LwIP_GetMAC(&netif[0]);
	mac = wpa_supplicant_get_mac();  //获取mac地址，agent层接口
	cf->mac_addr = mac;

#ifdef AYLA_WIFI_SUPPORT
	adw_conf_load();  //加载WiFi配置，agent层接口
#endif

	cf->enable = 1;
	cf->get_all = 1;
	cf->poll_interval = 30;

	/*
	 * Allow the region to default (NULL) for most of the world.
	 * DNS based on the OEM-model and OEM-ID will take
	 * care of directing the device to the correct service.
	 * The only exception as of this writing is China, for which you
	 * should set cf->region = "CN".
	 */

	demo_sched_init();  //演示定时调度初始化
}

/*
 * The user-registered flag changed.
 */
void adap_conf_reg_changed(void)
{
}

/*
 * client_conf_sw_build() returns the string to be reported to the cloud as the
 * module image version.
 */
const char *adap_conf_sw_build(void)
{
	return ada_version_build;
}

/*
 * client_conf_sw_version() returns the string reported to LAN clients with the
 * name, version, and more.
 */
const char *adap_conf_sw_version(void)
{
	return ada_version;
}

void rtl_sys_reset(int argc, char **argv)
{
	tls_sys_reset();
}

void demo_default_cli_cmd(int argc, char **argv)
{
    
}

static const char conf_cli_help[] = "<show|save>";
static const char client_cli_help[] = "usage: client test";
static const char file_cli_help[] = "<start 0|add [key string]>";
static const char id_cli_help[] = "dev_id <DSN>";
static const char conf_oem_cli_help[] = "key <secret> [oem-model]";
static const char setup_mode_cli_help[] = "<enable|disable|show> [<key>]";
#ifdef AYLA_WIFI_SUPPORT
static const char show_cli_help[] = "<conf|version|wifi>";
static const char wifi_cli_help[] = "<name> <value>";
#else
static const char show_cli_help[] = "<conf|version>";
#endif

static const struct cmd_info client_conf_cmds[] = {
	CMD_INIT("client", demo_client_cmd, client_cli_help),
	CMD_INIT("conf", conf_cli, conf_cli_help),
	CMD_INIT("factory-log", demo_fact_log_cmd, NULL),
	CMD_INIT("file", ada_conf_file_cli, file_cli_help),
	CMD_INIT("id", ada_conf_id_cli, id_cli_help),
	CMD_INIT("log", ada_log_cli, ada_log_cli_help),
	CMD_INIT("oem", ada_conf_oem_cli, conf_oem_cli_help),
	CMD_INIT("reset", demo_reset_cmd, NULL),
	CMD_INIT("save", demo_save_cmd, NULL),
	CMD_INIT("setup_mode", demo_setup_mode_cmd, setup_mode_cli_help),
	CMD_INIT("show", demo_show_cmd, show_cli_help),
	CMD_INIT("time", demo_time_cmd, NULL),
#ifdef AYLA_WIFI_SUPPORT
	CMD_INIT("wifi", adw_wifi_cli, wifi_cli_help),
#endif
	CMD_END_DEFAULT(demo_default_cli_cmd),
};

/* Ameba only supports the console command in the format CMD=PARAM,
  * and call the registered callback functions with arg=PARAM.
  * and maximum size of command line is LOG_SERVICE_BUFLEN, it is 100 defined
  * in the file Platform_opts.h
  */

#undef LIST_HEAD
//#include <at_cmd/log_service.h>

#define CLI_WRAPPER_DEFINE_WITH_FUN(cmd, fun) \
static void ayla_cli_wrapper_##fun(void *arg) \
{ \
	cli_wrapper(#cmd, (char *)arg); \
}
#define CLI_WRAPPER_DEFINE(cmd) \
	CLI_WRAPPER_DEFINE_WITH_FUN(cmd, cmd)

#define CLI_CMD_ENTRY_WITH_FUN(cmd, fun) \
{ #cmd, ayla_cli_wrapper_##fun }
#define CLI_CMD_ENTRY(cmd) \
	CLI_CMD_ENTRY_WITH_FUN(cmd, cmd)

static void cli_wrapper(char *cmd, char *arg)
{
	char *argv[CMD_ARGV_LIMIT];
	int argc = 1;

	argv[0] = cmd;
	if (arg != NULL) {
		argc = parse_argv(&argv[1], ARRAY_LEN(argv) - 1, arg) + 1;
		if (argc >= ARRAY_LEN(argv)) {
			return;
		}
	}
	cmd_handle_argv(client_conf_cmds, argc, argv);
}

CLI_WRAPPER_DEFINE(client)
CLI_WRAPPER_DEFINE(conf)
CLI_WRAPPER_DEFINE_WITH_FUN(factory-log, factory_log)
CLI_WRAPPER_DEFINE(file)
CLI_WRAPPER_DEFINE(id)
CLI_WRAPPER_DEFINE(log)
CLI_WRAPPER_DEFINE(oem)
CLI_WRAPPER_DEFINE(reset)
CLI_WRAPPER_DEFINE(save)
CLI_WRAPPER_DEFINE(setup_mode)
CLI_WRAPPER_DEFINE(show)
CLI_WRAPPER_DEFINE(time)
CLI_WRAPPER_DEFINE(wifi)

static log_item_t ayla_at_cmd_table[] = {
	CLI_CMD_ENTRY(client),
	CLI_CMD_ENTRY(conf),
	CLI_CMD_ENTRY_WITH_FUN(factory-log, factory_log),
	CLI_CMD_ENTRY(file),
	CLI_CMD_ENTRY(id),
	CLI_CMD_ENTRY(log),
	CLI_CMD_ENTRY(oem),
	CLI_CMD_ENTRY(reset),
	CLI_CMD_ENTRY(save),
	CLI_CMD_ENTRY(setup_mode),
	CLI_CMD_ENTRY(show),
	CLI_CMD_ENTRY(time),
	CLI_CMD_ENTRY(wifi),
};

void at_ayla_cli_init(void)
{
	//log_service_add_table(ayla_at_cmd_table, ARRAY_LEN(ayla_at_cmd_table));
}

int at_ayla_cli_parse(struct tls_uart *uart)
{
    struct tls_uart_circ_buf *recv = &uart->uart_port->recv;
	log_item_t * pItem = ayla_at_cmd_table;
	int cmd_len = 0, tail_len = 0;
	char *atcmd_start = &recv->buf[recv->tail];
	char *atcmd_end = &recv->buf[recv->head];
	char *buf = NULL;
	int i;
	volatile u32 tickStart = 0, tickNow = 0;

	tickStart = xTaskGetTickCount();
	while( 1 )
	{
		if( recv->buf[recv->head-1]=='\r' || recv->buf[recv->head-1]=='\n' )
			break;
		tickNow = xTaskGetTickCount();
		if( (tickNow - tickStart) > 50 )
			break;
		else
			tls_os_time_delay(1);
	}

	cmd_len = CIRC_CNT(recv->head, recv->tail, TLS_UART_RX_BUF_SIZE);
	atcmd_start[cmd_len] = '\0';
	printf("# %s", atcmd_start);  //turn off printf cmd 
	buf = tls_mem_alloc(cmd_len + 1);
    if (!buf)
    {
        return -1;
    }
    if ((tail_len=CIRC_CNT_TO_END(recv->head, recv->tail, TLS_UART_RX_BUF_SIZE)) >= cmd_len)
    {
        MEMCPY(buf, atcmd_start, cmd_len);
    }
    else
    {
        MEMCPY(buf, atcmd_start, tail_len);
        MEMCPY(buf + tail_len, &recv->buf[0], cmd_len - tail_len);
    }
    if( buf[cmd_len-1]=='\r' || buf[cmd_len-1]=='\n' ) {
        buf[cmd_len-1]='\0';
    }
    if( buf[cmd_len-2]=='\r' || buf[cmd_len-2]=='\n' ) {
        buf[cmd_len-2]='\0';
    }
    buf[cmd_len]='\0';
	pItem = ayla_at_cmd_table;
	for(i = 0; i <  ARRAY_LEN(ayla_at_cmd_table); i++)
	{
		if(cmd_len < strlen(pItem->log_cmd) || memcmp(buf, pItem->log_cmd, strlen(pItem->log_cmd)))
		{
			pItem++;
			continue;
		}
		pItem->at_act(buf + strlen(pItem->log_cmd));
        tls_mem_free(buf);
		return 0;
	}
	
    if( buf != NULL ) {
        tls_mem_free(buf);
    }
	return -1;
}

void print_ayla_cli_help(void)
{
	const struct cmd_info *cmd;
	printf("\n"
	       "\n"
	       "AYLA CLI COMMAND SET:\n"
	       "==============================\n");
	for (cmd = client_conf_cmds; cmd->name; cmd++) {
		if (cmd->help)
			printf("  %s %s\n", cmd->name, cmd->help);
		else
			printf("  %s\n", cmd->name);
	}
}

