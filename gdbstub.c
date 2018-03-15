#include <stdint.h>
#include "3264.h"
#include "regs.h"
#include "cache.h"
#include "gdbstub.h"

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

// 0~ GPR0-31(yes, include zero),[32]PS(status),LO,HI,BadVAddr,Cause,PC,[38]FPR0-31,[70]fpcs,fpir,[72]..(dsp?),[90]end
//static const uint64_t *regs = (void*)((32+6+32+2) * -8);

static uint8_t __attribute((aligned(8))) cartrxbuf[2048];
static uint8_t __attribute((aligned(8))) carttxbuf[2048];
static uint32_t origintcodes[4][2];
static struct {
	intptr_t addr; // -1 = unset
	uint32_t value;
} origbpcodes[2];

static void ed_enableregs(int en) {
	ED64->cfg;
	ED64->key = en ? 0x1234 : 0; // Enable/Disable ED64 regs
}

static void __attribute((unused)) ed_sdram(int sdram) {
	ED64->cfg;
	ED64->cfg = sdram ? ED64_CFG_SDRAM_ON : 0; // config: sdram <sdram>, swap off, wr_mod off, wr_addr_mask off.
	ED64->cfg;
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
	PI->dram2cart = len - 1; // 64bits unit

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
	dwbinval(buf, len); // TODO inval? only wb cause false write again but faster re-r/w. use case?
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
	// not dwbinval, discard to-be-write (that is not necessary data... overwritten soon.)
	dinval(buf, maxlen);

	// note: I want to fifo2cart many time and then one cart2dram,
	//       but ED64's len=512/addr=2048(!) align does not allow it...
	const uint32_t cartaddr = 0x04000000 - 2048;
	uintptr_t len = 0;
	while(len < maxlen) {
		if(0 < len) {
			ED64->cfg;
			if(ED64->status & ED64_STATUS_RXF) { // no more data (RXF#)
				break;
			}
		}

		fifo2cart(cartaddr, 512);

		if(ED64->status & ED64_STATUS_DMATIMEOUT) { // timeout
			break;
		}

		cart2dram(buf, cartaddr, 512);
		buf = (char*)buf + 512;
		len += 512;
	}
	return len;
}

static void install_handler(void *pfn) {
	uint32_t stubintcode[2];
	stubintcode[0] = 0x08000000 | (((uint32_t)(uintptr_t)pfn >> 2) & 0x03FFffff); // j *pfn
	stubintcode[1] = 0; // branch-delay-slot: nop

	for(uint32_t i = 0; i < 2; i++) {
		for(uint32_t j = 0; j < 4; j++) {
			*(uint32_t*)P32(0x80000000 + j * 0x80 + i * 4) = stubintcode[i];
		}
	}

	dwbinval((void*)P32(0x80000000), 0x200);
	iinval((void*)P32(0x80000000), 0x200);
}

static void install_switch(void) {
	extern void stub_switch(void);
	install_handler(stub_switch);
}
static void install_recover(void) {
	extern void stub_recover(void);
	install_handler(stub_recover);
}

void stub_install(void) {
	extern void stub_installtlb();
	stub_installtlb();

	for(uint32_t i = 0; i < sizeof(origbpcodes)/sizeof(*origbpcodes); i++) {
		origbpcodes[i].addr = -1;
	}

	for(uint32_t i = 0; i < 2; i++) {
		for(uint32_t j = 0; j < 4; j++) {
			origintcodes[j][i] = *(uint32_t*)P32(0x80000000 + j * 0x80 + i * 4);
		}
	}

	install_switch();
}

void stub_uninstall(void) {
	for(uint32_t i = 0; i < 2; i++) {
		for(uint32_t j = 0; j < 4; j++) {
			*(uint32_t*)P32(0x80000000 + j * 0x80 + i * 4) = origintcodes[j][i];
		}
	}

	dwbinval((void*)P32(0x80000000), 0x200);
	iinval((void*)P32(0x80000000), 0x200);
}

// sample program entry
void stub(void) {
	// make TLB initialized; TODO detect TLB initialized and skip... possible??
	extern void stub_tlbunmapall();
	stub_tlbunmapall();

	stub_install();
	__asm("syscall"); // invoke stub by triggering (safe&unique) exception
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
	static const char i2h[16] = "0123456789ABCDEF";
	for(uintptr_t lpos = nibbles; 0 < lpos; lpos--) {
		if(bufsize <= index) return -1; // overflow!
		buf[index] = i2h[(value >> (4 * (lpos - 1))) & 15];
		index++;
	}
	return index;
}

