/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_AME_JSON_H__
#define __AYLA_ADA_AME_JSON_H__

/* Initialize encoder to be a JSON encoder */
enum ada_err ame_json_enc_init(struct ame_encoder *enc);
/* Initialize decoder to be a JSON decoder */
enum ada_err ame_json_dec_init(struct ame_decoder *dec);
#ifdef AME_ENCODER_TEST
void ame_json_test(void);
#endif
#endif
