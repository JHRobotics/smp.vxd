/*
	This test check when threads can correctly handle SSE unit state,
	Also check if SMP can runs SIMD instruction on AP
	
	Usage:
	ssetest [number of threads]
	
	number of threads = 10 by default
*/

#pragma GCC target("sse2")
/* change GCC CPU target */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <windows.h>
#include <xmmintrin.h>

typedef struct tdata
{
	uint32_t id;
	uint32_t procindex;
	uint32_t procresult;
} tdata_t;

DWORD WINAPI th_main(LPVOID lpParameter)
{
	tdata_t *data = (tdata_t*)lpParameter;
	float f_id = data->id;
	float f_pi;
	int ms;
	/* need 16B align */
#pragma pack(push)
#pragma pack(16)
	float res[4];
	__m128 r1, r2, r3;
#pragma pack(pop)
	
	r1 = _mm_set1_ps(f_id); /* all values to id */
	r3 = _mm_set1_ps(100.0f);
	r1 = _mm_mul_ps(r1, r3);
	
	f_pi = GetCurrentProcessorNumber();
	r2 = _mm_set_ss(f_pi); /* first value to f_pi */
	r2 = _mm_add_ps(r2, r2);
	
	ms = rand() % 500;
	Sleep(ms);
	
	r3 = _mm_set1_ps(2.0f);
	r2 = _mm_div_ps(r2, r3);
	
	_mm_store1_ps(res, r1);
	data->procresult = res[0];
	_mm_store1_ps(res, r2);
	data->procindex = res[0];
	
	return 0;
}

int main(int argc, char *argv[])
{
	int cnt = 10;	
	int i;
	tdata_t *data;
	HANDLE *threads;

	if(!IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
	{
		fprintf(stderr, "SSE2 not supported, sorry\n");
		return EXIT_FAILURE;
	}

	if(argc > 1)
	{
		int n = atoi(argv[1]);
		if(n > 0)
		{
			cnt = n;
		}
	}
	
	data = malloc(sizeof(tdata_t)*cnt);
	threads = malloc(sizeof(HANDLE)*cnt);
	
	if(data == NULL || threads == NULL)
	{
		fprintf(stderr, "malloc fail!\n");
		return EXIT_FAILURE;
	}
	
	for(i = 0; i < cnt; i++)
	{
		DWORD id;
		data[i].id = i+1;
		data[i].procindex = 0;
		data[i].procresult = 0;
		threads[i] = CreateThread(NULL, 4096, th_main, &(data[i]), 0, &id);
	}
	
	WaitForMultipleObjects(cnt, threads, TRUE, INFINITE);
	
	printf("results SSE:\n");
	for(i = 0; i < cnt; i++)
	{
		printf("%d: %d (%d)\n", data[i].id, data[i].procresult, data[i].procindex);
	}
	
	free(data);
	free(threads);
	
	return EXIT_SUCCESS;
}

