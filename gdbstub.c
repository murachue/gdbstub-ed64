#include <stdint.h>
#include "gdbstub.h"

/* cache */

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
	do_cache(0, 1, 16, (intptr_t)(int32_t)0x80000000, 8 * 1024); /* 0=IndexWBInvd */
}

void dwbinval(const void *ptr, uintptr_t len) {
	/* if len is over d-cache size, assume whole is cached, flush all d-cache. */
	if(8192 <= len) {
		dwbinvalall();
		return;
	}

	do_cache(5, 1, 16, ptr, len); /* 5=HitWBInvd */
}

void dinval(const void *ptr, uintptr_t len) {
	/* if len is over d-cache size, assume whole is cached, flush all d-cache. */
	if(8192 <= len) {
		dwbinvalall(); /* note: there is no DIndexInvd (think it... too dangerous!!), do WBInvd instead (that is safe and intended). */
		return;
	}

	do_cache(4, 1, 16, ptr, len); /* 4=HitInvd */
}

void iinvalall() {
	do_cache(0, 0, 32, (intptr_t)(int32_t)0x80000000, 16 * 1024); /* 0=Index(WB)Invd */
}

void iinval(const void *ptr, uintptr_t len) {
	/* if len is over i-cache size, assume whole is cached, flush all i-cache. */
	if(16384 <= len) {
		iinvalall();
		return;
	}

	do_cache(4, 0, 32, ptr, len); /* 4=HitInvd */
}

/* N64/ED64v3 hardware regs that only used by this gdbstub */

struct {
	uint32_t dramaddr;
	uint32_t cartaddr;
	uint32_t dram2cart; /* read (to PI) */
	uint32_t cart2dram; /* write (from PI) */
	uint32_t status;
	uint32_t dom1lat; /* latency */
	uint32_t dom1pwd; /* pulse width */
	uint32_t dom1pgs; /* page size */
	uint32_t dom1rls; /* release */
	uint32_t dom2lat; /* latency */
	uint32_t dom2pwd; /* pulse width */
	uint32_t dom2pgs; /* page size */
	uint32_t dom2rls; /* release */
} static volatile * const __attribute((unused)) PI = (void*)P32(0xA4600000);
enum PI_STATUS_READ {
	PI_STATUS_DMABUSY = 1 << 0,
	PI_STATUS_IOBUSY  = 1 << 1,
	PI_STATUS_ERROR   = 1 << 2,
};
enum PI_STATUS_WRITE {
	PI_STATUS_RESET     = 1 << 0,
	PI_STATUS_CLEARINTR = 1 << 1,
};

struct {
	uint32_t cfg;
	uint32_t status;
	uint32_t dmalen;
	uint32_t dmaaddr;
	uint32_t msg;
	uint32_t dmacfg;
	uint32_t spi;
	uint32_t spicfg;
	uint32_t key;
	uint32_t savcfg;
	uint32_t sec;
	uint32_t ver;
} static volatile * const ED64 = (void*)P32(0xA8040000);
enum ED64_CFG {
	ED64_CFG_SDRAM_ON     = 1 << 0,
	ED64_CFG_SWAP         = 1 << 1,
	ED64_CFG_WR_MOD       = 1 << 2,
	ED64_CFG_WR_ADDR_MASK = 1 << 3,
};
enum ED64_STATUS {
	ED64_STATUS_DMABUSY    = 1 << 0,
	ED64_STATUS_DMATIMEOUT = 1 << 1,
	ED64_STATUS_TXE        = 1 << 2,
	ED64_STATUS_RXF        = 1 << 3,
	ED64_STATUS_SPI        = 1 << 4,
};
enum ED64_DMACFG {
	ED64_DMACFG_SD_TO_RAM   = 1,
	ED64_DMACFG_RAM_TO_SD   = 2,
	ED64_DMACFG_FIFO_TO_RAM = 3,
	ED64_DMACFG_RAM_TO_FIFO = 4,
};

/* gdbstub */

