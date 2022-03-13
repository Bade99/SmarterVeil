#pragma once

/// <summary>
/// Check to see if animation has been disabled in Windows
/// </summary>
/// <returns></returns>
internal BOOL GetDoAnimateMinimize()
{
	//Reference: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
	ANIMATIONINFO ai;

	ai.cbSize = sizeof(ai);
	SystemParametersInfo(SPI_GETANIMATION, sizeof(ai), &ai, 0);

	return ai.iMinAnimate ? TRUE : FALSE;
}

/// <summary>
/// Returns the rect of where we think the system tray is.
/// <para>If it can't find it, it returns a rect in the lower</para>
/// <para>right hand corner of the screen</para>
/// </summary>
/// <param name="lpTrayRect"></param>
internal void GetTrayWndRect(LPRECT lpTrayRect)
{
	//Reference: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
	// First, we'll use a quick hack method. We know that the taskbar is a window
	// of class Shell_TrayWnd, and the status tray is a child of this of class
	// TrayNotifyWnd. This provides us a window rect to minimize to. Note, however,
	// that this is not guaranteed to work on future versions of the shell. If we
	// use this method, make sure we have a backup!
	HWND hShellTrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);
	if (hShellTrayWnd)
	{
		HWND hTrayNotifyWnd = FindWindowEx(hShellTrayWnd, NULL, TEXT("TrayNotifyWnd"), NULL);
		if (hTrayNotifyWnd)
		{
			GetWindowRect(hTrayNotifyWnd, lpTrayRect);
			return;
		}
	}

	// OK, we failed to get the rect from the quick hack. Either explorer isn't
	// running or it's a new version of the shell with the window class names
	// changed (how dare Microsoft change these undocumented class names!) So, we
	// try to find out what side of the screen the taskbar is connected to. We
	// know that the system tray is either on the right or the bottom of the
	// taskbar, so we can make a good guess at where to minimize to
	APPBARDATA appBarData;
	appBarData.cbSize = sizeof(appBarData);
	if (SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData))
	{
		// We know the edge the taskbar is connected to, so guess the rect of the
		// system tray. Use various fudge factor to make it look good
		switch (appBarData.uEdge)
		{
		case ABE_LEFT:
		case ABE_RIGHT:
			// We want to minimize to the bottom of the taskbar
			lpTrayRect->top = appBarData.rc.bottom - 100;
			lpTrayRect->bottom = appBarData.rc.bottom - 16;
			lpTrayRect->left = appBarData.rc.left;
			lpTrayRect->right = appBarData.rc.right;
			break;

		case ABE_TOP:
		case ABE_BOTTOM:
			// We want to minimize to the right of the taskbar
			lpTrayRect->top = appBarData.rc.top;
			lpTrayRect->bottom = appBarData.rc.bottom;
			lpTrayRect->left = appBarData.rc.right - 100;
			lpTrayRect->right = appBarData.rc.right - 16;
			break;
		}

		return;
	}

	// Blimey, we really aren't in luck. It's possible that a third party shell
	// is running instead of explorer. This shell might provide support for the
	// system tray, by providing a Shell_TrayWnd window (which receives the
	// messages for the icons) So, look for a Shell_TrayWnd window and work out
	// the rect from that. Remember that explorer's taskbar is the Shell_TrayWnd,
	// and stretches either the width or the height of the screen. We can't rely
	// on the 3rd party shell's Shell_TrayWnd doing the same, in fact, we can't
	// rely on it being any size. The best we can do is just blindly use the
	// window rect, perhaps limiting the width and height to, say 150 square.
	// Note that if the 3rd party shell supports the same configuraion as
	// explorer (the icons hosted in NotifyTrayWnd, which is a child window of
	// Shell_TrayWnd), we would already have caught it above
	hShellTrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);

#define DEFAULT_RECT_WIDTH 150
#define DEFAULT_RECT_HEIGHT 30

	if (hShellTrayWnd)
	{
		GetWindowRect(hShellTrayWnd, lpTrayRect);
		if (lpTrayRect->right - lpTrayRect->left > DEFAULT_RECT_WIDTH)
			lpTrayRect->left = lpTrayRect->right - DEFAULT_RECT_WIDTH;
		if (lpTrayRect->bottom - lpTrayRect->top > DEFAULT_RECT_HEIGHT)
			lpTrayRect->top = lpTrayRect->bottom - DEFAULT_RECT_HEIGHT;

		return;
	}

	// OK. Haven't found a thing. Provide a default rect based on the current work
	// area
	SystemParametersInfo(SPI_GETWORKAREA, 0, lpTrayRect, 0);
	lpTrayRect->left = lpTrayRect->right - DEFAULT_RECT_WIDTH;
	lpTrayRect->top = lpTrayRect->bottom - DEFAULT_RECT_HEIGHT;

