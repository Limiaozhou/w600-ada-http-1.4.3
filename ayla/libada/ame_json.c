/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * JSON-based message encoding and decoding support.
 */
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/base64.h>
#include <ayla/utf8.h>
#include "ame.h"

/* #define AME_JSON_DEBUG */
/* #define AME_JSON_DEBUG2 */

#if defined(AME_JSON_DEBUG) || defined(AME_JSON_DEBUG2) || \
    defined(AME_ENCODER_TEST)
#include <ayla/log.h>
#include <ayla/mod_log.h>
#endif

static inline size_t ame_json_calc_len(struct ame_encoder_state *es)
{
	if (es->offset > es->size) {
		return 0;
	}
	return es->size - es->offset;
}

static inline void ame_json_enc_char(struct ame_encoder_state *es,
    const char c)
{
	if (es->offset < es->size) {
		es->buffer[es->offset] = (u8)c;
	}
	es->offset++;
}

static void ame_json_enc_quoted_string(struct ame_encoder_state *es,
    const char *string)
{
	char unicode_str[5];

	ame_json_enc_char(es, '"');
	while (*string != 0) {
		if (*string < 0x20) {
			ame_json_enc_char(es, '\\');
			switch (*string) {
			case '\b':
				ame_json_enc_char(es, 'b');
				break;
			case '\f':
				ame_json_enc_char(es, 'f');
				break;
			case '\n':
				ame_json_enc_char(es, 'n');
				break;
			case '\r':
				ame_json_enc_char(es, 'r');
				break;
			case '\t':
				ame_json_enc_char(es, 't');
				break;
			default:
				ame_json_enc_char(es, 'u');
				snprintf(unicode_str, sizeof(unicode_str),
				    "%04x", (u16)*(u8 *)string);
				ame_json_enc_char(es, unicode_str[0]);
				ame_json_enc_char(es, unicode_str[1]);
				ame_json_enc_char(es, unicode_str[2]);
				ame_json_enc_char(es, unicode_str[3]);
				break;
			}
		} else {
			switch (*string) {
			case '"':
			/* escaping the solidus character is optional
			case '/' :
			*/
			case '\\':
				ame_json_enc_char(es, '\\');
				ame_json_enc_char(es, *string);
				break;
			default:
				ame_json_enc_char(es, *string);
				break;
			}
		}
		string++;
	}
	ame_json_enc_char(es, '"');
}

static void ame_json_enc_prefix(struct ame_encoder_state *es, u32 flags)
{
	if (flags & EF_PREFIX_C) {
		ame_json_enc_char(es, ',');
	}
	if (flags & EF_PREFIX_A) {
		ame_json_enc_char(es, '[');
	}
	if (flags & EF_PREFIX_O) {
		ame_json_enc_char(es, '{');
	}
}

static void ame_json_enc_suffix(struct ame_encoder_state *es, u32 flags)
{
	if (flags & EF_SUFFIX_E) {
		ame_json_enc_char(es, '}');
	}
	if (flags & EF_SUFFIX_Z) {
		ame_json_enc_char(es, ']');
	}
	if (flags & EF_SUFFIX_M) {
		ame_json_enc_char(es, ',');
	}
}

/*
 * Encode the prefix and the key.
 * If the key is null, just encode the prefix.
 */
static void ame_json_enc_key(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key)
{
	ame_json_enc_prefix(es, flags);
	if (key) {
		ame_json_enc_quoted_string(es, key->tag);
		ame_json_enc_char(es, ':');
	}
}

static void ame_json_enc_s64(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s64 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%lld", value);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_s32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_s16(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s16 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_s8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s8 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_u64(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u64 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%llu", value);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_u32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u32 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_u16(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u16 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_u8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u8 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_d32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value, u8 scale)
{
	s32 factor = 1;
	s32 left;
	s32 right;
	size_t count;
	u8 i = scale;
	char format_str[16];

	ASSERT(scale < 10);

	while (i--) {
		factor *= 10;
	}
	left = value / factor;
	right = value % factor;
	/* must manage minus sign separately in case left is zero */
	if (value < 0) {
		left = -left;
		right = -right;
	}

	snprintf(format_str, sizeof(format_str), "%%s%%ld.%%0%uld", scale);

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), format_str,
	    value < 0 ? "-" : "", left, right);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_null(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%s", "null");
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_utf8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, const char *utf8)
{
	if (utf8) {
		ame_json_enc_key(es, flags, key);
		ame_json_enc_quoted_string(es, utf8);
		ame_json_enc_suffix(es, flags);
	} else {
		ame_json_enc_null(es, flags, key);
	}
}

