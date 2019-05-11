/* cpp defines for both C and asm */

#if _MIPS_SZPTR == 32
#define P32(addr) (addr)
#define LA_P32(reg,addr) la reg, addr
#define LA_SYM(reg,sym) la reg, sym
#define LI_U32(reg,val) li reg, val
#define MATC0(reg,creg) mtc0 reg, creg
#define MAFC0(reg,creg) mfc0 reg, creg
#define ADDAIU(dreg,sreg,imm) addiu dreg, sreg, imm
#define ADDAU(dreg,sreg,treg) addu dreg, sreg, treg
#elif _MIPS_SZPTR == 64
#define P32(addr) (0xFFFFffff00000000 | (addr))
#define LA_P32(reg,addr) dla reg, 0xFFFFffff00000000 | (addr)
#define LA_SYM(reg,sym) dla reg, sym
#define LI_U32(reg,val) dli reg, (val)
#define MATC0(reg,creg) dmtc0 reg, creg
#define MAFC0(reg,creg) dmfc0 reg, creg
#define ADDAIU(dreg,sreg,imm) daddiu dreg, sreg, imm
#define ADDAU(dreg,sreg,treg) daddu dreg, sreg, treg
#else
#error Unknown MIPS address size
#endif

#define REGS_START ((-32-6-32-2)*8)
#define REGS_GPR (0*8)
#define REGS_SR (32*8)
#define REGS_LO (33*8)
#define REGS_HI (34*8)
#define REGS_BAD (35*8)
#define REGS_CAUSE (36*8)
#define REGS_PC (37*8)
#define REGS_FGR (38*8)
#define REGS_FPCS (70*8)
#define REGS_FPIR (71*8)
#define REGS_END (72*8)
