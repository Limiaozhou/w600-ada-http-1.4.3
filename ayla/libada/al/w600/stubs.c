/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <sys/types.h>
#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/xml.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/clock.h>
#include <ayla/conf.h>
#include <ayla/http.h>
#include <ayla/nameval.h>
#include <ayla/timer.h>
#include <ayla/patch_state.h>

#include <net/stream.h>
#include <net/net.h>
#include <net/http_client.h>
#include <net/net_crypto.h>

#include <ada/err.h>
#include <ada/ada_conf.h>
#include <ada/ada_lan_conf.h>
#include <ada/client.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/metric.h>
#include <ada/server_req.h>
#include <ada/client_ota.h>
#include <ayla/malloc.h>

#include "notify_int.h"
#include "client_int.h"

void random_fill(void *buf, size_t len)
{
	static struct adc_rng rng;

	adc_rng_init(&rng);		/* inits only if not already done */
	if (adc_rng_random_fill(&rng, buf, len)) {
		ASSERT_NOTREACHED();
	}
}



