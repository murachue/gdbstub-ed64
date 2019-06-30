#include "gdbstub.h"

	.text
	.set push
	.set noat
	.set hardfloat

	# override ISA/GPsize(/FPsize) regardless of ABI... in O32 "sd" becomes "sw $reg; sw $reg+1" that is bad for GDB serial protocol's 64bit MIPS registers.
	.set mips3
	.set gp=64
	.set fp=64

	.type stub, function
	.globl stub
stub:

	## context save

#ifdef CONFIG_GDBSTUB_CTX_GP
	# save then use
	sd $REGS_BASE, %gp_rel(gdbstubctxk0)($gp)
	LA_SYM($REGS_BASE, gdbstubctx + GDBSTUBCTX_SIZE)
#endif
#ifdef CONFIG_GDBSTUB_CTX_K0
	# just clobber
	LA_SYM($REGS_BASE, gdbstubctx + GDBSTUBCTX_SIZE)
#endif

	# GPRs
	sd  $0, (-REGS_SIZE + REGS_GPR +  0*8)($REGS_BASE)
	sd  $1, (-REGS_SIZE + REGS_GPR +  1*8)($REGS_BASE)
	sd  $2, (-REGS_SIZE + REGS_GPR +  2*8)($REGS_BASE)
	sd  $3, (-REGS_SIZE + REGS_GPR +  3*8)($REGS_BASE)
	sd  $4, (-REGS_SIZE + REGS_GPR +  4*8)($REGS_BASE)
	sd  $5, (-REGS_SIZE + REGS_GPR +  5*8)($REGS_BASE)
	sd  $6, (-REGS_SIZE + REGS_GPR +  6*8)($REGS_BASE)
	sd  $7, (-REGS_SIZE + REGS_GPR +  7*8)($REGS_BASE)
	sd  $8, (-REGS_SIZE + REGS_GPR +  8*8)($REGS_BASE)
	sd  $9, (-REGS_SIZE + REGS_GPR +  9*8)($REGS_BASE)
	sd $10, (-REGS_SIZE + REGS_GPR + 10*8)($REGS_BASE)
	sd $11, (-REGS_SIZE + REGS_GPR + 11*8)($REGS_BASE)
	sd $12, (-REGS_SIZE + REGS_GPR + 12*8)($REGS_BASE)
	sd $13, (-REGS_SIZE + REGS_GPR + 13*8)($REGS_BASE)
	sd $14, (-REGS_SIZE + REGS_GPR + 14*8)($REGS_BASE)
	sd $15, (-REGS_SIZE + REGS_GPR + 15*8)($REGS_BASE)
	sd $16, (-REGS_SIZE + REGS_GPR + 16*8)($REGS_BASE)
	sd $17, (-REGS_SIZE + REGS_GPR + 17*8)($REGS_BASE)
	sd $18, (-REGS_SIZE + REGS_GPR + 18*8)($REGS_BASE)
	sd $19, (-REGS_SIZE + REGS_GPR + 19*8)($REGS_BASE)
	sd $20, (-REGS_SIZE + REGS_GPR + 20*8)($REGS_BASE)
	sd $21, (-REGS_SIZE + REGS_GPR + 21*8)($REGS_BASE)
	sd $22, (-REGS_SIZE + REGS_GPR + 22*8)($REGS_BASE)
	sd $23, (-REGS_SIZE + REGS_GPR + 23*8)($REGS_BASE)
	sd $24, (-REGS_SIZE + REGS_GPR + 24*8)($REGS_BASE)
	sd $25, (-REGS_SIZE + REGS_GPR + 25*8)($REGS_BASE)
	sd $26, (-REGS_SIZE + REGS_GPR + 26*8)($REGS_BASE)
	sd $27, (-REGS_SIZE + REGS_GPR + 27*8)($REGS_BASE)
	sd $28, (-REGS_SIZE + REGS_GPR + 28*8)($REGS_BASE)
	sd $29, (-REGS_SIZE + REGS_GPR + 29*8)($REGS_BASE)
	sd $30, (-REGS_SIZE + REGS_GPR + 30*8)($REGS_BASE)
	sd $31, (-REGS_SIZE + REGS_GPR + 31*8)($REGS_BASE)

