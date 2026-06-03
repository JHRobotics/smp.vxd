#include "vxd.h"

#ifdef DEBUG

void dbg_init()
{
	tinit(TERROR_COM1);
	tinit(TERROR_COM2);
}

void dbg_printf(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	terrorvf(TERROR_COM1, fmt, va);
	va_end(va);
}

void tracef(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	terrorvf(TERROR_COM2, fmt, va);
	va_end(va);
}

#endif

