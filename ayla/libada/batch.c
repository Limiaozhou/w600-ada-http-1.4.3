/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#if 1 
#include "wm_type_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <ayla/clock.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/tlv.h>
#include <ayla/log.h>
//#include <ayla/callback.h>
#include <ayla/http.h>
//#include <al/al_os_mem.h>
#include <ada/client.h>
//#include "prop_int.h"
//#include "prop_mgr_int.h"
#include "ada_lock.h"
#include "client_lock.h"
#include <ada/prop.h>
//#include <al/al_os_mem.h>

#include "batch_int.h"
//#include <al/al_os_mem.h>

#include "wm_include.h"



static void batch_default_err_cb(int batch_id, int status);
static void (*batch_err_cb)(int batch_id, int err) = batch_default_err_cb;
static s16 batch_id_count;

/*
 * Use ada_batch_set_err_cb() to override the default error callback.
 */
static void batch_default_err_cb(int batch_id, int status)
{
	 if (status != HTTP_STATUS_OK) 
		printcli("batch_id = %d, status = %d.\n", batch_id, status);
}

void batch_mgr_report_status(enum prop_cb_status status,
		u8 fail_mask, void *cb_arg)
{
	struct prop *prop = cb_arg;
	struct batch *b;
	struct batch_dp *dp;
	size_t size_sent = 0;
	int i;

	if (prop->type != ATLV_BATCH) {
		return;
	}
	b = (struct batch *)prop->val;
	for (i = 0; i < b->dps_cnt; i++) {
		dp = b->dps + i;
		if (!dp->prop) {
			continue;
		}
		if (dp->err == HTTP_STATUS_OK) {
			size_sent += dp->tx_size;
		}
		if (batch_err_cb) {
			batch_err_cb(dp->batch_id, dp->err);
		}
		tls_mem_free(dp->prop);
		dp->prop = NULL;
	}
	if (status == PROP_CB_DONE) {
		client_log(LOG_DEBUG "%s: send of \"%s\" (id: %d - %d) done",
		    __func__, prop->name, b->dps[0].batch_id,
		    b->dps[b->dps_cnt - 1].batch_id);
	} else {
		client_log(LOG_ERR "%s: send of \"%s\" (id: %d - %d) failed."
		   " status %d, mask %x", __func__, prop->name,
		   b->dps[0].batch_id, b->dps[b->dps_cnt - 1].batch_id,
		   status, fail_mask);
	}
	if (b->done_cb) {
		b->done_cb(size_sent);
	}
	tls_mem_free(b);
	prop->val = NULL;
	tls_mem_free(prop);
}

struct batch_ctx *ada_batch_create(u16 max_dps, u16 max_size)
{
	struct batch_ctx *ctx;

	ctx = tls_mem_alloc(sizeof(struct batch_ctx));
	memset(ctx,0,sizeof(struct batch_ctx));
	if (ctx) {
		ctx->dps_max = max_dps;
		ctx->len_max = max_size;
	}
	else printf("ada_batch_create fail\n");
	return ctx;
}

void batch_buf_free(struct batch *b)
{
	struct batch_dp *dp;
	struct prop *prop;
	int i;

	if (!b) {
		return;
	}
	for (i = 0; i < b->dps_cnt; i++) {
		dp = b->dps + i;
		prop = dp->prop;
		if (prop) {
			tls_mem_free(prop);
			dp->prop = NULL;
		}
	}
	tls_mem_free(b);
}

void ada_batch_destroy(struct batch_ctx *ctx)
{
	ada_batch_discard(ctx);
	free(ctx);
}

void ada_batch_discard(struct batch_ctx *ctx)
{
	if (ctx) {
		batch_buf_free(ctx->batch);
		ctx->batch = NULL;
	}
}

int batch_add_prop(struct batch_ctx *ctx, struct prop *prop, s64 t_stamp)
{
	/* Notice: prop contains metadata */
	struct batch *b;
	struct batch_dp *dp;
	int inc;

	if (!ctx) {
		return AE_INVAL_VAL;
	}
	if (!prop) {
		return AE_INVAL_VAL;
	}
	if (prop->type == ATLV_LOC || prop->type == ATLV_BATCH) {
		return AE_INVAL_TYPE;
	}
	if (prop->val == NULL) {
		/* Getting prop is not supported in batch */
		return AE_ERR;
	}
	b = ctx->batch;
	if (!b) {
		b = tls_mem_alloc(sizeof(*b) + ctx->dps_max *
		    sizeof(b->dps[0]));
		if (!b) {
			return AE_ALLOC;
		}
		memset(b,0,sizeof(*b) + ctx->dps_max * sizeof(b->dps[0]));
		b->age_time = (u32)clock_ms() / 1000;
		b->dps_max = ctx->dps_max;
		b->len_max = ctx->len_max;
		ctx->batch = b;
	}
	if (b->dps_cnt >= b->dps_max) {
		return AE_BUF;
	}
	inc = sizeof(*dp) + sizeof(*prop) + prop->len;
	if ((b->len_cnt + inc) > b->len_max) {
		return AE_BUF;
	}

	/* Add the data point */
	dp = b->dps + (b->dps_cnt);
	b->dps_cnt++;

	if (batch_id_count == 0) {
		batch_id_count = ((s64)(clock_ms())) & 0x7FFF;
	}
	if (batch_id_count <= 0) {
		batch_id_count = 1;
	}
	dp->batch_id = batch_id_count++;
	dp->err = AE_ERR; /* default is send failure */
	dp->prop = prop;
	dp->time_stamp = (t_stamp ? t_stamp : clock_get(NULL));
	b->len_cnt += inc;

	return dp->batch_id;
}

enum ada_err ada_batch_send(struct batch_ctx *ctx,
		void (*done_cb)(size_t size_sent))
{
	int err;
	struct batch *b;
	struct prop *prop;
	const struct prop_mgr *pm;

	if (!ctx) {
		return AE_ERR;
	}
	b = ctx->batch;
	if (!b) {
		/* nothing to send is ok. */
		return AE_OK;
	}
	if (b->dps_cnt <= 0) {
		return AE_ERR;
	}
	b->done_cb = done_cb;
	b->dps_tx = 0;
	prop = tls_mem_alloc(sizeof(*prop));
	if (!prop) {
		return AE_ALLOC;
	}
	prop->name = "batch";
	prop->type = ATLV_BATCH;
	prop->val = b;
	prop->len = 0;
	prop->echo = 0;

	pm = b->dps[0].prop->mgr;
	err = ada_prop_mgr_send(pm, prop, NODES_ADS, prop);
	if (err == AE_OK || err == AE_IN_PROGRESS) {
		ctx->batch = NULL;
		err = AE_OK;
	} else {
		tls_mem_free(prop);
	}
	return err;
}

void ada_batch_set_err_cb(void (*err_cb)(int batch_id, int err))
{
	batch_err_cb = err_cb;
}

u8 batch_is_all_dps_sent(struct batch *batch)
{
	return (batch->dps_tx >= batch->dps_cnt);
}
#endif
