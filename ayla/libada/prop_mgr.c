/*
 * Copyright 2014, 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/base64.h>
#include <ayla/tlv.h>

#include <ayla/log.h>
#include <ayla/clock.h>
#include <ayla/timer.h>

#include <net/net.h>
#include <net/base64.h>
#include <ada/err.h>
#include <ayla/malloc.h>
#include <ada/client.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include "ada_lock.h"
#include "client_lock.h"
#include "client_timer.h"

/*
 * State bits
 */
#define PROP_MGR_ENABLED	1	/* listen enable */
#define PROP_MGR_FLOW_CTL	2	/* flow controlled */

/*
 * Private structure for list of all property managers.
 */
struct prop_mgr_node {
	const struct prop_mgr *mgr;
	struct prop_mgr_node *next;
	u8 state;
};

static struct prop_mgr_node *prop_mgrs;
static struct prop *prop_mgr_sending;
static struct prop_queue prop_mgr_send_queue =
		STAILQ_HEAD_INITIALIZER(prop_mgr_send_queue);
static struct prop_queue prop_mgr_wait_queue =	/* waiting for response */
		STAILQ_HEAD_INITIALIZER(prop_mgr_wait_queue);		
static struct net_callback prop_mgr_send_callback;
static u8 prop_mgr_dest_mask;
static struct ada_lock *prop_mgr_lock;

static u16 prop_mgr_txn_id;
static u8 prop_mgr_ids_inited;

static u8 prop_mgr_unfiltered_mask;
static enum ada_svc_status prop_mgr_svc_status;

static void prop_mgr_send_next(void *);
static enum ada_err prop_mgr_send_cb(enum prop_cb_status stat,u16 txn_id, void *arg);

void prop_mgr_init(void)
{
	net_callback_init(&prop_mgr_send_callback, prop_mgr_send_next, NULL);
	prop_mgr_lock = ada_lock_create("prop_mgr");
	ASSERT(prop_mgr_lock);
}

/*
 * Queue request for the callback.
 */
static void ada_prop_mgr_enqueue(struct prop *prop)
{
	struct prop_queue *qp = &prop_mgr_send_queue;

	ada_lock(prop_mgr_lock);
	if (!prop_mgr_ids_inited) {
		/*
		 * Start id space such that if the device resets repeatedly,
		 * it is unlikely to overlap IDs from the prior boots.
		 * Dividing the millisecond counter by 16 results in a 16 bit
		 * value that will roll over every 17.5 minutes and advances
		 * 62.5 counts per second.
		 */
		prop_mgr_txn_id = clock_ms() / 16 + 0x7fff;
		prop_mgr_ids_inited = 1;
	}
	if (!prop_mgr_txn_id) {
		/* disallow 0 */
		prop_mgr_txn_id++;
	}
	prop->txn_id = prop_mgr_txn_id++;
	STAILQ_INSERT_TAIL(qp, prop, list);
	prop->queue = qp;
	ada_unlock(prop_mgr_lock);
	client_callback_pend(&prop_mgr_send_callback);
}
static struct prop *prop_mgr_find_waiting(u16 txn_id)
{
	struct prop *prop;

	ada_lock(prop_mgr_lock);
	if (txn_id)
	{
		STAILQ_FOREACH(prop, &prop_mgr_wait_queue, list) 
		{
			ASSERT(prop->queue == &prop_mgr_wait_queue);
			if (prop->txn_id == txn_id)
			{
				break;
			}
		}
	}
	else 
	{
		/* serialized ops, only one item is on queue */
		prop = STAILQ_FIRST(&prop_mgr_wait_queue);
		if (prop) {
			ASSERT(prop->queue == &prop_mgr_wait_queue);
			ASSERT(!prop->txn_id);
		}
	}
	ada_unlock(prop_mgr_lock);

	return prop;
}


static struct prop_mgr_node *prop_mgr_node_lookup(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		if (pm == pg->mgr) {
			return pg;
		}
	}
	return NULL;
}

static void prop_mgr_report_status(struct prop *prop, enum prop_cb_status stat,
	u8 dest, void *send_done_arg)
{
	const struct prop_mgr *pm;
	if (!prop) {
		return;
	}
	if (prop->type == ATLV_BATCH) {
		batch_mgr_report_status(stat, dest, prop);
		return;
	}
	pm = prop->mgr;
	if (pm) {
		ASSERT(pm->send_done);
		pm->send_done(stat, dest, send_done_arg);
	} else if (prop->prop_mgr_done) {
		prop->prop_mgr_done(prop);
	}
}

/*
 * If all property managers are now ready, enable listen to client.
 */
