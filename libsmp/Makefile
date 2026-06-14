
libsmp.dll:

CFLAGS=-std=c99 -O2 -Wall -DSMP9X -I. -I..

%.gcc.o: %.c $(GCC_DEPS)
	gcc $(CFLAGS) -c -o $@ $<

%.fasm.o: %.asm $(DEPS)
	fasm $< $@

CAT=cat

OBJS=../smp9x.gcc.o libsmp.gcc.o tryentercriticalsection.gcc.o

libsmp.txt: kernel32.txt msvcrt.txt
	$(CAT) kernel32.txt > $@
	$(CAT) msvcrt.txt >> $@

libsmp.dll: Makefile $(OBJS) generator.c libsmp.txt
	gcc -std=c99 -Wall generator.c -o generator
	generator libsmp.txt hooks.asm libsmp.def
	$(CAT) smp9x.def >> libsmp.def
	fasm hooks.asm hooks.o
	gcc $(CFLAGS) hooks.o $(OBJS) -shared -o $@ -Wl,--subsystem,windows -Wl,--kill-at -Wl,--enable-stdcall-fixup -lkernel32 -luser32 -lmsvcrt libsmp.def

clean:
	-$(RM) $(OBJS)
	-$(RM) libsmp.dll