static void ame_json_enc_opaque(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, const void *data, size_t length)
{
	size_t outlen;
	if (data) {
		ame_json_enc_key(es, flags, key);
		ame_json_enc_char(es, '"');
		if (es->size > es->offset) {
			outlen = es->size - es->offset;
			if (!ayla_base64_encode(data, length,
			    &es->buffer[es->offset], &outlen)) {
				goto done;
			}
		}
		/* Insufficient space. Calculate space required. */
		outlen = 4 * ((length + 2) / 3);
done:
		es->offset += outlen;
		ame_json_enc_char(es, '"');
		ame_json_enc_suffix(es, flags);
	} else {
		ame_json_enc_null(es, flags, key);
	}
}

static void ame_json_enc_boolean(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	if (value) {
		count = snprintf((char *)&es->buffer[es->offset],
		    ame_json_calc_len(es), "%s", "true");
	} else {
		count = snprintf((char *)&es->buffer[es->offset],
		    ame_json_calc_len(es), "%s", "false");
	}
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

enum ada_err ame_json_enc_init(struct ame_encoder *enc)
{
	enc->enc_prefix = ame_json_enc_prefix;
	enc->enc_suffix = ame_json_enc_suffix;
	enc->enc_key = ame_json_enc_key;
	enc->enc_s64 = ame_json_enc_s64;
	enc->enc_s32 = ame_json_enc_s32;
	enc->enc_s16 = ame_json_enc_s16;
	enc->enc_s8 = ame_json_enc_s8;
	enc->enc_u64 = ame_json_enc_u64;
	enc->enc_u32 = ame_json_enc_u32;
	enc->enc_u16 = ame_json_enc_u16;
	enc->enc_u8 = ame_json_enc_u8;
	enc->enc_d32 = ame_json_enc_d32;
	enc->enc_utf8 = ame_json_enc_utf8;
	enc->enc_opaque = ame_json_enc_opaque;
	enc->enc_boolean = ame_json_enc_boolean;
	enc->enc_null = ame_json_enc_null;

	return AE_OK;
}

static void ame_json_byte_backward(struct ame_decoder_state *ds)
{
	if (ds->ext_mode) {
		ds->data_len -= 1;
	}
	ds->offset -= 1;
}

static void ame_json_byte_forward(struct ame_decoder_state *ds)
{
	if (ds->ext_mode) {
		ds->data_buf[ds->data_len] = ds->buffer[ds->offset];
		ds->data_len++;
	}
	ds->offset++;
}

static enum ada_err ame_json_parse_string(struct ame_decoder_state *ds, u8 c)
{
	static int escape;
	static int hexdigs;

	if (ds->state == AME_STATE_VALUE || ds->state == AME_STATE_NAME) {
		if (c != '"') {
			return AE_PARSE;
		}
		escape = 0;
		hexdigs = 0;
	} else if (ds->state == AME_STATE_VALUE_STR ||
	    ds->state == AME_STATE_NAME_STR) {
		if (escape) {
			escape = 0;
			switch (c) {
			case '"':
			case '/':
			case '\\':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
				break;
			case 'u':
				hexdigs = 4;
				break;
			default:
				/* invalid escape sequence */
				return AE_PARSE;
			}
		} else if (hexdigs) {
			hexdigs--;
			if (!((c >= '0' && c <= '9') ||
			    (c >= 'a' && c <= 'f') ||
			    (c >= 'A' && c <= 'F'))) {
				/* not a hex digit */
				return AE_PARSE;
			}
		} else {
			switch (c) {
			case '\\':
				escape = 1;
				break;
			case '"':
				/* end of string */
				return AE_OK;
			default:
				if (c < 0x20) {
					/* unescaped control character */
					return AE_PARSE;
				}
				break;
			}
		}
	}
	return AE_IN_PROGRESS;
}

static enum ada_err ame_json_parse_number(struct ame_decoder_state *ds,
		enum ame_type *type, u8 c)
{
	/* Parse it byte by byte. -1.234E123 */
	switch (ds->state) {
	case AME_STATE_VALUE:
		/* must be one of: '-', '0~9' */
		if (c == '-') {
			*type = AME_TYPE_INTEGER;
			ds->state = AME_STATE_VALUE_NUM_1;
			break;
		}
		if (c >= '0' && c <= '9') {
			*type = AME_TYPE_INTEGER;
			ds->state = AME_STATE_VALUE_NUM_2;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_1:
		/* must be one of: '0~9' */
		if (c >= '0' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_2;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_2:
		/* expect: '.', 'e', 'E', 0~9' */
		if (c == '.') {
			*type = AME_TYPE_DECIMAL;
			ds->state = AME_STATE_VALUE_NUM_3;
			break;
		}
		if (c == 'e' || c == 'E') {
			*type = AME_TYPE_FLOAT;
			ds->state = AME_STATE_VALUE_NUM_4;
			break;
		}
		if (c >= '0' && c <= '9') {
			break;
		}
		ame_json_byte_backward(ds);
		return AE_OK;

	case AME_STATE_VALUE_NUM_3:
		/* expect: 'e', 'E', 0~9' */
		if (c == 'e' || c == 'E') {
			*type = AME_TYPE_FLOAT;
			ds->state = AME_STATE_VALUE_NUM_4;
			break;
		}
		if (c >= '0' && c <= '9') {
			break;
		}
		ame_json_byte_backward(ds);
		return AE_OK;

	case AME_STATE_VALUE_NUM_4:
		/* must be one of: '0~9' */
		if (c >= '0' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_5;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_5:
		/* expect: '0~9' */
		if (c >= '0' && c <= '9') {
			break;
		}
		ame_json_byte_backward(ds);
		return AE_OK;

	default:
		return AE_PARSE;
	}
	return AE_IN_PROGRESS;
}

static enum ada_err ame_json_parse_literal(struct ame_decoder_state *ds,
    enum ame_type *type, u8 c)
{
	static int chars;
	static u8 *literal;

	if (ds->state == AME_STATE_VALUE) {
		switch (c) {
		case 't':
			literal = (u8 *)"true";
			*type = AME_TYPE_BOOLEAN;
			break;
		case 'f':
			literal = (u8 *)"false";
			*type = AME_TYPE_BOOLEAN;
			break;
		case 'n':
			literal = (u8 *)"null";
			*type = AME_TYPE_NULL;
			break;
		default:
			return AE_INVAL_VAL;
		}
		chars = 1;
	} else if (ds->state == AME_STATE_VALUE_LITERAL) {
		if (c != literal[chars]) {
			return AE_PARSE;
		}
		++chars;
		if (literal[chars] == '\0') {
			return AE_OK;
		}
	}
	return AE_IN_PROGRESS;
}


static void ame_json_set_kvp_value(struct ame_decoder_state *ds,
		struct ame_kvp *kvp, int n)
{
	if (ds->ext_mode) {
		kvp->value = &ds->data_buf[ds->data_len] + n;
	} else {
		kvp->value = &ds->buffer[ds->offset] + n;
	}
}

static void ame_json_set_kvp_val_len(struct ame_decoder_state *ds,
		struct ame_kvp *kvp, int n)
{
	if (ds->ext_mode) {
		kvp->value_length = &ds->data_buf[ds->data_len]
		    - (u8 *)kvp->value + n;
	} else {
		kvp->value_length = &ds->buffer[ds->offset]
		    - (u8 *)kvp->value + n;
	}
}

static void ame_json_set_kvp_key(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	if (ds->ext_mode) {
		kvp->key = &ds->data_buf[ds->data_len] + 1;
	} else {
		kvp->key = &ds->buffer[ds->offset] + 1;
	}
}

static void ame_json_set_kvp_key_len(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	if (ds->ext_mode) {
		kvp->key_length = &ds->data_buf[ds->data_len] - (u8 *)kvp->key;
	} else {
		kvp->key_length = &ds->buffer[ds->offset] - (u8 *)kvp->key;
	}
}

static void ame_json_pop_children(struct ame_decoder_state *ds,
		struct ame_kvp *parent)
{
	struct ame_kvp *child;
	size_t old_len;

	if (ds->ext_mode) {
		child = parent->child;
		if (child) {
			old_len = ds->data_len;
			if (child->key) {
				ds->data_len = ((u8 *)child->key
				    - ds->data_buf);
			} else if (child->value) {
				ds->data_len = ((u8 *)child->value
				    - ds->data_buf);
			} else {
				ASSERT(0);
			}
			if (ds->data_buf[ds->data_len - 1] == ',') {
				ds->data_len -= 1;
			}
			memset(ds->data_buf + ds->data_len, 0,
			    old_len - ds->data_len);
		}
	}
	ds->kvp_used = (parent - ds->kvp_stack) + 1;
	parent->child = NULL;
	parent->value = NULL;
}

static enum ada_err ame_json_parse(struct ame_decoder_state *ds,
    enum ame_parse_op op, struct ame_kvp **pkvp)
{
	enum ada_err err = AE_PARSE;
	enum ame_parser_state save_state;
	size_t save_offset;
	size_t save_used;
	struct ame_kvp *save_parent;
	u8 c;

	ds->error_offset = 0;
	switch (op) {
	case AME_PARSE_FULL:
	default:
		ASSERT(ds->ext_mode == 0);
		ame_decoder_reset(ds);
		ds->kvp = NULL;
		ds->incr_parent = NULL;
		ds->is_first = 0;
		*pkvp = NULL;
		ds->pkvp = *pkvp;
		ds->incremental = 0;
		break;
	case AME_PARSE_FIRST:
		ame_decoder_reset(ds);
		ds->kvp = NULL;
		ds->incr_parent = NULL;
		ds->is_first = 0;
		*pkvp = NULL;
		ds->pkvp = *pkvp;
		ds->incremental = 1;
		ds->first_called = 1;
		break;
	case AME_PARSE_NEXT:
		ASSERT(ds->first_called);
		if (ds->ext_mode) {
			if (ds->obj_parsed) {
				/* last object parsed, start a new one */
				ds->kvp = NULL;
				*pkvp = NULL;
				ds->pkvp = *pkvp;
				ds->incremental = 1;
				ds->incr_parent = ds->parent;
				if (!ds->incr_parent) {
					/* done parsing root object */
					return AE_OK;
				}
				/* pop the children objects */
				ame_json_pop_children(ds, ds->incr_parent);
				ds->obj_parsed = 0;
			} else {
				*pkvp = ds->pkvp;
			}
		} else {
			ds->kvp = NULL;
			ds->is_first = 0;
			*pkvp = NULL;
			ds->pkvp = *pkvp;
			ds->incremental = 1;
			ds->incr_parent = ds->parent;
			if (!ds->incr_parent) {
				/* done parsing root object */
				return AE_OK;
			}
			ds->kvp_used = (ds->incr_parent - ds->kvp_stack) + 1;
			ds->incr_parent->child = NULL;
			ds->incr_parent->value = NULL;
			break;
		}
		break;
	}

	/* save state so parser can be restarted */
	save_offset = ds->offset;
	save_state = ds->state;
	save_used = ds->kvp_used;
	save_parent = ds->parent;

	while (ds->offset < ds->length) {
		c = ds->buffer[ds->offset];
#ifdef AME_JSON_DEBUG2
		printcli("state %d, offset %d, used %d, parent %lx: %c",
		    ds->state, ds->offset, ds->kvp_used, (u32)ds->parent,
		    c);
#endif
		/* consume whitespace */
		switch (c) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			ame_json_byte_forward(ds);
			continue;
		}
		switch (ds->state) {
		case AME_STATE_VALUE:
			/* { here to prevent cstyle error */
			if (!ds->kvp) {
				err = ame_kvp_alloc(ds, &ds->kvp);
				if (err) {
					goto error_exit;
				}
			}
			switch (c) {
			case '[':
				ds->kvp->type = AME_TYPE_ARRAY;
				ame_json_set_kvp_value(ds, ds->kvp, 0);
				ds->parent = ds->kvp;
				ds->is_first = 1;
				/* remain in value state */
				break;
			case '{':
				ds->kvp->type = AME_TYPE_OBJECT;
				ame_json_set_kvp_value(ds, ds->kvp, 0);
				ds->parent = ds->kvp;
				ds->is_first = 1;
				ds->state = AME_STATE_NAME;
				break;
			case '"':
				ds->kvp->type = AME_TYPE_UTF8;
				ame_json_set_kvp_value(ds, ds->kvp, 1);
				err = ame_json_parse_string(ds, c);
				if (err == AE_IN_PROGRESS) {
					ame_json_byte_forward(ds);
					ds->state = AME_STATE_VALUE_STR;
					continue;
				} else if (err == AE_OK) {
value_string_end:
					ame_json_set_kvp_val_len(ds,
					    ds->kvp, 0);
					ds->state = AME_STATE_CONTINUE;
					break;
				}
				goto error_exit;

			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ame_json_set_kvp_value(ds, ds->kvp, 0);
				err = ame_json_parse_number(ds,
				    &ds->kvp->type, c);
				if (err == AE_IN_PROGRESS) {
					ame_json_byte_forward(ds);
					continue;
				} else if (err == AE_PARSE) {
val_number_end:
					ame_json_set_kvp_val_len(ds,
					    ds->kvp, 1);
					ds->state = AME_STATE_CONTINUE;
					break;
				}
				goto error_exit;
			case 't':
			case 'f':
			case 'n':
				ame_json_set_kvp_value(ds, ds->kvp, 0);
				err = ame_json_parse_literal(ds,
				    &ds->kvp->type, c);
				if (err == AE_IN_PROGRESS) {
					ame_json_byte_forward(ds);
					ds->state = AME_STATE_VALUE_LITERAL;
					continue;
				} else if (err == AE_OK) {
value_literal_end:
					ame_json_set_kvp_val_len(ds,
					    ds->kvp, 1);
					ds->state = AME_STATE_CONTINUE;
					break;
				}
				goto error_exit;

			case ']':
				if (!ds->is_first) {
					goto parse_error;
				}
				/* empty array */
				ame_kvp_pop(ds);
				ds->kvp = NULL;
				ame_json_byte_backward(ds);
				ds->state = AME_STATE_CONTINUE;
				break;
			default:
				goto parse_error;
			}
			if (!ds->incr_parent && ds->parent) {
				ds->incr_parent = ds->parent;
			}
			if (ds->incremental && !*pkvp) {
				if (ds->kvp != ds->incr_parent) {
					*pkvp = ds->kvp;
					ds->pkvp = *pkvp;
				} else if (
				    ds->kvp->type == AME_TYPE_OBJECT ||
				    ds->kvp->type == AME_TYPE_ARRAY) {
					ds->kvp->value = NULL;
					ds->incr_parent = ds->kvp;
				}
			}
			ds->kvp = NULL;
			/* } to prevent cstyle error */
			break;

		case AME_STATE_VALUE_NUM_1:
		case AME_STATE_VALUE_NUM_2:
		case AME_STATE_VALUE_NUM_3:
		case AME_STATE_VALUE_NUM_4:
		case AME_STATE_VALUE_NUM_5:
			err = ame_json_parse_number(ds, &ds->kvp->type, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				goto val_number_end;
				break;
			}
			goto error_exit;

		case AME_STATE_VALUE_LITERAL:
			err = ame_json_parse_literal(ds, &ds->kvp->type, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				goto value_literal_end;
			}
			goto error_exit;

		case AME_STATE_VALUE_STR:
			err = ame_json_parse_string(ds, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				goto value_string_end;
			}
			goto error_exit;

		case AME_STATE_NAME_STR:
			err = ame_json_parse_string(ds, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				goto name_string_end;
			}
			goto error_exit;

		case AME_STATE_NAME:
			/* { to prevent cstyle error */
			switch (c) {
			case '"':
				err = ame_kvp_alloc(ds, &ds->kvp);
				if (err) {
					goto error_exit;
				}
				ame_json_set_kvp_key(ds, ds->kvp);
				err = ame_json_parse_string(ds, c);
				if (err == AE_IN_PROGRESS) {
					ame_json_byte_forward(ds);
					ds->state = AME_STATE_NAME_STR;
					continue;
				} else if (err == AE_OK) {
name_string_end:
					ame_json_set_kvp_key_len(ds, ds->kvp);
					ds->state = AME_STATE_SEPARATOR;
					break;
				}
				goto error_exit;

			case '}':
				if (!ds->is_first) {
					goto parse_error;
				}
				/* empty object */
				ame_json_byte_backward(ds);
				ds->state = AME_STATE_CONTINUE;
				break;
			default:
				goto parse_error;
			}
			/* } to prevent cstyle error */
			break;

		case AME_STATE_SEPARATOR:
			switch (c) {
			case ':':
				ds->state = AME_STATE_VALUE;
				break;
			default:
				goto parse_error;
			}
			break;

		case AME_STATE_CONTINUE:
			/* { to prevent cstyle error */
			ds->kvp1 = ds->parent;
			if (!ds->kvp1) {
				/* data after the end root value */
				goto parse_error;
			}
			ds->is_first = 0;
			switch (c) {
			case '}':
				if (ds->parent->type != AME_TYPE_OBJECT) {
					goto parse_error;
				}
				goto object_end;
			case ']':
				if (ds->parent->type != AME_TYPE_ARRAY) {
					goto parse_error;
				}
object_end:
				if (ds->kvp1->value) {
					ame_json_set_kvp_val_len(ds,
					    ds->kvp1, 1);
				}
				ds->parent = ds->kvp1->parent;
				break;
			case ',':
				if (ds->parent->type == AME_TYPE_OBJECT) {
					ds->state = AME_STATE_NAME;
				} else if (ds->parent->type ==
				    AME_TYPE_ARRAY) {
					ds->state = AME_STATE_VALUE;
				} else {
					goto parse_error;
				}
				break;
			default:
				goto parse_error;
			}
			if (ds->incremental && *pkvp) {
				if (ds->parent == ds->incr_parent) {
					ame_json_byte_forward(ds);
					ASSERT(ds->data_len <=
					    ds->data_buf_size);
					ds->obj_parsed = 1;
					return AE_OK;
				}
				if (ds->ext_mode && ds->parent->type
				    == AME_TYPE_ARRAY) {
					ame_json_byte_forward(ds);
					ASSERT(ds->data_len <=
					    ds->data_buf_size);
					ds->obj_parsed = 1;
					return AE_OK;
				}
			}
			/* } to prevent cstyle error */
			break;
		default:
			goto parse_error;
		}
		ame_json_byte_forward(ds);
	}

	if (ds->ext_mode) {
		return AE_UNEXP_END;
	}
	if (!ds->kvp_used) {
		/* no values found */
		err = AE_UNEXP_END;
		goto error_exit;
	}

	if (!ds->parent) {
		/* root object has been fully parsed */
		if (!ds->incremental) {
			if (ds->kvp_stack->type != AME_TYPE_UTF8) {
				ame_json_set_kvp_val_len(ds, ds->kvp_stack, 0);
			}
			*pkvp = ds->kvp_stack;
			ds->pkvp = *pkvp;
		}
		ASSERT(ds->data_len <= ds->data_buf_size);
		ds->obj_parsed = 1;
		return AE_OK;
	}

	if (ds->incremental && ds->parent == ds->incr_parent && *pkvp &&
	    (ds->state == AME_STATE_VALUE || ds->state == AME_STATE_NAME)) {
		/*
		 * Fully decoded an element of the parent and ready to begin
		 * processing the subsequent element. Avoid returning
		 * truncated numbers by waiting to return okay until a
		 * trailing token has been processed (i.e. state is back
		 * to AME_STATE_VALUE or AME_STATE_NAME).
		 */
		ASSERT(ds->data_len <= ds->data_buf_size);
		ds->obj_parsed = 1;
		return AE_OK;
	}

	err = AE_UNEXP_END;
	goto error_exit;

parse_error:
	err = AE_PARSE;
error_exit:
#ifdef AME_JSON_DEBUG
	if (err != AE_UNEXP_END) {
		printcli("JSON parse err %d, offset %d, c %c (%02x), state %d",
		    err, ds->offset, (char)c, c, ds->state);
	}
#endif
	ds->error_offset = ds->offset;
	ds->state = save_state;
	ds->offset = save_offset;
	ds->kvp_used = save_used;
	ds->parent = save_parent;
	*pkvp = NULL;
	return err;
}

static enum ada_err ame_json_get_opaque(struct ame_kvp *parent,
    struct ame_key *key, void *value, size_t *length)
{
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type == AME_TYPE_NULL) {
		*length = 0;
		return AE_OK;
	}
	if (kvp->type != AME_TYPE_UTF8) {
		return AE_INVAL_TYPE;
	}

	if (ayla_base64_decode(kvp->value, kvp->value_length, value, length)) {
		return AE_ERR;
	}

	return AE_OK;
}

static enum ada_err ame_json_get_utf8(struct ame_kvp *parent,
    struct ame_key *key, char *value, size_t *length)
{
	struct ame_kvp *kvp;
	enum ada_err err;
	int escape = 0;
	int hexdigs = 0;
	int count = 0;
	const char *p;
	const char *end;
	char unicode_str[5];
	u32 unicode;
	char *errptr;
	char utf8_str[6];
	ssize_t utf8_len;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type == AME_TYPE_NULL) {
		goto done;
	}

	if (kvp->type != AME_TYPE_UTF8) {
		return AE_INVAL_TYPE;
	}

	p = (const char *)kvp->value;
	end = p + kvp->value_length;
	while (p < end && count < *length - 1) {
		if (escape) {
			escape = 0;
			switch (*p) {
			case 'b':
				*value++ = '\b';
				count++;
				break;
			case 'f':
				*value++ = '\f';
				count++;
				break;
			case 'n':
				*value++ = '\n';
				count++;
				break;
			case 'r':
				*value++ = '\r';
				count++;
				break;
			case 't':
				*value++ = '\t';
				count++;
				break;
			case 'u':
				hexdigs = 4;
				break;
			default:
				*value++ = *p;
				count++;
				break;
			}
		} else if (hexdigs) {
			unicode_str[4-hexdigs] = *p;
			if (--hexdigs == 0) {
				/* convert unicode to utf8 */
				unicode_str[4] = '\0';
				unicode = strtoul(unicode_str, &errptr, 16);
				if (errptr != &unicode_str[4]) {
					return AE_ERR;
				}
				utf8_len = utf8_encode((u8 *)utf8_str,
				    sizeof(utf8_str), unicode);
				if (utf8_len == -1 ||
				    utf8_len > sizeof(utf8_str)) {
					return AE_ERR;
				}
				if (utf8_len >= *length - count) {
					/* truncate before character */
					break;
				}
				memcpy(value, utf8_str, utf8_len);
				value += utf8_len;
				count += utf8_len;
			}
		} else {
			switch (*p) {
			case '\\':
				escape = 1;
				break;
			default:
				*value++ = *p;
				count++;
				break;
			}
		}
		p++;
	}

done:
	*value = '\0';
	*length = count;

	return AE_OK;
}

static enum ada_err ame_json_get_s64(struct ame_kvp *parent,
    struct ame_key *key, s64 *value)
{
	char *tailptr;
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER && kvp->type != AME_TYPE_DECIMAL) {
		return AE_INVAL_TYPE;
	}

	*value = strtoll((char *)kvp->value, &tailptr, 10);
	return AE_OK;
}

