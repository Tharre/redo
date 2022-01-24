/* DSV.c
 *
 * Copyright (c) 2016 Tharre
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "dbg.h"
#include "DSV.h"

/* dest must be 2x src */
size_t encode_string(char *dest, const char *src) {
	char *start = dest, c;

	while ((c = *(src++))) {
		switch (c) {
		case '\n':
			*(dest++) = '\\';
			*(dest++) = 'n';
			break;
		case '\\':
			*(dest++) = '\\';
			*(dest++) = '\\';
			break;
		case ':':
			*(dest++) = '\\';
			/* fall-through */
		default:
			*(dest++) = c;
		}
	}

	*dest = '\0';
	return dest - start;
}

static void decode_string(char *dest, const char *src, size_t len) {
	for (size_t i = 0; i < len-1; ++i) {
		char c = src[i];
		if (c != '\\') {
			*(dest++) = c;
			continue;
		}

		/* check if we are at the end of the string */
		if (*src == '\0') {
			debug("DSV: ignored extra '\\' at end of string\n");
			break;
		}

		switch (c = *(src++)) {
		case 'n':
			*(dest++) = '\n';
			break;
		default:
			*(dest++) = c;
		}
	}

	*dest = '\0';
}

void dsv_init(struct dsv_ctx *ctx, size_t fields_count) {
	ctx->fields_count = fields_count;
	ctx->fields = xmalloc(fields_count * sizeof(char*));
	ctx->offset = 0;
	ctx->bufsize = 1024;
	ctx->buf = xmalloc(ctx->bufsize);
	ctx->buflen = 0;
}

void dsv_free(struct dsv_ctx *ctx) {
	free(ctx->fields);
	free(ctx->buf);
}

int dsv_parse_next_line(struct dsv_ctx *context, const char *src, size_t len) {
	char *newline = memchr(src, '\n', len);

	if (!newline) {
		context->status = E_NO_NEWLINE_FOUND;
		return 1;
	}

	context->processed = newline-src+1;
	if (context->processed == 1) {
		debug("DSV: empty newline detected\n");
		context->status = E_EMPTY_NEWLINE;
		return 1;
	}

	char *start = (char*) src;
	size_t i = 0;
	ptrdiff_t size;

	while (1) {
		char *colon = memchr(start, ':', newline-start+1);
		if (colon)
			size = colon-start;
		else
			size = newline-start;

		if (size <= 0) {
			if (!size) {
				debug("DSV: empty field detected\n");
				context->status = E_EMPTY_FIELD;
				goto error;
			}

			break;
		}

		if (i >= context->fields_count) {
			debug("DSV: too many fields\n");
			context->status = E_TOO_MANY_FIELDS;
			goto error;
		}

		char *buf = xmalloc(size+1);
		decode_string(buf, start, size+1);
		context->fields[i] = buf;

		start += size + 1;
		++i;
	}

	if (i+1 < context->fields_count) {
		debug("DSV: too few fields (%Iu)\n", i+1);
		context->status = E_TOO_FEW_FIELDS;
		goto error;
	}

	context->status = E_SUCCESS;
	return 0;

error:
	for (size_t j = 0; j < i; ++j)
		free(context->fields[j]);

	return 1;
}

int dsv_parse_file(struct dsv_ctx *ctx, FILE *fp) {
	size_t read = ctx->buflen;
	if (!read) {
		read = fread(ctx->buf, 1, ctx->bufsize, fp);
		ctx->buflen = read;
	}

	while (read > 0) {
		if (dsv_parse_next_line(ctx, ctx->buf+ctx->offset, read-ctx->offset)) {
			if (ctx->status != E_NO_NEWLINE_FOUND)
				return 1;

			/* are we using the full buffer already? */
			if (!ctx->offset) {
				/* then make it bigger! */
				ctx->bufsize *= 2;
				ctx->buf = xrealloc(ctx->buf, ctx->bufsize);
			} else {
				/* then move stuff around */
				memmove(ctx->buf, ctx->buf+ctx->offset, read-ctx->offset);
				ctx->buflen -= ctx->offset;
				ctx->offset = 0;
			}

			read = fread(ctx->buf+ctx->buflen, 1, ctx->bufsize-ctx->buflen, fp);
			ctx->buflen += read;
		} else {
			ctx->offset += ctx->processed;
			ctx->status = E_SUCCESS;
			return 0;
		}
	}

	if (ferror(fp)) {
		ctx->status = E_FREAD_ERROR;
		return 1;
	}

	return 1;
}
