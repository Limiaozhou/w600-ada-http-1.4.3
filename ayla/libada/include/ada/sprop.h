/*
 * Copyright 2015-2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_SPROP_H__
#define __AYLA_ADA_SPROP_H__

typedef unsigned int size_t;
typedef signed int ssize_t;


#define SPROP_NAME_MAX_LEN      20
#define SPROP_TABLE_ENTRIES     10
#define SPROP_DEF_FILECHUNK_SIZE	512

struct ada_sprop {
	const char *name;
	enum ayla_tlv_type type;
	void *val;
	size_t val_len;
	ssize_t (*get)(struct ada_sprop *, void *buf, size_t len);
	enum ada_err (*set)(struct ada_sprop *, const void *buf, size_t len);
	u8 send_req;	/* flag indicating to send once connected to cloud */
};

#ifdef AYLA_FILE_PROP_SUPPORT
enum file_dp_state {
	FD_IDLE = 0,	/* nothing to do */
	FD_CREATE,	/* send datapoint create */
	FD_SEND,	/* send datapoint value */
	FD_RECV,	/* request and receive datapoint value */
	FD_FETCHED,	/* send datapoint fetched opcode */
	FD_ABORT,	/* abort the datapoint operation */
};

struct file_dp {
	enum file_dp_state state;
	struct ada_sprop *sprop;	/* associated property entry */
	u32 next_off;		/* next offset to transfer */
	u32 tot_len;		/* total length expected (if known) */
	size_t chunk_size;		/* max chunk size for file transfer */
	u8 aborted;		/* 1 if last op for this prop was aborted */
	void *val_buf;		/* pointer to the next chunk */
	char loc[PROP_LOC_LEN];	/* location / data point key */
	/* callback to handle incoming data */
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len);
	/* callback to handle outgoing data */
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof);
};

/*
 * Initialize a file property struct. (*file_get) is used to
 * retrieve file chunks for file upload. (*file_set) is used
 * to send file property chunks during file download.
 * (chunk_size) is the maximum allowed size of file chunk for transfer.
 */
void ada_sprop_file_init(struct file_dp *dp,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	size_t chunk_size);

/*
 * Begin file property upload.
 */
enum ada_err ada_sprop_file_start_send(const char *name, size_t len);

/*
 * Begin file property download.
 */
enum ada_err ada_sprop_file_start_recv(const char *name, const void *buf,
				size_t len, u32 off);

/*
 * Abort ongoing file transfer.
 */
void ada_sprop_file_abort(void);

/*
 * Mark file datapoint fetched.
 */
enum ada_err ada_sprop_file_fetched(const char *name);
#endif

/*
 * Get an ATLV_INT or ATLV_CENTS type property from the
 * sprop structure.
 */
ssize_t ada_sprop_get_int(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_UINT type property from the sprop structure.
 */
ssize_t ada_sprop_get_uint(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_BOOL type property from the sprop structure.
 */
ssize_t ada_sprop_get_bool(struct ada_sprop *, void *buf, size_t len);

/*
 * Get an ATLV_UTF8 type property from the sprop structure.
 */
ssize_t ada_sprop_get_string(struct ada_sprop *, void *buf, size_t len);

/*
 * Set an ATLV_INT or ATLV_CENTS property value to the
 * value in *buf.
 */
enum ada_err ada_sprop_set_int(struct ada_sprop *, const void *buf, size_t len);

/*
 * Set an ATLV_UINT property value to the value in *buf.
 */
enum ada_err ada_sprop_set_uint(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Set an ATLV_BOOL property value to the value in *buf.
 */
enum ada_err ada_sprop_set_bool(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Set an ATLV_UTF8 property value to the value in *buf.
 */
enum ada_err ada_sprop_set_string(struct ada_sprop *,
				const void *buf, size_t len);

/*
 * Send property update.
 */
enum ada_err ada_sprop_send(struct ada_sprop *);

/*
 * Send a property update by name.
 */
enum ada_err ada_sprop_send_by_name(const char *);

/*
 * Register a table of properties to the generic prop_mgr.
 */
enum ada_err ada_sprop_mgr_register(char *name, struct ada_sprop *table,
		unsigned int entries);

/*
 * Mask of currently-connected destinations.
 */
extern u8 ada_sprop_dest_mask;

#endif /* __AYLA_ADA_SPROP_H__ */
