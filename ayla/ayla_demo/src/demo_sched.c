/*
 * Copyright 2016 Ayla Networks, Inc.  All rights reserved.
 */

/*
 * Ayla schedule configuration.
 */
#include <sys/types.h>
#include <ada/libada.h>
#include <ada/sched.h>
#include <ayla/nameval.h>
#include <ayla/assert.h>
#include "conf.h"

static const char *demo_scheds[] = DEMO_SCHED_NAMES;  //演示定时调度器名称数组

static u32 demo_sched_saved_run_time;	/* XXX should be in NVRAM */

void demo_sched_init(void)   //演示定时调度初始化
{
	enum ada_err err;
	unsigned int i;
	int count = ARRAY_LEN(demo_scheds);  //获取演示设置的定时调度器个数，名称个数

	if (!count) {
		return;
	}

	/*
	 * Create schedules.
	 */
	err = ada_sched_init(count);  //初始化定时调度器，分配count个定时调度器模块内存，agent层接口
	ASSERT(!err);

	for (i = 0; i < count; i++) {
		err = ada_sched_set_name(i, demo_scheds[i]);  //设置定时调度器模块名称，agent层接口
		ASSERT(!err);
	}
}

void adap_sched_run_time_write(u32 run_time)
{
	demo_sched_saved_run_time = run_time;
}

u32 adap_sched_run_time_read(void)
{
	return demo_sched_saved_run_time;
}
