# SMP.vxd

Symmetric multiprocessor driver for Windows 9x (95/98/Me). This driver allow to utilize multi-core CPU under these ancient operation systems.

## UNDER DEVELOPMENT

This is concept, expect large stability and performance issues.

Currently tested devices:
- VirtualBox (7.2.2)
- VMware Workstation (7.5.2)
- Gigabyte GA-G41M-ES2H + Intel Xeon X5460

## Important notice

This isn't magic trick how increase performance on legacy games but allow you to write new application or modify open-source older things to use multi-thread/multi-core/multi-cpu performance.

## Usage

### Installation

Copy smp.vxd to C:\WINDOWS\system (not system32!). Edit C:\WINDOWS\system.ini and under `[386enh]` and following line:

```
device=smp.vxd
```

And reboot computer. Boot you cant run `cpuinfo.exe` to check if driver is loaded and number of CPUs.

### Uninstallation

Edit C:\WINDOWS\system.ini and under under `[386enh]` remove line:
```
device=smp.vxd
```
(or you can comment line like, if you need only temporary disable)
```
rem device=smp.vxd
```


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

There we thing you should known at begging, good things first: Windows 9x known nothing about SMP and leave all AP untouched, Windows 9x are based on microcore (vmm.vxd) so is possible extend system functionality more than (semi-, hybrid) monolithic kernel - like NT or Unix (BDS and Linux included). Bad things: there is only one page directory (PD) - by Intel recommendation every process should have own PD (physical address to PD in on CR3 register) - but on Windows 9x is only one PD and for every process switch needs to rebuilt it (that why there so much issue with newer CPUs and virtual machines).


## Thanks

Many thanks to R. Loew (1952-2019) for his [MULTCORE](https://rloewelectronics.com/Programs/DEMOCORE.ZIP). Although this driver doesn't share any code (which is probably unknown/lost) without this software, I wouldn't have considered writing this driver possible.

## Licence

Code is under MIT License.