static void prop_mgr_listen_check(void)
{
	struct prop_mgr_node *pg;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		if (!(pg->state & PROP_MGR_ENABLED)) {
			return;
		}
	}
	client_enable_ads_listen();
}

/*
 * Check the send list for any property updates that can no longer be sent
 * because all destinations specified have lost connectivity.
 */
static void prop_mgr_send_queue_check(u8 mask)
{
	struct prop_queue *qp = &prop_mgr_send_queue;
	struct prop_queue lost_q;
	struct prop *prop;
	struct prop *next;
	const struct prop_mgr *pm;

	/*
	 * Look for props that can no longer be sent to any destination,
	 * take them off the list and put them on the local lost_q list.
	 * Then go through the local lost_q list and do the callbacks after.
	 * dropping the lock.
	 */
	STAILQ_INIT(&lost_q);
	ada_lock(prop_mgr_lock);
	for (prop = STAILQ_FIRST(qp); prop; prop = next) {
		next = STAILQ_NEXT(prop, list);
		if (!(prop->send_dest & mask)) {
			STAILQ_REMOVE(qp, prop, prop, list);
			STAILQ_INSERT_TAIL(&lost_q, prop, list);
			prop->queue = &lost_q;
		}
	}
	ada_unlock(prop_mgr_lock);
	for (;;) {
		prop = STAILQ_FIRST(&lost_q);
		if (!prop) {
			break;
		}
		STAILQ_REMOVE_HEAD(&lost_q, list);
		STAILQ_NEXT(prop, list) = NULL;
		prop->queue = NULL;
		/*
		 * report all dests failed
		 */
		/*pm = prop->mgr;
		if (pm) {
			ASSERT(pm->send_done);
			pm->send_done(PROP_CB_CONN_ERR, prop->send_dest,
			    prop->send_done_arg);
		} else if (prop->prop_mgr_done) {
			prop->prop_mgr_done(prop);
		}*/
		prop_mgr_report_status(prop, PROP_CB_CONN_ERR,
		    prop->send_dest, prop->send_done_arg);
	}
}

void prop_mgr_connect_sts(u8 mask)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	u8 lost_dests;
	u8 added_dests;

	if (mask == prop_mgr_unfiltered_mask) {
			/* no change, don't call PMs */
			goto callbacks_done;
		}
	prop_mgr_unfiltered_mask = mask;
	
	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (pm->connect_status) {
			if (!(mask & NODES_ADS)) {
				pg->state &= ~PROP_MGR_ENABLED;
			}
			pm->connect_status(mask);
		}
	}
	if (mask & NODES_ADS) {
		prop_mgr_listen_check();
	}
callbacks_done:
	if (prop_mgr_svc_status == ADA_SVC_OVERLOADED) {
		/* treat overloaded same as ADS down in prop_mgr */
		mask &= ~NODES_ADS;
	}
	if (mask == prop_mgr_dest_mask) {
			return;
	}
	/*
	 * Determine which destinations have been lost and added.
	 */
	lost_dests = prop_mgr_dest_mask & ~mask;
	added_dests = ~prop_mgr_dest_mask & mask;
	prop_mgr_dest_mask = mask;

	/*
	 * If connectivity has improved, send pending properties.
	 */
	if (added_dests) {
		client_lock();
		prop_mgr_send_next(NULL);
		client_unlock();
	}

	/*
	 * If some connectivity has been lost, check queued property updates.
	 */
	if (lost_dests) {
		prop_mgr_send_queue_check(mask);
	}
}

void prop_mgr_event(enum prop_mgr_event event, void *event_specific_arg)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;

	struct prop *prop;
	u16 txn_id;
	switch(event)
	{
		case PME_TIMEOUT:
			prop = prop_mgr_sending;
			if(!prop)
			{
				txn_id = (u16)(u32)event_specific_arg;
				prop = prop_mgr_find_waiting(txn_id);
			}
			if(prop)
			{
				pm = prop->mgr;
				if(pm && pm->event)
				{
					pm->event(event,event_specific_arg);
				}
			}
			return;
		#if 0
		case PME_SVC_STATUS:
			prop_mgr_svc_status = (enum ada_svc_status) event_specific_arg;
			prop_mgr_connect_sts(prop_mgr_unfiltered_mask);
		break;
		#endif
		default:
			break;
	}
	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;

		/*
		 * PME_TIMEOUT events are sent only to the active prop_mgr.
		 * Other events go to all prop_mgrs.
		 */
		/*if (event == PME_TIMEOUT &&
		    (prop_mgr_sending && pm != prop_mgr_sending->mgr)) {
			continue;
		}*/
		if (pm->event) {
			pm->event(event, event_specific_arg);
		}
	}
}

