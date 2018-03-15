#include "3264.h"
#include "gdbstub.h"

	.text
	.set noat

	.type stub_switch, function
	.globl stub_switch
stub_switch:
	# GPRs
	sd  $0, (REGS_START + REGS_GPR +  0*8)($zero)
	sd  $1, (REGS_START + REGS_GPR +  1*8)($zero)
	sd  $2, (REGS_START + REGS_GPR +  2*8)($zero)
	sd  $3, (REGS_START + REGS_GPR +  3*8)($zero)
	sd  $4, (REGS_START + REGS_GPR +  4*8)($zero)
	sd  $5, (REGS_START + REGS_GPR +  5*8)($zero)
	sd  $6, (REGS_START + REGS_GPR +  6*8)($zero)
	sd  $7, (REGS_START + REGS_GPR +  7*8)($zero)
	sd  $8, (REGS_START + REGS_GPR +  8*8)($zero)
	sd  $9, (REGS_START + REGS_GPR +  9*8)($zero)
	sd $10, (REGS_START + REGS_GPR + 10*8)($zero)
	sd $11, (REGS_START + REGS_GPR + 11*8)($zero)
	sd $12, (REGS_START + REGS_GPR + 12*8)($zero)
	sd $13, (REGS_START + REGS_GPR + 13*8)($zero)
	sd $14, (REGS_START + REGS_GPR + 14*8)($zero)
	sd $15, (REGS_START + REGS_GPR + 15*8)($zero)
	sd $16, (REGS_START + REGS_GPR + 16*8)($zero)
	sd $17, (REGS_START + REGS_GPR + 17*8)($zero)
	sd $18, (REGS_START + REGS_GPR + 18*8)($zero)
	sd $19, (REGS_START + REGS_GPR + 19*8)($zero)
	sd $20, (REGS_START + REGS_GPR + 20*8)($zero)
	sd $21, (REGS_START + REGS_GPR + 21*8)($zero)
	sd $22, (REGS_START + REGS_GPR + 22*8)($zero)
	sd $23, (REGS_START + REGS_GPR + 23*8)($zero)
	sd $24, (REGS_START + REGS_GPR + 24*8)($zero)
	sd $25, (REGS_START + REGS_GPR + 25*8)($zero)
	sd $26, (REGS_START + REGS_GPR + 26*8)($zero)
	sd $27, (REGS_START + REGS_GPR + 27*8)($zero)
	sd $28, (REGS_START + REGS_GPR + 28*8)($zero)
	sd $29, (REGS_START + REGS_GPR + 29*8)($zero)
	sd $30, (REGS_START + REGS_GPR + 30*8)($zero)
	sd $31, (REGS_START + REGS_GPR + 31*8)($zero)

	# set sp to safe value
	# note: you may modify "803FF000" to relocate stub reg buffer...
	LA_P32($29, 0x803FFFF0 + REGS_START)

	# CP0s
	mfc0 $12, $12 # status
	sd $12, (REGS_START + REGS_SR)($zero) # 32bit as 64bit
	# set SR known state; SR is saved, modify ok.
	# enable SR.cop1 to allow saving FGRs, and FR=1 to ensure 64bit*32 fgrs are usable.
	# 64bit enabled(just because). set KSU/EXL/ERL to 0. Interrupt is also disabled.
	li $12, 0x241000E0
	mtc0 $12, $12
	# changing SR requires two more cycles to be effective (or *sometimes* CpU exception at next sdc1!!)
	# ...that is consumed by next mflo/sd.
	# rest of CP0s
	mflo $12
	sd $12, (REGS_START + REGS_LO)($zero)
	mfhi $12
	sd $12, (REGS_START + REGS_HI)($zero)
	dmfc0 $12, $8 # badvaddr
	sd $12, (REGS_START + REGS_BAD)($zero)
	mfc0 $12, $13 # cause
	sd $12, (REGS_START + REGS_CAUSE)($zero) # 32bit as 64bit
	dmfc0 $12, $14 # epc (original pc)
	sd $12, (REGS_START + REGS_PC)($zero)

	# CP1 FGRs
	sdc1  $0, (REGS_START + REGS_FGR +  0*8)($zero)
	sdc1  $1, (REGS_START + REGS_FGR +  1*8)($zero)
	sdc1  $2, (REGS_START + REGS_FGR +  2*8)($zero)
	sdc1  $3, (REGS_START + REGS_FGR +  3*8)($zero)
	sdc1  $4, (REGS_START + REGS_FGR +  4*8)($zero)
	sdc1  $5, (REGS_START + REGS_FGR +  5*8)($zero)
	sdc1  $6, (REGS_START + REGS_FGR +  6*8)($zero)
	sdc1  $7, (REGS_START + REGS_FGR +  7*8)($zero)
	sdc1  $8, (REGS_START + REGS_FGR +  8*8)($zero)
	sdc1  $9, (REGS_START + REGS_FGR +  9*8)($zero)
	sdc1 $10, (REGS_START + REGS_FGR + 10*8)($zero)
	sdc1 $11, (REGS_START + REGS_FGR + 11*8)($zero)
	sdc1 $12, (REGS_START + REGS_FGR + 12*8)($zero)
	sdc1 $13, (REGS_START + REGS_FGR + 13*8)($zero)
	sdc1 $14, (REGS_START + REGS_FGR + 14*8)($zero)
	sdc1 $15, (REGS_START + REGS_FGR + 15*8)($zero)
	sdc1 $16, (REGS_START + REGS_FGR + 16*8)($zero)
	sdc1 $17, (REGS_START + REGS_FGR + 17*8)($zero)
	sdc1 $18, (REGS_START + REGS_FGR + 18*8)($zero)
	sdc1 $19, (REGS_START + REGS_FGR + 19*8)($zero)
	sdc1 $20, (REGS_START + REGS_FGR + 20*8)($zero)
	sdc1 $21, (REGS_START + REGS_FGR + 21*8)($zero)
	sdc1 $22, (REGS_START + REGS_FGR + 22*8)($zero)
	sdc1 $23, (REGS_START + REGS_FGR + 23*8)($zero)
	sdc1 $24, (REGS_START + REGS_FGR + 24*8)($zero)
	sdc1 $25, (REGS_START + REGS_FGR + 25*8)($zero)
	sdc1 $26, (REGS_START + REGS_FGR + 26*8)($zero)
	sdc1 $27, (REGS_START + REGS_FGR + 27*8)($zero)
	sdc1 $28, (REGS_START + REGS_FGR + 28*8)($zero)
	sdc1 $29, (REGS_START + REGS_FGR + 29*8)($zero)
	sdc1 $30, (REGS_START + REGS_FGR + 30*8)($zero)
	sdc1 $31, (REGS_START + REGS_FGR + 31*8)($zero)

	# CP1 controls
	cfc1 $12, $31 # fpcs
	sd $12, (REGS_START + REGS_FPCS)($zero) # 32bit as 64bit
	cfc1 $12, $0 # fpir
	sd $12, (REGS_START + REGS_FPIR)($zero) # 32bit as 64bit

	jal stub_entry

	# CP1 controls
	ld $12, (REGS_START + REGS_FPCS)($zero) # 32bit as 64bit
	ctc1 $12, $31 # fpcs
	ld $12, (REGS_START + REGS_FPIR)($zero) # 32bit as 64bit
	ctc1 $12, $0 # fpir... writable??

	# CP1 FGRs
	ldc1  $0, (REGS_START + REGS_FGR +  0*8)($zero)
	ldc1  $1, (REGS_START + REGS_FGR +  1*8)($zero)
	ldc1  $2, (REGS_START + REGS_FGR +  2*8)($zero)
	ldc1  $3, (REGS_START + REGS_FGR +  3*8)($zero)
	ldc1  $4, (REGS_START + REGS_FGR +  4*8)($zero)
	ldc1  $5, (REGS_START + REGS_FGR +  5*8)($zero)
	ldc1  $6, (REGS_START + REGS_FGR +  6*8)($zero)
	ldc1  $7, (REGS_START + REGS_FGR +  7*8)($zero)
	ldc1  $8, (REGS_START + REGS_FGR +  8*8)($zero)
	ldc1  $9, (REGS_START + REGS_FGR +  9*8)($zero)
	ldc1 $10, (REGS_START + REGS_FGR + 10*8)($zero)
	ldc1 $11, (REGS_START + REGS_FGR + 11*8)($zero)
	ldc1 $12, (REGS_START + REGS_FGR + 12*8)($zero)
	ldc1 $13, (REGS_START + REGS_FGR + 13*8)($zero)
	ldc1 $14, (REGS_START + REGS_FGR + 14*8)($zero)
	ldc1 $15, (REGS_START + REGS_FGR + 15*8)($zero)
	ldc1 $16, (REGS_START + REGS_FGR + 16*8)($zero)
	ldc1 $17, (REGS_START + REGS_FGR + 17*8)($zero)
	ldc1 $18, (REGS_START + REGS_FGR + 18*8)($zero)
	ldc1 $19, (REGS_START + REGS_FGR + 19*8)($zero)
	ldc1 $20, (REGS_START + REGS_FGR + 20*8)($zero)
	ldc1 $21, (REGS_START + REGS_FGR + 21*8)($zero)
	ldc1 $22, (REGS_START + REGS_FGR + 22*8)($zero)
	ldc1 $23, (REGS_START + REGS_FGR + 23*8)($zero)
	ldc1 $24, (REGS_START + REGS_FGR + 24*8)($zero)
	ldc1 $25, (REGS_START + REGS_FGR + 25*8)($zero)
	ldc1 $26, (REGS_START + REGS_FGR + 26*8)($zero)
	ldc1 $27, (REGS_START + REGS_FGR + 27*8)($zero)
	ldc1 $28, (REGS_START + REGS_FGR + 28*8)($zero)
	ldc1 $29, (REGS_START + REGS_FGR + 29*8)($zero)
	ldc1 $30, (REGS_START + REGS_FGR + 30*8)($zero)
	ldc1 $31, (REGS_START + REGS_FGR + 31*8)($zero)

	# CP0s
	ld $12, (REGS_START + REGS_SR)($zero) # 32bit as 64bit
	ori $12, 2 # set EXL=1 for allow restoreing rest. (=keep kernel mode regardless of KSU) (eret clears EXL)
	mtc0 $12, $12 # status; two more cycles required for new SR effective... consumed by next ld/mtlo.
	ld $12, (REGS_START + REGS_LO)($zero)
	mtlo $12
	ld $12, (REGS_START + REGS_HI)($zero)
	mthi $12
	ld $12, (REGS_START + REGS_BAD)($zero)
	dmtc0 $12, $8 # badvaddr
	ld $12, (REGS_START + REGS_CAUSE)($zero) # 32bit as 64bit
	mtc0 $12, $13 # cause
	ld $12, (REGS_START + REGS_PC)($zero)
	dmtc0 $12, $14 # epc (original pc)

	# GPRs
	#ld  $0, (REGS_START + REGS_GPR +  0*8)($zero)
	ld  $1, (REGS_START + REGS_GPR +  1*8)($zero)
	ld  $2, (REGS_START + REGS_GPR +  2*8)($zero)
	ld  $3, (REGS_START + REGS_GPR +  3*8)($zero)
	ld  $4, (REGS_START + REGS_GPR +  4*8)($zero)
	ld  $5, (REGS_START + REGS_GPR +  5*8)($zero)
	ld  $6, (REGS_START + REGS_GPR +  6*8)($zero)
	ld  $7, (REGS_START + REGS_GPR +  7*8)($zero)
	ld  $8, (REGS_START + REGS_GPR +  8*8)($zero)
	ld  $9, (REGS_START + REGS_GPR +  9*8)($zero)
	ld $10, (REGS_START + REGS_GPR + 10*8)($zero)
	ld $11, (REGS_START + REGS_GPR + 11*8)($zero)
	ld $12, (REGS_START + REGS_GPR + 12*8)($zero)
	ld $13, (REGS_START + REGS_GPR + 13*8)($zero)
	ld $14, (REGS_START + REGS_GPR + 14*8)($zero)
	ld $15, (REGS_START + REGS_GPR + 15*8)($zero)
	ld $16, (REGS_START + REGS_GPR + 16*8)($zero)
	ld $17, (REGS_START + REGS_GPR + 17*8)($zero)
	ld $18, (REGS_START + REGS_GPR + 18*8)($zero)
	ld $19, (REGS_START + REGS_GPR + 19*8)($zero)
	ld $20, (REGS_START + REGS_GPR + 20*8)($zero)
	ld $21, (REGS_START + REGS_GPR + 21*8)($zero)
	ld $22, (REGS_START + REGS_GPR + 22*8)($zero)
	ld $23, (REGS_START + REGS_GPR + 23*8)($zero)
	ld $24, (REGS_START + REGS_GPR + 24*8)($zero)
	ld $25, (REGS_START + REGS_GPR + 25*8)($zero)
	ld $26, (REGS_START + REGS_GPR + 26*8)($zero)
	ld $27, (REGS_START + REGS_GPR + 27*8)($zero)
	ld $28, (REGS_START + REGS_GPR + 28*8)($zero)
	ld $29, (REGS_START + REGS_GPR + 29*8)($zero)
	ld $30, (REGS_START + REGS_GPR + 30*8)($zero)
	ld $31, (REGS_START + REGS_GPR + 31*8)($zero)

	eret

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
	li $t0, 31 # TODO not 47?
	LA_P32($t1, 0x80000000)
	MATC0($t1, $10) # EntryHi
	MATC0($zero, $2) # EntryLo0
	MATC0($zero, $3) # EntryLo1
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
	# note: you may modify "003FF000" to relocate stub reg buffer...
	LI_U32($t1, (((0x003FF000 >> 12) << 6) | 0x1F)) # CachableNoncoherent(011), Dirty, Valid, Global
	MATC0($t1, $3) # EntryLo1
	MATC0($zero, $5) # PageMask; 4KiB
	nop
	nop # only one nop is enough??
	tlbwr
	nop # is this 2-nops required? (no more mtc0/mfc0)
	nop

	jr $ra
