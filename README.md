A GDBstub for Nintendo 64 + EverDrive-64.

# Build (with sample)

Note: Although gdbstub does not depends any libraries, sample program requires [libdragon](https://github.com/DragonMinded/libdragon)

```
$ export N64_INST=/path/to/toolchain
$ make
```

# Testrun

Turn on Nintendo 64 with EverDrive-64, and connect USB cable with it.

On some terminal:

```
$ ruby ./loader64.rb /dev/ttyUSB0 sample.z64 && ruby ./com64.rb /dev/ttyUSB0 23946
```

(tested with `ruby 2.6.3p62 (2019-04-16 revision 67580) [x86_64-linux]`)

Nintendo 64 should not any TV output, but it is ok. (stopping at gdbstub.)

Then, on another terminal:

```
$ gdb -q sample.elf
(gdb) target remote localhost:23946
```

And you'll see end of `stub_test()` (-O0), or just calling `init_interrupts()` that is following `stub_test()` call (-O2).

You can now place breakpoints, continue, step, ...

## Nintendo 64 hangs before getting stub? (or does not respond to GDB?)

Maybe libdragon's link script is broken...

Disassemble or `nm` your ELF file and check `_start` is at 0x80000400 (default libdragon entry point address).

If not, some non-code section are on the entry point. It crash ofcourse!

Modify your `$N64_INST/mips64-elf/lib/n64ld.x` to fix it.

On my some environment, `.MIPS.abiflags` and `.eh_frame` does. I modified as...

```
/* snip */
         *(.sdata)
         *(.sdata.*)
				 *(.eh_frame)   /* add this in .data */
      __data_end = . ;

/* snip */
	__bss_end = . ;
	end = . ;
   }

	/DISCARD/ : {
		*(.MIPS.abiflags)  /* simply discard this section */
	}
/* snip */
```

Also, check `__data_end`, `__bss_start` and `__bss_end` is aligned at 4-bytes boundary.

I hope this helps you.

## Stub refuses some commands

Stub refuses long (>512B) packet commands, ex. `G` (set registers) issued by `set $sr=$sr&~1` ?

It may be your FTDI USB driver is too slow to send bytes.
(fifo2dram thinks "no more data" after reads first 512 bytes because so slow.)

If you are using Linux, tweak ftdi_sio driver's bulk_(in|out)_size larger (ex. 4096).

## Stub breaks at non-problem instruction

See `cause` and last 8 bit is 0x00? That's interrupt!

`set $sr=$sr&~1` (disable interrupt) will help you... until program enables interrupt again.

If you don't want this behavior (to skip interrupt exception), see "Skip interrupts" section.

## Can't stop with Ctrl-C

Unsupported! Reset and redo from start...

Or, instead of Ctrl-C on host, you can implant the stub call at some debug-button handler in your program. See following section.

# Integrate stub with your (homebrew) program

You must keep following "Restrictions" section, then just implant the following code where you want to break:

```
extern void stub_install(void); stub_install(); /* if you want to invoke stub when MIPS exceptions happens */

extern void stub(void); stub(); /* if you want to drop into stub immediately */
```

And link with `gdbstub.o` and `gdbstubl.o`.

Tested with O64 and O32 ABI. Other ABIs are untested...

## Restrictions

- You can't put your exception handler into 80000000/80/100/180 directly.
	- Because this gdbstub also uses that (heavily).
	- You must place your exception handler out of there, and place only `j your_real_handler` there.
- You must obey one of following: (can be tweaked in `gdbstub.h`)
	- Don't use 0xFFFFE000-0xFFFFFFFF memory area (`CONFIG_GDBSTUB_CTX_ZERO`, default)
	- Set `$gp` correctly and do not change any time (`CONFIG_GDBSTUB_CTX_GP`)
	- `$k0` is always crobbered (`CONFIG_GDBSTUB_CTX_K0`)

## Skip interrupts

There are 2 options to skip interrupts automatically:

- Let this gdbstub calls interrupt handler automatically (easy)
- Write "enter gdbstub only if not interrupt" code yourself (runs faster)

### gdbstub-leads style (easy)

First, ensure interrupt handler is isolated from exception handler.

Then, un-comment and rewrite following line in `gdbstub.h`:

```
#define CONFIG_GDBSTUB_INTERRUPT_HANDLER inthandler /* jump to this handler if exception is interrupt. (sample value is for libdragon) */
```

In above example, gdbstub will call `inthandler` when interrupt happens.

### Your-code-leads style (faster)

You must obey following two things:

- Inject code that do "jump to `stub` if not interrupt" into your exception handler
- Do *NOT* use `stub_install` (that will overwrites your exception handler...)

Inject code will be like following:

```
	# invoke stub if ExcCode!=0 (not interrupt)
	mfc0 $k0, $13 # $cause
	andi $k0, $k0, 0xFF
	beqz $k0, 1f
	j stub
1:
	# ...your interrupt handler follows...
```

This must be placed on top of your project's exception handler (ex. `inthandler` for libdragon).

Above code crobbers `$k0` register, so you can't see real `$k0` value in gdbstub.

## Debugging your exception handler

This is required **only if you want debug your exception handler** (not for ordinal codes)

There are some more restrictions:

- You can't debug 80000000/80/100/180 handler
	- because this gdbstub also uses that (heavily)
	- you must place your exception handler out of there, and place only `j your_real_handler` there.
- You can't debug while EXL=1
	- because MIPS does not save EPC while EXL=1
	- turn off EXL while your exception handler runs
	- you may also need to set KSU=0 (and IE=0 if processing interrupt(s))
- You can't debug out of EPC-saved area
	- because MIPS overwrites it when exception happens (breakpoint, TLB refill, ...)

So, your exception handler code will be like:

```
	# 0x80000180
handler:
	j real_handler

	# somewhere
real_handler:
	# (maybe "Skip interrupts" code here if you want)

	# be kernel mode with EXL=0 and disallowing interrupt
	mfc0 $k0, $12 # $status
	li $k1, ~0x1B # KSU=0 EXL=0 IE=0
	and $k0, $k1
	mtc0 $k0, $12 # $status

	# save $epc
	.set push
	.set noat
	mfc0 $k0, $14 # $epc
	la $k1, save_epc
	sw $k0, 0($k1)
	.set pop

	#...your handler here...
	#...you can place breakpoints only here...

	# restore $epc
	.set push
	.set noat
	la $k1, save_epc
	lw $k0, 0($k1)
	mtc0 $k0, $14 # $epc
	.set pop

	eret
```

Above code crobbers `$k0` and `$k1`, so you can't see real `$k0` and `$k1` value in gdbstub.

# TODO

* accept longer packet? (com64.rb/gdbstub.c)
* TLB co-operate support (gdbstubl.S)
	* currently it assuming that stub is a ONLY user of TLB... or TLB-shutdown will happen in worst case
	* should tlbp before tlbwr, and use tlbwi if found
* support stepping eret (gdbstub.c)
* stepping into exception handler naturally
	* how to run the (first 2 instructions of) original handler?
	* currently you need to set pc to the handler...
	* EPC can't be restored... how to??

# License

MIT
