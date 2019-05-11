A GDBstub for Nintendo 64 + EverDrive-64.

# Build (with sample)

Note: Although gdbstub does not depends any libraries (maybe), sample program requires [libdragon](https://github.com/DragonMinded/libdragon)

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

(tested with `ruby 2.4.1p111 (2017-03-22 revision 58053) [x86_64-linux]`)

On another terminal:

```
$ gdb -q sample.elf
(gdb) target remote localhost:23946
```

You'll see `__asm("syscall")` that invokes gdbstub.

Continue with stub by skipping `syscall` and `jal stub_uninstall`:

```
(gdb) set $pc=$pc+8
```

Then break&continue, step, ...

## Nintendo 64 hangs before getting stub?

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

Stub refuses long (>512B) packet commands, ex. `G` (set registers) issued by `set $pc=$pc+8` ?

It may be your FTDI USB driver is too slow to send bytes.
(fifo2dram thinks "no more data" after reads first 512 bytes because so slow.)

If you are using Linux, tweak ftdi_sio driver's bulk_(in|out)_size larger (ex. 4096).

## Stub breaks at non-problem instruction

See `cause` and last 8 bit is 0x00? That's interrupt!

`set $sr=$sr&~1` (disable interrupt) will help you... until program enables interrupt again.

## Can't stop with Ctrl-C

Unsupported! Reset and redo from start...

# Integrate stub with your (homebrew) program

Just implant the following code where you want to break:

```
extern void stub(void); stub();
```

And link with `gdbstub.o` and `gdbstubl.o`.

Tested with O64 and O32 ABI. Other ABIs are untested...

It will initialize TLB and install starting stub code to vectors (80000000, 80000080, 80000100(meaningless :-), 80000180) then invoke stub with a dummy `syscall`.

# TODO

* accept longer packet? (com64.rb/gdbstub.c)
* TLB co-operate support (gdbstubl.S)
  * currently it assuming that stub is a ONLY user of TLB... or TLB-shutdown will happen in worst case
  * should tlbp before tlbwr, and use tlbwi if found
* support stepping eret (gdbstub.c)
* support skipping interrupt exception (or filtering some exception)
  * how to run the (first 2 instructions of) original handler?
* stepping into exception handler
  * currently you need to set pc to the handler...
	* EPC can't be restored... how to??

# License

MIT
