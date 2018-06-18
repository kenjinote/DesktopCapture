#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "gdiplus")

#include <windows.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <list>
#include "resource.h"

using namespace Gdiplus;

TCHAR szClassName[] = TEXT("Window");

int GetEncoderClsid(LPCWSTR format, LPCLSID pClsid)
{
	UINT num = 0, size = 0;
	GetImageEncodersSize(&num, &size);
	if (!size)return -1;
	ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)GlobalAlloc(GPTR, size);
	if (!pImageCodecInfo)return -1;
	GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			GlobalFree(pImageCodecInfo);
			return j;
		}
	}
	GlobalFree(pImageCodecInfo);
	return -1;
}

Gdiplus::Bitmap * SnapShot(const LPCLSID pClsid)
{
	HWND hWnd = GetDesktopWindow();
	HDC hDC = GetDC(hWnd);
	RECT rect;
	GetClientRect(hWnd, &rect);
	HDC hMemDC = CreateCompatibleDC(hDC);
	BITMAPINFO bitmapInfo = { sizeof(bitmapInfo.bmiHeader) };
	bitmapInfo.bmiHeader.biWidth = rect.right - rect.left;
	bitmapInfo.bmiHeader.biHeight = rect.bottom - rect.top;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 24;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = bitmapInfo.bmiHeader.biWidth * bitmapInfo.bmiHeader.biHeight * 3;
	LPVOID  pBits;
	HBITMAP hBitmap = CreateDIBSection(hMemDC, &bitmapInfo, DIB_RGB_COLORS, &pBits, 0, 0);
	HGDIOBJ hOldBitmap = SelectObject(hMemDC, hBitmap);
	BitBlt(hMemDC, 0, 0, bitmapInfo.bmiHeader.biWidth, bitmapInfo.bmiHeader.biHeight, hDC, 0, 0, SRCCOPY);
	ReleaseDC(hWnd, hDC);
	Bitmap* pBitmap = new Bitmap(hBitmap, DIB_RGB_COLORS);
	SelectObject(hMemDC, hOldBitmap);
	DeleteObject(hBitmap);
	DeleteDC(hMemDC);
	{
		// メモリー節約のため PNG に変換
		IStream *stream;
		CreateStreamOnHGlobal(NULL, true, &stream);
		pBitmap->Save(stream, pClsid);
		Bitmap * pBitmapPNG = new Bitmap(stream);
		stream->Release();
		delete pBitmap;
		pBitmap = pBitmapPNG;
	}
	return pBitmap;
}

BOOL CreateTempDirectory(LPTSTR pszDir)
{
	DWORD dwSize = GetTempPath(0, 0);
	if (dwSize == 0 || dwSize > MAX_PATH - 14) { goto END0; }
	LPTSTR pTmpPath;
	pTmpPath = (LPTSTR)GlobalAlloc(GPTR, sizeof(TCHAR)*(dwSize + 1));
	GetTempPath(dwSize + 1, pTmpPath);
	dwSize = GetTempFileName(pTmpPath, TEXT(""), 0, pszDir);
	GlobalFree(pTmpPath);
	if (dwSize == 0) { goto END0; }
	DeleteFile(pszDir);
	if (CreateDirectory(pszDir, 0) == 0) { goto END0; }
	return TRUE;
END0:
	return FALSE;
}