#ifdef CONFIG_GDBSTUB_CTX_GP
	# copy saved value (using already saved register)
	ld  $1, %gp_rel(gdbstubctxk0)($gp)
	sd  $1, (-REGS_SIZE + REGS_GPR + REGS_BASE*8)($REGS_BASE)
#endif

#ifdef CONFIG_GDBSTUB_INTERRUPT_HANDLER
	## passing interrupt
	mfc0 $1, $12 # status
	andi $1, 0x0002
	beqz $1, 1f
	# EXL is high: exception, not directly called
	mfc0 $1, $13 # cause
	andi $1, 0x00FF
	bnez $1, 1f
	# cause is 0=interrupt. let it pass.
	# restore $1
	ld  $1, (-REGS_SIZE + REGS_GPR +  1*8)($REGS_BASE)
	# then jump into handler
	j CONFIG_GDBSTUB_INTERRUPT_HANDLER
1:
#endif

	# CP0s
	mfc0 $12, $12 # status
	sd $12, (-REGS_SIZE + REGS_SR)($REGS_BASE) # 32bit as 64bit
	# set SR known state; SR is saved, modify ok.
	# enable SR.cop1 to allow saving FGRs, and FR=1 to ensure 64bit*32 fgrs are usable.
	# 64bit enabled(to enable sdc1/ldc1 access 64bit fgr). set KSU/EXL/ERL to 0. Interrupt is also disabled.
	li $12, 0x241000E0
	mtc0 $12, $12
	# changing SR requires two more cycles to be effective (or *sometimes* CpU exception at next sdc1!!)
	# ...that is consumed by next mflo/sd.
	# rest of CP0s
	mflo $12
	sd $12, (-REGS_SIZE + REGS_LO)($REGS_BASE)
	mfhi $12
	sd $12, (-REGS_SIZE + REGS_HI)($REGS_BASE)
	dmfc0 $12, $8 # badvaddr
	sd $12, (-REGS_SIZE + REGS_BAD)($REGS_BASE)
	mfc0 $12, $13 # cause
	sd $12, (-REGS_SIZE + REGS_CAUSE)($REGS_BASE) # 32bit as 64bit
	dmfc0 $12, $14 # epc (original pc)
	sd $12, (-REGS_SIZE + REGS_PC)($REGS_BASE)

	# CP1 FGRs
	sdc1  $0, (-REGS_SIZE + REGS_FGR +  0*8)($REGS_BASE)
	sdc1  $1, (-REGS_SIZE + REGS_FGR +  1*8)($REGS_BASE)
	sdc1  $2, (-REGS_SIZE + REGS_FGR +  2*8)($REGS_BASE)
	sdc1  $3, (-REGS_SIZE + REGS_FGR +  3*8)($REGS_BASE)
	sdc1  $4, (-REGS_SIZE + REGS_FGR +  4*8)($REGS_BASE)
	sdc1  $5, (-REGS_SIZE + REGS_FGR +  5*8)($REGS_BASE)
	sdc1  $6, (-REGS_SIZE + REGS_FGR +  6*8)($REGS_BASE)
	sdc1  $7, (-REGS_SIZE + REGS_FGR +  7*8)($REGS_BASE)
	sdc1  $8, (-REGS_SIZE + REGS_FGR +  8*8)($REGS_BASE)
	sdc1  $9, (-REGS_SIZE + REGS_FGR +  9*8)($REGS_BASE)
	sdc1 $10, (-REGS_SIZE + REGS_FGR + 10*8)($REGS_BASE)
	sdc1 $11, (-REGS_SIZE + REGS_FGR + 11*8)($REGS_BASE)
	sdc1 $12, (-REGS_SIZE + REGS_FGR + 12*8)($REGS_BASE)
	sdc1 $13, (-REGS_SIZE + REGS_FGR + 13*8)($REGS_BASE)
	sdc1 $14, (-REGS_SIZE + REGS_FGR + 14*8)($REGS_BASE)
	sdc1 $15, (-REGS_SIZE + REGS_FGR + 15*8)($REGS_BASE)
	sdc1 $16, (-REGS_SIZE + REGS_FGR + 16*8)($REGS_BASE)
	sdc1 $17, (-REGS_SIZE + REGS_FGR + 17*8)($REGS_BASE)
	sdc1 $18, (-REGS_SIZE + REGS_FGR + 18*8)($REGS_BASE)
	sdc1 $19, (-REGS_SIZE + REGS_FGR + 19*8)($REGS_BASE)
	sdc1 $20, (-REGS_SIZE + REGS_FGR + 20*8)($REGS_BASE)
	sdc1 $21, (-REGS_SIZE + REGS_FGR + 21*8)($REGS_BASE)
	sdc1 $22, (-REGS_SIZE + REGS_FGR + 22*8)($REGS_BASE)
	sdc1 $23, (-REGS_SIZE + REGS_FGR + 23*8)($REGS_BASE)
	sdc1 $24, (-REGS_SIZE + REGS_FGR + 24*8)($REGS_BASE)
	sdc1 $25, (-REGS_SIZE + REGS_FGR + 25*8)($REGS_BASE)
	sdc1 $26, (-REGS_SIZE + REGS_FGR + 26*8)($REGS_BASE)
	sdc1 $27, (-REGS_SIZE + REGS_FGR + 27*8)($REGS_BASE)
	sdc1 $28, (-REGS_SIZE + REGS_FGR + 28*8)($REGS_BASE)
	sdc1 $29, (-REGS_SIZE + REGS_FGR + 29*8)($REGS_BASE)
	sdc1 $30, (-REGS_SIZE + REGS_FGR + 30*8)($REGS_BASE)
	sdc1 $31, (-REGS_SIZE + REGS_FGR + 31*8)($REGS_BASE)

	# CP1 controls
	cfc1 $12, $31 # fpcs
	sd $12, (-REGS_SIZE + REGS_FPCS)($REGS_BASE) # 32bit as 64bit
	cfc1 $12, $0 # fpir
	sd $12, (-REGS_SIZE + REGS_FPIR)($REGS_BASE) # 32bit as 64bit

	## calling core

