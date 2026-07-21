# SMP.vxd

Symmetric multiprocessor driver for Windows 9x (95/98/Me). This driver allow to utilize multi-core CPU under these ancient operation systems. Also utilize AVX and SSE instruction set.

## UNDER DEVELOPMENT

This is concept, expect large stability and performance issues.

Currently tested devices:
- VirtualBox (7.2.2 - 7.2.12)
- VMware Workstation (7.5.2)
- Gigabyte GA-G41M-ES2H + Intel Xeon X5460

## Important notice

This isn't magic trick how increase performance on legacy games but allow you to write new application or modify open-source older things to use multi-thread/multi-core/multi-cpu performance.

## Features

<table>
<thead>
<tr>
<td rowspan="2">Feature</td>
<td colspan="2">native</td>
<td colspan="2">smp.vxd loaded</td>
</tr>
<tr>
<td>Windows 95</td>
<td>Windows 98/Me</td>
<td>kernel32.dll (95, 98, Me)</td>
<td>libsmp.dll (95, 98, Me)</td>
</tr>
</thead>
<tbody>
<tr>
<td>Max. CPUs</td>
<td>1</td>
<td>1</td>
<td>1</td>
<td><strong>256</strong></td>
<tr>
<td>x87/MMX</td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
</tr>
<tr>
<td>SSE</td>
<td>NO</td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
</tr>
<tr>
<td>AVX</td>
<td>NO</td>
<td>NO</td>
<td><strong>YES</strong></td>
<td><strong>YES</strong></td>
</tr>
<tr>
<td>AVX512</td>
<td>NO</td>
<td>NO</td>
<td>NO</td>
<td>NO</td>
</tr>
</tbody>
</table>

*Note to SSE/AVX: SMP.vxd implement thread context switch (though FXSAVE/XSAVE instructions), in contrast with older methods (like my [SIMD95](https://github.com/JHRobotics/simd95)) is save to using these instructions by multiple applications or in one application in multiple threads.*

*Note to AVX512: support could be relatively easy added, but currently I don't have got any system for testing/debugging a no application that can utilize it.*

## Usage

### Installation

Copy `smp.vxd` to `C:\WINDOWS\system` (not `system32`!). Edit `C:\WINDOWS\system.ini` and under `[386enh]` add following line:

```
device=smp.vxd
```

And reboot computer. After boot you cant run `smpload.exe` to check if driver is loaded and number of CPUs.

### Configuration

Driver can be configured though `[smp]` section in `system.ini` (you have to create it).

```
[smp]

; disable print information on startup
quiet=1

; disable AVX support (when CPU support it)
noavx=1

; disable SSE support on Windows 95 (Windows 98+ supports it natively)
nosse=1

; limit number of CPUs, when set to 1, disable SMP feature
; useful when you want use AVX but not SMP
maxcpus=2

; wait to enter after module loads
pause=1
```

### Uninstallation

Edit C:\WINDOWS\system.ini and under under `[386enh]` remove line:
```
device=smp.vxd
```
(or you can comment line, if you wish only temporary disable)
```
rem device=smp.vxd
```


## Compilation

You need:
1) GNU make
2) fasm
3) Watcom (1.9 or better)
4) mingw (careful for build with wired SSE instructions)


```
make
```

