/*
 * Secure Hash Algorith SHA-1, as published in FIPS PUB 180-2.
 *
 * This implementation is in the public domain.  Copyright abandoned.
 * You may do anything you like with it, including evil things.
 *
 * This is a rewrite from scratch, based on Linus Torvalds' "block-sha1"
 * from the git mailing list (August, 2009).  Additional optimization
 * ideas cribbed from
 * - Artur Skawina (x86, particularly P4, and much benchmarking)
 * - Nicolas Pitre (ARM)
 */

#include <stddef.h>	/* For size_t */
#include <stdint.h>	/* For uint32_t, uint64_t */

typedef struct SHA_context {
        uint64_t len;	/* May be shrunk to uint32_t */
        uint32_t iv[5];
        unsigned char buf[64];	/* Must be 32-bit aligned */
} SHA_CTX;

void SHA1_Init(struct SHA_context *c);
void SHA1_Update(struct SHA_context *c, void const *p, size_t n);
void SHA1_Final(unsigned char hash[20], struct SHA_context *c);