#ifdef CONFIG_GDBSTUB_CTX_ZERO
	# kseg0 and sseg must be coherent.
	# dwbinval both the gdbstubctx on kseg0 and the TLB mapped gdbstubctx on sseg
	li $1, REGS_SIZE
	LA_P32($2, -REGS_SIZE)
	LA_SYM($3, gdbstubctx + GDBSTUBCTX_SIZE - REGS_SIZE)
1:
	cache 21, 0($2)
	cache 21, 0($3)
	# N64's D-cache line is 16 bytes
	ADDAIU($2, $2, 16)
	ADDAIU($3, $3, 16)
	addiu $1, -16
	bgtz $1, 1b
#endif

	# set sp to safe value
	#LA_P32($29, -REGS_SIZE - 0x10($REGS_BASE))
	LA_SYM($29, gdbstubctx + GDBSTUBCTX_SIZE - REGS_SIZE - 0x10)

	# call the stub main!
	jal stub_main

	## context restoration

#ifdef CONFIG_GDBSTUB_CTX_ZERO
	# kseg0 and sseg must be coherent.
	# dwbinval both the gdbstubctx on kseg0 and the TLB mapped gdbstubctx on sseg
	li $1, REGS_SIZE
	LA_P32($2, -REGS_SIZE)
	LA_SYM($3, gdbstubctx + GDBSTUBCTX_SIZE - REGS_SIZE)
1:
	cache 21, 0($2)
	cache 21, 0($3)
	# N64's D-cache line is 16 bytes
	ADDAIU($2, $2, 16)
	ADDAIU($3, $3, 16)
	addiu $1, -16
	bgtz $1, 1b
