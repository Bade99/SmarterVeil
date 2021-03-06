
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define __cpp_lib_format  /*NOTE(fran): needed for std::format*/
#elif defined(_MSC_VER)
#define _CRT_NONSTDC_NO_DEPRECATE /*strnicmp*/
#pragma warning (disable: 4244) /*down & up conversion: possible loss of data*/
#pragma warning (disable: 5105) /*something in microsoft's code (winbase.h)*/
#elif defined(__GNUC__) || defined(__GNUG__)
#error TODO
#else
#error Unknown Compiler
#endif

#include <filesystem> //directory_iterator

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

#define definition 
#define declaration 

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

#if 0
global_persistence int _TIMED_tabcnt = 0;
#define TIMED_printtabs() for(int __i=0; __i < _TIMED_tabcnt; __i++) OutputDebugStringA("\t")

#define TIMEDBLOCK(blockname) \
    auto __timer = StartCounter(); \
    _TIMED_tabcnt++; \
    defer{ \
        char __printelapsed[128]; \
        snprintf(__printelapsed, ArrayCount(__printelapsed), "%s{}: %.2fms\n", #blockname, (f32)EndCounter(__timer)); \
        _TIMED_tabcnt--; \
        TIMED_printtabs(); \
        OutputDebugStringA(__printelapsed); \
        if (_TIMED_tabcnt == 0) OutputDebugStringA("----------------------\n"); \
    }

#define TIMEDFUNCTION() \
    auto __timer = StartCounter(); \
    auto __funcname = __func__; \
    _TIMED_tabcnt++; \
    defer{ \
        char __printelapsed[128]; \
        snprintf(__printelapsed, ArrayCount(__printelapsed), "%s(): %.2fms\n", __funcname, (f32)EndCounter(__timer)); \
        _TIMED_tabcnt--; \
        TIMED_printtabs(); \
        OutputDebugStringA(__printelapsed); \
        if (_TIMED_tabcnt == 0) OutputDebugStringA("----------------------\n"); \
    }
#else
#define TIMEDBLOCK(blockname) \
    auto __timer = StartCounter(); \
    defer{ \
        TIMETABLETEST->AddOrModify(timed_element{ .name = const_temp_s((utf8*)#blockname), .t = (f32)EndCounter(__timer), .cnt=1}); \
    }
#define TIMEDFUNCTION() \
    auto __timer = StartCounter(); \
    auto __funcname = __func__; \
    defer{ \
        TIMETABLETEST->AddOrModify(timed_element{ .name = const_temp_s((utf8*)__funcname), .t = (f32)EndCounter(__timer), .cnt=1}); \
    }
#endif

template <typename T> struct remove_const { using type = T; };
template <typename T> struct remove_const<const T> { using type = T; };

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
#define __appname(suffix,name) suffix##name
#define ___appname(suffix,name) __appname(suffix,#name)
#define ____appname(suffix,name) ___appname(suffix,name)

#define appname ____appname(u8,_appname)
#define appnameL ____appname(L,_appname)

#include <format>
#include "basic_types.h"
#include "basic_memory.h"
#include "basic_array.h"
#include "basic_string.h"
#include "basic_math.h"
#include "OS.h"
#include "basic_timing.h"
#include "basic_language.h"
#include "veil.h"

LRESULT CALLBACK VeilProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) //TODO(fran): it's pointless to have this, anything that's here should be moved to the ui code
{
    switch (message)
    {
        case WM_DISPLAYCHANGE://indicates change in the display resolution & refresh rate
        {
            // Make sure the Veil window remains maximized in the new display
            if (IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_MAXIMIZE);
            goto default_handling;
        } break;
        //TODO(fran): WM_DEVICECHANGE apparently tells you when a new monitor has been installed, we should maximize in that case as well
        default:
        {
        default_handling:
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
    wc.lpfnWndProc = &VeilProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"veil_class";

    HWND wnd = nil;
    if (RegisterClassExW(&wc))
    {
        u32 ClickThroughFlags = /*WS_EX_COMPOSITED |*/ WS_EX_LAYERED | WS_EX_TRANSPARENT; //IMPORTANT(fran): WS_EX_TRANSPARENT is required for clickthrough to work in all cases, for example Windows's Configuration app has problems with its titlebar if only WS_EX_LAYERED is used
        wnd = CreateWindowExW(
            ClickThroughFlags | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            wc.lpszClassName,
            L"Veil",
            WS_POPUP,
            0, 0, 0, 0,
            nil, nil, nil, nil);
        //NOTE(fran): apparently WS_EX_NOREDIRECTIONBITMAP is not necessary for the veil to work with D3D, at least on Windows 10. It's a different question whether performance will be afected by removing it
        //TODO(fran): look for another way to make the window click through without having to use the ClickThroughFlags (I feel like WS_EX_LAYERED is gonna destroy our performance somehow)

        ShowWindow(wnd, SW_MAXIMIZE); //TODO(fran): Veil code should take care of maximizing
        UpdateWindow(wnd);
    }

    return wnd;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    HWND veil = CreateVeilWindow();
    if (!veil) return 0;

    ui_state* veil_ui_wnd = iu::Window((OS::window_creation_flags)(OS::topmost | OS::resizeborder));
    if (!veil_ui_wnd->wnd) return 0;

    veil_start_data start_data;
    start_data.veil_wnd.hwnd = veil; //TODO(fran): create all windows from OS code
    start_data.veil_ui_base_state = veil_ui_wnd;

    VeilProcessing(&start_data);

    return 0;
}
