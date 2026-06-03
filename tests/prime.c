#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include <windows.h>

#ifdef SMP9X
#include "smp9x.h"
#endif

uint64_t sqrt64(uint64_t n)
{
	uint64_t left = 0;
	uint64_t right = n+1;
	
	while(left != right -1)
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

void mersenne()
{
	uint64_t m = 0;
	for(m = 0; m <= 63; m++)
	{
		uint64_t n = (1ULL << m) - 1;
		printf("testing prime for n=%llu ... ", n);
		printf("%d\n", is_prime(n));
	}
}

uint64_t count_primes(uint64_t from, uint64_t to)
{
	uint64_t n;
	uint64_t cnt = 0;
	for(n = from; n <= to; n++)
	{
		cnt += is_prime(n);
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
#ifdef SMP9X
		volatile DWORD lock;
		/* ^ lock is per thread, simplest option is put it on thread stack */
		smp9x_thread_elevate(&lock);
#endif
	
	mp_result_t *r = (mp_result_t *)lpParameter;
	
	printf("thread start: %d\n", r->tnum);
	
#ifdef SMP9X
	smp9x_thread_fly();
#endif

	/* computing part */
	r->result = count_primes(r->from, r->to);

#ifdef SMP9X
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
					threads[i] = CreateThread(NULL, 4096, count_primes_mp_main, results+i, 0, &id);
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

//#define BENCH 20000000
#define BENCH  2000000

int main(int argc, char **argv)
{
	SYSTEM_INFO si;
	uint64_t cnt;
	clock_t t;
	int threads = 0;
	
	GetSystemInfo(&si);
	
#ifdef SMP9X
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
	
	if(threads == 0)
	{
		threads = si.dwNumberOfProcessors;
	}

	//cnt = count_primes(0, 20000000); 1270608
	printf("cpus = %lu, using threads = %d\n", si.dwNumberOfProcessors, threads);
	cnt = count_primes_mp(0, BENCH, threads);
	
	t = clock() - t;
	
	printf("primes = %llu, time = %f\n", cnt, ((float)t)/CLOCKS_PER_SEC);

#ifdef SMP9X
	smp9x_close();
#endif

	return EXIT_SUCCESS;
}