void ada_prop_mgr_register(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	pg = calloc(1, sizeof(*pg));
	ASSERT(pg);
	pg->mgr = pm;
	pg->next = prop_mgrs;
	prop_mgrs = pg;
}

void ada_prop_mgr_ready(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	pg = prop_mgr_node_lookup(pm);
	if (!pg || (pg->state & PROP_MGR_ENABLED)) {
		return;
	}
	pg->state |= PROP_MGR_ENABLED;
	prop_mgr_listen_check();
}
static enum ada_err prop_mgr_cb_done(struct prop *prop,
    enum prop_cb_status stat, u8 dests)
{
	client_log(LOG_DEBUG2 "%s:%s", __func__, prop->name);

	ASSERT(!prop->queue);
	prop_mgr_report_status(prop, stat,
	    prop->send_dest, prop->send_done_arg);
	client_callback_pend(&prop_mgr_send_callback);
	return AE_OK;
}
	static void prop_mgr_send_timeout(struct timer *timer)
{
	struct prop *prop = CONTAINER_OF(struct prop, timer, timer);

	client_log(LOG_WARN "prop %s send timeout", prop->name);
	prop_mgr_send_cb(PROP_CB_TIMEOUT, prop->txn_id,NULL);
}
/*
 * Callback from client state machine to send a property or a request.
 */
static enum ada_err prop_mgr_send_cb(enum prop_cb_status stat,u16 txn_id, void *arg)
{
	struct prop *prop;
	const struct prop_mgr *pm;
	enum ada_err err = AE_OK;
	int serialize = 0;
	ASSERT(client_locked);

	switch (stat) {
	case PROP_CB_BEGIN:
		serialize = 1;
		prop = prop_mgr_sending;
		if (prop) {
			/* same prop, next client */
			goto send_prop;
		}
		/* next property */
		goto send_next_prop;
		
	case PROP_CB_BEGIN_MULTI:
		do {
send_next_prop:
			ada_lock(prop_mgr_lock);
			prop = STAILQ_FIRST(&prop_mgr_send_queue);
			if (!prop) {
				ada_unlock(prop_mgr_lock);
				break;
			}
			ASSERT(prop->queue == &prop_mgr_send_queue);
			STAILQ_REMOVE_HEAD(&prop_mgr_send_queue, list);
			STAILQ_NEXT(prop, list) = NULL;
			prop->queue = NULL;
			if (serialize) {
				prop_mgr_sending = prop;
			} else {
				/*
				 * put on wait queue first in case callback is
				 * immediate
				 */
				STAILQ_INSERT_TAIL(&prop_mgr_wait_queue, prop,
				    list);
				prop->queue = &prop_mgr_wait_queue;
				client_timer_set(&prop->timer, PROP_SEND_TIMEOUT);
			}
			ada_unlock(prop_mgr_lock);
send_prop:
			if (!prop->val) {	/* request property from ADS */
				err = client_get_prop_val(prop->name);
			}
			else
			{
				err = client_send_data(prop);
			}
			if (err == AE_BUSY) 
			{
				ada_lock(prop_mgr_lock);
				if(prop->queue)
				{	ASSERT(prop->queue == &prop_mgr_wait_queue);
					STAILQ_REMOVE(&prop_mgr_wait_queue,prop, prop, list);
				}
				prop_mgr_sending = NULL;
				STAILQ_INSERT_HEAD(&prop_mgr_send_queue, prop, list);
				prop->queue = &prop_mgr_send_queue;
				ada_unlock(prop_mgr_lock);
				break;
			}
		}while(!serialize);
		break;
		/* fall through */
	case PROP_CB_CONTINUE:
		prop = prop_mgr_sending;
		if (!prop) {
			break;
		}
		if (prop->type == ATLV_BATCH) {
			if (!batch_is_all_dps_sent(prop->val)) {
				client_callback_pend(&prop_mgr_send_callback);
				err = AE_OK;
			}
			break;
		}
	default:
		if(prop_mgr_sending)
		{
			prop = prop_mgr_sending;
			prop_mgr_sending = NULL;
		}
		else 
		{
			if(txn_id)
			{
				prop = prop_mgr_find_waiting(txn_id);
				if(!prop)
				{
					client_log(LOG_DEBUG2
								"prop not found txn_id %u",txn_id);
					break;
				}
				client_timer_cancel(&prop->timer);
				ada_lock(prop_mgr_lock);
				STAILQ_REMOVE(&prop_mgr_wait_queue,prop,prop,list);
				STAILQ_NEXT(prop,list) = NULL;
				prop->queue = NULL;
				ada_unlock(prop_mgr_lock);
			}
			else
			{
				client_log(LOG_DEBUG2"prop not found");
				break;
			}
			
		}
		ASSERT(!prop->queue);
		prop_mgr_sending = NULL;
		if((stat == PROP_CB_DONE) && prop->val)
		{
			prop_mgr_event(PME_PROP_SET,(void *)prop->name);
		}
		err = prop_mgr_cb_done(prop,stat, client_get_failed_dests());
#if 0
		//pm = prop->mgr;
		if (pm) {
			ASSERT(pm->send_done);
			pm->send_done(stat, client_get_failed_dests(),
			    prop->send_done_arg);
		} else if (prop->prop_mgr_done) {
			prop->prop_mgr_done(prop);
		}
		client_callback_pend(&prop_mgr_send_callback);
		err = AE_OK;
#endif
		break;
	}
	return err;
}

