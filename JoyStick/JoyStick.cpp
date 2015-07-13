//
// Logitech Extreme3D Pro Joystick Monitor program
//
//

#include "stdafx.h"
#include "JoyStick.h"
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <hidsdi.h>

#pragma comment(lib, "hid.lib")


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);


#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define WC_MAINFRAME	TEXT("MainFrame")
#define MAX_BUTTONS		128
#define CHECK(exp)		{ if(!(exp)) goto Error; }
#define SAFE_FREE(p)	{ if(p) { HeapFree(hHeap, 0, p); (p) = NULL; } }

//
// Global variables
//
BOOL bButtonStates[MAX_BUTTONS];
LONG lAxisX;
LONG lAxisY;
LONG lAxisZ;
LONG lAxisRz;
LONG lAxisH;
LONG lHat;
INT  g_NumberOfButtons;


void ParseRawInput(PRAWINPUT pRawInput)
{
	PHIDP_PREPARSED_DATA pPreparsedData = NULL;
	HIDP_CAPS            Caps;
	PHIDP_BUTTON_CAPS    pButtonCaps = NULL;
	PHIDP_BUTTON_CAPS    pButtonCaps2 = NULL;
	PHIDP_VALUE_CAPS     pValueCaps = NULL;
	USHORT               capsLength;
	UINT                 bufferSize;
	HANDLE               hHeap = GetProcessHeap();
	USAGE                usage[MAX_BUTTONS];
	ULONG                i, usageLength, value;

	//
	// Get the preparsed data block
	//

	CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0);
	CHECK(pPreparsedData = (PHIDP_PREPARSED_DATA)HeapAlloc(hHeap, 0, bufferSize));
	CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);

	//
	// Get the joystick's capabilities
	//

	// Button caps
	CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS)
		CHECK(pButtonCaps = (PHIDP_BUTTON_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps));

	capsLength = Caps.NumberInputButtonCaps;
	CHECK(HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS);

	g_NumberOfButtons = 0;
	for (int i = 0; i < capsLength; ++i)
	{ 
		PHIDP_BUTTON_CAPS bcaps = &pButtonCaps[i];
		g_NumberOfButtons += bcaps->Range.UsageMax - bcaps->Range.UsageMin + 1;
	}


	// Value caps
	CHECK(pValueCaps = (PHIDP_VALUE_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps));
	capsLength = Caps.NumberInputValueCaps;
	CHECK(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS)

		//
		// Get the pressed buttons
		//
		usageLength = g_NumberOfButtons;
	CHECK(
		HidP_GetUsages(
		HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
		(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
		) == HIDP_STATUS_SUCCESS);

	ZeroMemory(bButtonStates, sizeof(bButtonStates));
	for (i = 0; i < usageLength; i++)
		bButtonStates[usage[i] - pButtonCaps->Range.UsageMin] = TRUE;

	//
	// Get the state of discrete-valued-controls
	//

	for (i = 0; i < Caps.NumberInputValueCaps; i++)
	{
		CHECK(
			HidP_GetUsageValue(
			HidP_Input, pValueCaps[i].UsagePage, 0, pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
			(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
			) == HIDP_STATUS_SUCCESS);

		switch (pValueCaps[i].Range.UsageMin)
		{
		case 0x30:	// X-axis
			lAxisX = (LONG)value - 128;
			break;

		case 0x31:	// Y-axis
			lAxisY = (LONG)value - 128;
			break;

		case 0x32: // Z-axis
			lAxisZ = (LONG)value - 128;
			break;

		case 0x35: // Rotate-Z
			lAxisRz = (LONG)value - 128;
			break;

		case 0x36: // Heave
			lAxisH = (LONG)value - 128;
			break;

		case 0x39:	// Hat Switch
			lHat = value;
			break;
		}
	}

	//
	// Clean up
	//

Error:
	SAFE_FREE(pPreparsedData);
	SAFE_FREE(pButtonCaps);
	SAFE_FREE(pValueCaps);
}


void DrawButton(HDC hDC, int i, int x, int y, BOOL bPressed)
{
	HBRUSH hOldBrush, hBr;
	TCHAR  sz[4];
	RECT   rc;

	if (bPressed)
	{
		hBr = CreateSolidBrush(RGB(192, 0, 0));
		hOldBrush = (HBRUSH)SelectObject(hDC, hBr);
	}

	rc.left = x;
	rc.top = y;
	rc.right = x + 30;
	rc.bottom = y + 30;
	Ellipse(hDC, rc.left, rc.top, rc.right, rc.bottom);
	//_stprintf(sz, ARRAY_SIZE(sz), TEXT("%d"), i);
	swprintf_s(sz, TEXT("%d"), i);
	DrawText(hDC, sz, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

	if (bPressed)
	{
		SelectObject(hDC, hOldBrush);
		DeleteObject(hBr);
	}
}


void DrawCrosshair(HDC hDC, int x, int y, LONG xVal, LONG yVal)
{
	Rectangle(hDC, x, y, x + 256, y + 256);
	MoveToEx(hDC, x + xVal - 5 + 128, y + yVal + 128, NULL);
	LineTo(hDC, x + xVal + 5 + 128, y + yVal + 128);
	MoveToEx(hDC, x + xVal + 128, y + yVal - 5 + 128, NULL);
	LineTo(hDC, x + xVal + 128, y + yVal + 5 + 128);
}



// xVal Range = -128 ~ 895, center = 383
// yVal Range = -128 ~ 895, center = 383
void DrawCrosshair2(HDC hDC, int x, int y, LONG xVal, LONG yVal)
{
	// crossX Range = -512 ~ +512
	const int crossX = (xVal + 128) - 512;
	// scaleCrossX Range = -128 ~ +128
	const int scaleCrossX = crossX / 4;

	// crossY Range = -512 ~ +512
	const int crossY = (yVal + 128) - 512;
	// scaleCrossY Range = -128 ~ +128
	const int scaleCrossY = crossY / 4;


	Rectangle(hDC, x, y, x + 256, y + 256);	
	MoveToEx(hDC, x + scaleCrossX - 5 + 128, y + scaleCrossY + 128, NULL);
	LineTo(hDC, x + scaleCrossX + 5 + 128, y + scaleCrossY + 128);
	MoveToEx(hDC, x + scaleCrossX + 128, y + scaleCrossY - 5 + 128, NULL);
	LineTo(hDC, x + scaleCrossX + 128, y + scaleCrossY + 5 + 128);
}


// yVal: -128 ~ +128
void DrawLevel(HDC hDC, int x, int y, LONG yVal)
{
	const int crossY = yVal + 128;
	const int scaleCrossY = crossY;

	Rectangle(hDC, x, y, x + 60, y + 256);
	MoveToEx(hDC, x, y + scaleCrossY, NULL);
	LineTo(hDC, x+60, y + scaleCrossY);
	MoveToEx(hDC, x + 30, y + scaleCrossY - 5, NULL);
	LineTo(hDC, x + 30, y + scaleCrossY + 5);
}


void DrawDPad(HDC hDC, int x, int y, LONG value)
{
	LONG i;

	for (i = 0; i < 8; i++)
	{
		HBRUSH hOldBrush = NULL;
		HBRUSH hBr = NULL;
		int xPos = (int)(sin(-2 * M_PI * i / 8 + M_PI) * 80.0) + 80;
		int yPos = (int)(cos(2 * M_PI * i / 8 + M_PI) * 80.0) + 80;

		if (value == i)
		{
			hBr = CreateSolidBrush(RGB(192, 0, 0));
			hOldBrush = (HBRUSH)SelectObject(hDC, hBr);
		}

		Ellipse(hDC, x + xPos, y + yPos, x + xPos + 20, y + yPos + 20);

		if (value == i)
		{
			SelectObject(hDC, hOldBrush);
			DeleteObject(hBr);
		}
	}
}


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_JOYSTICK, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_JOYSTICK));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_JOYSTICK));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = NULL;// MAKEINTRESOURCE(IDC_JOYSTICK);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
	   CW_USEDEFAULT, CW_USEDEFAULT, 900, 500, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hDC;

	switch (message)
	{
	case WM_CREATE:
	{
		//
		// Register for joystick devices
		//
		RAWINPUTDEVICE rid;
		rid.usUsagePage = 1;
		rid.usUsage = 4;	// Joystick
		rid.dwFlags = 0;
		rid.hwndTarget = hWnd;
		if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
			return -1;
	}
	break;


	case WM_INPUT:
	{
		//
		// Get the pointer to the raw device data, process it and update the window
		//

		RAWINPUT RawInput;
		ZeroMemory(&RawInput, sizeof(RAWINPUT));
		UINT      bufferSize = sizeof(RawInput);

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &RawInput, &bufferSize, sizeof(RAWINPUTHEADER));
		ParseRawInput(&RawInput);

		// 시리얼통신을 주기적으로 보낸다.
		static int oldT = 0;
		int curT = GetTickCount();
		if (curT - oldT > 300)
		{
			char buff[5] = { 'S', (char)lAxisX, (char)lAxisY, (char)lAxisZ, (char)lAxisRz };
			oldT = curT;
		}
		//

		InvalidateRect(hWnd, NULL, TRUE);
		UpdateWindow(hWnd);
	}
	return 0;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
	{
		//
		// Draw the buttons and axis-values
		//
		hDC = BeginPaint(hWnd, &ps);
		SetBkMode(hDC, TRANSPARENT);

		for (int i = 0; i < g_NumberOfButtons; i++)
			DrawButton(hDC, i + 1, 20 + i * 40, 20, bButtonStates[i]);

		Rectangle(hDC, 10, 90, 10 + 622, 90 + 276);
		DrawCrosshair2(hDC, 20, 100, lAxisX, lAxisY);
		DrawCrosshair(hDC, 296, 100, lAxisZ, lAxisRz);
		DrawLevel(hDC, 560, 100, lAxisH);
		DrawDPad(hDC, 650, 140, lHat);

		EndPaint(hWnd, &ps);
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