#undef DEFAULT_RECT_WIDTH
#undef DEFAULT_RECT_HEIGHT
}

/// <summary>
/// Animates a window going to the tray and hides it / Animates a window coming out of the tray and shows it
/// <para>Does not check whether the window is already minimized</para>
/// <para>The window position is left right where it started, so you can later use GetWindowRect to determine the "to" in TrayToWindow</para>
/// </summary>
internal void WindowToFromTray(HWND hWnd, POINT from, POINT to, int duration_milliseconds, BOOL to_tray) 
{
	float frametime = 1000.f / GetRefreshRateHz(hWnd);
	float frames = duration_milliseconds / frametime;
	RECT rc = { from.x,from.y,to.x,to.y };
	SIZE offset = { (LONG)(RECTW(rc) / frames), (LONG)(RECTH(rc) / frames) }; //TODO(fran): should start faster and end slower, NOT be constant
	POINT sign = { (from.x >= to.x) ? -1 : 1, (from.y >= to.y) ? -1 : 1 };
	float alphaChange = 100 / frames;

	// Set WS_EX_LAYERED on this window 
	SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	if (!to_tray) //We put the window at 0 alpha then we make the window visible 
	{
		SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);
		ShowWindow(hWnd, SW_SHOW);
	}

	for (int i = 1; i <= frames; i++) {
		SetLayeredWindowAttributes(hWnd, NULL, (BYTE)((255 * (to_tray?(100 - alphaChange * i): (alphaChange * i))) / 100), LWA_ALPHA);
		SetWindowPos(hWnd, NULL, from.x + sign.x * offset.cx * i, from.y + sign.y * offset.cy * i, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOSIZE);
		Sleep((int)frametime);//good enough
	}

	if (to_tray)
	{
		SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);
		SetWindowPos(hWnd, NULL, from.x, from.y, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOSIZE);
		ShowWindow(hWnd, SW_HIDE);
		SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) ^ WS_EX_LAYERED);
		//TODO(fran): restore to the original style, it may be the window was originally layered (NOTE(fran): be careful, I think that ShowWindow(hWnd, SW_HIDE) modifies the style also)
	}
	else
	{
		SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA); //just to make sure
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) ^ WS_EX_LAYERED);
		SetWindowPos(hWnd, NULL, to.x, to.y, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOSIZE);//just to make sure, dont tell it to redraw cause it takes a long time to do it and looks bad

		//TODO(fran): restore to the original style, it may be the window was originally layered

		SetActiveWindow(hWnd);
		SetForegroundWindow(hWnd);
	}
}

/// <summary>
/// Minimizes a window and creates an animation to make it look like it goes to the tray
/// </summary>
/// <param name="hWnd"></param>
internal void MinimizeWndToTray(HWND hWnd)
{//Thanks to: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
	if (GetDoAnimateMinimize())
	{
		RECT rcFrom, rcTo;

		// Get the rect of the window. It is safe to use the rect of the whole
		// window - DrawAnimatedRects will only draw the caption
		GetWindowRect(hWnd, &rcFrom);
		GetTrayWndRect(&rcTo);

		WindowToFromTray(hWnd, { rcFrom.left,rcFrom.top }, { rcTo.left,rcTo.top }, 200, true);
	}
	else ShowWindow(hWnd, SW_HIDE);// Just hide the window
}

/// <summary>
/// Restores a window and makes it look like it comes out of the tray 
/// <para>and makes it back to where it was before minimizing</para>
/// </summary>
/// <param name="hWnd"></param>
internal void RestoreWndFromTray(HWND hWnd)
{//Thanks to: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
	if (GetDoAnimateMinimize())
	{
		// Get the rect of the tray and the window. Note that the window rect
		// is still valid even though the window is hidden
		RECT rcFrom, rcTo;
		GetTrayWndRect(&rcFrom);
		GetWindowRect(hWnd, &rcTo);

		// Get the system to draw our animation for us
		WindowToFromTray(hWnd, { rcFrom.left,rcFrom.top }, { rcTo.left,rcTo.top }, 200, false);
	}
	else {
		// Show the window, and make sure we're the foreground window
		ShowWindow(hWnd, SW_SHOW);
		SetActiveWindow(hWnd);
		SetForegroundWindow(hWnd);
	}
}