static enum ada_err ame_json_get_s32(struct ame_kvp *parent,
    struct ame_key *key, s32 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S32 || result < MIN_S32) {
		return AE_INVAL_VAL;
	}

	*value = (s32)result;
	return AE_OK;
}

static enum ada_err ame_json_get_s16(struct ame_kvp *parent,
    struct ame_key *key, s16 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S16 || result < MIN_S16) {
		return AE_INVAL_VAL;
	}

	*value = (s16)result;
	return AE_OK;
}

static enum ada_err ame_json_get_s8(struct ame_kvp *parent,
    struct ame_key *key, s8 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S8 || result < MIN_S8) {
		return AE_INVAL_VAL;
	}

	*value = (s8)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u64(struct ame_kvp *parent,
    struct ame_key *key, u64 *value)
{
	char *tailptr;
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER && kvp->type != AME_TYPE_DECIMAL) {
		return AE_INVAL_TYPE;
	}

	*value = strtoull((char *)kvp->value, &tailptr, 10);
	return AE_OK;
}

static enum ada_err ame_json_get_u32(struct ame_kvp *parent,
    struct ame_key *key, u32 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U32) {
		return AE_INVAL_VAL;
	}

	*value = (u32)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u16(struct ame_kvp *parent,
    struct ame_key *key, u16 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U16) {
		return AE_INVAL_VAL;
	}

	*value = (u16)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u8(struct ame_kvp *parent,
    struct ame_key *key, u8 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U8) {
		return AE_INVAL_VAL;
	}

	*value = (u8)result;
	return AE_OK;
}