VOID CreateFileFromResource(TCHAR *szResourceName, TCHAR *szResourceType, TCHAR *szResFileName)
{
	HRSRC hRs = FindResource(0, szResourceName, szResourceType);
	DWORD dwResSize = SizeofResource(0, hRs);
	HGLOBAL hMem = LoadResource(0, hRs);
	LPBYTE lpByte = (BYTE *)LockResource(hMem);
	HANDLE hFile = CreateFile(szResFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD dwWritten;
	WriteFile(hFile, lpByte, dwResSize, &dwWritten, NULL);
	CloseHandle(hFile);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HFONT hFont;
	static BOOL bStopCapture = TRUE;
	static HWND hButton1;
	static HWND hButton2;
	static std::list<Bitmap*> list;
	static CLSID clsid;
	static DWORD dwStartTime;
	switch (msg)
	{
	case WM_CREATE:
		hFont = CreateFontW(-MulDiv(10, 96, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, L"MS Shell Dlg");
		GetEncoderClsid(L"image/png", &clsid);
		hButton1 = CreateWindow(TEXT("BUTTON"), TEXT("キャプチャースタート"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)IDOK, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hButton2 = CreateWindow(TEXT("BUTTON"), TEXT("キャプチャーストップ"), WS_VISIBLE | WS_CHILD | WS_DISABLED, 0, 0, 0, 0, hWnd, (HMENU)IDCANCEL, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hButton1, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hButton2, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_SIZE:
		MoveWindow(hButton1, 10, 10, 256, 32, TRUE);
		MoveWindow(hButton2, 10, 52, 256, 32, TRUE);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			EnableWindow(hButton1, FALSE);
			dwStartTime = GetTickCount();
			bStopCapture = FALSE;
			SetTimer(hWnd, 0x1234, 1, NULL);
			EnableWindow(hButton2, TRUE);
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EnableWindow(hButton2, FALSE);
			LPCTSTR lpszCaptureFileName = TEXT("capture.mp4");
			bStopCapture = TRUE;
			KillTimer(hWnd, 0x1234);
			DWORD dwTime = GetTickCount() - dwStartTime;
			TCHAR szTempDir[MAX_PATH];
			CreateTempDirectory(szTempDir);
			TCHAR szFFMpegPath[MAX_PATH];
			lstrcpy(szFFMpegPath, szTempDir);
			PathAppend(szFFMpegPath, TEXT("ffmpeg.exe"));
			CreateFileFromResource(MAKEINTRESOURCE(IDR_EXE1), TEXT("EXE"), szFFMpegPath);
			WCHAR szFileName[MAX_PATH];
			int nNumber = 1;
			for (auto item : list)
			{
				wsprintfW(szFileName, L"%s\\png%010d.png", szTempDir, nNumber);
				if (item->Save(szFileName, &clsid) == Ok)
				{
					nNumber++;
				}
				delete item;
			}
			{
				PROCESS_INFORMATION pInfo;
				STARTUPINFO sInfo;
				ZeroMemory(&sInfo, sizeof(STARTUPINFO));
				sInfo.cb = sizeof(STARTUPINFO);
				sInfo.dwFlags = STARTF_USEFILLATTRIBUTE | STARTF_USECOUNTCHARS | STARTF_USESHOWWINDOW;
				sInfo.wShowWindow = SW_HIDE;
				TCHAR szCommandLine[1024];
				int nFrameRate = (int)(list.size() / (dwTime / 1000.0));
				wsprintf(szCommandLine, TEXT("\"%s\" -r %d -i png%%010d.png -vcodec libx264 -pix_fmt yuv420p -r %d %s"), szFFMpegPath, nFrameRate, nFrameRate, lpszCaptureFileName);
				CreateProcess(0, szCommandLine, 0, 0, 0, 0, 0, szTempDir, &sInfo, &pInfo);
				CloseHandle(pInfo.hThread);
				WaitForSingleObject(pInfo.hProcess, INFINITE);
				CloseHandle(pInfo.hProcess);
			}
			{
				TCHAR szToPath[MAX_PATH] = { 0 };
				SYSTEMTIME systemtime;
				GetLocalTime(&systemtime);
				wsprintf(szToPath, TEXT("画面キャプチャ―_%04d%02d%02d%02d%02d%02d"),
					systemtime.wYear,
					systemtime.wMonth,
					systemtime.wDay,
					systemtime.wHour,
					systemtime.wMinute,
					systemtime.wSecond);
				OPENFILENAME of = { sizeof(OPENFILENAME) };
				of.hwndOwner = hWnd;
				of.lpstrFilter = TEXT("動画ファイル(*.mp4)\0*.mp4\0\0");
				of.lpstrFile = szToPath;
				of.nMaxFile = MAX_PATH;
				of.Flags = OFN_OVERWRITEPROMPT;
				of.lpstrDefExt = TEXT("mp4");
				of.lpstrTitle = TEXT("動画ファイルの保存");
				if (GetSaveFileName(&of) != 0)
				{
					TCHAR szFromPath[MAX_PATH] = { 0 };
					lstrcpy(szFromPath, szTempDir);
					PathAppend(szFromPath, lpszCaptureFileName);
					MoveFileEx(szFromPath, szToPath, MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
				}
			}
			{
				TCHAR szDeleteFolder[MAX_PATH] = { 0 };
				lstrcpy(szDeleteFolder, szTempDir);
				SHFILEOPSTRUCT FileOp = { hWnd,FO_DELETE,szDeleteFolder,0,FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,0,0,0 };
				SHFileOperation(&FileOp);
			}
			list.clear();
			EnableWindow(hButton1, TRUE);
		}
		break;
	case WM_TIMER:
		KillTimer(hWnd, 0x1234);
		{
			Bitmap * pBitmap = SnapShot(&clsid);
			if (pBitmap)
			{
				list.push_back(pBitmap);
			}
		}
		if (bStopCapture == FALSE)
		{
			SetTimer(hWnd, 0x1234, 1, NULL);
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		bStopCapture = TRUE;
		KillTimer(hWnd, 0x1234);
		for (auto item : list)
		{
			delete item;
		}
		list.clear();
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		DLGWINDOWEXTRA,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	const DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT rect;
	SetRect(&rect, 0, 0, 276, 94);
	AdjustWindowRect(&rect, dwStyle, 0);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("画面キャプチャ―動画を撮る"),
		dwStyle,
		CW_USEDEFAULT,
		0,
		rect.right - rect.left,
		rect.bottom - rect.top,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (!IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;
}
