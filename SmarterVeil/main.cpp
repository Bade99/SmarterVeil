
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#define _CRT_NONSTDC_NO_DEPRECATE /*strnicmp*/
#pragma warning (disable: 4244) /*down & up conversion: possible loss of data*/
#pragma warning (disable: 5105) /*something in microsoft's code (winbase.h)*/
#elif defined(__GNUC__) || defined(__GNUG__)
#error TODO
#else
#error Unknown Compiler
#endif

#include <filesystem>

#if defined(_DEBUG)
//Internal build, meant only for developers
//NOTE(fran): the whole idea to restart this project came from this article: https://docs.microsoft.com/en-us/archive/msdn-magazine/2014/june/windows-with-c-high-performance-window-layering-using-the-windows-composition-engine

#define DEBUG_BUILD
#elif defined(_ENDUSER)
//Build ready for final user
#define USER_BUILD
//#elif defined(_TESTER)
////Optimized build but meant for testing, with code the final product will not run
//#define TESTER_BUILD
#else
#error No build type defined
#endif
//TODO(fran): try to add the Tester Build again

#ifdef DEBUG_BUILD
#define assert(assertion) if(!(assertion))*(volatile int*)0=0
#else
#define assert(assertion) 
#endif

#define crash() *(volatile int*)0=0

#define nil nullptr

#define ArrayCount(arr) (sizeof(arr)/sizeof(*arr))

#define local_persistence static
#define global_persistence static

#define internal static
//NOTE(fran): compilation time is reduced _a lot_ by marking all functions as internal (static), since it's much less work for the linker, and probably for other reasons too. Here's proof, with _only_ 20 functions in my code, compiling with clang, doing a complete rebuild each time and twice to confirm the values:
//      no internal:
//          debug x64   : 3.56 sec & 3.41 sec
//          release x64 : 2.48 sec & 2.89 sec
//      
//      using internal:
//          debug x64   : 2.56 sec & 2.64 sec
//          release x64 : 1.68 sec & 1.77 sec

#define TIMERSTART(name) auto name = StartCounter()
#define TIMEREND(name) EndCounter(name)

