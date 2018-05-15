#include <stdint.h>
#include "cache.h"

/*
 * note: N64 specs:
 *   I-Cache: 32bytes/line, 16KiB
 *   D-Cache: 16bytes/line, 8KiB
 */

#define do_cache(op, cache, _linesize, _start, _len) \
	do { \
		if((_len) != 0) { \
			const uintptr_t xlinesize = (_linesize); \
			const uintptr_t xstart = (uintptr_t)(_start); \
			const uintptr_t xlen = (_len); \
			const uintptr_t xend = (xstart + (xlen - 1)) | (xlinesize - 1); \
			uintptr_t xline; \
			for(xline = xstart; xline <= xend; xline += xlinesize) { /* TODO specifying end of virtual memory cause inf-loop!! */ \
				__asm("cache %0, %1" : : "i"(((op)<<2)|(cache)), "m"(*(uint8_t*)xline)); \
			} \
		} \
	} while(0)

void dwbinvalall() {
	do_cache(0, 1, 16, (intptr_t)(int32_t)0x80000000, 8 * 1024); // 0=IndexWBInvd
}

void dwbinval(const void *ptr, uintptr_t len) {
	// if len is over d-cache size, assume whole is cached, flush all d-cache.
	if(8192 <= len) {
		dwbinvalall();
		return;
	}

	do_cache(5, 1, 16, ptr, len); // 5=HitWBInvd
}

void dinval(const void *ptr, uintptr_t len) {
	// if len is over d-cache size, assume whole is cached, flush all d-cache.
	if(8192 <= len) {
		dwbinvalall(); // note: there is no DIndexInvd (think it... too dangerous!!), do WBInvd instead (that is safe and intended).
		return;
	}

	do_cache(4, 1, 16, ptr, len); // 4=HitInvd
}

void iinvalall() {
	do_cache(0, 0, 32, (intptr_t)(int32_t)0x80000000, 16 * 1024); // 0=Index(WB)Invd
}

void iinval(const void *ptr, uintptr_t len) {
	// if len is over i-cache size, assume whole is cached, flush all i-cache.
	if(16384 <= len) {
		iinvalall();
		return;
	}

	do_cache(4, 0, 32, ptr, len); // 4=HitInvd
}
