#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "gdiplus")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "comctl32")
#pragma comment(lib, "dwmapi")

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <dwmapi.h>

#include "GifEncoder.h"

#define DEFAULT_DPI 96
#define SCALEX(X) MulDiv(X, uDpiX, DEFAULT_DPI)
#define SCALEY(Y) MulDiv(Y, uDpiY, DEFAULT_DPI)
#define POINT2PIXEL(PT) MulDiv(PT, uDpiY, 72)

TCHAR szClassName[] = TEXT("Window");
RECT rcRecordingRect;

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

LRESULT CALLBACK LayerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static BOOL bDrag;
	static BOOL bDown;
	static POINT posStart;
	static RECT OldRect;
	switch (msg) {
	case WM_KEYDOWN:
	case WM_RBUTTONDOWN:
		{
			RECT rect;
			HWND hDesktopWnd = GetDesktopWindow();
			GetWindowRect(hDesktopWnd, &rect);
			rcRecordingRect = rect;
		}
		SendMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	case WM_LBUTTONDOWN:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		POINT point = { xPos, yPos };
		ClientToScreen(hWnd, &point);
		posStart = point;
		SetCapture(hWnd);
	}
	break;
	case WM_MOUSEMOVE:
		if (GetCapture() == hWnd)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			POINT point = { xPos, yPos };
			ClientToScreen(hWnd, &point);
			if (!bDrag) {
				if (abs(xPos - posStart.x) > GetSystemMetrics(SM_CXDRAG) && abs(yPos - posStart.y) > GetSystemMetrics(SM_CYDRAG)) {
					bDrag = TRUE;
				}
			}
			else {
				HDC hdc = GetDC(hWnd);
				RECT rect = { min(point.x, posStart.x), min(point.y, posStart.y), max(point.x, posStart.x), max(point.y, posStart.y) };
				HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
				HRGN hRgn1 = CreateRectRgn(OldRect.left, OldRect.top, OldRect.right, OldRect.bottom);
				HRGN hRgn2 = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
				CombineRgn(hRgn1, hRgn1, hRgn2, RGN_DIFF);
				FillRgn(hdc, hRgn1, (HBRUSH)GetStockObject(BLACK_BRUSH));
				FillRect(hdc, &rect, hBrush);
				OldRect = rect;
				DeleteObject(hBrush);
				DeleteObject(hRgn1);
				DeleteObject(hRgn2);
				ReleaseDC(hWnd, hdc);
			}
		}
		break;
	case WM_LBUTTONUP:
		if (GetCapture() == hWnd) {
			ReleaseCapture();
			if (bDrag) {
				bDrag = FALSE;
				int xPos = GET_X_LPARAM(lParam);
				int yPos = GET_Y_LPARAM(lParam);
				POINT point = { xPos, yPos };
				ClientToScreen(hWnd, &point);
				RECT rect = { min(point.x, posStart.x), min(point.y, posStart.y), max(point.x, posStart.x), max(point.y, posStart.y) };
				ShowWindow(hWnd, SW_HIDE);
				rcRecordingRect = rect;
			}
			else {
				ShowWindow(hWnd, SW_HIDE);
				HWND hTargetWnd = WindowFromPoint(posStart);
				hTargetWnd = GetAncestor(hTargetWnd, GA_ROOT);
				if (hTargetWnd) {
					RECT rect;
					if (DwmGetWindowAttribute(hTargetWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect)) != S_OK) {
						GetWindowRect(hTargetWnd, &rect);
					}
					rcRecordingRect = rect;
				}
			}
		}
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static DWORD dwTick = 30;
	static HWND hTrackBar;
	static HWND hEdit1;
	static CGifEncoder* pGifEncoder = NULL;
	static HWND hButton1;
	static HWND hButton2;
	static HWND hButton3;
	static HFONT hFont;
	static UINT uDpiX = DEFAULT_DPI, uDpiY = DEFAULT_DPI;
	static BOOL bProgramEvent = FALSE;
	static BOOL bRecording = FALSE;
	static LPCWSTR lpszLayerWindowClass = L"LayerWindow";
	static HWND hLayerWnd;

	switch (msg)
	{
	case WM_CREATE:
		InitCommonControls();
		{
			WNDCLASS wndclass = { 0,LayerWndProc,0,0,((LPCREATESTRUCT)lParam)->hInstance,0,LoadCursor(0,IDC_CROSS),(HBRUSH)GetStockObject(BLACK_BRUSH),0,lpszLayerWindowClass };
			RegisterClass(&wndclass);
		}
		{
			RECT rect;
			HWND hDesktopWnd = GetDesktopWindow();
			GetWindowRect(hDesktopWnd, &rect);
			rcRecordingRect = rect;
		}
		hButton3 = CreateWindow(L"BUTTON", L"ウィンドウ/領域 指定", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)1001, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hButton1 = CreateWindow(L"BUTTON", L"録画", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hButton2 = CreateWindow(L"BUTTON", L"録画終了", WS_VISIBLE | WS_CHILD | WS_DISABLED, 0, 0, 0, 0, hWnd, (HMENU)IDCANCEL, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hTrackBar = CreateWindowEx(0, TRACKBAR_CLASS, 0, WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_HORZ | TBS_TOOLTIPS, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);				
		SendMessage(hTrackBar, TBM_SETRANGE, TRUE, MAKELPARAM(1, 60));
		SendMessage(hTrackBar, TBM_SETTICFREQ, 1, 0);
		SendMessage(hTrackBar, TBM_SETPOS, TRUE, dwTick);
		SendMessage(hTrackBar, TBM_SETPAGESIZE, 0, 1);
		SendMessage(hTrackBar, TBM_SETBUDDY, 0, 1);
		{
			WCHAR szText[8];
			wsprintf(szText, L"%d", dwTick);
			hEdit1 = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", szText, WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		}
		SendMessage(hWnd, WM_DPICHANGED, 0, 0);
		break;
	case WM_SIZE:
		MoveWindow(hButton3, POINT2PIXEL(10), POINT2PIXEL(10), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hButton1, POINT2PIXEL(10), POINT2PIXEL(50), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hButton2, POINT2PIXEL(10), POINT2PIXEL(90), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hTrackBar, POINT2PIXEL(10), POINT2PIXEL(130), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		MoveWindow(hEdit1, POINT2PIXEL(10), POINT2PIXEL(170), POINT2PIXEL(256), POINT2PIXEL(32), TRUE);
		break;
	case WM_HSCROLL:
		if (LOWORD(wParam) == SB_THUMBTRACK && bProgramEvent == FALSE) {
			dwTick = HIWORD(wParam);
			WCHAR szText[256];
			wsprintf(szText, L"%d", dwTick);
			bProgramEvent = TRUE;
			SetWindowText(hEdit1, szText);
			bProgramEvent = FALSE;
		}
		break;
	case WM_TIMER:
		if (pGifEncoder) {
			Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(rcRecordingRect.right - rcRecordingRect.left, rcRecordingRect.bottom - rcRecordingRect.top);
			if (bmp) {
				Gdiplus::Graphics g(bmp);
				{
					const HDC hdc = g.GetHDC();
					HWND hDesktopWnd = GetDesktopWindow();
					{
						HDC hDC = GetDC(hDesktopWnd);
						BitBlt(hdc, 0, 0, rcRecordingRect.right - rcRecordingRect.left, rcRecordingRect.bottom - rcRecordingRect.top, hDC, rcRecordingRect.left, rcRecordingRect.top, SRCCOPY);
						ReleaseDC(hDesktopWnd, hDC);
						CURSORINFO cursor = { sizeof(cursor) };
						GetCursorInfo(&cursor);
						if (cursor.flags == CURSOR_SHOWING) {
							ICONINFO info = { sizeof(info) };
							GetIconInfo(cursor.hCursor, &info);
							const int x = cursor.ptScreenPos.x - rcRecordingRect.left - info.xHotspot;
							const int y = cursor.ptScreenPos.y - rcRecordingRect.top - info.yHotspot;
							BITMAP bmpCursor = { 0 };
							GetObject(info.hbmColor, sizeof(bmpCursor), &bmpCursor);
							DrawIconEx(hdc, x, y, cursor.hCursor, bmpCursor.bmWidth, bmpCursor.bmHeight,
								0, NULL, DI_NORMAL);
						}
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
			if (pGifEncoder == 0 && bRecording == FALSE) {
				pGifEncoder = new CGifEncoder();
				if (pGifEncoder) {
					pGifEncoder->SetFrameSize(rcRecordingRect.right - rcRecordingRect.left, rcRecordingRect.bottom - rcRecordingRect.top);
					pGifEncoder->SetFrameRate((float)dwTick);
					WCHAR szFilePath[MAX_PATH];
					GetModuleFileName(NULL, szFilePath, MAX_PATH);
					PathRemoveFileSpec(szFilePath);
					WCHAR szFileName[MAX_PATH];
					SYSTEMTIME st;
					GetLocalTime(&st);
					wsprintf(szFileName, L"%04d%02d%02d%02d%02d%02d%03d.gif", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
					PathAppend(szFilePath, szFileName);
					pGifEncoder->StartEncoder(std::wstring(szFilePath));
					bRecording = TRUE;
					EnableWindow(hButton1, FALSE);
					EnableWindow(hButton2, TRUE);
					EnableWindow(hButton3, FALSE);
					EnableWindow(hTrackBar, FALSE);
					EnableWindow(hEdit1, FALSE);
					SetTimer(hWnd, 1, 1000 / dwTick, NULL);
				}
			}
			else {
				KillTimer(hWnd, 1);
				if (pGifEncoder) {
					pGifEncoder->FinishEncoder();
					delete pGifEncoder;
					pGifEncoder = NULL;
				}
				EnableWindow(hButton1, TRUE);
				EnableWindow(hButton2, FALSE);
				EnableWindow(hButton3, TRUE);
				EnableWindow(hTrackBar, TRUE);
				EnableWindow(hEdit1, TRUE);
			}
		} else if (LOWORD(wParam) == IDCANCEL) {
			KillTimer(hWnd, 1);
			if (pGifEncoder) {
				pGifEncoder->FinishEncoder();
				delete pGifEncoder;
				pGifEncoder = NULL;
			}
			EnableWindow(hButton1, TRUE);
			EnableWindow(hButton2, FALSE);
			EnableWindow(hButton3, TRUE);
			EnableWindow(hTrackBar, TRUE);
			EnableWindow(hEdit1, TRUE);
		}
		else if (LOWORD(wParam) == 1001) {
			hLayerWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST, lpszLayerWindowClass, 0, WS_POPUP, 0, 0, 0, 0, hWnd, 0, GetModuleHandle(0), 0);
			SetLayeredWindowAttributes(hLayerWnd, RGB(255, 0, 0), 64, LWA_ALPHA | LWA_COLORKEY);
			SetWindowPos(hLayerWnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN), SWP_NOSENDCHANGING);
			ShowWindow(hLayerWnd, SW_NORMAL);
			UpdateWindow(hLayerWnd);
		}
		else if (hEdit1 == (HWND)lParam && HIWORD(wParam) == EN_CHANGE) {
			if (bProgramEvent == FALSE) {
				WCHAR szText[256];
				GetWindowText(hEdit1, szText, 256);
				dwTick = _wtoi(szText);
				if (dwTick < 1) {
					dwTick = 1;
					bProgramEvent = TRUE;
					SetWindowText(hEdit1, L"1");
					bProgramEvent = FALSE;
				}
				else if (dwTick > 60) {
					dwTick = 60;
					bProgramEvent = TRUE;
					SetWindowText(hEdit1, L"60");
					bProgramEvent = FALSE;
				}
				bProgramEvent = TRUE;
				SendMessage(hTrackBar, TBM_SETPOS, TRUE, dwTick);
				bProgramEvent = FALSE;
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
		SendMessage(hButton2, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hButton3, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hEdit1, WM_SETFONT, (WPARAM)hFont, 0);
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

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
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
		TEXT("GIF Screen Recorder"),
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
