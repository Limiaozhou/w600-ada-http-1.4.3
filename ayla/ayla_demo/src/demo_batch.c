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
#include <ada/batch.h>


#ifdef AYLA_BATCH_PROP_SUPPORT

extern int output; //to be defined in demo_ledevb.c
extern int decimal_out;


#define BATCH_DPS_MAX			10
#define BATCH_DATA_MAX		(4 * 1024)

#define SAMPLE_TIMER_PERIOD	10000
#define BATCH_TIMER_PERIOD	(SAMPLE_TIMER_PERIOD * 5)

static struct batch_ctx *batch = NULL;
static struct timer batch_timer;
static struct timer sample_timer;

static void demo_batch_send_done(size_t size_sent) //演示批处理发送完成处理
{
	printf("send done! Sent batch data size = %u\n", size_sent);
}

static void demo_batch_error_cb(int batch_id, int status) //演示批处理出错处理
{
	if (status != HTTP_STATUS_OK) 
		printf("send err! batch_id = %d, status = %d\n", batch_id, status);
}

static void demo_put_prop_to_batch(struct ada_sprop *sprop,
		const char *prop_name)
{
	int ret;
	int retry;
	s64 stamp;

	if (!sprop && !prop_name) {
		printf("%s() params error\n", __func__);
		return;
	}
	if (!batch) {
		return;
	}

	stamp = clock_get(NULL);
	for (retry = 0; retry < 2; retry++) {
		if (sprop) {
			ret = ada_batch_add_prop(batch, sprop, stamp);
		} else {
			ret = ada_batch_add_prop_by_name(batch,
				prop_name, stamp);
		}
		if (ret > 0) {
			printf("ada_batch_add_prop(%s): batch_id = %d\n",
			    sprop ? sprop->name : prop_name, ret);
			break;
		} else if (ret == AE_BUF) {
			/* batch buffer is full, send the batch to cloud */
			ada_batch_send(batch, demo_batch_send_done);
			/* Restart batch timer after the batch is sent */
			client_timer_set(&batch_timer, BATCH_TIMER_PERIOD);
			continue;
		} else {
			printf("ada_batch_add_prop(%s) err = %d\n",
			    sprop ? sprop->name : prop_name, ret);
			break;
		}
	}
}

static void demo_prop_send(const char *name)
{
	if (!batch) {
		prop_send_by_name(name);
	} else {
		demo_put_prop_to_batch(NULL, name);
	}
}

static void demo_batch_timer_handler(struct timer *timer)
{
	int err = ada_batch_send(batch, demo_batch_send_done);
	/* Restart batch timer after the batch is sent */
	client_timer_set(timer, BATCH_TIMER_PERIOD);
}

static void demo_sample_timer_handler(struct timer *timer)
{
	s8 temp[8];

	/* Emulate property "output" sampling */
	random_fill(temp, 5);
	if (temp[0] >= -40 && temp[0] <= 50) {
		if (temp[0] != output) {
			output = temp[0];
			printf("output : %d\n",output);
			demo_put_prop_to_batch(NULL, "output");
		}
	}
	/* Emulate property "decimal_out" sampling */
	random_fill(temp, 7);
	if (temp[0] >= -64 && temp[0] <= 64) {
		if (temp[0] != decimal_out) {
			decimal_out = temp[0];
			demo_put_prop_to_batch(NULL, "decimal_out");
		}
	}
	/* restart the sampling timer */
	client_timer_set(timer, SAMPLE_TIMER_PERIOD);


}

/* pack prop for output and decimal_out send to cloud together*/
int demo_batch_test(void) //在配置命令行接口函数void conf_cli(int argc, char **argv)中调用
{
	if (!batch) {
		batch = ada_batch_create(BATCH_DPS_MAX, BATCH_DATA_MAX); //创建agent批处理，agent层
		ada_batch_set_err_cb(demo_batch_error_cb); //设置批处理出错回调函数为demo_batch_error_cb，agent层
	}
	
#if 1
	s64 stamp = clock_get(NULL); //获取时间，agent层
	int	ret;

	ret = ada_batch_add_prop_by_name(batch,"output", stamp); //批处理按名字添加属性，agent层
	if(ret == AE_BUF)
	{
		printf("batch buffer is full or buffer size is small\n");
		ada_batch_send(batch, demo_batch_send_done); //发送批处理，注册发送完成回调函数demo_batch_send_done，agent层
	}
	else
	{
		ret = ada_batch_add_prop_by_name(batch,"decimal_out", stamp);
		if(ret == AE_BUF)
		{
			printf("batch buffer is full or buffer size is small\n");
			//ada_batch_send(batch, demo_batch_send_done);
		}	
	}
	ada_batch_send(batch, demo_batch_send_done);

#else
		/* Timer for batch sending */
		timer_init(&batch_timer, demo_batch_timer_handler);
		client_timer_set(&batch_timer, BATCH_TIMER_PERIOD);
	
		/* Timer for data sampling */
		timer_init(&sample_timer, demo_sample_timer_handler);
		client_timer_set(&sample_timer, SAMPLE_TIMER_PERIOD);
#endif


}





#endif


















