/* 
  This is simple CPU benchmark and example of how to use smp.vxd:
	1) compile without extra defs:
		- program use only system resources (1 CPU on Windows 9X)
	2) compile without extra defs but run on output exe file:
		fixlink -relink prime.exe libsmp.dll kernel32.dll msvcrt.dll
		- program now can use smp.vxd driver
	3) compile program with -DSMP9X_LIB and link together with smp9x.c 
		- now you needs smp9x_init, smp9x_close and replace
			CreateThread => smp9x_CreateThread
			(folow #if)
		- this is semi-controled way how use driver
	4) compile program with -DSMP9X_FULLCTL and link together with smp9x.c
		- now you control tranfer between BSP and AP

	NOTE: This is CPU benchmark, not way how counting prime numbers,
	      just because there is much smarter way:
	      https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes
*/

#if defined(SMP9X_FULLCTL) && defined(SMP9X_LIB)
#error "You can't define SMP9X_FULLCTL and SMP9X_LIB same time!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include <windows.h>
#if defined(SMP9X_FULLCTL) || defined(SMP9X_LIB)
#include "smp9x.h"
#endif

uint64_t atoull(const char *ptr)
{
	int i = 0;
	uint64_t n = 0;
	while(ptr[i] >= '0' && ptr[i] <= '9')
	{
		n  = n*10;
		n += ptr[i] - '0';
		i++;
	}

	return n;
}

uint64_t sqrt64(uint64_t n)
{
	uint64_t left = 0;
	uint64_t right = n+1;
	
	while(left != right - 1)
	{
		uint64_t midpoint = (left + right) / 2;
		if(midpoint > 0xFFFFFFFFULL) /* overflow check */
		{
			right = midpoint;
		}
		else
		{
			if(midpoint * midpoint <= n)
				left = midpoint;
			else
				right = midpoint;
		}
	}
	
	return left;
}

/* this is is not right algorithm when you want to check real prime, but burn CPU for while */
int is_prime(uint64_t n)
{
	if(n == 0ULL)
	{
		return 0;
	}
	else if((n & 0x1) == 0) /* even number */
	{
		if(n == 2ULL)
		{
			return 1;
		}
		return 0;
	}
	else
	{
		uint64_t d = sqrt64(n);
		
		while(d > 1)
		{
			if((n % d) == 0)
			{
				return 0;
			}
			d--;
		}
	}
	
	return 1;
}

uint64_t count_primes(uint64_t from, uint64_t to)
{
	uint64_t n;
	uint64_t cnt = 0;
	for(n = from; n <= to; n++)
	{
		cnt += is_prime(n);
		
#ifndef SMP9X_FULLCTL
		/* when you full control, you have eliminated system call, or
			call smp9x_land() before and smp9x_fly() after them
		 */
		if(n % 100000 == 0)
		{
			printf("tested prime: %llu\n", n);
		}
#endif
	}
	
	return cnt;
}

typedef struct mp_result
{
	uint64_t from;
	uint64_t to;
	uint64_t result;
	uint32_t tnum;
	uint32_t pad;
} mp_result_t;


DWORD WINAPI count_primes_mp_main(LPVOID lpParameter)
{
#ifdef SMP9X_FULLCTL
		volatile DWORD lock;
		/* ^ lock is per thread, simplest option is put it on thread stack */
		smp9x_thread_elevate(&lock, SMP_MODE_MANUAL);
#endif
	
	mp_result_t *r = (mp_result_t *)lpParameter;
	
	printf("thread start: %d\n", r->tnum);
	
#ifdef SMP9X_FULLCTL
	smp9x_thread_fly();
#endif

	/* computing part */
	r->result = count_primes(r->from, r->to);

#ifdef SMP9X_FULLCTL
	smp9x_thread_land();
#endif

	printf("thread end: %d\n", r->tnum);
	return 0;
}

uint64_t count_primes_mp(uint64_t from, uint64_t to, int cpus)
{
	if(cpus < 2)
	{
		return count_primes(from, to);
	}
	else
	{
		mp_result_t *results = (mp_result_t*)malloc(sizeof(mp_result_t)*cpus);
		HANDLE *threads = (HANDLE*)malloc(sizeof(HANDLE)*cpus);
		int i;
		uint64_t fr = 0;
		
		if(results != NULL && threads != NULL)
		{	
			uint64_t range = (to - from) / cpus;
			
			if(range > 0)
			{
				results[0].from = from;
				results[0].to = range;
				results[0].result = 0;
				results[0].tnum = 1;
				
				for(i = 1; i < cpus-1; i++)
				{
					results[i].from = results[i-1].to + 1;
					results[i].to = results[i].from + range;
					results[i].result = 0;
					results[i].tnum = i+1;
				}
				
				results[i].from = results[i-1].to + 1;
				results[i].to = to;
				results[i].result = 0;
				results[i].tnum = i+1;
				
				for(i = 0; i < cpus; i++)
				{
					DWORD id;
					#ifdef SMP9X_LIB
					threads[i] = smp9x_CreateThread(NULL, 4096, count_primes_mp_main, results+i, 0, &id);
					#else
					threads[i] = CreateThread(NULL, 4096, count_primes_mp_main, results+i, 0, &id);
					#endif
				}
				
				WaitForMultipleObjects(cpus, threads, TRUE, INFINITE);
				
				for(i = 0; i < cpus; i++)
				{
					fr += results[i].result;
				}
				
				return fr;
			}
			else
			{
				fprintf(stderr, "to low range\n");
			}
		}
		else
		{
			fprintf(stderr, "malloc failed\n");
		}
	}
	
	return 0;
}

#define BENCH_DEFAULT  4000000

int main(int argc, char **argv)
{
	SYSTEM_INFO si;
	uint64_t cnt;
	clock_t t;
	int threads = 0;
	uint64_t bench = BENCH_DEFAULT;
	
	GetSystemInfo(&si);
	
#if defined(SMP9X_FULLCTL) || defined(SMP9X_LIB)
	smp9x_init();
	si.dwNumberOfProcessors = smp9x_cpus();
#endif
	
	t = clock();

	if(argc >= 2)
	{
		threads = atoi(argv[1]);
		if(threads < 0)
		{
			fprintf(stderr, "number of threads must be positive number or 0 for auto (number of cpus)\n");
			return EXIT_FAILURE;
		}
	}
	
	if(argc >= 3)
	{
		bench = atoull(argv[2]);
	}
	
	if(bench < 1000ULL)
	{
		fprintf(stderr, "minimal bench is 1000\n");
		return EXIT_FAILURE;
	}
	
	if(threads == 0)
	{
		threads = si.dwNumberOfProcessors;
	}

	printf("cpus = %lu, using threads = %d, bench=%llu\n", si.dwNumberOfProcessors, threads, bench);
	cnt = count_primes_mp(0, bench, threads);
	
	t = clock() - t;
	
	printf("primes = %llu, time = %f\n", cnt, ((float)t)/CLOCKS_PER_SEC);

#if defined(SMP9X_FULLCTL) || defined(SMP9X_LIB)
	smp9x_close();
#endif

	return EXIT_SUCCESS;
}