#define TIMEDBLOCK(blockname) \
    auto __timer = StartCounter(); \
    defer{ \
        char __printelapsed[128]; \
        snprintf(__printelapsed, ArrayCount(__printelapsed), "%s{}: %.2fms\n", #blockname, (f32)EndCounter(__timer)); \
        OutputDebugStringA(__printelapsed); \
    }

#define TIMEDFUNCTION() \
    auto __timer = StartCounter(); \
    auto __funcname = __func__; \
    defer{ \
        char __printelapsed[128]; \
        snprintf(__printelapsed, ArrayCount(__printelapsed), "%s(): %.2fms\n", __funcname, (f32)EndCounter(__timer)); \
        OutputDebugStringA(__printelapsed); \
    }

//template <typename T> struct add_pointer { T* t; using type = decltype(t); };

//NOTE: all this template crap is needed in order to be able to use defer
//INFO: visual studio gives you most of the c++ libs by default in debug builds, and those things are stripped down only for release ones, in which case you gotta manually include the lib
template <typename T> struct remove_reference { using type = T; };
template <typename T> struct remove_reference<T&> { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };
template <typename> inline constexpr bool is_lvalue_reference_v = false;
template <typename T> inline constexpr bool is_lvalue_reference_v<T&> = true;
template <typename T>
[[nodiscard]] constexpr T&& forward( typename remove_reference<T>::type & arg) noexcept {
    return static_cast<T&&>(arg); /*forward an lvalue as either an lvalue or an rvalue*/
}
template <typename T>
[[nodiscard]] constexpr T&& forward(typename remove_reference<T>::type && arg) noexcept {
    static_assert(!is_lvalue_reference_v<T>, "bad forward call");
    return static_cast<T&&>(arg); /*forward an rvalue as an rvalue*/
}
//#include <type_traits> //std::forward
//Thanks to https://handmade.network/forums/t/1273-post_your_c_c++_macro_tricks/3
template <typename F> struct Defer { Defer(F f) : f(f) {} ~Defer() { f(); } F f; };
template <typename F> Defer<F> makeDefer(F f) { return Defer<F>(f); };
#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )
struct defer_dummy { };
template<typename F> Defer<F> operator+(defer_dummy, F && f) { return makeDefer<F>(forward<F>(f)); }
//usage: defer{block of code;}; //the last defer in a scope gets executed first (LIFO)
#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )

//TODO(fran): choose a real name (ideas: Darkness, )
#define _appname SmarterVeil
#define __appname8(name) u8##name
#define ___appname8(name) __appname8(#name)
#define ____appname8(name) ___appname8(name)
#define appname ____appname8(_appname)

#include "basic_types.h"
#include "basic_memory.h"
#include "basic_string.h"
#include "win32_sdk.h"
#include "resource.h"

#define WM_GETTOPMARGIN (WM_USER+1)
internal f32 GetWindowTopMargin(HWND wnd) //TODO(fran): OS code
{
    f32 res = (f32)(i32)SendMessage(wnd, WM_GETTOPMARGIN, 0, 0);
    return res;
}

#include "basic_timing.h"
#include "basic_math.h"
#include "OS.h"
#include "lang.h"
#include "veil.h"

LRESULT CALLBACK VeilProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //auto _msgName = msgToString(message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        case WM_DISPLAYCHANGE://indicates change in the display resolution
        {
            // Make sure the Veil window remains maximized in the new display
            ShowWindow(hWnd, SW_MAXIMIZE);
            break;
        }
        //TODO(fran): WM_DEVICECHANGE apparently tells you when a new monitor has been installed, we should maximize in that case as well
        /*case WM_SIZE:
        {
            PostThreadMessageW(RenderThreadID, Message, WParam, LParam);
        } break;*/
        default:
        {
            return DefWindowProcW(hWnd, message, wParam, lParam);
        } break;
    }
    return 0;
}

internal bool win32_test_pt_rc(POINT p, RECT r) {
    bool res = false;
    if (p.y >= r.top && p.y < r.bottom && p.x >= r.left && p.x < r.right) res = true;
    return res;
}

LRESULT CALLBACK VeilUIProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //auto _msgName = msgToString(message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");

    //TODO(fran): do not allow the window to get smaller than a certain threshold, currently the user can make the window be almost 0 width & 0 height

    local_persistence i32 top_margin = 6;
#if 0
    local_persistence u32 last_hittest;
    local_persistence POINT mouse_pivot; //in screen coordinates
    local_persistence RECT rect_pivot; //in screen coordinates
