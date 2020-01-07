/* cpp defines for both C and asm */

/* you must define one of following. */
#define CONFIG_GDBSTUB_CTX_ZERO /* requires a (virtually wired) TLB entry */
/* #define CONFIG_GDBSTUB_CTX_GP */ /* requires 8 bytes in sbss */
/* #define CONFIG_GDBSTUB_CTX_K0 */ /* requires clobbering $k0 */

/* you can optionally set following. */
/* #define CONFIG_GDBSTUB_INTERRUPT_HANDLER inthandler */ /* jump to this handler if exception is interrupt. (sample value is for libdragon) */

#if _MIPS_SZPTR == 32
#define P32(addr) (addr)
#define LA_P32(reg,addr) la reg, addr
#define LA_SYM(reg,sym) la reg, sym
#define MATC0(reg,creg) mtc0 reg, creg
#define MAFC0(reg,creg) mfc0 reg, creg
#define ADDAIU(dreg,sreg,imm) addiu dreg, sreg, imm
#elif _MIPS_SZPTR == 64
#define P32(addr) (0xFFFFffff00000000 | (addr))
#define LA_P32(reg,addr) dla reg, 0xFFFFffff00000000 | (addr)
#define LA_SYM(reg,sym) dla reg, sym
#define MATC0(reg,creg) dmtc0 reg, creg
#define MAFC0(reg,creg) dmfc0 reg, creg
#define ADDAIU(dreg,sreg,imm) daddiu dreg, sreg, imm
#else
#error Unknown MIPS address size
#endif

/* must be ordered as GDB serial protocol's 64bit MIPS registers */
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
#define REGS_SIZE (72*8)
#if defined(CONFIG_GDBSTUB_CTX_ZERO)
/* balign and size must be at least the page size (=4KiB) */
# define GDBSTUBCTX_BALIGN 4096
# define GDBSTUBCTX_SIZE 4096
/* REGS_BASE must be register number without $ sign. */
# define REGS_BASE 0 /* $zero */
#elif defined(CONFIG_GDBSTUB_CTX_GP) || defined(CONFIG_GDBSTUB_CTX_K0)
/* balign must be at least doubleword (8bytes), but size is only restricted by regs + stack size aligned by 8bytes */
# define GDBSTUBCTX_BALIGN 8
# define GDBSTUBCTX_SIZE 4096
/* REGS_BASE must be...
 *   CTX_GP: any register except (zero,s0-s7,gp,sp,fp,ra), k0 and k1 can be also used.
 *   CTX_K0: k0 or k1.
 * REGS_BASE must be register number without $ sign. */
# define REGS_BASE 26 /* CTX_GP: not clobbered, saved using gp. CTX_K0: clobbered. */
#else
# error You must define one of CONFIG_GDBSTUB_CTX_*
#endif
