
libsmp.dll:

CFLAGS=-std=c99 -Wall -DSMP9X -I. -I..

%.gcc.o: %.c $(GCC_DEPS)
	gcc $(CFLAGS) -c -o $@ $<

FNLIST=list.txt

OBJS=../smp9x.gcc.o libsmp.gcc.o tryentercriticalsection.gcc.o

libsmp.dll: Makefile $(OBJS) generator.c $(FNLIST)
	gcc -std=c99 -Wall generator.c -o generator
	generator $(FNLIST) hooks.asm libsmp.def
	echo CreateThread = smp9x_CreateThread >> libsmp.def
	echo ExitThread = smp9x_ExitThread >> libsmp.def
	echo GetSystemInfo = smp9x_GetSystemInfo >> libsmp.def
	echo GetCurrentProcessorNumber = smp9x_GetCurrentProcessorNumber >> libsmp.def
#	echo __mb_cur_max = __mb_cur_max_smp >> libsmp.def
#	echo _iob = msvcrt._iob DATA >> libsmp.def
#	echo __mb_cur_max = msvcrt.__mb_cur_max DATA >> libsmp.def
#	echo __initenv = msvcrt.__initenv DATA >> libsmp.def
#	echo _iob = _iob_smp >> libsmp.def
	echo TryEnterCriticalSection = TryEnterCriticalSection9x >> libsmp.def
	echo _beginthread = smp9x__beginthread >> libsmp.def
	echo _beginthreadex = smp9x__beginthreadex >> libsmp.def
	echo _endthread = smp9x__endthread >> libsmp.def
	echo _endthreadex = smp9x__endthreadex >> libsmp.def
	echo GetThreadId = smp9x_GetThreadId >> libsmp.def
#	echo InitOnceExecuteOnce = smp9x_InitOnceExecuteOnce >> libsmp.def
#	echo InitializeConditionVariable = smp9x_InitializeConditionVariable >> libsmp.def
#	echo WakeAllConditionVariable = smp9x_WakeAllConditionVariable >> libsmp.def
#	echo SleepConditionVariableCS = smp9x_SleepConditionVariableCS >> libsmp.def
#	echo WakeConditionVariable = smp9x_WakeConditionVariable >> libsmp.def
#	echo SwitchToThread = smp9x_SwitchToThread >> libsmp.def
	fasm hooks.asm hooks.o
	gcc $(CFLAGS) hooks.o $(OBJS) -shared -o $@ -Wl,--subsystem,windows -Wl,--kill-at -Wl,--enable-stdcall-fixup -lkernel32 -luser32 -lmsvcrt libsmp.def
