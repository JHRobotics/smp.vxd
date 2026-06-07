#ifndef __TASKSWITCH_H__INCLUDED__
#define __TASKSWITCH_H__INCLUDED__

typedef struct ts_thread
{
	DWORD tid;                /* 0, ring0 id */
	struct ts_thread *next;  /* 4, next item in ht */
	DWORD smp_status;         /* 8, 1 when on AP */
	DWORD smp_apid;           /* 12, APID */
	DWORD smp_bsp_idle_proc;  /* 16, placement procedure for BSP */
	DWORD *smp_bsp_idle_lock; /* 20, pointer to BSP lock */
	DWORD dirty;              /* 24, 1 when there is no xsave */
	DWORD *data;              /* 28, pointer do data area */
	DWORD mode;               /* 32, switching type */
	DWORD pad[7];             /* 36, pad to XSAVE area */
	/* XSAVE */
} ts_thread_t;


#define TS_THREAD_PAGES 1

void ts_init();
DWORD ts_thread_tid();
ts_thread_t *ts_thread_create(DWORD tid);
void ts_thread_destroy(DWORD tid);
ts_thread_t *ts_thread_get(DWORD tid);


#endif /* __TASKSWITCH_H__INCLUDED__ */