enum {
	STUBERR_UNKNOWN = 1,
	STUBERR_GRTOOSHORT,
	STUBERR_SRINV1,
	STUBERR_SRINV2,
	STUBERR_GMADDRSHORT,
	STUBERR_GMADDRINV,
	STUBERR_GMLENINV,
	STUBERR_GMSEGV,
	STUBERR_SMADDRSHORT,
	STUBERR_SMADDRINV,
	STUBERR_SMLENSHORT,
	STUBERR_SMLENINV,
	STUBERR_SMBYTESINV,
	STUBERR_SMSEGV,
	STUBERR_CADDRNOTIMPL,
	STUBERR_STPCSEGV,
	STUBERR_STJRSEGV,
	STUBERR_STFNSEGV,
	STUBERR_STTNSEGV,
};

/* 0~ GPR0-31(yes, include zero),[32]PS(status),LO,HI,BadVAddr,Cause,PC,[38]FPR0-31,[70]fpcs,fpir,[72]..(dsp?),[90]end */
extern uint8_t gdbstubctx[];
static uint64_t *regs = (uint64_t *)(gdbstubctx + GDBSTUBCTX_SIZE - REGS_SIZE);

/* TODO: if supporting binary-patch install, move these into gdbstubctx makes life easier. really do that?? */
static uint8_t __attribute((aligned(8))) cartrxbuf[2048];
static uint8_t __attribute((aligned(8))) carttxbuf[2048];
static uint32_t originstcodes[4][2], origrcvrcodes[4][2];
/* NOTE: origbpcodes must be zero-cleared. */
static struct {
	intptr_t invaddr; /* 0(~-1) = unset. inverted to be able to initialized as part of bss. */
	uint32_t value;
} origbpcodes[2];

static void ed_enableregs(int en) {
	ED64->cfg;
	ED64->key = en ? 0x1234 : 0; /* Enable/Disable ED64 regs */
}

static uint32_t ed_waitdma(void) {
	uint32_t last;
	for(;;) {
		ED64->cfg;
		last = ED64->status;
		if(!(last & ED64_STATUS_DMABUSY)) {
			break;
		}
	}
	return last;
}

static void dram2cart(const void *dramptr, uint32_t cartaddr, uint32_t len) {
	PI->status = PI_STATUS_RESET | PI_STATUS_CLEARINTR;
	PI->dramaddr = (uint32_t)(uintptr_t)dramptr & 0x00FFffff;
	PI->cartaddr = 0x10000000 + cartaddr;
	PI->dram2cart = len - 1; /* 64bits unit */

	while(PI->status & PI_STATUS_DMABUSY) /*nothing*/;
}

static uint32_t cart2fifo(uint32_t cartaddr, uint32_t len) {
	for(;;) {
		ED64->cfg;
		if(!(ED64->status & ED64_STATUS_TXE)) { break; }
	}

	ED64->cfg;
	ED64->dmalen = len / 512 - 1;
	ED64->cfg;
	ED64->dmaaddr = cartaddr / 2048;
	ED64->cfg;
	ED64->dmacfg = ED64_DMACFG_RAM_TO_FIFO;
	return ed_waitdma();
}

static uint32_t dram2fifo(const void *buf, uint32_t len) {
	const uint32_t cartaddr = 0x04000000 - ((len + 2047) & -2048);
	dwbinval(buf, len); /* TODO inval? only wb cause false write again but faster re-r/w. use case? */
	dram2cart(buf, cartaddr, len);
	return cart2fifo(cartaddr, len);
}

static uint32_t fifo2cart(uint32_t cartaddr, uint32_t len) {
	ED64->cfg;
	ED64->dmalen = len / 512 - 1;
	ED64->cfg;
	ED64->dmaaddr = cartaddr / 2048;
	ED64->cfg;
	ED64->dmacfg = ED64_DMACFG_FIFO_TO_RAM;
	return ed_waitdma();
}

static void cart2dram(void *dramptr, uint32_t cartaddr, uint32_t len) {
	PI->status = PI_STATUS_RESET | PI_STATUS_CLEARINTR;
	PI->dramaddr = (uint32_t)(uintptr_t)dramptr & 0x00FFffff;
	PI->cartaddr = 0x10000000 + cartaddr;
	PI->cart2dram = len - 1;

	while(PI->status & PI_STATUS_DMABUSY) /*nothing*/;
}