#endif

	# using $31($ra) as a base looks strange at a glance, but restore code become looks simpler.
	LA_SYM($31, gdbstubctx + GDBSTUBCTX_SIZE)

	# CP1 controls
	ld $12, (-REGS_SIZE + REGS_FPCS)($31) # 32bit as 64bit
	ctc1 $12, $31 # fpcs
	ld $12, (-REGS_SIZE + REGS_FPIR)($31) # 32bit as 64bit
	ctc1 $12, $0 # fpir... writable??

	# CP1 FGRs
	ldc1  $0, (-REGS_SIZE + REGS_FGR +  0*8)($31)
	ldc1  $1, (-REGS_SIZE + REGS_FGR +  1*8)($31)
	ldc1  $2, (-REGS_SIZE + REGS_FGR +  2*8)($31)
	ldc1  $3, (-REGS_SIZE + REGS_FGR +  3*8)($31)
	ldc1  $4, (-REGS_SIZE + REGS_FGR +  4*8)($31)
	ldc1  $5, (-REGS_SIZE + REGS_FGR +  5*8)($31)
	ldc1  $6, (-REGS_SIZE + REGS_FGR +  6*8)($31)
	ldc1  $7, (-REGS_SIZE + REGS_FGR +  7*8)($31)
	ldc1  $8, (-REGS_SIZE + REGS_FGR +  8*8)($31)
	ldc1  $9, (-REGS_SIZE + REGS_FGR +  9*8)($31)
	ldc1 $10, (-REGS_SIZE + REGS_FGR + 10*8)($31)
	ldc1 $11, (-REGS_SIZE + REGS_FGR + 11*8)($31)
	ldc1 $12, (-REGS_SIZE + REGS_FGR + 12*8)($31)
	ldc1 $13, (-REGS_SIZE + REGS_FGR + 13*8)($31)
	ldc1 $14, (-REGS_SIZE + REGS_FGR + 14*8)($31)
	ldc1 $15, (-REGS_SIZE + REGS_FGR + 15*8)($31)
	ldc1 $16, (-REGS_SIZE + REGS_FGR + 16*8)($31)
	ldc1 $17, (-REGS_SIZE + REGS_FGR + 17*8)($31)
	ldc1 $18, (-REGS_SIZE + REGS_FGR + 18*8)($31)
	ldc1 $19, (-REGS_SIZE + REGS_FGR + 19*8)($31)
	ldc1 $20, (-REGS_SIZE + REGS_FGR + 20*8)($31)
	ldc1 $21, (-REGS_SIZE + REGS_FGR + 21*8)($31)
	ldc1 $22, (-REGS_SIZE + REGS_FGR + 22*8)($31)
	ldc1 $23, (-REGS_SIZE + REGS_FGR + 23*8)($31)
	ldc1 $24, (-REGS_SIZE + REGS_FGR + 24*8)($31)
	ldc1 $25, (-REGS_SIZE + REGS_FGR + 25*8)($31)
	ldc1 $26, (-REGS_SIZE + REGS_FGR + 26*8)($31)
	ldc1 $27, (-REGS_SIZE + REGS_FGR + 27*8)($31)
	ldc1 $28, (-REGS_SIZE + REGS_FGR + 28*8)($31)
	ldc1 $29, (-REGS_SIZE + REGS_FGR + 29*8)($31)
	ldc1 $30, (-REGS_SIZE + REGS_FGR + 30*8)($31)
	ldc1 $31, (-REGS_SIZE + REGS_FGR + 31*8)($31)

	# CP0s
	ld $12, (-REGS_SIZE + REGS_SR)($31) # 32bit as 64bit
	ori $12, 2 # set EXL=1 for allow restoreing rest. (=keep kernel mode regardless of KSU) (eret clears EXL)
	mtc0 $12, $12 # status; two more cycles required for new SR effective... consumed by next ld/mtlo.
	ld $12, (-REGS_SIZE + REGS_LO)($31)
	mtlo $12
	ld $12, (-REGS_SIZE + REGS_HI)($31)
	mthi $12
	ld $12, (-REGS_SIZE + REGS_BAD)($31)
	dmtc0 $12, $8 # badvaddr
	ld $12, (-REGS_SIZE + REGS_CAUSE)($31) # 32bit as 64bit
	mtc0 $12, $13 # cause
	ld $12, (-REGS_SIZE + REGS_PC)($31)
	dmtc0 $12, $14 # epc (original pc)

	# GPRs
	#ld  $0, (-REGS_SIZE + REGS_GPR +  0*8)($31)
	ld  $1, (-REGS_SIZE + REGS_GPR +  1*8)($31)
	ld  $2, (-REGS_SIZE + REGS_GPR +  2*8)($31)
	ld  $3, (-REGS_SIZE + REGS_GPR +  3*8)($31)
	ld  $4, (-REGS_SIZE + REGS_GPR +  4*8)($31)
	ld  $5, (-REGS_SIZE + REGS_GPR +  5*8)($31)
	ld  $6, (-REGS_SIZE + REGS_GPR +  6*8)($31)
	ld  $7, (-REGS_SIZE + REGS_GPR +  7*8)($31)
	ld  $8, (-REGS_SIZE + REGS_GPR +  8*8)($31)
	ld  $9, (-REGS_SIZE + REGS_GPR +  9*8)($31)
	ld $10, (-REGS_SIZE + REGS_GPR + 10*8)($31)
	ld $11, (-REGS_SIZE + REGS_GPR + 11*8)($31)
	ld $12, (-REGS_SIZE + REGS_GPR + 12*8)($31)
	ld $13, (-REGS_SIZE + REGS_GPR + 13*8)($31)
	ld $14, (-REGS_SIZE + REGS_GPR + 14*8)($31)
	ld $15, (-REGS_SIZE + REGS_GPR + 15*8)($31)
	ld $16, (-REGS_SIZE + REGS_GPR + 16*8)($31)
	ld $17, (-REGS_SIZE + REGS_GPR + 17*8)($31)
	ld $18, (-REGS_SIZE + REGS_GPR + 18*8)($31)
	ld $19, (-REGS_SIZE + REGS_GPR + 19*8)($31)
	ld $20, (-REGS_SIZE + REGS_GPR + 20*8)($31)
	ld $21, (-REGS_SIZE + REGS_GPR + 21*8)($31)
	ld $22, (-REGS_SIZE + REGS_GPR + 22*8)($31)
	ld $23, (-REGS_SIZE + REGS_GPR + 23*8)($31)
	ld $24, (-REGS_SIZE + REGS_GPR + 24*8)($31)
	ld $25, (-REGS_SIZE + REGS_GPR + 25*8)($31)
	ld $26, (-REGS_SIZE + REGS_GPR + 26*8)($31)
	ld $27, (-REGS_SIZE + REGS_GPR + 27*8)($31)
	ld $28, (-REGS_SIZE + REGS_GPR + 28*8)($31)
	ld $29, (-REGS_SIZE + REGS_GPR + 29*8)($31)
	ld $30, (-REGS_SIZE + REGS_GPR + 30*8)($31)
	ld $31, (-REGS_SIZE + REGS_GPR + 31*8)($31) /* finally, restore $31 using itself :) */

	eret

	.set pop

	.type stub_recover, function
	.globl stub_recover