static enum ada_err ame_json_get_d32(struct ame_kvp *parent,
    struct ame_key *key, s32 *value, u8 scale)
{
	struct ame_kvp *kvp;
	enum ada_err err;
	u8 i = 0;
	const char *p;
	const char *end;
	char in[32];
	int point;
	int exponent = 0;
	s32 temp;

	ASSERT(scale < 10);

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER &&
	    kvp->type != AME_TYPE_DECIMAL &&
	    kvp->type != AME_TYPE_FLOAT) {
		return AE_INVAL_TYPE;
	}

	p = (const char *)kvp->value;
	end = p + kvp->value_length;

	if (*p == '-') {
		in[i++] = *p++;
	}
	while (p < end && *p >= '0' && *p <= '9') {
		in[i++] = *p++;
		if (i >= sizeof(in)) {
			return AE_INVAL_VAL;
		}
	}

	point = i;
	if (p < end && *p == '.') {
		p++;
		while (p < end && *p >= '0' && *p <= '9') {
			in[i++] = *p++;
			if (i >= sizeof(in)) {
				return AE_INVAL_VAL;
			}
		}
	}
	in[i]  = '\0';

	if (p < end && (*p == 'e' || *p == 'E')) {
		p++;
		errno = 0;
		exponent = strtol(p, NULL, 10);
		if (errno) {
			return AE_INVAL_VAL;
		}
	}

	point = point + scale + exponent;
	if (point < 0) {
		*value = 0;
		return AE_OK;
	} else if (point >= sizeof(in)) {
		return AE_INVAL_VAL;
	} else if (point < i) {
		in[point] = '\0';
	}

	errno = 0;
	temp = strtol(in, NULL, 10);
	if (errno) {
		return AE_INVAL_VAL;
	}
	while (i < point) {
		if (temp > LONG_MAX / 10 || temp < LONG_MIN / 10) {
			return AE_INVAL_VAL;
		}
		temp *= 10;
		i++;
	}

	*value = temp;
	return AE_OK;
}

