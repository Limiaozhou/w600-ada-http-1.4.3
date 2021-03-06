/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * AME parser based on ame-json.
 */
#ifndef __CLIENT_AME_H__
#define __CLIENT_AME_H__

#include "ame.h"

/*
 * Macro AME_TAG is used to define json-tag list.
 */
#define AME_TAG(_name, _subtags, _cb)	\
	{				\
		.name = _name,		\
		.subtags = _subtags,	\
		.cb = _cb,		\
	}

/*
 * Structure of ame_tag.
 */
struct ame_tag {
	const char *name;
	const struct ame_tag *subtags;
	int (*cb)(struct ame_decoder *dec, void *arg, struct ame_kvp *kvp);
};

/*
 * Structure of ame_state used in client.
 */
struct ame_state {
	struct ame_encoder enc;
	struct ame_encoder_state es;
	struct ame_decoder dec;
	struct ame_decoder_state ds;
	const struct ame_tag *list;	/* registered */
	void *cb_arg;
};

/*
 * Initialize all ame-json objects which will be used by client.
 *
 * \param ctx points to struct ame_state provided by client.
 * \param stack is a stack of struct ame_kvp type. It is provided by client
 * , and used by ame-json decoder.
 * \param stack_elems is element count of the stack.
 * \param parsed_buf is buffer for storing parsed input data.
 * \param buf_size is the size of parsed_buf.
 * \param list is a json-tag list. Ame-json decoder will find callback
 * in the list according to tag (key) name.
 * \param cb_arg is argument of the callback.
 *
 * \returns 0 on success.
 */
enum ada_err client_ame_init(struct ame_state *ctx,
		struct ame_kvp *stack, size_t stack_elems,
		u8 *parsed_buf, size_t buf_size,
		const struct ame_tag *list, void *cb_arg);

/*
 * Find sub-tag in a tag list.
 *
 * \param tag is the current tag.
 * \param kvp is an ame-json parsed onject.
 * \returns an sub-tag, NULL on not found.
 */
const struct ame_tag *client_ame_find_tag(const struct ame_tag *tag,
		const struct ame_kvp *kvp);

/*
 * Parse json objects from the input data stream. On the parsing process,
 * when an object is parsed, tag-list will be searched, if a sub-tag is found
 * and its callback is not null, the callback is called to handler the object.
 * On the function returning AE_UNEXP_END, you need to call it with new
 * intput data again and again, until it returns AE_OK or AE_PARSE.
 *
 * \param state is a structure of ame_state.
 * \param input is the input data stream to be parsed,
 * \param len is length of the input data.
 * \returns:
 *	AE_UNEXP_END on needing more input data;
 *	AE_PARSE on syntax error.
 *	AE_OK on success (an object is parsed, or totally finished).
 */
enum ada_err client_ame_parse(struct ame_state *state,
		const u8 *input, int len);

#endif /* __CLIENT_AME_H__ */