#endif
    switch (message)
    {
        //IMPORTANT(fran): on WM_NCLBUTTONDOWN:
        // Rant: when resizing the window with the mouse this a$hole sends WM_SYSCOMMAND with SC_MOVE which triggers the stupid hooking into your thread (and very politely notifies about it with wm_entersizemove), this is stupid, and probably just old code that no one ever got to fix. Probably they originally decided to hook your thread and call your wndproc straight up for performance concerns but it now means that you can either handle all non client functions yourself or create a different thread that wont get hooked by windows and simply resend any important messages to there
        // Problem: your execution loop gets stopped and a Windows hook takes over your thread and straight calls the wndprocs like this one, you cannot execute code and get completely and unavoidably locked until the user stops resizing the window attached to this wndproc, this means that the contents of your window get horribly stretched without updating
        // Solution: escape from Windows completely, run all your code in another thread. Only create the windows from the main thread so their wndprocs get linked to that one and that's the one that stalls
        // No, dont try it: do _not_ try to handle WM_NCLBUTTONDOWN by yourself, it's not hard (except for the autoresizing preview animation that happens when touching screen borders, which idk how to do, nor did I bother to) but it's pointless wasted work. If Windows, in all its wisdom decided the best way to handle window resizing was by taking over the user's thread then hats off to them, we're leaving to another thread.
        // Another solution: if anyone has another solution that does not require moving to another thread please let me know
#if 0 //NOTE(fran): attempt to handle window resizing
    case WM_NCLBUTTONDOWN:
    {
        if (wParam == HTLEFT || wParam == HTRIGHT || wParam == HTTOP || wParam == HTTOPLEFT || wParam == HTTOPRIGHT || wParam == HTBOTTOM || wParam == HTBOTTOMLEFT || wParam == HTBOTTOMRIGHT)
        {
            mouse_pivot = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            GetWindowRect(hWnd, &rect_pivot);
            last_hittest = wParam;
            SetCapture(hWnd);
            return 0;
        }
        else return DefWindowProcW(hWnd, message, wParam, lParam);
    } break;
    case WM_MOUSEMOVE:
    {
        if (GetCapture() == hWnd) 
        {
            POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            MapWindowPoints(hWnd, HWND_DESKTOP, &mouse, 1);
            switch (last_hittest)
            {
                case HTLEFT:
                case HTRIGHT:
                case HTTOP:
                case HTTOPLEFT:
                case HTTOPRIGHT:
                case HTBOTTOM:
                case HTBOTTOMLEFT:
                {
                    crash();
                } break;
                case HTBOTTOMRIGHT:
                {
                    SetWindowPos( hWnd, nil, 0, 0, RECTW(rect_pivot) + mouse.x - mouse_pivot.x, RECTH(rect_pivot) + mouse.y - mouse_pivot.y, SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);
                } break;
            }
            return 0;
        }
        else return DefWindowProcW(hWnd, message, wParam, lParam);
    } break;
    case WM_LBUTTONUP:
    {
        if (GetCapture() == hWnd)
            ReleaseCapture();
    } break;
#endif
    case WM_NCLBUTTONDOWN:
    {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    } break;
    case WM_NCCALCSIZE: //TODO(fran): for some magical reason the veil wnd can automatically work with WS_THICKBORDER without all this crap, but the border is transparent and exists outside the window on _all_ sides, which is not really preferable for the top border, but much less painful to program. Therefore at least find out what this window lacks in order to work too, for that continue with the UICOMPOSITOR macro (I feel like we need to set the D3DDevice with _premultiplied alpha_)
    {
        if ((BOOL)wParam){
            auto res = DefWindowProcW(hWnd, message, wParam, lParam);
            NCCALCSIZE_PARAMS* calcsz = (NCCALCSIZE_PARAMS*)lParam;

            RECT emptyrc{ 0 };
            AdjustWindowRectEx(&emptyrc, GetWindowLongPtrW(hWnd, GWL_STYLE), false, 0);
            top_margin = RECTH(emptyrc) / 2;

            RECT winrc; GetWindowRect(hWnd, &winrc);
            RECT clirc; GetClientRect(hWnd, &clirc);

            calcsz->rgrc[0].top -= top_margin;
            return res;
        }
        else return DefWindowProcW(hWnd, message, wParam, lParam);
    } break;
    case WM_NCHITTEST:
    {
        auto res = DefWindowProcW(hWnd, message, wParam, lParam);
        if (res == HTCLIENT)
        {
            RECT caption_rc{0};
            AdjustWindowRectEx(&caption_rc, WS_OVERLAPPEDWINDOW, false, 0);

            i32 top_side_margin = absolute_value(caption_rc.top);
            POINT mouse{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; ScreenToClient(hWnd, &mouse);

            RECT rc; GetClientRect(hWnd, &rc);
            RECT top_border = { 0, 0, RECTW(rc), top_margin };
            RECT topleft_border = top_border; topleft_border.right = top_side_margin;
            RECT topright_border = top_border; topright_border.left = RECTW(rc) - top_side_margin;

            if (win32_test_pt_rc(mouse, topleft_border)) res = HTTOPLEFT;
            else if (win32_test_pt_rc(mouse, topright_border)) res = HTTOPRIGHT;
            else if (win32_test_pt_rc(mouse, top_border)) res = HTTOP;

            //TODO(fran): this solution also has another small problem, hittesting for buttons that overlap with the border, what chrome does for example is to give the buttons precedence over the border and provide a 1 pixel gap on top of the buttons where border hittesting will trigger. We cant really do any of that since there's no connection between windows' UI system and ours, therefore both inputs will happen
        }
        return res;
    } break;
    case WM_CLOSE:
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    case WM_GETTOPMARGIN:
    {
        return top_margin;
    } break;
    case WM_TRAY:
    {
        //NOTE(fran): Windows strikes again! the tray message is sent straight to the proc bypassing the message queue
        PostMessageW(hWnd, message, wParam, lParam);
    } break;
    default:
    {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    } break;
    }
    return 0;
}