/*
 * Start send for next property on the queue.
 * This is called through a callback so it may safely drop the client_lock.
 */
static void prop_mgr_send_next(void *arg)
{
	struct prop_queue *qp = &prop_mgr_send_queue;
	struct prop *prop;
	struct prop *prev = NULL;
	
	ASSERT(client_locked);
	for (;;) {
		if (!prop_mgr_dest_mask) {
			return;
		}
		if (prop_mgr_sending) {
			if (prop_mgr_sending->type == ATLV_BATCH) {
				client_send_callback_set(prop_mgr_send_cb,
				    prop_mgr_sending);
			} else {
				client_log(LOG_DEBUG2 "%s busy", __func__);
			}
			return;
		}
		ada_lock(prop_mgr_lock);
		prop = STAILQ_FIRST(qp);
		ada_unlock(prop_mgr_lock);
		if (!prop || prop == prev) {
			return;
		}
		ASSERT(prop->queue == qp);
		client_log(LOG_DEBUG2 "%s send %s", __func__, prop->name);
		prev = prop;
		//STAILQ_REMOVE_HEAD(qp, list);
		//prop_mgr_sending = prop;
		//ada_unlock(prop_mgr_lock);
		client_unlock();

		client_send_callback_set(prop_mgr_send_cb, prop);

		client_lock();
	}
}
enum ada_err ada_prop_mgr_prop_send(struct prop *prop)
{

	client_log(LOG_DEBUG2 "%s %s", __func__, prop->name);

	if (prop->type == ATLV_LOC) {
		return AE_INVAL_VAL;
	}
	timer_init(&prop->timer, prop_mgr_send_timeout);
	ada_prop_mgr_enqueue(prop);
	return AE_IN_PROGRESS;
}
/*
 * Send a property.
 * The prop structure must be available until (*send_cb)() is called indicating
 * the value has been completely sent.
 */
enum ada_err ada_prop_mgr_send(const struct prop_mgr *pm, struct prop *prop,
			u8 dest_mask, void *cb_arg)
{
	if (!dest_mask) {
		return AE_INVAL_STATE;
	}
	prop->mgr = pm;
	prop->send_dest = dest_mask;
	prop->send_done_arg = cb_arg;
	//timer_init(&prop->timer, prop_mgr_send_timeout);
	//ada_prop_mgr_enqueue(prop);
	//return AE_IN_PROGRESS;
	return ada_prop_mgr_prop_send(prop);
}

/*
 * Get a prop using a property manager.
 */
enum ada_err ada_prop_mgr_get(const char *name,
		enum ada_err (*get_cb)(struct prop *, void *arg, enum ada_err),
		void *arg)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	enum ada_err err;

	if (!prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}

	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (!pm->get_val) {
			continue;
		}
		err = pm->get_val(name, get_cb, arg);
		if (err != AE_NOT_FOUND) {
			return err;
		}
	}
	return AE_NOT_FOUND;
}

/*
 * Set a prop using a property manager.
 */
enum ada_err ada_prop_mgr_set(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len,
			size_t *offset, u8 src, void *set_arg)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	enum ada_err err = AE_NOT_FOUND;

	if (!prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}
	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (!pm->prop_recv) {
			continue;
		}
		err = pm->prop_recv(name, type, val, val_len,
		    offset, src, set_arg);
		if (err != AE_NOT_FOUND) {
			if (err == AE_OK || err == AE_IN_PROGRESS) {
				prop_mgr_event(PME_PROP_SET, (void *)name);
			} else {
				client_log(LOG_WARN "%s: name %s err %d",
				    __func__, name, err);
			}
			break;
		}
	}
	return err;
}