static uint32_t fifo2dram(void* buf, uintptr_t maxlen) {
	/* note: I want to fifo2cart many time and then one cart2dram, */
	/*       but ED64's len=512/addr=2048(!) align does not allow it... */
	const uint32_t cartaddr = 0x04000000 - 2048;
	uintptr_t len = 0;

	/* not dwbinval, discard to-be-write (that is not necessary data... overwritten soon.) */
	dinval(buf, maxlen);

	while(len < maxlen) {
		if(0 < len) {
			ED64->cfg;
			if(ED64->status & ED64_STATUS_RXF) { /* no more data (RXF#) */
				break;
			}
		}

		fifo2cart(cartaddr, 512);

		if(ED64->status & ED64_STATUS_DMATIMEOUT) { /* timeout */
			break;
		}

		cart2dram(buf, cartaddr, 512);
		buf = (char*)buf + 512;
		len += 512;
	}
	return len;
}

static void install_handler(void *pfn) {
	uint32_t i, j;
	uint32_t stubintcode[2];
	stubintcode[0] = 0x08000000 | (((uint32_t)(uintptr_t)pfn >> 2) & 0x03FFffff); /* j *pfn */
	stubintcode[1] = 0; /* branch-delay-slot: nop */

	for(i = 0; i < 4; i++) {
		for(j = 0; j < 2; j++) {
			*(uint32_t*)P32(0x80000000 + i * 0x80 + j * 4) = stubintcode[j];
		}
	}

	dwbinval((void*)P32(0x80000000), 0x200);
	iinval((void*)P32(0x80000000), 0x200);
}

static void backup_and_install_handlers(uint32_t origcodes[4][2], void *pfn) {
	uint32_t i, j;

	for(i = 0; i < 4; i++) {
		for(j = 0; j < 2; j++) {
			origcodes[i][j] = *(uint32_t*)P32(0x80000000 + i * 0x80 + j * 4);
		}
	}

	install_handler(pfn);
}

static void restore_handlers(uint32_t origcodes[4][2]) {
	uint32_t i, j;

	for(i = 0; i < 4; i++) {
		for(j = 0; j < 2; j++) {
			*(uint32_t*)P32(0x80000000 + i * 0x80 + j * 4) = origcodes[i][j];
		}
	}

	dwbinval((void*)P32(0x80000000), 0x200);
	iinval((void*)P32(0x80000000), 0x200);
}

static int is_in_stub(void) {
	uint8_t *sp;
	__asm("move %0, $sp" : "=r"(sp));
	return gdbstubctx <= sp && sp < (gdbstubctx + GDBSTUBCTX_SIZE);
}

void stub_install(void) {
#ifdef CONFIG_GDBSTUB_CTX_ZERO
	extern void stub_installtlb(void);
	stub_installtlb();
#endif

	extern void stub(void);

	if(is_in_stub()) {
		/* prepare to call from stub... */
		restore_handlers(origrcvrcodes);

		backup_and_install_handlers(originstcodes, stub);

		extern void stub_recover(void);
		backup_and_install_handlers(origrcvrcodes, stub_recover);
	} else {
		backup_and_install_handlers(originstcodes, stub);
	}
}

void stub_uninstall(void) {
	restore_handlers(originstcodes);
}

/* sample program entry TODO: remove this */
void stub_test(void) {
	/* make TLB initialized; TODO this can be removed if stub_installtlb does tlbp */
	extern void stub_tlbunmapall(void);
	stub_tlbunmapall();

	stub_install();
	__asm("syscall"); /* invoke stub by triggering (safe&unique) exception */
	stub_uninstall();
}

static int32_t hex2int(uint8_t c) {
	if('0' <= c && c <= '9') { return c - '0'; }
	if('A' <= c && c <= 'F') { return c - 'A' + 10; }
	if('a' <= c && c <= 'f') { return c - 'a' + 10; }
	return -1;
}