This should product:
- smp.vxd: driver itself
- libsmp.dll: intermediate library
- mesasmp.dll: intermediate library for Mesa9x
- tests/*.exe: some functionality test

## Programmers guide

### Basics

With SMP (Symmetric Multiprocessing) there two kinds of CPUs - 1x **BSP** (bootstrap processor) and more **AP** (application processor). System see only BSP and all system services must be done on BSP. Simplest way is use high level API in smp9x.c (include this file to your project or though libsmp.dll).


```
#include <windows.h>
#include <stdlib.h>
#include "smp9x.h"

DWORD WINAPI thread_main(LPVOID lpParameter)
{
  MessageBoxA(NULL, "Hello from thread!", "Thread!", MB_OK);
  return 0;
}

int main(int argc, char **argv)
{
  smp9x_init();

  DWORD id = 0;
	HANDLE th = smp9x_CreateThread(NULL, 0, thread_main, NULL, 0, &id);
	
	MessageBoxA(NULL, "Hello from main!", "Main!", MB_OK);
	WaitForSingleObject(th, INFINITE);
	
	return EXIT_SUCCESS;
}
```

Only needed is call `smp9x_init()` and replace `CreateThread` => `smp9x_CreateThread`. If you with use CRT like function, there are `smp9x__beginthread`/`smp9x__beginthreadex`.

Sometimes may be quite difficult to replace all calls in source, so another way is replace calls by replacing EXE (or DLL) imports. So build binary like usual with MS original functions:

```
#include <windows.h>
#include <stdlib.h>

DWORD WINAPI thread_main(LPVOID lpParameter)
{
  MessageBoxA(NULL, "Hello from thread!", "Thread!", MB_OK);
  return 0;
}

int main(int argc, char **argv)
{
  DWORD id = 0;
	HANDLE th = CreateThread(NULL, 0, thread_main, NULL, 0, &id);
	
	MessageBoxA(NULL, "Hello from main!", "Main!", MB_OK);
	WaitForSingleObject(th, INFINITE);
	
	return EXIT_SUCCESS;
}
```

(MinGW for example)

```
gcc -std=c99 -O2 -Wall example.c -o example.exe
```

And use [fixlink](https://github.com/JHRobotics/fixlink) to replace import libraries:

```
fixlink -relink test.exe libsmp.dll kernel32.dll msvcrt.dll
```

The `libsmp.dll` export most kernel32.dll and msvcrt.dll functions and redirect to original variants or reimplementing them in some cases, also add some function which not present in original like `GetThreadId` or `TryEnterCriticalSection`. 

### Advanced

Internally your program needs do these stages:
1) define dummy function which is periodically hit by Windows scheduler and use as placement to thread on AP.
2) connect do VXD driver
3) create Windows thread and define thread specific variable as synchronization mechanism between dummy function and your thread on AP.
4) call VDX to connect thread, dummy function and lock
5) call `fly()` function which move your thread to first free AP
6) do the computing
7) call `land()` function which move your thread back to AP

As I preciously say, you cannot call system function when you running on AP, so you can call `land()`, call system function and call `fly()` again. But this is relatively complicated marks all these calls. But there is another mechanism - is AP catch exception, the thread will be moved back to BSP. When this is real exception, system may solve it (like swapped memory page) or report error to user. But when you code on AP call system function, AP catch exception, return your code back to BSP and there code can to needed operation, so `land()` is redundant. But you still need call `fly()` to moved the thread back to AP.

For this, driver has special feature to watch thread switch and inactive marked thread move to AP, unfortunately this isn't very effective (this this is much hack to original scheduler not real multi-cpu scheduling). Let's show thread main looks like.

```
DWORD WINAPI winthread(LPVOID lpParameter)
{
	volatile DWORD lock;
	DWORD rc = 0;
	
	smp9x_thread_elevate(&lock, SMP_MODE_AUTORUN);
	smp9x_thread_fly(); /* not needed but faster kickoff */
	
	/* do whatever you want */
	
	smp9x_thread_land(); /* require before return from thread main */
	return rc;
}
```

Second `smp9x_thread_elevate`parameter control thread planning. Lets compare to "manual" mode:

```
DWORD WINAPI winthread(LPVOID lpParameter)
{
	volatile DWORD lock;
	DWORD rc = 0;
	
	smp9x_thread_elevate(&lock, SMP_MODE_MANUAL);
	
	
	smp9x_thread_fly();
	do_computing_here();
	smp9x_thread_land();
	
	/* synchronization/report results etc. */
	
	return rc;
}
```

If `do_computing_here` call system function, it'll return thread to BSP but never move back to AP. Also `fly()` and `land()` does nothing if runs on wrong CPU, so you can combine automatic mode and fly() to make sure, that thread will be run on AP from now.

Some informations to `lock`, this variable, this variable must be thread specific (best option is on thread stack):

```
volatile DWORD lock; // Wrong - shared by all

DWORD WINAPI winthread(LPVOID lpParameter)
{
	volatile DWORD lock; // OK
	
	static volatile DWORD; // Wrong - shared by all
	
	lpvData = TlsGetValue(dwTlsIndex); 
	((thread_data_t*)lpvData)->lock; // OK
}

int runthread()
{
	volatile DWORD lock; // Wrong, dangerous - overwriting main thread stack
}

int main()
{
	volatile DWORD lock; // Wrong - shared by all
}