// TODO how represent when buffer overflow??
// TODO integer overflow are there!!
static intptr_t tohex(uint8_t *buf, uintptr_t bufsize, intptr_t index, const void *src_, uintptr_t srclen) {
	uintptr_t i = 0;
	stub_recovered = 0;

	if((((uintptr_t)buf & 3) == 0) && ((srclen & 3) == 0)) {
		// word access
		const uint32_t *src = src_;
		for(; i < srclen / 4; i++) {
			uint32_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 8)) < 0) {
				return -1; // overflow!
			}
		}
	} else if((((uintptr_t)buf & 1) == 0) && ((srclen & 1) == 0)) {
		// halfword access
		const uint16_t *src = src_;
		for(; i < srclen / 2; i++) {
			uint16_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 4)) < 0) {
				return -1; // overflow!
			}
		}
	} else {
		// byte access
		const uint8_t *src = src_;
		for(; i < srclen; i++) {
			uint8_t s = src[i];
			if(stub_recovered) {
				return -1;
			}

			if((index = int2hex(buf, bufsize, index, s, 2)) < 0) {
				return -1; // overflow!
			}
		}
	}
	return index;
}

static void sendraw(uint8_t *buf, uintptr_t bufsize, uintptr_t len) {
	// zerofill tail
	for(; len < bufsize; len++) {
		buf[len] = 0;
	}

	// go!!
	dram2fifo(buf, bufsize);
}