uint32_t stub_recovered;

static intptr_t int2hex(uint8_t *buf, uintptr_t bufsize, intptr_t index, uintptr_t value, uintptr_t nibbles) {
	uintptr_t lpos;
	static const char i2h[16] = "0123456789ABCDEF";
	for(lpos = nibbles; 0 < lpos; lpos--) {
		if(bufsize <= index) return -1; /* overflow! */
		buf[index] = i2h[(value >> (4 * (lpos - 1))) & 15];
		index++;
	}
	return index;
}

static intptr_t tohex(uint8_t *buf, uintptr_t bufsize, intptr_t index, const void *src_, uintptr_t srclen) {
	uintptr_t i = 0;
	stub_recovered = 0;

	/* note: it sometimes access hardware register that requires specific bit width... don't simplify. */

	if((((uintptr_t)src_ & 3) == 0) && ((srclen & 3) == 0)) {
		/* word access */
		const uint32_t *src = src_;
		for(; i < srclen / 4; i++) {
			uint32_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 8)) < 0) {
				return -1; /* overflow! */
			}
		}
	} else if((((uintptr_t)src_ & 1) == 0) && ((srclen & 1) == 0)) {
		/* halfword access */
		const uint16_t *src = src_;
		for(; i < srclen / 2; i++) {
			uint16_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 4)) < 0) {
				return -1; /* overflow! */
			}
		}
	} else {
		/* byte access */
		const uint8_t *src = src_;
		for(; i < srclen; i++) {
			uint8_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 2)) < 0) {
				return -1; /* overflow! */
			}
		}
	}
	return index;
}

static void sendraw(uint8_t *buf, uintptr_t bufsize, uintptr_t len) {
	/* zerofill tail */
	for(; len < bufsize; len++) {
		buf[len] = 0;
	}

	/* go!! */
	dram2fifo(buf, bufsize);
}

static int sendpkt(uint8_t *buf, intptr_t bufsize) {
	intptr_t si, ei;

	if(bufsize < 4) {
		/* no minimum space is available. illegal function call!! */
		return -1;
	}

	for(si = 0; si < bufsize - 3; si++) {
		if(buf[si] == '$') {
			break;
		}
	}
	if(si == bufsize - 3) {
		/* no start marker found */
		return -2;
	}
	si++;

	/* find end marker */
	for(ei = si; ei < bufsize - 2; ei++) {
		if(buf[ei] == '#') {
			break;
		}
	}
	if(ei == bufsize - 2) {
		/* no end marker found */
		return -3;
	}

	/* calculate and fill checksum */
	{
		uintptr_t sum = 0;
		{
			uintptr_t i;
			for(i = si; i < ei; i++) {
				sum += buf[i];
			}
		}
		{
			uint8_t sumb[1] = {sum};
			if(tohex(buf, bufsize, ei + 1, sumb, 1) < 0) {
				/* should not be happened! */
				return -4;
			}
		}
	}
	ei += 1 + 2;

	/* go!! */
	{
		uintptr_t ssize = (ei + (512 - 1)) & -512;
		sendraw(buf, ssize, ei);
	}

	return 0;
}

static void die(uint16_t color) {
#if 0
	extern uint16_t vram[];
	for(uintptr_t i = 0; i < 320*240; i++) {
		vram[i] = color;
	}
#endif
	dwbinvalall();
	for(;;) /*nothing*/ ;
}

/* g -> <regbytes> */
static uint8_t cmd_getregs(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	carttxbuf[0] = '+';
	carttxbuf[1] = '$';
	{
		intptr_t ei = tohex(carttxbuf, sizeof(carttxbuf), 2, regs, REGS_SIZE);
		if(ei < 0) {
			return STUBERR_GRTOOSHORT;
		}
		carttxbuf[ei] = '#';
	}
	if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0x07C0);

	return 0;
}

