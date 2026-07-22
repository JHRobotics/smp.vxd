#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "counter.h"
#include "cpuid_smp.h"

#include "smp9x.h"
#include "perf.h"
#include "smpcon.h"

#include "nocrt.h"

#define WM_NOTIFYMSG (WM_USER + 1)
#define IDT_TIMER1 0x1001

#define IDT_CPU0_LOAD 0x1200

typedef BOOL (WINAPI * SetProcessDPIAware_f)();

#define REFRESH_MS 500

float rdpiX = 1.0;
float rdpiY = 1.0;

void setHightDPI()
{
	HDC hdc;
#if 0
	SetProcessDPIAware_f SetProcessDPIAware_h = NULL;
	HMODULE hmod = GetModuleHandleA("user32.dll");
	if(hmod != NULL)
	{
		SetProcessDPIAware_h = (SetProcessDPIAware_f)GetProcAddress(hmod, "SetProcessDPIAware");
		
		if(SetProcessDPIAware_h != NULL)
		{
			SetProcessDPIAware_h();
		}
	}
#endif
		
	hdc = GetDC(NULL);
	if (hdc)
	{
		rdpiX = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
		rdpiY = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
		ReleaseDC(NULL, hdc);
	}
	
}
#define DPIX(_spx) ((int)(ceil((_spx)*rdpiX)))
#define DPIY(_spx) ((int)(ceil((_spx)*rdpiY)))
#define WND_MCINFO_CLASS_NAME "MCPUINFOCLS"

BOOL win_closed = FALSE;

static void update_stats(HWND hwnd);

LRESULT CALLBACK mcinfo_win_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CLOSE:
			win_closed = TRUE;
			break;
		case WM_DESTROY:
		{
			win_closed = TRUE;
			PostQuitMessage(0);
			break;
		}
		case WM_TIMER:
		{
			if(wParam == IDT_TIMER1)
			{
				update_stats(hwnd);
			}
			break;
		}
		case WM_SIZE:
		{
			LONG style = GetWindowLongA(hwnd, GWL_STYLE);
			if(wParam == SIZE_MINIMIZED)
			{
				style |= WS_POPUP;
			}
			else
			{
				style &= ~WS_POPUP;
			}
			//SetWindowLongA(hwnd, GWL_STYLE, style);
			
			break;
		}
		case WM_NOTIFYMSG:
			switch(lParam)
			{
				case WM_CONTEXTMENU:
				case WM_RBUTTONDOWN:
				case WM_LBUTTONDOWN:
					//LONG style = GetWindowLongA(hwnd, GWL_STYLE) | WS_VISIBLE;
					//SetWindowLongA(hwnd, GWL_STYLE, style);
					//ShowWindow(hwnd, SW_SHOWNORMAL);
					break;
			}
			break;
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static char cpu_vendor[16];
static char cpu_name[64];

static char cpuid_dump[1024] = "";

BOOL get_vendor(uint32_t *maxleaf)
{
	cpuid_result_t r;
	if(cpuid(0, &r))
	{
		memcpy(cpu_vendor,   r.bytes+4,  4); // ebx
		memcpy(cpu_vendor+4, r.bytes+12, 4); // edx
		memcpy(cpu_vendor+8, r.bytes+8,  4); // ecx
		
		*maxleaf = r.regs.regEAX;
		
		cpu_vendor[12] = '\0';
		return TRUE;
	}
	else
	{
		strcpy(cpu_vendor, "NO CPUID");
	}
	
	return FALSE;
}

void get_cpuname()
{
	cpuid_result_t r;
	cpuid(0x80000002, &r);
	memcpy(cpu_name, r.bytes, 16);
	cpuid(0x80000003, &r);
	memcpy(cpu_name+16, r.bytes, 16);
	cpuid(0x80000004, &r);
	memcpy(cpu_name+32, r.bytes, 16);
	cpu_name[48] = '\0';
}

