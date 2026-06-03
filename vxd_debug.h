#ifndef __VXD_DEBUG_H__INCLUDED__
#define __VXD_DEBUG_H__INCLUDED__

/* debug */
#ifdef DBGPRINT
void dbg_printf( const char *s, ... );
#else
#define dbg_printf(s, ...)
#endif

void debug2(const char *s);

#endif /* __VXD_DEBUG_H__INCLUDED__ */
