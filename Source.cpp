#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "gdiplus")
#pragma comment(lib, "shlwapi")

#include <windows.h>
#include <shlwapi.h>
#include <gdiplus.h>

#include "GifEncoder.h"

#define DEFAULT_DPI 96
#define SCALEX(X) MulDiv(X, uDpiX, DEFAULT_DPI)
#define SCALEY(Y) MulDiv(Y, uDpiY, DEFAULT_DPI)
#define POINT2PIXEL(PT) MulDiv(PT, uDpiY, 72)

TCHAR szClassName[] = TEXT("Window");

BOOL GetScaling(HWND hWnd, UINT* pnX, UINT* pnY)
{
	BOOL bSetScaling = FALSE;
	const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (hMonitor)
	{
		HMODULE hShcore = LoadLibrary(TEXT("SHCORE"));
		if (hShcore)
		{
			typedef HRESULT __stdcall GetDpiForMonitor(HMONITOR, int, UINT*, UINT*);
			GetDpiForMonitor* fnGetDpiForMonitor = reinterpret_cast<GetDpiForMonitor*>(GetProcAddress(hShcore, "GetDpiForMonitor"));
			if (fnGetDpiForMonitor)
			{
				UINT uDpiX, uDpiY;
				if (SUCCEEDED(fnGetDpiForMonitor(hMonitor, 0, &uDpiX, &uDpiY)) && uDpiX > 0 && uDpiY > 0)
				{
					*pnX = uDpiX;
					*pnY = uDpiY;
					bSetScaling = TRUE;
				}
			}
			FreeLibrary(hShcore);
		}
	}
	if (!bSetScaling)
	{
		HDC hdc = GetDC(NULL);
		if (hdc)
		{
			*pnX = GetDeviceCaps(hdc, LOGPIXELSX);
			*pnY = GetDeviceCaps(hdc, LOGPIXELSY);
			ReleaseDC(NULL, hdc);
			bSetScaling = TRUE;
		}
	}
	if (!bSetScaling)
	{
		*pnX = DEFAULT_DPI;
		*pnY = DEFAULT_DPI;
		bSetScaling = TRUE;
	}
	return bSetScaling;
}

BITMAPINFOHEADER createBitmapHeader(int width, int height)
{
	BITMAPINFOHEADER  bi;

	// create a bitmap
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	return bi;
}

HBITMAP GdiPlusScreenCapture(HWND hWnd)
{
	// get handles to a device context (DC)
	HDC hwindowDC = GetDC(hWnd);
	HDC hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	// define scale, height and width
	int scale = 1;
	int screenx = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int screeny = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	// create a bitmap
	HBITMAP hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	BITMAPINFOHEADER bi = createBitmapHeader(width, height);

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);

	// Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that call HeapAlloc using a handle to the process's default heap.
	// Therefore, GlobalAlloc and LocalAlloc have greater overhead than HeapAlloc.
	DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;
	HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
	char* lpbitmap = (char*)GlobalLock(hDIB);

	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, screenx, screeny, width, height, SRCCOPY);   //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	// avoid memory leak
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hWnd, hwindowDC);

	return hbwindow;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static CGifEncoder* pGifEncoder = NULL;
	static HWND hButton1;
	static HWND hButton2;
	static HWND hEdit;
	static HFONT hFont;
	static UINT uDpiX = DEFAULT_DPI, uDpiY = DEFAULT_DPI;
	switch (msg)
	{
	case WM_CREATE:
		hButton1 = CreateWindow(TEXT("BUTTON"), TEXT("録画"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hButton2 = CreateWindow(TEXT("BUTTON"), TEXT("終了"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDCANCEL, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hWnd, WM_DPICHANGED, 0, 0);
		break;
	case WM_SIZE:
		MoveWindow(hButton1, POINT2PIXEL(10), POINT2PIXEL(10), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hButton2, POINT2PIXEL(10), POINT2PIXEL(50), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hEdit, POINT2PIXEL(10), POINT2PIXEL(90), LOWORD(lParam) - POINT2PIXEL(20), HIWORD(lParam) - POINT2PIXEL(100), TRUE);
		break;
	case WM_TIMER:
		if (pGifEncoder) {
			Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(256, 256);
			if (bmp) {
				Gdiplus::Graphics g(bmp);
				{
					const HDC hdc = g.GetHDC();
					HWND hDesktopWnd = GetDesktopWindow();
					{
						HDC hDC = GetDC(hDesktopWnd);
						BitBlt(hdc, 0, 0, 256, 256, hDC, 0, 0, SRCCOPY);
						ReleaseDC(hDesktopWnd, hDC);
					}
					g.ReleaseHDC(hdc);
				}
				pGifEncoder->AddFrame(bmp);
				delete bmp;
			}
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			if (pGifEncoder == 0) {
				pGifEncoder = new CGifEncoder();
				if (pGifEncoder) {
					pGifEncoder->SetFrameSize(256, 256);
					pGifEncoder->SetFrameRate(60);
					WCHAR szFilePath[MAX_PATH];
					GetModuleFileName(NULL, szFilePath, MAX_PATH);
					PathRemoveFileSpec(szFilePath);
					PathAppend(szFilePath, L"test.gif");
					pGifEncoder->StartEncoder(std::wstring(szFilePath));
					SetTimer(hWnd, 1, 1000 / 60, NULL);
				}
			}
			else {
				KillTimer(hWnd, 1);
				pGifEncoder->FinishEncoder();
				delete pGifEncoder;
				pGifEncoder = NULL;
			}
		} else if (LOWORD(wParam) == IDCANCEL)
		{
			if (pGifEncoder) {
				KillTimer(hWnd, 1);
				pGifEncoder->FinishEncoder();
				delete pGifEncoder;
				pGifEncoder = NULL;
			}
		}
		break;
	case WM_NCCREATE:
		{
			const HMODULE hModUser32 = GetModuleHandle(TEXT("user32.dll"));
			if (hModUser32)
			{
				typedef BOOL(WINAPI*fnTypeEnableNCScaling)(HWND);
				const fnTypeEnableNCScaling fnEnableNCScaling = (fnTypeEnableNCScaling)GetProcAddress(hModUser32, "EnableNonClientDpiScaling");
				if (fnEnableNCScaling)
				{
					fnEnableNCScaling(hWnd);
				}
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_DPICHANGED:
		GetScaling(hWnd, &uDpiX, &uDpiY);
		DeleteObject(hFont);
		hFont = CreateFontW(-POINT2PIXEL(10), 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, L"MS Shell Dlg");
		SendMessage(hButton1, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_DESTROY:
		if (pGifEncoder) {
			KillTimer(hWnd, 1);
			pGifEncoder->FinishEncoder();
			delete pGifEncoder;
			pGifEncoder = NULL;
		}
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	ULONG_PTR gdiToken;
	Gdiplus::GdiplusStartupInput gdiSI;
	Gdiplus::GdiplusStartup(&gdiToken, &gdiSI, NULL);
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Window"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	Gdiplus::GdiplusShutdown(gdiToken);
	return (int)msg.wParam;
}