static enum ada_err ame_json_get_boolean(struct ame_kvp *parent,
    struct ame_key *key, s32 *value)
{
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_BOOLEAN) {
		return AE_INVAL_TYPE;
	}

	if (*(char *)kvp->value == 'f') {
		*value = 0;
	} else {
		*value = 1;
	}
	return AE_OK;
}

enum ada_err ame_json_dec_init(struct ame_decoder *dec)
{
	dec->parse = ame_json_parse;
	dec->get_opaque = ame_json_get_opaque;
	dec->get_utf8 = ame_json_get_utf8;
	dec->get_s64 = ame_json_get_s64;
	dec->get_s32 = ame_json_get_s32;
	dec->get_s16 = ame_json_get_s16;
	dec->get_s8 = ame_json_get_s8;
	dec->get_u64 = ame_json_get_u64;
	dec->get_u32 = ame_json_get_u32;
	dec->get_u16 = ame_json_get_u16;
	dec->get_u8 = ame_json_get_u8;
	dec->get_d32 = ame_json_get_d32;
	dec->get_boolean = ame_json_get_boolean;

	return AE_OK;
}

#ifdef AME_ENCODER_TEST
static void ame_json_test_d32_hlpr(struct ame_decoder *dec,
    struct ame_decoder_state *ds, const char *in, u8 scale, s32 expect)
{
	enum ada_err err;
	s32 *out = (s32 *)ame_test_output;
	size_t length = strlen(in);
	struct ame_kvp *kvp1;