/* G<regbytes> -> OK|Exx */
static uint8_t cmd_setregs(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uint8_t *p = (uint8_t *)regs;
	uintptr_t len = (endi - starti - 1) / 2;
	uintptr_t i;
	if(REGS_SIZE < len) {
		len = REGS_SIZE;
	}
	for(i = starti + 1; i < starti + 1 + len * 2; i++) {
		uintptr_t b = 0;
		int32_t nib;
		nib = hex2int(buf[i++]);
		if(nib < 0) {
			return STUBERR_SRINV1;
		}
		b = nib << 4;

		nib = hex2int(buf[i]);
		if(nib < 0) {
			return STUBERR_SRINV2;
		}
		b |= nib;

		*p++ = b;
	}

	carttxbuf[0] = '+';
	carttxbuf[1] = '$';
	carttxbuf[2] = 'O';
	carttxbuf[3] = 'K';
	carttxbuf[4] = '#';
	if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0x07C0);

	return 0;
}

/* m<ADDR>,<LEN> -> <membytes>|Exx */
static uint8_t cmd_getmemory(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uintptr_t i = starti + 1;
	uintptr_t addr = 0, len = 0;
	for(; buf[i] != ','; i++) {
		if(endi <= i) {
			return STUBERR_GMADDRSHORT;
		}
		{
			int32_t nib = hex2int(buf[i]);
			if(nib < 0) {
				return STUBERR_GMADDRINV;
			}
			addr = (addr << 4) | nib;
		}
	}
	i++; /* skip ',' */
	for(; buf[i] != '#'; i++) {
		int32_t nib = hex2int(buf[i]);
		if(nib < 0) {
			return STUBERR_GMLENINV;
		}
		len = (len << 4) | nib;
	}

	if(sizeof(carttxbuf) - 5 < len / 2) {
		len = (sizeof(carttxbuf) - 5) / 2;
	}

	stub_recovered = 0;

	carttxbuf[0] = '+';
	carttxbuf[1] = '$';
	if(tohex(carttxbuf, sizeof(carttxbuf), 2, (void*)addr, len) < 0) {
		return STUBERR_GMSEGV;
	}
	carttxbuf[2 + len * 2] = '#';
	if(stub_recovered) {
		return 6;
	}
	if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0x07C0);

	return 0;
}

/* M<ADDR>,<LEN>:<BYTES> -> OK|Exx */
static uint8_t cmd_setmemory(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uintptr_t i = starti + 1;
	uintptr_t addr = 0, len = 0;
	for(; buf[i] != ','; i++) {
		if(endi <= i) {
			return STUBERR_SMADDRSHORT;
		}
		{
			int32_t nib = hex2int(buf[i]);
			if(nib < 0) {
				return STUBERR_SMADDRINV;
			}
			addr = (addr << 4) | nib;
		}
	}
	i++; /* skip ',' */
	for(; buf[i] != ':'; i++) {
		if(endi <= i) {
			return STUBERR_SMLENSHORT;
		}
		{
			int32_t nib = hex2int(buf[i]);
			if(nib < 0) {
				return STUBERR_SMLENINV;
			}
			len = (len << 4) | nib;
		}
	}
	i++; /* skip ':' */

	{
		/* remember addr,len at entry for flushing cache */
		const uintptr_t start_addr = addr, tobe_len = len;

		uintptr_t width;
		uint32_t by, phase;

		/* note: it sometimes access hardware register that requires specific bit width... don't simplify. */

		if(((addr & 3) == 0) && ((len & 3) == 0)) {
			width = 4;
		} else if(((addr & 1) == 0) && ((len & 1) == 0)) {
			width = 2;
		} else {
			width = 1;
		}

		stub_recovered = 0;
		for(by = 0, phase = 0; len; i++) {
			int32_t nib;
			if(buf[i] == '#') {
				break;
			}

			nib = hex2int(buf[i]);
			if(nib < 0) {
				return STUBERR_SMBYTESINV;
			}
			by = (by << 4) | nib;
			switch(width) {
			case 1:
				phase = (phase + 1) % 2;
				if(phase == 0) {
					*(uint8_t*)addr = by;
				}
				break;
			case 2:
				phase = (phase + 1) % 4;
				if(phase == 0) {
					*(uint16_t*)addr = by;
				}
				break;
			case 4:
				phase = (phase + 1) % 8;
				if(phase == 0) {
					*(uint32_t*)addr = by;
				}
				break;
			}
			if(phase == 0) {
				if(stub_recovered) {
					return STUBERR_SMSEGV;
				}
				addr += width;
				len -= width;
			}
		}

		/* to recognize new insn, ex. "break" */
		dwbinval((void*)start_addr, tobe_len);
		iinval((void*)start_addr, tobe_len);

		carttxbuf[0] = '+';
		carttxbuf[1] = '$';
		carttxbuf[2] = 'O';
		carttxbuf[3] = 'K';
		carttxbuf[4] = '#';
		if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0x07C0);

		return 0;
	}
}