internal HWND CreateVeilWindow()
{
    HINSTANCE instance = GetModuleHandleW(nil);

    WNDCLASSEXW wc{0};
    wc.cbSize = sizeof(wc);
    //w.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &VeilProc;
    wc.hInstance = instance;
    i32 SmallIconX = GetSystemMetrics(SM_CXSMICON);//TODO(fran): GetSystemMetricsForDpi?
    i32 LargeIconX = GetSystemMetrics(SM_CXICON);
    wc.hIcon = (HICON)LoadImageW(instance,MAKEINTRESOURCE(ICO_LOGO),IMAGE_ICON,LargeIconX,LargeIconX,0);
    wc.hIconSm = (HICON)LoadImageW(instance,MAKEINTRESOURCE(ICO_LOGO),IMAGE_ICON,SmallIconX,SmallIconX,0);
    wc.hCursor = LoadCursorW(nil, IDC_ARROW);
    wc.lpszClassName = L"veil_class";

    HWND wnd = nil;
    if (RegisterClassExW(&wc))
    {
        u32 ClickThroughFlags = /*WS_EX_COMPOSITED |*/ WS_EX_LAYERED /*| WS_EX_TRANSPARENT*/;
        wnd = CreateWindowExW(
            ClickThroughFlags | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            wc.lpszClassName,
            L"Veil",
#define VEILCOMPOSITORTEST 0
#if VEILCOMPOSITORTEST 
            WS_POPUP | WS_THICKFRAME,
            50, 50, 500, 500,
#else
            WS_POPUP,
            0, 0, 0, 0,
#endif
            0, 0, 0, 0);
        //NOTE(fran): apparently WS_EX_NOREDIRECTIONBITMAP is not necessary for the veil to work with D3D, at least on Windows 10. It's a different question whether performance will be afected by removing it
        //TODO(fran): look for another way to make the window click through without having to use the ClickThroughFlags (I feel like WS_EX_LAYERED is gonna destroy our performance somehow) (apparently WS_EX_LAYERED is the only flag needed to make the window click through, I have no clue if adding WS_EX_TRANSPARENT has any performance benefit or otherwise and WS_EX_COMPOSITED probably takes performance down)

        //TODO(fran): try WS_POPUP for creating a pain free non_client window for this and my other windows projects

#if VEILCOMPOSITORTEST 
        ShowWindow(wnd, SW_SHOW);
#else
        ShowWindow(wnd, SW_MAXIMIZE);
#endif
        UpdateWindow(wnd);
    }

    return wnd;
}

internal void SetWindowsDPIAware()
{
    typedef BOOL WINAPI set_process_dpi_aware(void);
    typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);

    HMODULE WinUser = LoadLibraryW(L"user32.dll");
    set_process_dpi_awareness_context* SetProcessDPIAwarenessContext = (set_process_dpi_awareness_context*)GetProcAddress(WinUser, "SetProcessDPIAwarenessContext");
    if (SetProcessDPIAwarenessContext)
    {
        SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    }
    else
    {
        set_process_dpi_aware* SetProcessDPIAware = (set_process_dpi_aware*)GetProcAddress(WinUser, "SetProcessDPIAware");
        if (SetProcessDPIAware)
        {
            SetProcessDPIAware();
        }
    }
    //TODO(fran): there's also a Windows 8 specific function
}

