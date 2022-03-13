#pragma once

//----------------Windows SDK related----------------: (alters windows.h)
#define NTDDI_VERSION	0x0A000000	//Minimun windows 10 version //TODO(fran): I can go much lower, same with the others
#define _WIN32_WINNT	0x0A00		//Windows 10
#define WINVER			_WIN32_WINNT
#define _WIN32_IE		0x0500		//Internet Explorer aka who cares

#define WIN32_LEAN_AND_MEAN			//Exclude APIs such as Cryptography, DDE, RPC, Shell, and Windows Sockets
#define NOCOMM						//Excludes the serial communication API
#define NOATOM
//#define NOCTLMGR //Excludes things needed for CB_GETCURSEL and similar
//#define NOKERNEL
//#define NOUSER
#define NOMEMMGR
//#define NOMETAFILE //Excludes things Needed for gdi+
#define NOSERVICE
#define NOSOUND
//#define NOWH //Excludes TOOLINFO, TTM_UPDATETIPTEXT, ...
//#define NOWINOFFSETS //Excludes SetWindowLongPtr,...
#define NOHELP
#define NOPROFILER
#define NOMCX

#ifndef UNICODE 
#define UNICODE
#endif
#ifndef _UNICODE 
#define _UNICODE
#endif
//IMPORTANT NOTE: as an extra we will _only_ work with the Unicode versions of all functions (funcW), no pointless ASCII support, also some functions stopped accepting ASCII

//----------------Undocumented Windows----------------:
#define DCX_USESTYLE 0x00010000 /*Windows never disappoints with its crucial undocumented features, for WM_NCPAINT: HDC dc = GetDCEx(hwnd, 0, DCX_WINDOW | DCX_USESTYLE);*/

#define WM_UAHDESTROYWINDOW 0x90
#define WM_UAHDRAWMENU 0x91
#define WM_UAHDRAWMENUITEM 0x92
#define WM_UAHINITMENU 0x93
#define WM_UAHMEASUREMENUITEM 0x94
#define WM_UAHNCPAINTMENUPOPUP 0x95
#define WM_UAHUPDATE 0x96

#include <windows.h>
#include <windowsx.h>