/*
 * Returns 1 if the property type means the value has to be in quotes
 */
int prop_type_is_str(enum ayla_tlv_type type)
{
	return type == ATLV_UTF8 || type == ATLV_BIN || type == ATLV_SCHED ||
	    type == ATLV_LOC;
}

/*
 * Format a property value.
 * Returns the number of bytes placed into the value buffer.
 */
size_t prop_fmt(char *buf, size_t len, enum ayla_tlv_type type,
		void *val, size_t val_len, char **out_val)
{
	int rc;
	size_t i;
	s32 ones;
	unsigned int tenths;
	unsigned int cents;
	char *sign;

	*out_val = buf;
	switch (type) {
	case ATLV_INT:
		rc = snprintf(buf, len, "%ld", *(s32 *)val);
		break;

	case ATLV_UINT:
		rc = snprintf(buf, len, "%lu", *(u32 *)val);
		break;

	case ATLV_BOOL:
		rc = snprintf(buf, len, "%u", *(u8 *)val != 0);
		break;

	case ATLV_UTF8:
		/* for strings, use the original buffer */
		ASSERT(val_len <= TLV_MAX_STR_LEN);
		rc = val_len;
		*out_val = val;
		break;

	case ATLV_CENTS:
		ones = *(s32 *)val;
		sign = "";
		if (ones < 0 && ones != 0x80000000) {
			sign = "-";
			ones = -ones;
		}
		tenths = (unsigned)ones;
		cents = tenths % 10;
		tenths = (tenths / 10) % 10;
		ones /= 100;
		rc = snprintf(buf, len, "%s%ld.%u%u",
		    sign, ones, tenths, cents);
		break;

	case ATLV_BIN:
	case ATLV_SCHED:
		rc = net_base64_encode((u8 *)val, val_len, buf, &len);
		if (rc < 0) {
			log_put("prop_fmt: enc fail rc %d", rc);
			buf[0] = '\0';
			return 0;
		}
		rc = len;
		break;
	default:
		rc = 0;
		for (i = 0; i < val_len; i++) {
			rc += snprintf(buf + rc, len - rc, "%2.2x ",
			    *((unsigned char *)val + i));
		}
		break;
	}
	return rc;
}

/*
 * Return non-zero if property name is valid.
 * Valid names need no XML or JSON encoding.
 * Valid names include: alphabetic chars numbers, hyphen and underscore.
 * The first character must be alphabetic.  The max length is 27.
 *
 * This could use ctypes, but doesn't due to library license.
 */
int prop_name_valid(const char *name)
{
	const char *cp = name;
	char c;
	size_t len = 0;

	while ((c = *cp++) != '\0') {
		len++;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			continue;
		}
		if (len > 1) {
			if (c >= '0' && c <= '9') {
				continue;
			}
			if (c == '_' || c == '-') {
				continue;
			}
		}
		return 0;
	}
	if (len == 0 || len >= PROP_NAME_LEN) {
		return 0;
	}
	return 1;
}

static void ada_prop_mgr_request_done(struct prop *prop)
{
	free(prop);
}

/*
 * Request a property (or all properties if name is NULL).
 */
enum ada_err ada_prop_mgr_request(const char *name)
{
	struct prop *prop;

	if (name && !prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}

	prop = calloc(1, sizeof(*prop));
	if (!prop) {
		return AE_ALLOC;
	}

	/*
	 * GET request is indicated by the NULL val pointer.
	 * A NULL or empty name will indicate all to-device properties.
	 */
	prop->name = name;
	prop->send_dest = NODES_ADS;
	prop->prop_mgr_done = ada_prop_mgr_request_done;

	ada_prop_mgr_enqueue(prop);
	return 0;
}
void ada_prop_mgr_final(void)
{
	struct prop_mgr_node *pg;
	struct prop_mgr_node *next;
	struct prop *prop;
	struct batch *b;

	prop = prop_mgr_sending;
	if (prop) {
		prop_mgr_sending = NULL;
		free(prop);
	}
	ada_lock(prop_mgr_lock);
	while (1) {
		prop = STAILQ_FIRST(&prop_mgr_send_queue);
		if (!prop) {
			break;
		}
		STAILQ_REMOVE_HEAD(&prop_mgr_send_queue, list);
		if (prop->type == ATLV_BATCH) {
			b = (struct batch *)prop->val;
			batch_buf_free(b);
		}
		free(prop);
	}
	ada_unlock(prop_mgr_lock);

	pg = prop_mgrs;
	prop_mgrs = NULL;
	while (pg) {
		next = pg->next;
		free(pg);
		pg = next;
	}
}

