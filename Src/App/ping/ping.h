#ifndef __PING_H__
#define __PING_H__

#ifdef AYLA_FreeRTOS
#define TLS_CONFIG_WIFI_PING_TEST   0
#endif

#if TLS_CONFIG_WIFI_PING_TEST
struct ping_param{
    char host[64];
    u32 interval;/* ms */
    u32 cnt;/* -t */
};

void ping_test_create_task(void);
void ping_test_start(struct ping_param *para);
void ping_test_stop(void);
#endif

#endif /* __PING_H__ */