static int sendpkt(uint8_t *buf, intptr_t bufsize) {
	if(bufsize < 4) {
		// no minimum space is available. illegal function call!!
		return -1;
	}

	intptr_t si;
	for(si = 0; si < bufsize - 3; si++) {
		if(buf[si] == '$') {
			break;
		}
	}
	if(si == bufsize - 3) {
		// no start marker found
		return -2;
	}
	si++;

	// find end marker
	intptr_t ei;
	for(ei = si; ei < bufsize - 2; ei++) {
		if(buf[ei] == '#') {
			break;
		}
	}
	if(ei == bufsize - 2) {
		// no end marker found
		return -3;
	}

	// calculate and fill checksum
	uintptr_t sum = 0;
	for(uintptr_t i = si; i < ei; i++) {
		sum += buf[i];
	}
	uint8_t sumb[1] = {sum};
	if(tohex(buf, bufsize, ei + 1, sumb, 1) < 0) {
		// should not be happened!
		return -4;
	}
	ei += 1 + 2;

	// go!!
	uintptr_t ssize = (ei + (512 - 1)) & -512;
	sendraw(buf, ssize, ei);

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

// g -> <regbytes>
static uint8_t cmd_getregs(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	carttxbuf[0] = '+';
	carttxbuf[1] = '$';
	intptr_t ei = tohex(carttxbuf, sizeof(carttxbuf), 2, (void*)P32(REGS_START), REGS_END);
	if(ei < 0) {
		return STUBERR_GRTOOSHORT;
	}
	carttxbuf[ei] = '#';
	if(sendpkt(carttxbuf, sizeof(carttxbuf))) die(0x07C0);

	return 0;
}

// G<regbytes> -> OK|Exx
static uint8_t cmd_setregs(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uint8_t *p = (void*)P32(REGS_START);
	uintptr_t len = (endi - starti - 1) / 2;
	if(REGS_END < len) {
		len = REGS_END;
	}
	for(uintptr_t i = starti + 1; i < starti + 1 + len * 2; i++) {
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

// m<ADDR>,<LEN> -> <membytes>|Exx
static uint8_t cmd_getmemory(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uintptr_t i = starti + 1;
	uintptr_t addr = 0, len = 0;
	for(; buf[i] != ','; i++) {
		if(endi <= i) {
			return STUBERR_GMADDRSHORT;
		}
		int32_t nib = hex2int(buf[i]);
		if(nib < 0) {
			return STUBERR_GMADDRINV;
		}
		addr = (addr << 4) | nib;
	}
	i++; // skip ','
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

// M<ADDR>,<LEN>:<BYTES> -> OK|Exx
static uint8_t cmd_setmemory(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	uintptr_t i = starti + 1;
	uintptr_t addr = 0, len = 0;
	for(; buf[i] != ','; i++) {
		if(endi <= i) {
			return STUBERR_SMADDRSHORT;
		}
		int32_t nib = hex2int(buf[i]);
		if(nib < 0) {
			return STUBERR_SMADDRINV;
		}
		addr = (addr << 4) | nib;
	}
	i++; // skip ','
	for(; buf[i] != ':'; i++) {
		if(endi <= i) {
			return STUBERR_SMLENSHORT;
		}
		int32_t nib = hex2int(buf[i]);
		if(nib < 0) {
			return STUBERR_SMLENINV;
		}
		len = (len << 4) | nib;
	}
	i++; // skip ':'

	// remember addr,len at entry for flushing cache
	const uintptr_t start_addr = addr, tobe_len = len;

	uintptr_t width;
	if(((addr & 3) == 0) && ((len & 3) == 0)) {
		width = 4;
	} else if(((addr & 1) == 0) && ((len & 1) == 0)) {
		width = 2;
	} else {
		width = 1;
	}

	stub_recovered = 0;
	for(uint32_t by = 0, phase = 0; len; i++) {
		if(buf[i] == '#') {
			break;
		}
		int32_t nib = hex2int(buf[i]);
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

	// to recognize new insn, ex. "break"
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

static void bpset(uint32_t index, uintptr_t addr, uint32_t value) {
	origbpcodes[index].addr = addr;
	origbpcodes[index].value = *(uint32_t*)addr;
	*(uint32_t*)addr = value;

	dwbinval((void*)addr, 4);
	iinval((void*)addr, 4);
}

// s -> (stop_reply)
// ref: Linux 2.6.25:arch/mips/kernel/gdb-stub.c
static uint8_t cmd_step(uint8_t *buf, uintptr_t starti, uintptr_t endi) {
	stub_recovered = 0;
	intptr_t pc = *(int64_t*)(P32(REGS_START) + REGS_PC);
	uint32_t insn = *(uint32_t*)pc;
	if(stub_recovered) {
		return STUBERR_STPCSEGV;
	}

	intptr_t tnext = pc + 4;
	intptr_t fnext = -1;

	if((insn & 0xF8000000) == 0x08000000) { // J/JAL
		tnext = (tnext & 0xFFFFffffF0000000) | ((insn & 0x03FFffff) << 2);
	} else if(0
		|| ((insn & 0xFC0C0000) == 0x04000000) // B{LT/GE}Z[AL][L]
		|| ((insn & 0xF0000000) == 0x10000000) // B{EQ/NE/LE/GT}
		|| ((insn & 0xF3E00000) == 0x41000000) // BCz{T/F}[L]
		|| ((insn & 0xF0000000) == 0x50000000) // B{EQ/NE/LE/GT}L
		) {
		fnext = tnext + 4; // next of branch-delay
		tnext = tnext + ((intptr_t)(int16_t)insn << 2);
	} else if((insn & 0xFC00003E) == 0x00000008) { // JR/JALR
		uint32_t reg = (insn >> 21) & 0x1F;
		tnext = *(int64_t*)(P32(REGS_START) + REGS_GPR + (reg * 8));
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

// interrupt entry
void stub_entry(void) {
	install_recover();

	// bprestore
	for(uint32_t i = 0; i < sizeof(origbpcodes)/sizeof(*origbpcodes); i++) {
		if(origbpcodes[i].addr != -1) {
			uint32_t *addr = (uint32_t*)origbpcodes[i].addr;

			*addr = origbpcodes[i].value;

			dwbinval(addr, 4);
			iinval(addr, 4);

			origbpcodes[i].addr = -1;
		}
	}

	// move enableregs out of infloop for debugging ed64 with gdb.
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
		//ed_enableregs(1);

		// wait for arriving data
		for(;;) {
			ED64->cfg;
			if(!(ED64->status & ED64_STATUS_RXF)) {
				break;
			}
		}

		uintptr_t len = fifo2dram(cartrxbuf, sizeof(cartrxbuf));

		//ed_enableregs(0);

		for(uintptr_t i = 0; i < len; i++) {
			if(cartrxbuf[i] != '$') {
				// not start of GDB remote packet
				// TODO ignoreing '+' is ok, but '-' is not!!
				continue;
			}

			// found packet head
			i++; if(len <= i) goto pkterr;

			uintptr_t starti = i;

			uint32_t realsum = 0;
			for(;;) {
				if(cartrxbuf[i] == '#') {
					break;
				}
				realsum += cartrxbuf[i];
				i++; if(len <= i) goto pkterr;
			}

			uintptr_t endi = i;

			// skip '#'
			i++; if(len <= i) goto pkterr;
			// read expect cksum
			int32_t expectsum;
			expectsum = hex2int(cartrxbuf[i]) << 4;
			i++; if(len <= i) goto pkterr;
			expectsum |= hex2int(cartrxbuf[i]);

			// error if checksum mismatch (inclding malformed expect cksum)
			if((realsum & 0xFF) != expectsum) {
				goto pkterr;
			}

			// here, we can assume whole command fits in rxbuf.

			// process command
			uint8_t error = 0;
			switch(cartrxbuf[starti]) {
			case 'g': // register target2host
				error = cmd_getregs(cartrxbuf, starti, endi);
				break;
			case 'G': // register host2target
				error = cmd_setregs(cartrxbuf, starti, endi);
				break;
			case 'm': // memory target2host
				error = cmd_getmemory(cartrxbuf, starti, endi);
				break;
			case 'M': // memory host2target
				error = cmd_setmemory(cartrxbuf, starti, endi);
				break;
			case '?': // last signal
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
			case 's': // step
				{
					error = cmd_step(cartrxbuf, starti, endi);
					if(error == 0) {
						carttxbuf[0] = '+';
						sendraw(carttxbuf, 512, 1);
						goto bye;
					}
				}
				break;
			case 'c': // continue
				// c[ADDR]
				{
					if(cartrxbuf[starti + 1] != '#') {
						// argument not supported
						error = STUBERR_CADDRNOTIMPL;
						break;
					}

					// nothing to do

					if(error == 0) {
						carttxbuf[0] = '+';
						sendraw(carttxbuf, 512, 1);
						goto bye;
					}
				}
				break;
			default:
				// unknown command...
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
			// return NAK
			carttxbuf[0] = '-'; // NAK
			sendraw(carttxbuf, 512, 1);
			continue; // if not invalid sum, should be break. if it is, should be continue.
		}
	}
bye:
	ed_enableregs(0);

	install_switch();
}
