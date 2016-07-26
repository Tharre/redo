/* build.h
 *
 * Copyright (c) 2016 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __RDSV_H__
#define __RDSV_H__

enum dsv_status {
	E_SUCCESS = 0,
	E_NO_NEWLINE_FOUND,
	E_EMPTY_NEWLINE,
	E_EMPTY_FIELD,
	E_TOO_MANY_FIELDS,
	E_TOO_FEW_FIELDS,
	E_FREAD_ERROR,
};

struct dsv_ctx {
	size_t processed;
	size_t fields_count;
	size_t offset;
	size_t bufsize;
	size_t buflen;

	char **fields;
	char *buf;

	enum dsv_status status;
};

size_t encode_string(char *dest, const char *src);
void dsv_init(struct dsv_ctx *context, size_t fields_count);
void dsv_free(struct dsv_ctx *context);
int dsv_parse_next_line(struct dsv_ctx *context, const char *src, size_t len);
int dsv_parse_file(struct dsv_ctx *ctx, FILE *fp);

#endif