void get_cpufamily()
{
	cpuid_result_t r;
	cpuid(0x00000001, &r);
	
	uint32_t family  = ((r.regs.regEAX >> 20) & 0xFF) + ((r.regs.regEAX >> 8) & 0xF);
	uint32_t model   = (r.regs.regEAX >> 4) & 0xF;
	uint32_t steping =  r.regs.regEAX & 0xF;
	
	switch(family)
	{
		case 4:
			sprintf(cpu_name, "i486 compatible, model=0x%X, steping=0x%X", model, steping);
			break;
		case 5:
			sprintf(cpu_name, "Pentium compatible, model=0x%X, steping=0x%X", model, steping);
			break;
		case 6:
			sprintf(cpu_name, "Pentium Pro compatible, model=0x%X, steping=0x%X", model, steping);
			break;
		default:
			sprintf(cpu_name, "Family=0x%X, model: 0x%X, steping=0x%X", family, model, steping);
			break;
	}
}

#define DUMP_LEAF(_leaf) cpuid(_leaf, &e); \
		ptr += sprintf(ptr, "%08X: EAX=0x%08X EBX=0x%08X\r\n%08X: ECX=0x%08X EDX=0x%08X\r\n", \
			_leaf, e.regs.regEAX, e.regs.regEBX, _leaf, e.regs.regECX, e.regs.regEDX)

void cpu_ident()
{
	uint32_t maxleaf;
	BOOL extends = FALSE;
	if(get_vendor(&maxleaf))
	{
		cpuid_result_t e;
		if(maxleaf > 1)
		{
			cpuid(0x80000000, &e);
			if(e.regs.regEAX >= 0x80000001)
			{
				extends = TRUE;
			}
			if(e.regs.regEAX >= 0x80000004)
			{
				get_cpuname();
			}
			else
			{
				get_cpufamily();
			}
		}
		
		char *ptr = cpuid_dump;
		DUMP_LEAF(0x00000000);
		
		DUMP_LEAF(0x00000001);
		if(maxleaf >= 7)
		{
			DUMP_LEAF(0x00000007);
		}
		if(extends)
		{
			DUMP_LEAF(0x80000000);
			DUMP_LEAF(0x80000001);
		}
		if(maxleaf >= 0x16)
		{
			DUMP_LEAF(0x00000015);
			DUMP_LEAF(0x00000016);
			
			ptr += sprintf(ptr, "TSC frequency: %u kHz\n", cpuid_tsc_freq());
		}
	}
	else
	{
		sprintf(cpu_name, "Unknown 80386/80486");
	}
}

SYSTEM_INFO sysi;

#define WIN_W 400
#define WIN_H 400

void winf(HINSTANCE hInst, HWND win, int x, int y, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	CreateWindowA("STATIC", buf, (WS_VISIBLE | WS_CHILD | SS_LEFT), DPIX(x), DPIY(y), DPIX(WIN_W-x), DPIY(20), win, NULL, hInst, NULL);
	va_end(args);
}

void box(HINSTANCE hInst, HWND win, int x, int y, int w, int h, int id, const char *txt)
{
	CreateWindowA("EDIT", txt, (WS_VISIBLE | WS_CHILD | ES_RIGHT | ES_READONLY | WS_BORDER | WS_GROUP), DPIX(x), DPIY(y), DPIX(w), DPIY(h), win, (HMENU)id, hInst, NULL);
}

static HICON load_icon = NULL;
static HICON old_icon = NULL;
static BOOL shell_installed = FALSE;
static NOTIFYICONDATAA nid;

static void notify_set(HWND win, HICON icon)
{
	nid.cbSize = sizeof(nid);
	nid.hWnd = win;
	nid.uID = 100;
	nid.uVersion = NOTIFYICON_VERSION;
	nid.uCallbackMessage = WM_NOTIFYMSG;
	nid.hIcon = icon;

	strcpy(nid.szTip, "M-CPU info");
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

	if(shell_installed)
	{
		Shell_NotifyIconA(NIM_MODIFY, &nid);
	}
	else
	{
		shell_installed = Shell_NotifyIconA(NIM_ADD, &nid);
	}
}