	ame_decoder_stack_set(ds, ame_test_stack,
	    ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, (u8 *)in, length);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		goto exit;
	}
	err = dec->get_d32(kvp1, NULL, out, scale);
	if (err) {
		printcli("error getting value %d\n", err);
		goto exit;
	}
	if (*out != expect) {
		printcli("error expected: %ld, decoded: %ld\n", expect, *out);
	}
exit:
	ame_test_dump((u8 *)in, length);
}

static void ame_json_test_d32(struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	printcli("JSON d32 tests\n");
	ame_json_test_d32_hlpr(dec, ds, "0", 2, 0);
	ame_json_test_d32_hlpr(dec, ds, "1", 2, 100);
	ame_json_test_d32_hlpr(dec, ds, "-1", 2, -100);
	ame_json_test_d32_hlpr(dec, ds, "1e-2", 3, 10);
	ame_json_test_d32_hlpr(dec, ds, "-1E-2", 3, -10);
	ame_json_test_d32_hlpr(dec, ds, "1.234", 2, 123);
	ame_json_test_d32_hlpr(dec, ds, "-1.234e1", 2, -1234);
	ame_json_test_d32_hlpr(dec, ds, "-12.34e+3", 2, -1234000);
	ame_json_test_d32_hlpr(dec, ds, "-1234e-5", 2, -1);
	ame_json_test_d32_hlpr(dec, ds, "0.123456e2", 2, 1234);
	ame_json_test_d32_hlpr(dec, ds, "-0.123456e2", 2, -1234);
	ame_json_test_d32_hlpr(dec, ds, "0.123456e2", 4, 123456);
	ame_json_test_d32_hlpr(dec, ds, "-0.123456E2", 4, -123456);
	ame_json_test_d32_hlpr(dec, ds, "0.123456E-2", 4, 12);
	ame_json_test_d32_hlpr(dec, ds, "-0.1234567e+2", 4, -123456);
	ame_json_test_d32_hlpr(dec, ds, "1e-100", 4, 0);
	ame_json_test_d32_hlpr(dec, ds, "2147483647", 0, 2147483647);
	ame_json_test_d32_hlpr(dec, ds, "-2147483648", 0, -2147483648);

	printcli("JSON d32 negative tests\n");
	ame_json_test_d32_hlpr(dec, ds, "+1", 2, -1);
	ame_json_test_d32_hlpr(dec, ds, "1a2b3c", 2, -1);
	/* value too large */
	ame_json_test_d32_hlpr(dec, ds, "2147483648", 0, -1);
	ame_json_test_d32_hlpr(dec, ds, "-2147483649", 0, -1);
	ame_json_test_d32_hlpr(dec, ds, "1e100", 4, 0);
}

void ame_json_test(void)
{
	struct ame_encoder encoder;
	struct ame_encoder_state encoder_state;
	struct ame_decoder decoder;
	struct ame_decoder_state decoder_state;

	ame_json_enc_init(&encoder);
	ame_json_dec_init(&decoder);
	ame_test(&encoder, &encoder_state, &decoder, &decoder_state);
	ame_json_test_d32(&decoder, &decoder_state);
}
#endif