```

## Hackers guide

There are things you should known at beginning, good things first: Windows 9x known nothing about SMP and leave all AP untouched, Windows 9x are based on microcore (vmm.vxd) so is possible extend system functionality more than (semi-, hybrid) monolithic kernel - like NT or Unix (BDS and Linux included). Bad things: there is only one page directory (PD) - by Intel recommendation every process should have own PD (physical address to PD is on CR3 register) - but on Windows 9x is only one PD and for every process switch needs to rebuilt it (that why there so much issue with newer CPUs and virtual machines).

How basically SMP works you can learn from [OSDev WIKI](https://wiki.osdev.org/Symmetric_Multiprocessing). When writing OS from sketch is adding SMP support very easy (much then NUMA) but implement it to close source more than 30 years old OS is a bit tough. Where is basic concept:

1) On **BSP** runs Windows (vmm.vxd) as usual.
2) On **AP** runs special OS called APC/S (Application Processor Controller/Supervisor)
	-	boot sequence is in [apboot.asm](blob/master/apboot.asm)
	-	kernel itself is in [apkernel.asm](blob/master/apkernel.asm)
3) When **AP** in not used, is goes to sleep state (ASM: sti; hlt). This is quite important for VM or laptops because when AP burns NOP in loop its affect BSP performance to.

APC/S is very simple, after setup the AP, there is FSM (finite state machine) which circles process states:
- S_READY: AP is free to do some work (go to S_SLEEP when no WORK)
- S_SLEEP: AP is free to do some work but needs wake-up first
- S_LOADED: process was moved from BSP and ready to run
- S_RUNNING: process running
- S_CARGO: process is done and needs to be moved to BSP
- S_DIRCARD: process is done but result will be drop (process was terminated for example).

Communication between AP and Windows/BSP is done by using int3 (0xCC opcode). To determine what is control sequence and debug code we're checking address when INT occurs.

Kernel code (kernel page) is shared between APs and BSP and code from it can be called by AP and BSP (see `is_bsp()` on `kernel.asm` how determine where we are). But every AP need own private mem:
- private stack 32k (8 pages):
  - 16k: kernel
  - 8k: place holder (low priority procedure for Windows scheduler, when context is running on AP)
  - 8k: TSS - task segment switch, when switching between RING 0 (process loading/unloading here) and RING 3 (running context).

- data pages 8k (2 pages):
  - process state (CPU registers + FPU/XMM/AVX state)
  - PD (page dictionary): when process switch, we need copy it, not only assign other CR3 (there is only one PD on Windows 9x and rebuilds on every process switch, linear address is 0xFFBFE000, when you want to look).

### BSP to AP

To switch context from BSP to AP you need call `fly()` on kernel page. Also `smp.vxd` hooks thread switch so every suitable process can be switched to AP automatically. When done, on BSP is running placement (decrease priory and call `Sleep(0)` - [see zero value description on MSDN](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep)), AP call `iret` do switch RING and loaded CPU user CPU state.

APC/S using following segments (flat memory model):
- system code: 0x8
- system data: 0x10
- user code 0x18
- user data 0x20
- TSS: 0x28

But how AP can handle system call or exception? Very easy, on Win9x system call are done thought call gate on `FS` segment and system DLL are placed on 0xBFFxxxx address. As you see even though
VMM.VXD is dynamic core, kernel32.dll subsystem is nice monolith. So APC/S has traps on these pages  and FS segment and on exception (also on any other besides int3 on kernel pages) is switched back and rerun instruction which fired this exception.

### AP to BSP

When code is done on AP (by int/exception or called `land()`), is released lock on placement function, priority is updated and called `reattach()`. `reattach()` just fire int3 and in exception handler is context replaced to one from AP.

### Debug

For debugging I'm using COM0 and COM1 (on VM you can route output to pipe/TCP/file). For enable debug create file `config.mk` with this content:

```
HAVE_CONFIG_MK=1

DEBUG=1
APP_DEBUG=1
```

When `DEBUG` defined SMP.VXD unit COM0 + C0M1 and output messages to them. When `APP_DEBUG` defined `libsmp.dll` and examples are build for debug.

Don't forget call `make clean` when changes these defines'


## Thanks

Many thanks to R. Loew (1952-2019) for his [MULTCORE](https://rloewelectronics.com/Programs/DEMOCORE.ZIP). Although this driver doesn't share any code (which is probably unknown/lost) without this software, I wouldn't have considered writing this driver possible.

## Licence

Code is under MIT License.