static void notify_delete()
{
	if(shell_installed)
	{	
		Shell_NotifyIconA(NIM_DELETE, &nid);
		shell_installed = FALSE;
	}
}

#define OWN_CPUID
#include "ipfp.h"

static smp9x_IsProcessorFeaturePresent_t getIPFP()
{
	smp9x_IsProcessorFeaturePresent_t ipfp = smpcon_IsProcessorFeaturePresent;
	if(!smpcon_usable)
	{
		if(is_nt())
		{
			/* NOTE: we need check for NT, because on Win9x function exists, but returns always FALSE */
			HMODULE k32 = GetModuleHandle("kernel32.dll");
			if(k32)
			{
				ipfp = GetProcAddress(k32, "IsProcessorFeaturePresent");
			}
		}
	}
	
	if(ipfp == NULL)
	{
		ipfp = IsProcessorFeaturePresentCPUID;
	}
	
	return ipfp;
}

static void sse_version(char *out)
{
	smp9x_IsProcessorFeaturePresent_t ipfp = getIPFP();
	
	if(ipfp(PF_SSE4_2_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "SSE4.2");
	}
	else if(ipfp(PF_SSE4_1_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "SSE4.1");
	}
	else if(ipfp(PF_SSSE3_INSTRUCTIONS_AVAILABLE) ||  ipfp(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		if(ipfp(PF_SSSE3_INSTRUCTIONS_AVAILABLE) &&  ipfp(PF_SSE3_INSTRUCTIONS_AVAILABLE))
		{
			strcpy(out, "SSE3 + SSSE3");
		}
		else if(ipfp(PF_SSSE3_INSTRUCTIONS_AVAILABLE))
		{
			strcpy(out, "SSSE3");
		}
		else
		{
			strcpy(out, "SSE3");
		}
	}
	else if(ipfp(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "SSE2");
	}
	else if(ipfp(PF_XMMI_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "SSE1");
	}
	else
	{
		strcpy(out, "unsupported");
	}
}

static void x87_version(char *out)
{
	smp9x_IsProcessorFeaturePresent_t ipfp = getIPFP();
	
	if(!ipfp(PF_FLOATING_POINT_EMULATED))
	{
		strcpy(out, "x87");
		if(ipfp(PF_MMX_INSTRUCTIONS_AVAILABLE))
		{
			strcat(out, ", MMX");
		}
	}
	else
	{
		strcpy(out, "emulated");
	}
}

static void avx_version(char *out)
{
	smp9x_IsProcessorFeaturePresent_t ipfp = getIPFP();
	
	if(ipfp(PF_AVX2_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "AVX2");
	}
	else if(ipfp(PF_AVX_INSTRUCTIONS_AVAILABLE))
	{
		strcpy(out, "AVX");
	}
	else
	{
		strcpy(out, "unsupported");
	}
	
	if(ipfp(PF_AVX512F_INSTRUCTIONS_AVAILABLE))
	{
		strcat(out, ", AVX512");
	}
}

int main()
{
	HINSTANCE hInst = GetModuleHandle(NULL);
	WNDCLASS wc;
	HWND win;
	MSG msg;

	setHightDPI();
	if(smpcon())
	{
		smpcon_GetSystemInfo(&sysi);
	}
	else
	{
		GetSystemInfo(&sysi);
	}
	memset(&wc,  0, sizeof(WNDCLASS));
	
	load_icon = counterIconSegments("12456BCDE", 0x00FF0000);
	
	cpu_ident();
	
	int win_h = WIN_H;
	
	win_h += ((sysi.dwNumberOfProcessors + 3) / 4) * 25;
	
	perf_fetcher_start(REFRESH_MS);
	
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = mcinfo_win_proc;
	wc.lpszClassName = WND_MCINFO_CLASS_NAME;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hIcon         = load_icon;
	wc.hInstance     = hInst;

	RegisterClass(&wc);
	
	win_closed = FALSE;
	win = CreateWindowA(WND_MCINFO_CLASS_NAME, "M-CPU Info", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(WIN_W), DPIY(win_h), 0, 0, NULL, 0);
	
	winf(hInst, win, 10, 10,  "CPU: %s", cpu_name);
	winf(hInst, win, 10, 35,  "CPU vendor: %s", cpu_vendor);	
	winf(hInst, win, 10, 60,  "CPU threads: %d (usable)", sysi.dwNumberOfProcessors);
	if(smpcon_usable)
	{
		winf(hInst, win, 10, 85,  "Source: " SMP_DLL);
	}
	else if(is_nt())
	{
		winf(hInst, win, 10, 85,  "Source: native");
	}
	else
	{
		winf(hInst, win, 10, 85,  "Source: cpuid");
	}
	
	char features[128];
	x87_version(features);
	winf(hInst, win, 10, 110, "x87: %s", features);
	sse_version(features);
	winf(hInst, win, 10, 135, "SSE: %s", features);
	avx_version(features);
	winf(hInst, win, 10, 160, "AVX: %s", features);
	
	DWORD i;
	int cpu_w, cpu_h = 0;
	for(i = 0; i < sysi.dwNumberOfProcessors; i++)
	{
		cpu_w = (i % 4) * 94; 
		cpu_h = (i / 4) * 25;
		
		winf(hInst, win, 10+cpu_w, 185+cpu_h, "CPU %02d:", i);
		box(hInst, win, 10+cpu_w+55, 185-2+cpu_h, 30, 20, IDT_CPU0_LOAD+i, "0");
	}
	
	cpu_h += 25;
	CreateWindowA("EDIT", cpuid_dump, (WS_VISIBLE | WS_CHILD | ES_LEFT | ES_READONLY | ES_MULTILINE | WS_BORDER | WS_GROUP | ES_AUTOHSCROLL | ES_AUTOVSCROLL),
		DPIX(10), DPIY(185+cpu_h), DPIX(WIN_W-30), DPIY((win_h-20) - (185+cpu_h+25)), win, 0, hInst, NULL);

	SetTimer(win, IDT_TIMER1, REFRESH_MS,  NULL);

  while(!win_closed && GetMessage(&msg, NULL, 0, 0))
  {
  	if(!IsDialogMessageA(win, &msg))
  	{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
  }
  
  perf_fetcher_stop();
  
  notify_delete();

  DestroyWindow(win);
	if(old_icon)
	{
		DestroyIcon(old_icon);
 	}
  
	if(load_icon)
	{
		DestroyIcon(load_icon);
	}
	
	smpcon_free();

	ExitProcess(0);
	return EXIT_SUCCESS;
}

static void update_stats(HWND win)
{
	static DWORD last_sum = -1;
	DWORD sum = 0;
	DWORD c = 0;
	DWORD p, i;
	char display[64];
	
	for(i = 0; i < sysi.dwNumberOfProcessors; i++)
	{
		if(perf_cpu_load(i, &p))
		{
			sum += p;
			c++;
			
			HWND subwin = GetDlgItem(win, IDT_CPU0_LOAD+i);
			if(subwin)
			{
				sprintf(display, "%u", p);
				SetWindowTextA(subwin, display);
			}
		}
		//printf("CPU %d: %d\n", i, p);
	}
	
	DWORD total = 0;
	if(c > 0) total = sum / c;
	
	//printf("total: %d\n", total);
	
	if(last_sum != sum)
	{
		HICON icon = counterIcon(total);
		SetClassLongPtr(win, GCLP_HICON, (LONG_PTR)icon);
		
		notify_set(win, icon);
		
		HICON very_old_icon = old_icon;
		old_icon = load_icon;
		load_icon = icon;
		
		if(very_old_icon)
		{
			DestroyIcon(very_old_icon);
		}
		
		last_sum = sum;
	}
}

