#ifndef __VXD_DEBUG_H__INCLUDED__
#define __VXD_DEBUG_H__INCLUDED__

/* debug */
#ifdef DEBUG
void dbg_init();
void dbg_printf(const char *fmt, ...);
void tracef(const char *fmt, ...);
#else
#define dbg_init()
#define dbg_printf(s, ...)
#define tracef(s, ...)
#endif

#endif /* __VXD_DEBUG_H__INCLUDED__ */
