#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "counter.h"
#include "nocrt.h"

#define COUNTER_WIDTH 16
#define COUNTER_HEIGHT 16
#define COUNTER_SIZE (COUNTER_WIDTH*COUNTER_HEIGHT)

static const BYTE counter_map[COUNTER_SIZE] =
	"                "
	"     AAA   000  "
	"  S F   B 5   1 "
	"  S F   B 5   1 "
	"  S F   B 5   1 "
	"  S F   B 5   1 "
	"  S F   B 5   1 "
	"     GGG   666  "
	"  T E   C 4   2 "
	"  T E   C 4   2 "
	"  T E   C 4   2 "
	"  T E   C 4   2 "
	"  T E   C 4   2 "
	"     DDD   333  "
	"                "
	"                ";

/* NOTE: for next digit add 0x11 to segment ASCII code */

static const char *counter_digits[10] = {
	"012345",  // 0
	"12",      // 1
	"01346",   // 2
	"01236",   // 3
	"1256",    // 4
	"02356",   // 5
	"023456",  // 6
	"012",     // 7
	"0123456", // 8
	"012356",  // 9
};

#if 0
#define COUNTER_BMP_HEADER_SIZE 118

static const BYTE counter_bmp[246] = {
0x42, 0x4D, 0xF6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x28, 0x00,
0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x10, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 
/* palette (ARGB in little endian) */
/* B     G     R    A */
0x25, 0x1D, 0xED, 0x00, /* 1 - red */
0x00, 0x80, 0x00, 0x00, /* 2 - green */
0x00, 0xFF, 0xFF, 0x00, /* 3 - yellow */
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
/* folows one pixel per every nibble */
};
#endif

static DWORD color32[COUNTER_SIZE];

HICON counterIconSegments(const char *segments, DWORD color)
{
	HICON icon = NULL;
	ICONINFO ii;
	int i;
	const char *s = segments;

	while(*s != '\0')
	{
		BYTE c = (BYTE)(*s);
		for(i = 0; i < COUNTER_SIZE; i++)
		{
			if(counter_map[i] == c)
			{
				color32[i] = color;
			}
		}
		s++;
	}
	
	ii.fIcon = TRUE;
	
	ii.hbmColor = CreateBitmap(COUNTER_WIDTH, COUNTER_HEIGHT, 1, 32, color32);
	if(ii.hbmColor != NULL)
	{
		ii.hbmMask = CreateBitmap(COUNTER_WIDTH, COUNTER_HEIGHT, 1, 1, NULL);
		if(ii.hbmMask != NULL)
		{
			icon = CreateIconIndirect(&ii);
			DeleteObject(ii.hbmColor);
		}
		DeleteObject(ii.hbmColor);
	}
	
	return icon;
}

HICON counterIcon(int percent)
{
	DWORD color = 0x00FF0000;
	char segments[32] = "";
	int si = 0;
	int n, pos;
	
	if(percent > 100) percent = 100;
	if(percent < 0)   percent = 0;
	
	memset(color32, 0, sizeof(color32));
	n = percent;
	
	for(pos = 0; pos < 3; pos++)
	{
		const char *s;
		int d;
		
		d = n % 10;
		s = counter_digits[d];
		while(*s != '\0')
		{
			segments[si++] = (*s) + 0x11 * pos;
			s++;
		}
		n /= 10;
		if(n == 0) break;
	}
	segments[si] = '\0';
	
	return counterIconSegments(segments, color);
}