internal HWND CreateVeilUIWindow()
{
    HINSTANCE instance = GetModuleHandleW(nil);

    WNDCLASSEXW wc{ 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;// CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &VeilUIProc;
    wc.hInstance = instance;
    i32 SmallIconX = GetSystemMetrics(SM_CXSMICON);//TODO(fran): GetSystemMetricsForDpi?
    i32 LargeIconX = GetSystemMetrics(SM_CXICON);
    wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, LargeIconX, LargeIconX, 0);
    wc.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, SmallIconX, SmallIconX, 0);
    wc.hCursor = LoadCursorW(nil, IDC_ARROW);
    wc.lpszClassName = L"veil_ui_class";

    HWND wnd = nil;
    if (RegisterClassExW(&wc))
    {
        wnd = CreateWindowExW(
#ifdef DEBUG_BUILD
            0
#else
            WS_EX_TOPMOST
#endif
            | WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            wc.lpszClassName,
            L"SmarterVeil",
            WS_POPUP | WS_THICKFRAME |
            WS_MAXIMIZEBOX, //SUPER IMPORTANT NOTE(fran): WS_MAXIMIZEBOX is _required_ for the aero window auto-resizing when touching screen borders
            0, 0, 0, 0,
            nil, nil, nil, nil);

        UpdateWindow(wnd);
    }
    
    return wnd;
}

internal DWORD WINAPI _VeilProcessing(LPVOID param) {
    veil_start_data* start_data = (veil_start_data*)param;
    VeilProcessing(start_data);
    ExitProcess(0);
    return 0;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    SetWindowsDPIAware(); //TODO(fran): doesnt seem to be working correctly

    HWND veil = CreateVeilWindow();
    if (!veil) return 0;

    HWND veil_ui = CreateVeilUIWindow();
    if (!veil_ui) return 0;

    veil_start_data start_data;
    start_data.veil_wnd = veil;
    start_data.veil_ui_wnd = veil_ui;
    start_data.main_thread_id = GetCurrentThreadId();//TODO(fran): OS code


#if 0 // Single Thread Version
    VeilProcessing(veil, &start_data);
#else // Separate Thread Version (because of the resizing problem, refer to VeilUIProc's WM_NCLBUTTONDOWN, for info on it)
    DWORD ProcessAndRenderThreadID = 0;
    HANDLE thread_res = CreateThread(nil, 0, _VeilProcessing, &start_data, 0, &ProcessAndRenderThreadID);
    assert(thread_res);

    while (true) //TODO(fran): can a crash on the other thread leave us with an infinite loop here?
    {
        MSG msg;
        GetMessageW(&msg, 0, 0, 0);
        TranslateMessage(&msg);

        //auto _msgName = msgToString(msg.message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");

        //TODO(fran): we know that sometimes Windows bypasses the msg queue and goes straight to the wndproc, in those cases we would not have to option to PostThreadMessageW from here, that'd need doing it from inside the wndproc. See if there's anything that needs us doing that

        switch (msg.message) //IMPORTANT TODO(fran): handle the close button message that you get when closed from the statusbar or from the Alt+Tab tab view
        {
            //case WM_CHAR:
            //case WM_KEYDOWN:
            case WM_QUIT:
            //case WM_SIZE:
            case WM_DPICHANGED:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            case WM_MOUSEWHEEL:
            //TODO(fran): case WM_MOUSEHWHEEL:
            //case WM_HOTKEY: //NOTE(fran): it is sent straight to the other thread by Windows
            {
                PostThreadMessageW(ProcessAndRenderThreadID, msg.message, msg.wParam, msg.lParam);
            } break;
            case WM_TRAY:
            {
                PostThreadMessageW(ProcessAndRenderThreadID, msg.message, msg.wParam, msg.lParam);
                continue; //NOTE(fran): we stop the msg from going back to the wndproc and generating an infinite loop. Look at WM_TRAY inside VeilUIProc for more info
            } break;
            //case WM_NCLBUTTONDOWN: { crash(); } break;
        }

        DispatchMessageW(&msg); //TODO(fran): maybe some msgs should not be dispatched to the wndproc, like WM_QUIT?
    }
#endif

    return 0;
}
