#ifndef __PREF_H__INCLUDED__
#define __PREF_H__INCLUDED__

//BOOL perf_nt_read();
BOOL perf_cpu_load(int cpu_index, DWORD *result);
BOOL perf_fetcher_start(DWORD delay_ms);
void perf_fetcher_stop();
BOOL is_nt();

#endif /* __PREF_H__INCLUDED__ */