static void bpset(uint32_t index, uintptr_t addr, uint32_t value) {
	origbpcodes[index].invaddr = ~addr;
	origbpcodes[index].value = *(uint32_t*)addr;
	*(uint32_t*)addr = value;

	dwbinval((void*)addr, 4);
	iinval((void*)addr, 4);
}

static void bprestore() {
	uint32_t i;

	for(i = 0; i < sizeof(origbpcodes)/sizeof(*origbpcodes); i++) {
		if(~origbpcodes[i].invaddr != -1) {
			uint32_t *addr = (uint32_t*)~origbpcodes[i].invaddr;

			*addr = origbpcodes[i].value;

			dwbinval(addr, 4);
			iinval(addr, 4);

			origbpcodes[i].invaddr = ~-1;
		}
	}
}

/* s -> (stop_reply) */
/* ref: Linux 2.6.25:arch/mips/kernel/gdb-stub.c */
static uint8_t cmd_step(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	intptr_t pc;
	uint32_t insn;
	intptr_t tnext, fnext;

	stub_recovered = 0;
	pc = regs[REGS_PC / 8];
	insn = *(uint32_t*)pc;
	if(stub_recovered) {
		return STUBERR_STPCSEGV;
	}

	tnext = pc + 4;
	fnext = -1;

	if((insn & 0xF8000000) == 0x08000000) { /* J/JAL */
		tnext = (tnext & 0xFFFFffffF0000000) | ((insn & 0x03FFffff) << 2);
	} else if(0
		|| ((insn & 0xFC0C0000) == 0x04000000) /* B{LT/GE}Z[AL][L] */
		|| ((insn & 0xF0000000) == 0x10000000) /* B{EQ/NE/LE/GT} */
		|| ((insn & 0xF3E00000) == 0x41000000) /* BCz{T/F}[L] */
		|| ((insn & 0xF0000000) == 0x50000000) /* B{EQ/NE/LE/GT}L */
		) {
		fnext = tnext + 4; /* next of branch-delay */
		tnext = tnext + ((intptr_t)(int16_t)insn << 2);
	} else if((insn & 0xFC00003E) == 0x00000008) { /* JR/JALR */
		uint32_t reg = (insn >> 21) & 0x1F;
		tnext = regs[REGS_GPR / 8 + reg];
		if(stub_recovered) {
			return STUBERR_STJRSEGV;
		}
	}

	if(fnext != -1) {
		bpset(1, fnext, 0x0000000D);
		if(stub_recovered) {
			return STUBERR_STFNSEGV;
		}
	}
	bpset(0, tnext, 0x0000000D);
	if(stub_recovered) {
		return STUBERR_STTNSEGV;
	}

	return 0;
}

