/**************************************************************************

Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*****************************************************************************/
#include "vxd.h"

#define IO_IN8
#define IO_OUT8
#include "io32.h"

#define TERROR_MAX 256
#define TERROR_MAX_NUM 64

#define IO_COM1   0x3F8 /* default COM1 I/O */
#define IO_COM2   0x2F8 /* COM2 I/O */
#define IO_COM3   0x3E8 /* COM3 I/O */
#define IO_COM4   0x2E8 /* COM3 I/O */

static char hex_lower[] = "0123456789abcdef";
static char hex_upper[] = "0123456789ABCDEF";

static void terrorf_d(char **outp, long d, int length, char pad)
{
	char buf[TERROR_MAX_NUM];
	int n = d;
	char *ptr = buf;
	char *out = *outp;
	
	if(d == 0)
	{
		*out = '0';
		out++;
		*outp = out;
		return;
	}
	
	while(n != 0)
	{
		int k = (n % 10);
		if(k < 0) k = -k;
		*ptr = k + '0';
		n /= 10;
		ptr++;
	}
	
	if(d < 0)
	{
		length--;
	}
	
	while(length > 0)
	{
		*ptr = pad;
		ptr++;
		length--;
	}
	
	if(d < 0)
	{
		*ptr = '-';
		ptr++;
	}
	
	while(ptr != buf)
	{
		ptr--;
		*out = *ptr;
		out++;
	}
	*outp = out;
}

static void terrorf_u(char **outp, unsigned long d, int length, char pad)
{
	char buf[TERROR_MAX_NUM];
	unsigned long n = d;
	char *ptr = buf;
	char *out = *outp;
	
	if(d == 0)
	{
		*out = '0';
		out++;
		*outp = out;
		return;
	}
	
	while(n != 0)
	{
		*ptr = (n % 10) + '0';
		n /= 10;
		ptr++;
		length--;
	}
	
	while(length > 0)
	{
		*ptr = pad;
		ptr++;
		length--;
	}
	
	while(ptr != buf)
	{
		ptr--;
		*out = *ptr;
		out++;
	}
	*outp = out;
}

static void terrorf_x(char **outp, unsigned long x, int upper, int length, char pad)
{
	char buf[TERROR_MAX_NUM];
	unsigned long n = x;
	char *ptr = buf;
	char *out = *outp;
	const char *hex = hex_lower;

	if(upper)
	{
		hex = hex_upper;
	}

	if(x == 0)
	{
		*ptr = '0';
		ptr++;
		length--;
	}

	while(n != 0)
	{
		unsigned long k = n % 16;
		*ptr = hex[k];
		n /= 16;
		ptr++;
		length--;
	}
	
	while(length > 0)
	{
		*ptr = pad;
		ptr++;
		length--;
	}

	while(ptr != buf)
	{
		ptr--;
		*out = *ptr;
		out++;
	}
	*outp = out;
}

static void terrorf_s(char **outp, const char *s)
{
	char *out = *outp;
	while(*s != '\0')
	{
		*out = *s;
		s++;
		out++;
	}
	*outp = out;
}


static int serial_transmit_empty(unsigned short port)
{
	return inp(port + 5) & 0x20;
}

static void serial_out(unsigned short port, char c)
{
	while(serial_transmit_empty(port) == 0);
	outp(port, c);
}

static volatile void terror_out(char c)
{
	_asm mov ax, 0x200
	_asm mov dl, [c]
	_asm push dword ptr 0x21
	VMMCall(Exec_VxD_Int)
}

static volatile char terror_in()
{
	volatile char c = 0;
	
	_asm mov ax, 0x100
	_asm push dword ptr 0x21
	VMMCall(Exec_VxD_Int)
	_asm mov [c], al
	
	return c;
}

/**
 * Very simple support only:
 * d, i,
 * u
 * x, X
 * s
 **/
void terrorvf(int type, const char *fmt, va_list va)
{
	char fmtbuffer[TERROR_MAX];
	
	const char *ptr;
	char *out = fmtbuffer;
	
	int in_command = 0;
	char cmd_pad = ' ';
	int length = 0;

	for(ptr = fmt;*ptr != '\0'; ptr++)
	{
		if(in_command == 0)
		{
			switch(*ptr)
			{
				case '%':
					in_command = 1;
					cmd_pad = ' ';
					length = 0;
					break;
				default:
					*out = *ptr;
					out++;
					break;
			}
		}
		else
		{
			switch(*ptr)
			{
				case '%':
					*out = '%';
					in_command = 0;
					break;
				case '0':
					if(length == 0)
					{
						cmd_pad = '0';
					}
					else
					{
						length *= 10;
					}
					break;
				case '1': case '2': case '3':
				case '4': case '5': case '6':
				case '7': case '8': case '9':
					length *= 10;
					length += *ptr - '0';
					break;
				case 'i':
				case 'd':
				{
					long d = va_arg(va, long);
					terrorf_d(&out, d, length, cmd_pad);
					in_command = 0;
					break;
				}
				case 'u':
				{
					unsigned long u = va_arg(va, unsigned long);
					terrorf_u(&out, u, length, cmd_pad);
					in_command = 0;
					break;
				}
				case 's':
				{
					const char *s = va_arg(va, const char *);
					terrorf_s(&out, s);
					in_command = 0;
					break;
				}
				case 'x':
				case 'X':
				{
					unsigned long u = va_arg(va, unsigned long);
					terrorf_x(&out, u, *ptr == 'X', length, cmd_pad);
					in_command = 0;
					break;
				}				
				default:
					/* eat all potencial formating... */
					break;
			}
		}
	}
	*out = '\0';
	

	terror(type, fmtbuffer);
}

void terrorf(int type, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	terrorvf(type, fmt, va);
	va_end(va);
}

void terror(int type, const char *str)
{
	const char *ptr = str;
	switch(type)
	{
		case TERROR_COM1:
			while(*ptr != '\0')
			{
				serial_out(IO_COM1, *ptr);
				ptr++;
			}
			break;
		case TERROR_COM2:
			while(*ptr != '\0')
			{
				serial_out(IO_COM2, *ptr);
				ptr++;
			}
			break;
		default:
			while(*ptr != '\0')
			{
				/* auto include return */
				if(*ptr == '\n')
				{
					terror_out('\r');
				}
				
				terror_out(*ptr);
				ptr++;
			}
	}
}

void tpause()
{
	terror(TERROR_MGA, "Press Enter to continue...\n");
	for(;;)
	{
		char c = terror_in();
		if(c == '\r' || c == '\n')
		{
			break;
		}
	}
}

static void tinit_serial(unsigned short port)
{
	outp(port + 1, 0x00);    // Disable all interrupts
	outp(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outp(port + 0, 0x01);    // Set divisor to 3 (lo byte) 38400 baud 0x0C = 9600, 0x01 = 115200
	outp(port + 1, 0x00);    //                  (hi byte)
	outp(port + 3, 0x03);    // 8 bits, no parity, one stop bit
	outp(port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
}

void tinit(int type)
{
	switch(type)
	{
		case TERROR_COM1:
			tinit_serial(IO_COM1);
			break;
		case TERROR_COM2:
			tinit_serial(IO_COM2);
			break;
	}
}
