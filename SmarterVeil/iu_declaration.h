#pragma once

namespace iu{

enum ui_key : u8{ //TODO(fran): convert to namespace + annonymous enum so this names dont leak into the codebase, or enum class if possible
    MouseLeft /*Left Click*/, MouseRight /*Right Click*/, MouseMiddle /*Scrollwheel/Middlebutton Click*/, MouseExtra1, MouseExtra2 /*Extra mouse buttons, usually used for forwards/backwards navigation (eg in a browser)*/,
    Esc,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z, //TODO(fran): Ñ
    _0,_1,_2,_3,_4,_5,_6,_7,_8,_9,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9, //TODO(fran): why would the user care about whether it is a normal or numpad number other than for hotkeys? remove it?
    NumpadMultiply, NumpadAdd, NumpadSubtract, NumpadDecimal /*, or . depending on current language*/, NumpadDivide, NumpadEnter, //TODO(fran): Mac: NumpadClear. They have a "clear" key
    Ctrl, Shift, Alt /*Mac: Option*/, OS /*Win: Windows Key, Mac: Command Key*/,
    Tab,
    CapsLock, NumLock, ScrollLock, //TODO(fran): do we want to tell the ui about this ones?
    UpArrow, DownArrow, LeftArrow, RightArrow,
    Apostrophe /*'*/, Comma /*,*/, Period /*.*/, Semicolon /*;*/, Minus /*-*/, Equal /*=*/, LeftBracket /*[*/, RightBracket /*]*/, Backslash /*\*/, Forwardslash /*/*/, GraveAccent /*`*/, LessThan /*<*/, //NOTE(fran): the values in this line are for a standard US keyboard, their real representation will vary by the current OS language
    PageUp, PageDown, Home, End, Insert /*TODO(fran): InsertLock instead?*/, 
    DeleteForward /*Delete next character*/, DeleteBackward /*Backspace*/ /*Delete previous character*/,
    Space, Enter,
    PrintScreen,
    ContextMenu /*Windows: acts as a right click, requesting to open the context menu*/,
    VolumeUp, VolumeDown, VolumeMute,
    MediaNextTrack, MediaPrevTrack, MediaPlayPause, MediaStop,

    _COUNT,
};

enum class ui_key_state : u8 {
    //NOTE(fran): all the states are mutually exclusive
    doubleclicked = (1 << 0),
    clicked = (1 << 1),
    pressed = (1 << 2),
    unclicked = (1 << 3),
    //TODO(fran): analyze times between clicks (or click unclick) to see if it's worth it to register multiple clicks per run
    //TODO(fran): Windows: check out rawinput (for gamedev)

    //IMPORTANT(fran): OS's handling of double click: 
        //Windows: we recieve a click message for the first click, and a doubleclick msg for the second click, therefore we should interpret 'doubleclicked' also as 'clicked' for the people that dont handle doubleclick

    //TODO(fran): we could handle this on our side, have the ui code interpret very close clicks as a double click, that'd be easier to port to other OSs that may not do the same as Windows
};

enum class ui_event_type {
    MouseP,
    MouseWheel,
    MouseButton,
    Tray,
    Dpi,
    SystemGlobalHotkey,
    Close,
    Key,
    Text,
};

struct ui_event {
    ui_event_type type;
    OS::window_handle destination;
    union {
        struct { v2 p; v2 screenP; } mousep; //TODO(fran): some day we'll need to handle multi-touch
        struct { v2 wheel; } mousewheel;
        struct { ui_key button; ui_key_state state; } mousebutton, tray, key; //TODO(fran): the tray event should store the screenmouse position to allow for opening context menues from there //TODO(fran): analogue key for controllers. The mouse should also be analogue, in case a pen is used
        struct { utf8 c; } text; //TODO(fran): we may wanna make this a little bigger, eg utf8 c[8], in order to reduce the number of events in the queue
        struct { f32 dpi; } dpi; //TODO(fran): remove
        struct { u8 _; } close;
        struct { i32 id; } systemwidehotkey;
    };
};

declaration void PushEventMousePos(OS::window_handle wnd, v2 mouseP, v2 mouseScreenP);
declaration void PushEventMouseWheel(OS::window_handle wnd, v2 wheelStep);
declaration void PushEventMouseButton(OS::window_handle wnd, ui_key mousebutton, ui_key_state state);
declaration void PushEventTray(OS::window_handle wnd, ui_key mousebutton, ui_key_state state);
declaration void PushEventKey(OS::window_handle wnd, ui_key key, ui_key_state state);
declaration void PushEventText(OS::window_handle wnd, utf8 c);
declaration void PushEventDpi(OS::window_handle wnd, f32 dpi /*a dpi of 1.0 means no scaling (100%)*/);
declaration void PushEventClose(OS::window_handle wnd);
declaration void PushEventSystemWideHotkey(OS::window_handle wnd, i32 hotkey_id);

}