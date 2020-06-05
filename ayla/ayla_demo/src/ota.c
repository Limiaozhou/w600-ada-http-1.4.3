/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
//#define HAVE_UTYPES
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

//#include <device_lock.h>
//#include <ota_8195a.h>
#undef LOG_INFO
#undef LOG_DEBUG
#undef CONTAINER_OF

#include <ayla/utypes.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>

#include <ada/libada.h>
#include <ada/client_ota.h>
#include <net/base64.h>
#include <net/net.h>
#include "conf.h"

#include "FreeRTOSConfig.h"
#include "wm_osal.h"
#include "wm_fwup.h"
#include "wm_crypto_hard.h"
#include "wm_internal_flash.h"

#define FLASH_BASE_ADDR	(0x8000000)

struct demo_ota {
	u32 exp_len;
	u32 rx_len;

	u32 partition_addr;
	u32 partition_size;
    u32 org_checksum;
    u32 updated_len;
	u8 save_done;
};
static struct demo_ota demo_ota;

static enum patch_state demo_ota_notify(unsigned int len, const char *ver)
{
	struct demo_ota *ota = &demo_ota;

	log_put(LOG_INFO "OTA notification: length %u version \"%s\"\r\n",
	    len, ver);

	if (len < 4) {
		return PB_ERR_PHEAD;
	}

	ota->exp_len = len;
	ota->rx_len = 0;
	ota->save_done = 0;

#ifdef AYLA_DEMO_TEST
	ada_api_call(ADA_OTA_START, OTA_HOST);
#else
	ada_ota_start(OTA_HOST);
#endif
	return PB_DONE;
}

static enum patch_state demo_ota_save(unsigned int offset, const void *buf,
	size_t req_len)
    {
        struct demo_ota *ota = &demo_ota;   
        static T_BOOTER booter;
        u8 *buffer = NULL;
        u32 len = 0;
    
        buffer = (u8 *)buf;
        if(ota->rx_len < sizeof(T_BOOTER))
        {
            len = sizeof(T_BOOTER) - ota->rx_len;
            if(req_len < len) {
				len = req_len;
			}
            memcpy(&booter, buffer, sizeof(T_BOOTER));
            req_len -= len;
            buffer += len;
            ota->rx_len += len;
            if(ota->rx_len == sizeof(T_BOOTER))
            {               
                if (!tls_fwup_img_header_check(&booter))
                {
                    return PB_ERR_PHEAD;
                }
                
                {
                    ota->partition_addr = booter.upd_img_addr | FLASH_BASE_ADDR;
                    ota->exp_len = booter.upd_img_len;
                    ota->org_checksum = booter.upd_checksum;
                }
    
                printcli("%x, %d, %x", ota->partition_addr, ota->exp_len, ota->org_checksum);
                ota->updated_len = 0;
            }
        }
    
        ota->rx_len += req_len;
        (offset > sizeof(T_BOOTER))?(offset = offset-sizeof(T_BOOTER)):(offset = offset);
    
        if (req_len > 0) 
        {
            int ret = tls_fls_fast_write(ota->partition_addr + offset, buffer, req_len);
            if(ret != TLS_FLS_STATUS_OK) {
                return PB_ERR_WRITE;
            }
            ota->updated_len += req_len;
    
            if(ota->updated_len >= ota->exp_len) 
            {
                u8 *buffer_t;
                u32 len_p=0, left=0, offset_p=0;
                u32 image_checksum = 0;
                psCrcContext_t  crcContext;
    
                tls_fls_fast_write_destroy();
                buffer_t = tls_mem_alloc(1024);
                if (buffer_t == NULL) 
                {
                    return PB_ERR_FATAL;
                }
                else 
                {                           
                    tls_crypto_crc_init(&crcContext, 0xFFFFFFFF, CRYPTO_CRC_TYPE_32, 3);
                    offset_p = 0;
                    left = ota->exp_len;
                    while (left > 0) 
                    {
                        len_p = left > 1024 ? 1024 : left;
                        tls_fls_read(ota->partition_addr + offset_p, buffer_t, len_p);
                        tls_crypto_crc_update(&crcContext, buffer_t, len_p);
                        offset_p += len_p;
                        left -= len_p;
                    }
                    tls_crypto_crc_final(&crcContext, &image_checksum);
                    tls_mem_free(buffer_t);
                }
                if (ota->org_checksum != image_checksum)            
                {
                    log_put(LOG_WARN "varify incorrect[0x%02x, but 0x%02x]\n", ota->org_checksum, image_checksum);
                    return PB_ERR_FILE_CRC;
                }
                tls_fwup_img_update_header(&booter);
            }
        }
    
        ota->save_done = 1;
        
        return PB_DONE;
    }


static void demo_ota_save_done(void)
{
	struct demo_ota *ota = &demo_ota;
	enum patch_state status;

	if (!ota->save_done) {
		log_put(LOG_WARN "OTA save_done: OTA not completely saved");
		status = PB_ERR_FATAL;
#ifdef AYLA_DEMO_TEST
		ada_api_call(ADA_OTA_REPORT, OTA_HOST, status);
#else
		ada_ota_report(OTA_HOST, status);
#endif
		return;
	}

    tls_os_time_delay(HZ);
    tls_sys_reset();
}

static struct ada_ota_ops demo_ota_ops = {
	.notify = demo_ota_notify,
	.save = demo_ota_save,
	.save_done = demo_ota_save_done,
};

void demo_ota_init(void)
{
    
    tls_fls_fast_write_init();
	ada_ota_register(OTA_HOST, &demo_ota_ops);
}