stub_recover:
	# skip current faulting insn
	MAFC0($k0, $14)
	ADDAIU($k0, $k0, 4)
	MATC0($k0, $14)

	# remember fault happened
	LA_SYM($k0, stub_recovered)
	li $k1, 1
	sw $k1, 0($k0)

	eret

	.type stub_tlbunmapall, function
	.globl stub_tlbunmapall
stub_tlbunmapall:
	# initialize TLB; as osUnmapTLBAll (not Linux like!)
	li $t0, 31 # R4300i have only 32 TLB entries, not 48.
	LA_P32($t1, 0x80000000)
	MATC0($t1, $10) # EntryHi; 80000000=never hit VA
	MATC0($zero, $2) # EntryLo0; Valid=0
	MATC0($zero, $3) # EntryLo1; Valid=0
	MATC0($zero, $5) # PageMask; 4KiB

1:
	mtc0 $t0, $0 # Index; 1cycle penalty?
	nop
	tlbwi # 2cycle penalty, 2cycle additional penalty for mfc0/mtc0?
	nop
	nop
	addiu $t0, -1
	bgez $t0, 1b

	jr $ra

	.type stub_installtlb, function
	.globl stub_installtlb
stub_installtlb:
	LA_P32($t1, 0xFFFFE000)
	MATC0($t1, $10) # EntryHi
	MATC0($zero, $2) # EntryLo0
	LA_P32($t1, gdbstubctx)
	and $t1, 0x1FFFffff # virt2phys
	srl $t1, 12 # byte2pfn
	sll $t1, 6 # PFN[16:0] -> EntryLo.PFN(29:6)
	ori $t1, 0x1F # CachableNoncoherent(011), Dirty, Valid, Global
	MATC0($t1, $3) # EntryLo1
	MATC0($zero, $5) # PageMask; 4KiB
	nop
	nop # only one nop is enough??
	tlbwr
	nop # is this 2-nops required? (no more mtc0/mfc0)
	nop

	jr $ra

#ifdef CONFIG_GDBSTUB_CTX_GP
	.sbss
	.balign 8
	.type gdbstubctxk0, @object
gdbstubctxk0:
	.space 8
#endif

	.bss
	.balign GDBSTUBCTX_BALIGN
	.globl gdbstubctx
	.type gdbstubctx, @object
gdbstubctx:
	.space GDBSTUBCTX_SIZE