void stub_main(void) {
	extern void stub_recover(void);
	backup_and_install_handlers(origrcvrcodes, stub_recover);

	bprestore();

	/* epc=ra if called with EXL=0, to make eret returns to call site. */
	if((regs[REGS_SR / 8] & 0x0000002) == 0) {
		regs[REGS_PC / 8] = regs[REGS_GPR / 8 + 31];
	}

	/* move enableregs out of infloop for debugging ed64 with gdb. */
	ed_enableregs(1);

	{
		carttxbuf[0] = '$';
		carttxbuf[1] = 'S';
		carttxbuf[2] = '0';
		carttxbuf[3] = '5';
		carttxbuf[4] = '#';
		if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0xFFC0);
	}

	for(;;) {
		uintptr_t len;
		uintptr_t i;

		/*ed_enableregs(1);*/

		/* wait for arriving data */
		for(;;) {
			ED64->cfg;
			if(!(ED64->status & ED64_STATUS_RXF)) {
				break;
			}
		}

		len = fifo2dram(cartrxbuf, sizeof(cartrxbuf));

		/*ed_enableregs(0);*/

		for(i = 0; i < len; i++) {
			uintptr_t starti, endi;
			uint32_t realsum;
			uint8_t error;

			if(cartrxbuf[i] != '$') {
				/* not start of GDB remote packet */
				/* TODO ignoreing '+' is ok, but '-' is not!! */
				continue;
			}

			/* found packet head */
			i++; if(len <= i) goto pkterr;

			starti = i;

			realsum = 0;
			for(;;) {
				if(cartrxbuf[i] == '#') {
					break;
				}
				realsum += cartrxbuf[i];
				i++; if(len <= i) goto pkterr;
			}

			endi = i;

			/* skip '#' */
			i++; if(len <= i) goto pkterr;
			{
				int32_t expectsum;
				/* read expect cksum */
				expectsum = hex2int(cartrxbuf[i]) << 4;
				i++; if(len <= i) goto pkterr;
				expectsum |= hex2int(cartrxbuf[i]);

				/* error if checksum mismatch (inclding malformed expect cksum) */
				if((realsum & 0xFF) != expectsum) {
					goto pkterr;
				}
			}

			/* here, we can assume whole command fits in rxbuf. */

			/* process command */
			error = 0;
			switch(cartrxbuf[starti]) {
			case 'g': /* register target2host */
				error = cmd_getregs(cartrxbuf, starti, endi);
				break;
			case 'G': /* register host2target */
				error = cmd_setregs(cartrxbuf, starti, endi);
				break;
			case 'm': /* memory target2host */
				error = cmd_getmemory(cartrxbuf, starti, endi);
				break;
			case 'M': /* memory host2target */
				error = cmd_setmemory(cartrxbuf, starti, endi);
				break;
			case '?': /* last signal */
				{
					carttxbuf[0] = '+';
					carttxbuf[1] = '$';
					carttxbuf[2] = 'S';
					carttxbuf[3] = '0';
					carttxbuf[4] = '5';
					carttxbuf[5] = '#';
					if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0xFFC0);
				}
				break;
			case 's': /* step */
				{
					error = cmd_step(cartrxbuf, starti, endi);
					if(error == 0) {
						carttxbuf[0] = '+';
						sendraw(carttxbuf, 512, 1);
						goto bye;
					}
				}
				break;
			case 'c': /* continue */
				/* c[ADDR] */
				{
					if(cartrxbuf[starti + 1] != '#') {
						/* argument not supported */
						error = STUBERR_CADDRNOTIMPL;
						break;
					}

					/* nothing to do */

					if(error == 0) {
						carttxbuf[0] = '+';
						sendraw(carttxbuf, 512, 1);
						goto bye;
					}
				}
				break;
			default:
				/* unknown command... */
				carttxbuf[0] = '+';
				carttxbuf[1] = '$';
				carttxbuf[2] = '#';
				if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0xFFC0);
			}

			if(error != 0) {
				carttxbuf[0] = '+';
				carttxbuf[1] = '$';
				carttxbuf[2] = 'E';
				if(tohex(carttxbuf, sizeof(carttxbuf), 3, &error, 1) < 0) die(0xFC00);
				carttxbuf[5] = '#';
				if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0xFFC0);
			}

			continue;
pkterr:
			/* return NAK */
			carttxbuf[0] = '-'; /* NAK */
			sendraw(carttxbuf, 512, 1);
			continue; /* if not invalid sum, should be break. if it is, should be continue. */
		}
	}
bye:
	ed_enableregs(0);

	restore_handlers(origrcvrcodes);
}
