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
