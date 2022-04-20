﻿#pragma once

#define IU_RENDERER_DIRECTX11

#include "assets/embedded_images.h"

#include <initializer_list> 


//TODO(fran): see where/how to put this so it's accesible to the renderer
enum class horz_text_align {
    left = 0, center, right, justified
};

enum class vert_text_align {
    top = 0, center, bottom
};

#ifdef IU_RENDERER_DIRECTX11
#include "iu_renderer_directx11.h"
#endif

#define safe_call(funcptr, ...) if(funcptr) funcptr(__VA_ARGS__); else 0

template<typename T>
struct fixed_array_header {
    u64 cnt;
    T* arr;

    T* begin()
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    T* end()
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

    const T* begin() const
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    const T* end() const
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

};

template<typename T, u64 _cnt>
struct fixed_array { //TODO(fran): move to basic_array.h
    u64 cnt; //cnt in use / cnt_used
    T arr[_cnt];

    constexpr u64 cnt_allocd()
    {
        u64 res = ArrayCount(this->arr);
        return res;
    }

    T& operator[] (u64 idx)
    {
        return this->arr[idx];
    }

    const T& operator[] (u64 idx) const
    {
        return this->arr[idx];
    }

    void clear() { this->cnt = 0; zero_struct(this->arr); }

#if 0
    fixed_array<T, _cnt>& operator+=(const T e)//TODO(fran): see if this is a good idea
    {
        if (this->cnt + 1 > this->cnt_allocd()) crash();
        this->arr[this->cnt++] = e;
        return *this;
    }

    fixed_array<T, _cnt>& add(const T e)
    {
        return this += e;
    }
#endif

    fixed_array<T, _cnt>& add(T e)
    {
        if (this->cnt + 1 > this->cnt_allocd()) crash();
        this->arr[this->cnt++] = e;
        return *this;
    }

    fixed_array<T, _cnt>& remove_idx(u64 idx)
    {
        assert(idx < this->cnt);

        if (this->cnt) this->cnt -= 1;

        this[idx] = this[this->cnt];

        return this; //TODO(fran): we may want to return an iterator to the next element? aka this[idx]
    }

    fixed_array<T, _cnt>(std::initializer_list<T> elems)
    {
        this->cnt = 0;
        assert(elems.size() <= this->cnt_allocd());
        for (auto& e : elems) this->arr[this->cnt++] = e;
    }

    T* begin()
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    T* end()
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

    const T* begin() const
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    const T* end() const
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

    operator fixed_array_header<T>() { return {this->cnt, this->arr}; }
};

template<typename T>
struct fifo_queue {
    u64 cnt;
    u64 cnt_allocd;
    u64 current;
    T* arr;

    //NOTE(fran): this can be used multi-threaded as long as there's only 2 threads & one only pushes and the other one only pops, and non stalls horribly
        //TODO(fran): better multi-threading support, at least make sure this basic case always works

    void push(T elem)
    {
        this->arr[this->cnt] = elem;
        this->cnt = this->cnt + 1 >= this->cnt_allocd ? 0 : this->cnt + 1;
    }

    //TODO(fran): we may want to only return a pointer to the element
    struct pop_res { b32 found; T elem; };
    pop_res pop() //TODO(fran): we may only want to peek at all the elements without advancing the current 'pointer'
    {
        pop_res res;
        u64 current_cnt = *(volatile u64*)&this->cnt; //TODO(fran): force compiler to do a copy, maybe use volatile? though we only need a copy here, not in push()
        res.found = this->current != current_cnt;
        if (res.found)
        {
            res.elem = this->arr[this->current];
            this->current = this->current + 1 >= this->cnt_allocd ? 0 : this->current + 1;
        }
        return res;
    }

    b32 pop(T* res) //TODO(fran): we may only want to peek at all the elements without advancing the current 'pointer'
    {
        auto [found, elem] = this->pop();
        *res = elem;
        return found;
    }
};

enum class input_key_state : u8 {//NOTE(fran): all the states are mutually exclusive
    doubleclicked = (1 << 0), //NOTE(fran): double click is an annoying feature, we'll simply ignore it as much as we can for now
    clicked = (1 << 1),
    pressed = (1 << 2),
    unclicked = (1 << 3),
    //TODO(fran): analyze times between clicks (or click unclick) to see if it's worth it to register multiple clicks per run
    //TODO(fran): check out rawinput (for gamedev)

    //IMPORTANT(fran): OS's handling of double click: 
        //Windows: we recieve a click message for the first click, and a doubleclick msg for the second click, therefore we should interpret 'doubleclicked' also as 'clicked' for the people that dont handle doubleclick

    //TODO(fran): we could handle this on our side, have the ui code interpret very close clicks as a double click, that'd be easier to port to other OSs that may not do the same as Windows
};

internal b32 IsClicked(input_key_state key)
{
    b32 res = key == input_key_state::clicked || key == input_key_state::doubleclicked;
    return res;
}

internal b32 IsPressed(input_key_state key)
{
    b32 res = key == input_key_state::pressed;
    return res;
}

internal b32 IsUnclicked(input_key_state key)
{
    b32 res = key == input_key_state::unclicked;
    return res;
}

internal b32 IsDoubleclicked(input_key_state key)
{
    b32 res = key == input_key_state::doubleclicked;
    return res;
}

namespace input_key { //NOTE(fran): this is an 'enum class' but without the type conversion issues
    enum : u32 {
        left_mouse = 0,
        right_mouse,
    };
}

struct ui_input { //user input
    b32 close;//NOTE(fran): the os/user can request the app to be closed through means outside of the ui's control
    v2 mouseP;//INFO(fran): mouse coordinates must be relative to the window area (client area on Windows) and with x-axis going right and y-axis going down
    v2 screen_mouseP;
    input_key_state keys[2];
    f32 mouseVScroll; //NOTE(fran): indicates the number of units to scroll (eg a value of 1 means to scroll 1 unit), the unit will be determined by whoever handles the scroll
    //TODO(fran): some mice, like the G502 Hero that Im using, allow horizontal scrolling (by moving the scroll wheel to the sides), how do they do it & can we handle that case?
    //TODO(fran): on Windows mouseVScroll is positive when the user moves the finger upwards through the wheel, & negative when moving the finger downwards, idk how we should do it in other OSs

    OS::hotkey_data hotkey; //TODO(fran): this should be replaceable by keyboard input
    i32 global_hotkey_id; //System wide hotkey, received from the OS at any time

    struct {
        b32 on_unclick, on_unrclick;
        //TODO(fran): store mouseP
    } tray;

    //TODO(fran): f32 dt; or calculate dt on our own inside uiprocessing
};

enum class ui_type {
    vpad = 0,
    hpad,
    sizer, //NOTE(fran): strictly used for placing and resizing other non-sizer elements, they are not rendered nor collision tested
    subelement_table,
    button,
    slider,
    hotkey,
    background,
    contextmenu_button,
};

union ui_color {
    struct { v4 normal, disabled, mouseover, pressed, inactive; };
    v4 E[5];
    //NOTE(fran): mouseover will also be used in the case where the user clicks the item but then moves the mouse away from it, indicating they didnt really want to click it
};

enum class ui_style {
    round_rect, rect, circle
};

struct button_theme {
    struct color {
        ui_color foreground, background, border;
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
    HFONT font = 0;//TODO(fran)
};

struct background_theme {
    struct color {
        ui_color background, border;
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
};

struct hotkey_theme {
    struct color {
        ui_color foreground_placeholder, foreground_validhk, foreground_invalidhk, background, border;
        //TODO(fran): maybe the validhk is pointless, simply use normal & invalidhk
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
    HFONT font = 0;//TODO(fran)
};

//FUTURE TODO(fran): the style flags should all be placed inside a single common enum

struct slider_theme {
    struct color {
        ui_color track_fill, track_empty, thumb;
    } color;
    struct dimension {
        f32 track_thickness = 0.f;//[0.f to 1.f]
        f32 thumb_thickness = 0.f;//[0.f to 1.f]
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    //TODO(fran): icon in place of the thumb (thumb being the thingy in the middle that the user controls)
    ui_style thumb_style;
    ui_style track_style;
};
struct slider_value {
    f32* v;
    f32 min, max;
};

struct ui_element;

enum class element_sizing_type {
    font,
    font_clamp_to_bounds, //NOTE(fran): currently we only clamp to 100% of the bound
    bounds,
    opposite_bounds, /*eg in a horizontal sizer it'll use the vertical height as the width*/
    remaining,
    os_non_client_top,
};

struct element_sizing_desc {
    element_sizing_type type;
    union {
        struct {
            f32 v_scale_factor; //Eg font_height * 1.5f
            u32 w_extra_chars; //Eg leave additional space for 2 empty spaces on _each_ side of the text
        } font;
        struct {
            f32 scale_factor; //Eg bounds_w * .5f
        } bounds;
    };
};

struct element_sizing_desc2 {
    element_sizing_desc w;
    element_sizing_desc h;
};

struct ui_sizer_element {
    ui_element* element; //NOTE(fran): the element itself can and will contain multiple other elements
    element_sizing_desc sizing_desc;
    ui_sizer_element* next;
};

constexpr u32 subelement_max_cnt = 10;

struct ui_subelement_table_element {
    ui_element* element;
    fixed_array<element_sizing_desc2, subelement_max_cnt> sizing_desc;
    f32 max_h;//NOTE(fran): used internally
    ui_subelement_table_element* next;
};

enum class sizer_alignment : u8 {
    left = 1 << 0,
    right = 1 << 1,
    center = 1 << 2,

    top = left,
    bottom = right,
};

struct sizer_props {
    b32 is_horizontal; //TODO(fran): idk whether to use this variable or add more unique enum types and check the enum's value is_horizontal(alignment){ return alignment >= sizer_alignment::h_center }
    sizer_alignment alignment;
};

/* TODO(fran): having vertical and horizontal pads we could, for example, tabulate the placement of objects in a different axis than the containing resizer's, is that useful for anything?
struct pad_desc {
    b32 is_horizontal;//NOTE(fran): pads can be horizontal or vertical
}
*/


enum class interaction_state {
    normal = 0, disabled, mouseover, pressed
};

#define _UI_ACTION_RET void
#define _UI_ACTION_ARGS (ui_element* element, void* context)
#define UI_ACTION(name) _UI_ACTION_RET (name) _UI_ACTION_ARGS
#define UI_ACTION_LAMBDA []_UI_ACTION_ARGS -> _UI_ACTION_RET
typedef UI_ACTION(_ui_action);

struct ui_action {
    void* context;
    _ui_action* action;
};

#define _UI_STRING_DYN_ID_RET u32
#define _UI_STRING_DYN_ID_ARGS (void* context)
#define UI_STRING_DYN_ID(name) _UI_STRING_DYN_ID_RET (name) _UI_STRING_DYN_ID_ARGS
#define UI_STRING_DYN_ID_LAMBDA []_UI_STRING_DYN_ID_ARGS -> _UI_STRING_DYN_ID_RET
typedef UI_STRING_DYN_ID(string_dynamic_id);

enum class ui_string_type {
    id = 0, str, dynamic_id
    //TODO(fran): BENCHMARK: performance using none vs using an id equal to 0 that always maps to an empty string in hash table element 0
};
struct ui_string {
    ui_string_type type;
    union {
        u32 str_id;
        s8 str;
        struct {
            void* context;
            string_dynamic_id* get_str_id;
        } str_dyn_id;
    };
};

struct ui_hotkey {
    OS::hotkey_data* hk;
    i32 id; //NOTE(fran): unique id per hotkey
};

enum class hotkey_string_state {
    placeholder = 0, validhk, invalidhk
};

//TODO(fran): use iu::ui_renderer
#define _UI_IMAGE_RENDERCOMANDS_ARGS (ID2D1DeviceContext* renderer2D, ID2D1SolidColorBrush* brush, rc2 placement, void* context)
#define UI_IMAGE_RENDERCOMANDS(name) void(name)_UI_IMAGE_RENDERCOMANDS_ARGS
#define UI_IMAGE_RENDERCOMANDS_LAMBDA []_UI_IMAGE_RENDERCOMANDS_ARGS -> void
typedef UI_IMAGE_RENDERCOMANDS(image_rendercommands);

enum class ui_image_type {
    none = 0, img, mask, render_commands //TODO(fran): remove 'none' once we implement the img hash table
};
struct ui_image {
    ui_image_type type;
    //TODO(fran): add antialias mode and pixel snapping (round_rc()) option flags
    struct {
        v2 scale_factor; //NOTE(fran): the resulting square bounds will be centered in relation to the original bounds (TODO(fran): allow for options other than centering)
    } bounds;

    union {
        const embedded_img* img;
        struct {
            void* context;
            image_rendercommands* commands; //NOTE(fran): works sort of like an svg
        }render_commands;
    };
};

typedef button_theme contextmenu_button_theme;

enum class ui_cursor_type { os = 0, image };

struct ui_cursor {
    ui_cursor_type type;
    union {
        OS::cursor_style os_cursor;
    };
};

struct ui_element {
    ui_type type;
    rc2 placement;
    interaction_state interaction_st;
    ui_cursor cursor;
    union {
        struct {
            sizer_props properties;
            ui_sizer_element* childs;
            ui_sizer_element** last;
#ifdef DEBUG_BUILD
            v4 bk_color;
#endif
        } sizer;
        struct {
            //NOTE(fran): this table contains only on element per row
            fixed_array<f32, subelement_max_cnt> w_max_sizes;//TODO(fran): this could go in the one_frame arena
            u32 columns;
            ui_subelement_table_element* childs;
            ui_subelement_table_element** last;
#ifdef DEBUG_BUILD
            v4 bk_color;
#endif
        } subelement_table;
        struct {
            background_theme theme;

            ui_action OnClick;
            ui_action OnDoubleClick;
            ui_action OnUnRClick;
        } background;
        struct {
            button_theme theme;

            ui_action OnUnclick;
            ui_action OnClick;
            ui_action OnDoubleClick;
            ui_action OnUnRClick;

            ui_string text;
            ui_image image;
        } button;
        struct {
            contextmenu_button_theme theme;
            ui_action OnMousehover;//NOTE(fran): mousehover indicates a prolonged period of continuous mouseover //TODO(fran): implement
            ui_action OnUnclick;

            ui_image image;
            ui_string text;
            OS::hotkey_data hotkey;
        } contextmenu_button;
        struct {
            slider_theme theme;
            slider_value value;

            v2 thumb_pivot;
            v2 mouse_pivot;
            b32 has_pivot() { return this->mouse_pivot.x != -1 && this->mouse_pivot.y != -1; }

            struct {
                rc2 full_track;
                rc2 fill_track;
                rc2 empty_track;
                rc2 thumb;
            } colliders;
            b32 is_horizontal(rc2 placement) //NOTE(fran): for now sliders can only be horizontal or vertical //TODO(fran): use dot product to project a v2 slider direction and not have to use conditional
            {
                b32 res = placement.w > placement.h;
                return res;
            }
            void calculate_colliders(rc2 placement)
            {
                //TODO(fran): try to get placement using addressoff or something similar, we know were the placement variable is relative to our location
                rc2 full_track, fill_track, empty_track, thumb;
                f32 fill_percentage = percentage_between(this->value.min, *this->value.v, this->value.max);

                if (this->is_horizontal(placement)) {
                    fill_track.h = placement.h * this->theme.dimension.track_thickness;
                    fill_track.w = placement.w * fill_percentage;
                    fill_track.y = placement.y + (placement.h - fill_track.h) * .5f;
                    fill_track.x = placement.x;

                    empty_track.h = fill_track.h;
                    empty_track.w = placement.w * (1.f - fill_percentage);
                    empty_track.y = fill_track.y;
                    empty_track.x = fill_track.right();

                    f32 thumb_diameter = placement.h * this->theme.dimension.thumb_thickness;
                    thumb = rc_center_diameter({ fill_track.right(), fill_track.centerY() }, { thumb_diameter, thumb_diameter });
                }
                else {
                    //NOTE(fran): vertical sliders for the most part grow from bottom to top, so that's what we'll do: empty_track at the top, fill_track at the bottom
                    empty_track.y = placement.y;
                    empty_track.w = placement.w * this->theme.dimension.track_thickness;
                    empty_track.h = placement.h * (1.f - fill_percentage);
                    empty_track.x = placement.x + (placement.w - empty_track.w) * .5f;

                    fill_track.h = placement.h * fill_percentage;
                    fill_track.w = empty_track.w;
                    fill_track.y = empty_track.bottom();
                    fill_track.x = empty_track.x;

                    f32 thumb_diameter = placement.w * this->theme.dimension.thumb_thickness;
                    thumb = rc_center_diameter({ empty_track.centerX() ,empty_track.bottom() }, { thumb_diameter, thumb_diameter });
                }
                full_track = add_rc(fill_track, empty_track);
                this->colliders = { .full_track = full_track, .fill_track = fill_track, .empty_track = empty_track, .thumb = thumb };
            }
            f32 value_for_mouseP(rc2 placement, v2 mouseP) //NOTE(fran): mouseP relative to ui area
            {
                f32 res;
                f32 p;
                if (this->is_horizontal(placement))
                    p = percentage_between(placement.left, mouseP.x, placement.right());
                else
                    p = 1 - percentage_between(placement.top, mouseP.y, placement.bottom());
                res = lerp(this->value.min, p, this->value.max);
                return res;
            }
        } slider;
        struct {
            hotkey_theme theme;
            ui_string placeholder_text;
            hotkey_string_state string_state;
            ui_hotkey current_hotkey;
            ui_action on_hotkey;
        } hotkey;
    } data;

    //TODO(fran): add general ui_actions for all elements

    ui_element* child;//TODO(fran): maybe change to childs, elements that will be placed inside the parent
};

struct ui_tray_state {
    OS::tray_icon tray;
    ui_action on_unclick;
    ui_action on_unrclick;
};

struct ui_state {
    OS::window_handle wnd;
    rc2 placement;
    f32 scaling;
    f32 _last_scaling; //TODO(fran): find better solution, possibly to query for dpi on every frame
    b32 render_and_update_screen;
    b32 is_context_menu;

    ui_element* elements;
    memory_arena permanent_arena;//NOTE(fran): UI elements are placed here
    //memory_arena one_frame_arena;//NOTE(fran):
    ui_action OnClose; //TODO(fran): see how to make the whole 'close' concept official

    iu::ui_renderer renderer;

    language_manager* LanguageManager;

    ui_element* interacting_with;
    ui_element* under_the_mouse;
    ui_element* keyboard_focus;

    struct {
        ui_action on_triggered;
        ui_element* element;
        i64 registration_time; //TODO(fran): look for a better alternative, we should accept the hotkey once the buttons that were pressed to register it are unpressed
        b32 enabled; //TODO(fran): I could instead set registration_time to negative and use that as the enabled flag
    } global_registered_hotkeys[64]; //TODO(fran): as the name implies we want this global, to be a member of iu_state and simply to have a reference here

    ui_input input;

    ui_tray_state tray;

    ui_state* context_menu; //TODO(fran): I dont see the need to store a pointer to another window, all state is handled outside of it, so I dont see reason to need it, change this to a dynamic array that handles & dispatches all windows (all ui_states)
    //NOTE(fran): each time we use a context menu we need to create a whole new state object, in order to save time we will not later destroy those objects but instead leave them for reuse
    //NOTE(fran): a benefit of context menus is they never get resized by the user, meaning we can actually create those windows directly from this thread

    //b32 _last_mouse_was_inside;
};

declaration internal void AcquireUIState(ui_state* res, OS::window_handle ui_wnd, b32 is_context_menu);
declaration internal void ReleaseUIState(ui_state* ui);
declaration internal void UIProcessing(ui_state* ui);

namespace iu {

    IF_OS_WINDOWS(struct win32_event { HWND hwnd; UINT msg; WPARAM wparam; LPARAM lparam; });

    declaration internal void io_thread_handler();

    struct iu_state {
        memory_arena arena;
        struct {
            fixed_array<ui_state*, 128> active{};
            fixed_array<ui_state*, 128> inactive{};
        } windows;
        struct {
            fixed_array<ui_state*, 16> active{};
            fixed_array<ui_state*, 16> inactive{};
        }contextmenuwindows;

        IF_OS_WINDOWS(fifo_queue<win32_event> events); //TODO(fran): OS independent event
        IF_OS_WINDOWS(HANDLE io_thread);

        //NOTE(fran): single thread idea:
        //https://github.com/libsdl-org/SDL/issues/1059#issuecomment-933625247
        //https://gist.github.com/RT222/804bda0bb1ed305e6351dc3a9a07869b
        //https://www.gamedev.net/forums/topic/488074-win32-message-pump-and-opengl---rendering-pauses-while-draggingresizing/
        //https://www.google.com/search?q=win32+stop+thread+locking+on+resize&rlz=1C1GIWA_enAR589AR589&oq=win32+stop+thread+locking+on+resize&aqs=chrome..69i57j33i160.4553j0j7&sourceid=chrome&ie=UTF-8

        iu_state()
        {
            u64 sz = Megabytes(1);
            void* mem = VirtualAlloc(nil, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            assert(mem); //TODO(fran): release build assertion

            initialize_arena(&this->arena, (u8*)mem, sz);

            OS::SetDPIAware();

            IF_OS_WINDOWS(
                constexpr int event_cap = 1000;
            this->events = { .cnt = 0, .cnt_allocd = event_cap, .current = 0, .arr = push_arr(&this->arena, win32_event, event_cap) };
            this->io_thread = ::CreateThread(nil, 0, [](void*)->DWORD {io_thread_handler(); return 0; }, nil, 0, nil);
            //::AttachThreadInput(::GetThreadId(this->io_thread), ::GetCurrentThreadId(),true); //TODO(fran): doesnt work either way, maybe because the msg queues havent been created in neither thread???
            );
        }

        ~iu_state()
        {
            fixed_array_header<ui_state*> all[] = { this->windows.active, this->contextmenuwindows.active, this->windows.inactive, this->contextmenuwindows.inactive };

            for (auto& a : all)
                for (auto& ui : a)
                    ReleaseUIState(ui);

            VirtualFree((void*)this->arena.base, 0, MEM_RELEASE);
            zero_struct(this->arena);
            IF_OS_WINDOWS(TerminateThread(this->io_thread, 0)); //TODO(fran): we may want to do termination from inside the thread, TerminateThread is not too safe
            //fixed_array_header<ui_state*> arrs[] = { windows.active, windows.inactive, contextmenuwindows.active, contextmenuwindows.inactive };
            //for(auto& a : arrs)
            //    for(auto& w : a)
                    //TODO(fran): any per window destruction needed?
        }

    } local_persistence state;

    iu_state* get_state() { return &state; }

    definition internal void io_thread_handler()
    {
        //TODO(fran): we could move all this msg processing code to the OS specific section and simply provide access to the iu's message queue
        while (true)
        {
            MSG msg;
            //NOTE(fran): under the hood PeekMessage is responsible for handling SendMessage (aka dispatching straight to the wndproc), the msgs sent through that way are hidden from the main msg queue & not available to the user, aka me, or at least I havent found a way to get to them yet before they are dispatched
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
            {
                //GetMessageW(&msg, 0, 0, 0);
                TranslateMessage(&msg);

                switch (msg.message)
                {
                case WM_LBUTTONDOWN:
                {
                    SetCapture(msg.hwnd);
                    goto push_event;
                } break;
                case WM_LBUTTONUP:
                {
                    ReleaseCapture();
                    goto push_event;
                } break;
                case WM_MOUSEMOVE:
                {
                    TRACKMOUSEEVENT tme{ .cbSize = sizeof(tme), .dwFlags = TME_LEAVE, .hwndTrack = msg.hwnd };
                    ::TrackMouseEvent(&tme); //TODO(fran): only ask for tracking if not already tracking
                    goto push_event;
                } break;
                case WM_TRAY:
                case WM_DPICHANGED_CUSTOM:
                case WM_CLOSE_CUSTOM:
                {
                    goto push_event_no_dispatch; //TODO(fran): we could simply dispatch, it will go to the defwndproc and do nothing
                } break;
                //case WM_CHAR:
                //case WM_KEYDOWN:
                //TODO(fran): case WM_MOUSEHWHEEL:
                case WM_HOTKEY:
                case WM_CLOSE:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_RBUTTONDBLCLK:
                case WM_SYSKEYDOWN:
                case WM_KEYDOWN:
                case WM_MOUSEWHEEL:
                case WM_MOUSELEAVE:
                {
                push_event:
                    get_state()->events.push({ .hwnd = msg.hwnd, .msg = msg.message, .wparam = msg.wParam, .lparam = msg.lParam });
                    break;
                push_event_no_dispatch:
                    get_state()->events.push({ .hwnd = msg.hwnd, .msg = msg.message, .wparam = msg.wParam, .lparam = msg.lParam });
                    continue;
                } break;
                }
                DispatchMessageW(&msg);
            }
            ::MsgWaitForMultipleObjectsEx(0, nil, INFINITE, QS_ALLINPUT, 0 | MWMO_ALERTABLE | MWMO_INPUTAVAILABLE); //TODO(fran): MWMO_INPUTAVAILABLE
        }
    }

    internal void PreAdjustInput(ui_input* ui_input)
    {
        ui_input->close = false;
        ui_input->hotkey = { 0 };
        ui_input->global_hotkey_id = 0;//TODO(fran): we could also send the full info, id+vk+mods
        ui_input->mouseVScroll = 0;

        for (i32 i = 0; i < ArrayCount(ui_input->keys); i++)
            if (ui_input->keys[i] == input_key_state::clicked || ui_input->keys[i] == input_key_state::doubleclicked)
                ui_input->keys[i] = input_key_state::pressed;

        //IMPORTANT NOTE(fran): We'll use Windows style right click handling, where it does not care about anything but the element that was under the mouse when the unrclick event happens. This means we wont have an interacting_with element and therefore we need to manually reset the key state in order to avoid infinite key repeats
            //TODO(fran): this may need to be specialized for different OSs inside veil_ui
        if (ui_input->keys[input_key::right_mouse] == input_key_state::unclicked)
            ui_input->keys[input_key::right_mouse] = {};

        ui_input->tray.on_unclick = false;
        ui_input->tray.on_unrclick = false;
    }

    internal void GetInput(ui_state* ui, MSG msg)
    {
        ui_input& ui_input = ui->input;
        //TODO(fran): msg.hwnd is null when clicking inside the client area of the window, we cant know which window is being addressed if we had multiple ones (all this because PostThreadMessageW doesnt let you pass the hwnd, is there a way to pass it?)
                    //if (msg.hwnd == Veil->ui.wnd) { //Check for interesting messages to the UI

        //IMPORTANT TODO(fran): now we have hwnds created on both threads we'd need to copy the extra handling that we have on specific messages on the main thread, eg SetCapture(),...
        switch (msg.message)
        {
        case WM_CLOSE_CUSTOM:
        {
            ui_input.close = true;
        } break;
        case WM_DPICHANGED_CUSTOM:
        {
            ui->scaling = (f32)LOWORD(msg.wParam) / 96.f /*aka USER_DEFAULT_SCREEN_DPI */;
            assert(LOWORD(msg.wParam) == HIWORD(msg.wParam));
            //RECT suggested_window = *(RECT*)msg.lParam;
            //MovWindow(Veil->wnd, suggested_window); //TODO(fran): test the rcs we get, I've seen that for many applications this suggested new window is terrible, both in position & size, see if we can come up with a better suggestion
        } break;
        case WM_MOUSEMOVE:
        {
            POINT p{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
            POINT screenP = p;
            MapWindowPoints(ui->wnd.hwnd, HWND_DESKTOP, &screenP, 1);

            ui_input.mouseP = v2_from(p.x, p.y);

            ui_input.screen_mouseP = v2_from(screenP.x, screenP.y);
        } break;
        case WM_MOUSELEAVE:
        {
            ui_input.mouseP = ui_input.screen_mouseP =
#ifdef DEBUG_BUILD
            { -1, -1 }; //NOTE(fran): for ease of debugging
#else
            { F32MIN, F32MIN };
#endif
        } break;
        case WM_LBUTTONDOWN:
        {
            ui_input.keys[input_key::left_mouse] = input_key_state::clicked;
        } break;
        case WM_LBUTTONUP:
        {
            ui_input.keys[input_key::left_mouse] = input_key_state::unclicked;
        } break;
        case WM_LBUTTONDBLCLK:
        {
            ui_input.keys[input_key::left_mouse] = input_key_state::doubleclicked;
        } break;
        case WM_RBUTTONDOWN:
        {
            ui_input.keys[input_key::right_mouse] = input_key_state::clicked;
        } break;
        case WM_RBUTTONUP:
        {
            ui_input.keys[input_key::right_mouse] = input_key_state::unclicked;
        } break;
        case WM_RBUTTONDBLCLK:
        {
            ui_input.keys[input_key::right_mouse] = input_key_state::doubleclicked;
        } break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            b32 ctrl_is_down = HIBYTE(GetKeyState(VK_CONTROL));
            b32 alt_is_down = HIBYTE(GetKeyState(VK_MENU));
            b32 shift_is_down = HIBYTE(GetKeyState(VK_SHIFT));

            u8 vk = (u8)msg.wParam;

            //TODO(fran): keyboard keys should also have clicked, pressed and unclicked states

            switch (vk) {
            case VK_LWIN: case VK_RWIN:
            case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
            case VK_MENU:
            case VK_F12:
            {
                ui_input.hotkey.vk = (u8)0;
                ui_input.hotkey.translation_nfo = 0;
            } break;
            default:
            {
                ui_input.hotkey.vk = (u8)vk;
                ui_input.hotkey.translation_nfo = msg.lParam;
            } break;
            }
            ui_input.hotkey.mods = (ctrl_is_down ? MOD_CONTROL : 0) | (alt_is_down ? MOD_ALT : 0) | (shift_is_down ? MOD_SHIFT : 0);
        } break;
        case WM_HOTKEY: //NOTE(fran): system-wide global hotkey
        {
            i32 id = msg.wParam;
            u16 mods = LOWORD(msg.lParam);
            u8 vk = HIWORD(msg.lParam);
            ui_input.global_hotkey_id = id;
        } break;
        case WM_MOUSEWHEEL:
        {
            //TODO(fran): we can also use the wParam to get the state of Ctrl, Shift, Mouse Click & others
            f32 zDelta = (f32)GET_WHEEL_DELTA_WPARAM(msg.wParam) / (f32)WHEEL_DELTA;
            //NOTE(fran): zDelta indicates the number of units to scroll (the unit will be determined by whoever handles the scroll)
            ui_input.mouseVScroll = zDelta;
        } break;
        case WM_TRAY:
        {
            switch (LOWORD(msg.lParam))
            {
                //NOTE(fran): for some reason the tray only sends WM_LBUTTONDOWN once the mouse has been released, so here WM_LBUTTONDOWN counts as WM_LBUTTONUP, same for right click
            case WM_LBUTTONDOWN:
            {
                ui_input.tray.on_unclick = true;
            } break;
            case WM_RBUTTONDOWN:
            {
                ui_input.tray.on_unrclick = true;
            } break;
            }
        } break;
        }
        //}
    }

    internal void update_and_render()
    {
        TIMEDFUNCTION();
        //TODO(fran): move windows to inactive state or let the ui itself do it?
        auto state = get_state();

        IF_OS_WINDOWS(
            local_persistence b32 ret = ::AttachThreadInput(::GetCurrentThreadId(), ::GetThreadId(state->io_thread), true);
        assert(ret);
        //NOTE(fran): works here, it probably needed the msg queues to be already initialized
        //TODO(fran): it can happen that this function is called when no windows have yet been created on the threads (also im currently creating a window on the main thread by hand but that shouldnt happen normally), we should add some code that makes sure that both threads can be attached:
            //Docs say: The AttachThreadInput function fails if either of the specified threads does not have a message queue. The system creates a thread's message queue when the thread makes its first call to one of the USER or GDI functions
        );

        fixed_array_header<ui_state*> all[] = { state->windows.active, state->contextmenuwindows.active, state->windows.inactive, state->contextmenuwindows.inactive };
        fixed_array_header<ui_state*> actives[] = { state->windows.active, state->contextmenuwindows.active };

        for (auto& a : all)
            for (auto& ui : a)
                PreAdjustInput(&ui->input);

        win32_event event;
        while (state->events.pop(&event))
        {
            //TODO(fran): OPTIMIZE: hashtable?
            for (auto& a : all)
                for (auto& ui : a)
                {
                    if (ui->wnd.hwnd == event.hwnd) //TODO(fran): find out if some broadcast messages simply have the hwnd set to null, in that case we'd need to send that single event to all windows
                    {
                        MSG msg = { .hwnd = event.hwnd, .message = event.msg, .wParam = event.wparam, .lParam = event.lparam };
                        GetInput(ui, msg);
                        goto next_event; //TODO(fran): find out how to skip to the next while step without goto
                    }
                }
        next_event:
            ; //NOTE(fran): really C++? how stupid is this
        }

        //TODO(fran): AdjustMouse, but first try to fix it with WM_MOUSELEAVE

        //TODO(fran): one frame initialization for global ui things
        //Temporary Arena initialization
        ui_state* ui = state->windows.active.arr[0]; //TODO(fran): lang mgr should be global and not obtained through a window in this hacky way
        initialize_arena(&ui->LanguageManager->temp_string_arena, ui->LanguageManager->_temp_string_arena, sizeof(ui->LanguageManager->_temp_string_arena));

        for (auto& a : actives)
            for (auto& ui : a)
                UIProcessing(ui);
    }

    internal OS::window_handle _createwindow(OS::window_creation_flags creation_flags)
    {
        OS::window_handle res;
        //TODO(fran): OS independent, not only for Windows' crap
        //TODO(fran): looks like a combination of AttachThreadInput & QueueUserAPC could solve all my problems
        auto state = get_state();

        //TODO(fran): remove the b32 done and check the wnd handle?
        struct _create_wnd { OS::window_handle wnd; OS::window_creation_flags flags; b32 done; };

        volatile _create_wnd create_wnd{ .wnd = {}, .flags = creation_flags, .done = false };

        //TODO(fran): instead of doing this crazy stuff I could hack it, do a SendMessage to a hidden window we create on startup from the other thread & that lives through the entirety of the program

        ::QueueUserAPC(
            [](ULONG_PTR args) {
                _create_wnd* create_wnd = (decltype(create_wnd))args;
                create_wnd->wnd = OS::Window(create_wnd->flags);
                create_wnd->done = true;
            }
        , state->io_thread, (ULONG_PTR)&create_wnd);

        while (!create_wnd.done) {}

        res = *(OS::window_handle*)(&create_wnd.wnd); //removing volatile before being able to copy to a non volatile, this is so dumb

        assert(res);

        return res;
    }

    internal ui_state* window(OS::window_creation_flags creation_flags)
    {
        auto state = get_state();
        ui_state* res = push_type(&state->arena, ui_state); //TODO(fran): zeroing required?
        auto wnd = _createwindow(creation_flags); assert(wnd);

        AcquireUIState(res, wnd, false); //TODO(fran): we may wanna just return the window handle and allow the user to attach its state later?

        state->windows.active.add(res);

        return res;
    }

    definition internal ui_state* window_contextmenu()
    {
        auto state = get_state();
        ui_state* res = push_type(&state->arena, ui_state); //TODO(fran): zeroing required?
        auto wnd = _createwindow(OS::window_creation_flags::contextmenu); assert(wnd);

        AcquireUIState(res, wnd, true);

        state->contextmenuwindows.active.add(res);

        return res;

        //TODO(fran): I think we can get rid of the is_context_menu special case:
            //Resizing: we need a sizing_type that can resize its own window based on its childs, may also require of an invisible element that contains all the others
            //All elements in a context menu should have a on_mouseover action of hiding the next context menu (aka the context_menu pointer), elements that open another subcontext menu simply should first hide the next, then show the new one
            //The final thing would be to hide all context menus when a click happens (except when the click is in an element that opens another submenu window). This is the hardest one since the click could be outisde of our window
    }

}

struct ui_sized_element {
    element_sizing_desc sizing;
    ui_element* element;
};

internal void SizerAddElement(memory_arena* arena, ui_element* sizer, element_sizing_desc sizing_desc, ui_element* new_elem)
{
    assert(sizer->type == ui_type::sizer);

    ui_sizer_element** elements = sizer->data.sizer.last;

    *elements = push_type(arena, ui_sizer_element);
    (*elements)->element = new_elem;
    (*elements)->sizing_desc = sizing_desc;
    (*elements)->next = nil;

    sizer->data.sizer.last = &(*elements)->next;
}

internal ui_element* Sizer(memory_arena* arena, b32 is_horizontal, enum sizer_alignment alignment)
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::sizer;
    elem->placement = { 0 };
    elem->child = nil;
    elem->data.sizer.childs = nil;
    elem->data.sizer.properties.alignment = alignment;
    elem->data.sizer.properties.is_horizontal = is_horizontal;
    elem->data.sizer.last = &elem->data.sizer.childs;
#ifdef DEBUG_BUILD
    elem->data.sizer.bk_color = { random01(),random01(),random01(),1.f };
#endif
    return elem;
}

//TODO(fran): I'd like to try variadic functions (...) but I couldnt find a good way to retrieve the number of arguments. My problem is initializer_list requires you to add an extra pair of {} which adds to the confusion

/*
internal ui_element* HSizer(memory_arena* arena, enum sizer_alignment alignment)
{
    return Sizer(arena, true, alignment);
}
internal ui_element* VSizer(memory_arena* arena, enum sizer_alignment alignment)
{
    return Sizer(arena, false, alignment);
}
*/

/*
internal ui_element* HSizer(memory_arena* arena, enum sizer_alignment alignment, std::initializer_list<ui_sized_element> elements)
{
    auto res = Sizer(arena, true, alignment);

    for (auto& sized_element : elements) 
        SizerAddElement(arena, res, sized_element.sizing, sized_element.element);

    return res;
}
internal ui_element* VSizer(memory_arena* arena, enum sizer_alignment alignment, std::initializer_list<ui_sized_element> elements)
{
    auto res = Sizer(arena, false, alignment);

    for (auto& sized_element : elements)
        SizerAddElement(arena, res, sized_element.sizing, sized_element.element);

    return res;
}
*/

internal ui_element* HSizer(memory_arena* arena, enum sizer_alignment alignment, const ui_sized_element& e0 = {}, const ui_sized_element& e1 = {}, const ui_sized_element& e2 = {}, const ui_sized_element& e3 = {}, const ui_sized_element& e4 = {}, const ui_sized_element& e5 = {}, const ui_sized_element& e6 = {}, const ui_sized_element& e7 = {}, const ui_sized_element& e8 = {}, const ui_sized_element& e9 = {}, const ui_sized_element& e10 = {})
{
    auto res = Sizer(arena, true, alignment);
    
#define __SizerAddElement(arena, res, sz_elem) if (sz_elem.element) SizerAddElement(arena, res, sz_elem.sizing, sz_elem.element); else return res

    __SizerAddElement(arena, res, e0);
    __SizerAddElement(arena, res, e1);
    __SizerAddElement(arena, res, e2);
    __SizerAddElement(arena, res, e3);
    __SizerAddElement(arena, res, e4);
    __SizerAddElement(arena, res, e5);
    __SizerAddElement(arena, res, e6);
    __SizerAddElement(arena, res, e7);
    __SizerAddElement(arena, res, e8);
    __SizerAddElement(arena, res, e9);
    __SizerAddElement(arena, res, e10);

#undef __SizerAddElement
    return res;
}

internal ui_element* VSizer(memory_arena* arena, enum sizer_alignment alignment, const ui_sized_element& e0 = {}, const ui_sized_element& e1 = {}, const ui_sized_element& e2 = {}, const ui_sized_element& e3 = {}, const ui_sized_element& e4 = {}, const ui_sized_element& e5 = {}, const ui_sized_element& e6 = {}, const ui_sized_element& e7 = {}, const ui_sized_element& e8 = {}, const ui_sized_element& e9 = {}, const ui_sized_element& e10 = {})
{
    auto res = Sizer(arena, false, alignment);

#define __SizerAddElement(arena, res, sz_elem) if (sz_elem.element) SizerAddElement(arena, res, sz_elem.sizing, sz_elem.element); else return res

    __SizerAddElement(arena, res, e0);
    __SizerAddElement(arena, res, e1);
    __SizerAddElement(arena, res, e2);
    __SizerAddElement(arena, res, e3);
    __SizerAddElement(arena, res, e4);
    __SizerAddElement(arena, res, e5);
    __SizerAddElement(arena, res, e6);
    __SizerAddElement(arena, res, e7);
    __SizerAddElement(arena, res, e8);
    __SizerAddElement(arena, res, e9);
    __SizerAddElement(arena, res, e10);

#undef __SizerAddElement

    return res;
}

/*
template <typename... ui_sized_element> //NOTE: elements must be of type ui_sized_element
internal ui_element* HSizer(memory_arena* arena, sizer_alignment alignment, ui_sized_element... elements)
{
    //Reference: https://stackoverflow.com/questions/7230621/how-can-i-iterate-over-a-packed-variadic-template-argument-list
    auto res = Sizer(arena, true, alignment);

    for (const ui_sized_element sized_element : { elements... })
        SizerAddElement(arena, res, sized_element.sizing, sized_element.element);

    return res;
}
//template<memory_arena* arena, sizer_alignment alignment, typename... T> HSizer(arena, alignment, T...) -> HSizer<decltype(T)...>;


template <typename... ui_sized_element> //NOTE: elements must be of type ui_sized_element
internal ui_element* VSizer(memory_arena* arena, sizer_alignment alignment, ui_sized_element... elements)
{
    auto res = Sizer(arena, false, alignment);

    for (const ui_sized_element sized_element : { elements... })
        SizerAddElement(arena, res, sized_element.sizing, sized_element.element);

    return res;
}
*/
#define VSizer VSizer
#define HSizer HSizer

struct ui_sized_subelement_table_element {
    fixed_array<element_sizing_desc2, subelement_max_cnt> sizing;
    ui_element* element;
};

internal void SubETableAddElement(memory_arena* arena, ui_element* subetable, const ui_sized_subelement_table_element& new_elem)
{
    assert(subetable->type == ui_type::subelement_table);

    ui_subelement_table_element** elements = subetable->data.subelement_table.last;

    *elements = push_type(arena, ui_subelement_table_element);
    (*elements)->element = new_elem.element;
    (*elements)->sizing_desc = new_elem.sizing;
    (*elements)->max_h = 0;
    (*elements)->next = nil;

    subetable->data.subelement_table.last = &(*elements)->next;
}

internal ui_element* SubelementTable(memory_arena* arena, u32 column_cnt,  const ui_sized_subelement_table_element& e0 = {}, const ui_sized_subelement_table_element& e1 = {}, const ui_sized_subelement_table_element& e2 = {}, const ui_sized_subelement_table_element& e3 = {}, const ui_sized_subelement_table_element& e4 = {}, const ui_sized_subelement_table_element& e5 = {}, const ui_sized_subelement_table_element& e6 = {}, const ui_sized_subelement_table_element& e7 = {}, const ui_sized_subelement_table_element& e8 = {}, const ui_sized_subelement_table_element& e9 = {}, const ui_sized_subelement_table_element& e10 = {})
{
    ui_element* res = push_type(arena, ui_element);
    res->type = ui_type::subelement_table;
    res->placement = { 0 };
    res->child = nil;
    res->data.subelement_table.childs = nil;
    res->data.subelement_table.columns = column_cnt;
    res->data.subelement_table.w_max_sizes.clear();
    res->data.subelement_table.last = &res->data.subelement_table.childs;
#ifdef DEBUG_BUILD
    res->data.subelement_table.bk_color = { random01(),random01(),random01(),1.f };
#endif
    
#define __SubETableAddElement(arena, res, sz_elem) if (sz_elem.element) SubETableAddElement(arena, res, sz_elem); else return res

    __SubETableAddElement(arena, res, e0);
    __SubETableAddElement(arena, res, e1);
    __SubETableAddElement(arena, res, e2);
    __SubETableAddElement(arena, res, e3);
    __SubETableAddElement(arena, res, e4);
    __SubETableAddElement(arena, res, e5);
    __SubETableAddElement(arena, res, e6);
    __SubETableAddElement(arena, res, e7);
    __SubETableAddElement(arena, res, e8);
    __SubETableAddElement(arena, res, e9);
    __SubETableAddElement(arena, res, e10);

#undef __SubETableAddElement

    return res;
}
#define SubelementTable SubelementTable

struct button_creation_args {
    memory_arena* arena;
    button_theme* theme;
    ui_string text;
    ui_image image;
    ui_cursor cursor;
    ui_action on_click;
    ui_action on_unclick;
    ui_action on_doubleclick;
    ui_action on_unrclick;
    ui_element* child;
};
internal ui_element* Button(const button_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::button;
    elem->placement = { 0 };
    elem->cursor = args.cursor;
    elem->child = args.child;
    elem->data.button.theme = *args.theme; //TODO(fran): this could be changed to be a straight pointer, and allow for quick and easy realtime update of all elements sharing the same theme

    elem->data.button.text = args.text;
    elem->data.button.image = args.image;
    elem->data.button.OnClick = args.on_click;
    elem->data.button.OnUnclick = args.on_unclick;
    elem->data.button.OnDoubleClick = args.on_doubleclick;
    elem->data.button.OnUnRClick = args.on_unrclick;

    return elem;
}
#define Button(...) Button({__VA_ARGS__})


struct contextmenu_button_creation_args {
    memory_arena* arena;
    contextmenu_button_theme* theme;
    ui_image image;
    ui_string text;
    OS::hotkey_data hotkey; //TODO(fran): should be a pointer
    ui_action on_unclick;
    ui_action on_mousehover;
    void* context;
    ui_element* child;
};
internal ui_element* ContextMenuButton(const contextmenu_button_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::contextmenu_button;
    elem->placement = { 0 };
    elem->child = args.child;

    elem->data.contextmenu_button.theme = *args.theme;
    elem->data.contextmenu_button.text = args.text;
    elem->data.contextmenu_button.image = args.image;
    elem->data.contextmenu_button.hotkey = args.hotkey;
    elem->data.contextmenu_button.OnUnclick = args.on_unclick;
    elem->data.contextmenu_button.OnMousehover = args.on_mousehover;

    return elem;
}
#define ContextMenuButton(...) ContextMenuButton({__VA_ARGS__})

struct background_creation_args {
    memory_arena* arena;
    background_theme* theme;
    ui_action on_click;
    ui_action on_doubleclick;
    ui_action on_unrclick;
    ui_element* child;
};
internal ui_element* Background(const background_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::background;
    elem->placement = { 0 };
    elem->child = args.child;
    elem->data.background.theme = *args.theme;
    elem->data.background.OnClick = args.on_click;
    elem->data.background.OnDoubleClick = args.on_doubleclick;
    elem->data.background.OnUnRClick = args.on_unrclick;

    return elem;
}
#define Background(...) Background({__VA_ARGS__})

struct slider_creation_args {
    memory_arena* arena;
    slider_theme* theme;
    slider_value value;
    ui_element* child;
};
internal ui_element* Slider(const slider_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::slider;
    elem->placement = { 0 };
    elem->child = args.child;
    elem->data.slider.theme = *args.theme;
    elem->data.slider.value = args.value;

    return elem;
}
#define Slider(...) Slider({__VA_ARGS__})

internal ui_element* VPad(memory_arena* arena) 
{
    //TODO(fran): assign a font to the pad, so it can decide its padding based on that
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::vpad;
    elem->placement = { 0 };
    elem->child = nil;

    return elem;
}
#define VPad VPad

internal ui_element* HPad(memory_arena* arena) 
{
    //TODO(fran): assign a font to the pad, so it can decide its padding based on that
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::hpad;
    elem->placement = { 0 };
    elem->child = nil;

    return elem;
}
#define HPad HPad

struct hotkey_creation_args {
    memory_arena* arena;
    hotkey_theme* theme;
    OS::hotkey_data* hotkey_value;
    ui_string placeholder_text;
    ui_cursor cursor;
    ui_action on_hotkey;
    ui_element* child;
};
internal ui_element* Hotkey(const hotkey_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::hotkey;
    elem->placement = { 0 };
    elem->cursor = args.cursor;
    elem->child = args.child;
    elem->data.hotkey.theme = *args.theme;
    elem->data.hotkey.on_hotkey = args.on_hotkey;
    //elem->data.hotkey.context = args.context;
    elem->data.hotkey.placeholder_text = args.placeholder_text;
    elem->data.hotkey.string_state = hotkey_string_state::placeholder;

    local_persistence i32 hotkey_id = 1; //NOTE(fran): id 0 is invalid
    elem->data.hotkey.current_hotkey.id = hotkey_id++;
    assert(hotkey_id <= ArrayCount(ui_state::global_registered_hotkeys));
    elem->data.hotkey.current_hotkey.hk = args.hotkey_value;//TODO(fran): start with a hotkey and try to register it

    return elem;
}
#define Hotkey(...) Hotkey({__VA_ARGS__})

namespace common_ui_actions
{
    auto MinimizeOrRestore = UI_ACTION_LAMBDA{
        //TODO(fran): add the minimize/restore to tray code to the future animation system, currently it stalls the thread (via Sleep) until it finishes minimizing/restoring
        //TODO(fran): check for active tray icon, otherwise do normal minimize
        ui_state* ui = (decltype(ui))context;
        if (!OS::IsWindowVisible(ui->wnd)) { //window is minimized
            RestoreWndFromTray(ui->wnd);
            //TODO(fran): the veil could be occluded, we should check that the veil is on top too
        }
        else {
            //TODO(fran): the window could be occluded (in which case we want to SW_SHOW), there doesnt seem to be an easy way to know whether your window is actually visible to the user
            MinimizeWndToTray(ui->wnd);
        }
    };

    auto MaximizeOrRestore = UI_ACTION_LAMBDA{
        ui_state* ui = (decltype(ui))context;
        if (IsWindowMaximized(ui->wnd)) OS::RestoreWindow(ui->wnd);
        else OS::MaximizeWindow(ui->wnd);
    };

    auto B32_Flip = UI_ACTION_LAMBDA{
        b32* val = (decltype(val))context;
        *val = !(*val);
    };

    auto B32_Set = UI_ACTION_LAMBDA{
    b32* val = (decltype(val))context;
    *val = true;
    };
}

internal language_manager* AcquireLanguageManager()
{
    local_persistence b32 initialized = false;
    local_persistence language_manager _res;
    language_manager* res = &_res;

    if (!initialized) {
        initialized = true;

        utf8 lang_folder[] = u8"lang/";
        memcpy(res->lang_folder, lang_folder, sizeof(lang_folder));

        initialize_arena(&res->string_mapping_arena, res->_string_mapping_arena, sizeof(res->_string_mapping_arena));

        language default_langs[] = {
            { lang_english_entries, sizeof(lang_english_entries)},
            { lang_español_entries, sizeof(lang_español_entries)},
        };

        //TODO(fran): timing & optimization for save_languages_to_disc & load_languages_from_disc (on ssd & hdd), im sure that if many languages are added our start time will be horrible
        save_languages_to_disc(default_langs, ArrayCount(default_langs), temp_s(res->lang_folder));

        res->available_languages_cnt = 0;

        res->load_languages_from_disc();

        utf8 language_to_use[locale_max_length] = u8"en-us"; //TODO(fran): get from config file

        res->ChangeLanguage(temp_s(language_to_use));
    }
    return res;
}

internal f32 GetNewScaling(ui_state* ui)
{
    f32 res = OS::GetScalingForWindow(ui->wnd);
    return res;
}

internal void AcquireUIState(ui_state* res, OS::window_handle ui_wnd, b32 is_context_menu = false/*TODO(fran): this is kinda pointless, we could create a ui_element that resizes the window it's in based on the size of its childs*/)
{
    res->wnd = ui_wnd;
    res->interacting_with = nil;
    res->under_the_mouse = nil;
    res->keyboard_focus = nil;
    res->placement = { 0 };
    res->render_and_update_screen = true;

    local_persistence i32 alloc_cnt = 0;

    u32 total_sz = Megabytes(1);
#ifdef DEBUG_BUILD
    LPVOID base_address = (LPVOID)(Terabytes(1) + alloc_cnt++ * total_sz); //Fixed address for ease of debugging
#else
    LPVOID base_address = 0;
#endif

    //NOTE(fran): ui states can be reacquired, for that we must know wether or not this ui state has been previously initialized

    b32 already_initialized = res->permanent_arena.base!=0;

    if (!already_initialized)
    {
        res->is_context_menu = is_context_menu; //TODO(fran): allow changing a window from normal to context_menu?

        res->_last_scaling = res->scaling = GetNewScaling(res); //TODO(fran): this is actually wrong since we dont yet know the location of the window and therefore the dpi of its containing monitor, this only works for single monitor setups

        res->renderer = iu::AcquireRenderer(res->wnd, res->scaling);
    }

    //TODO(fran): require default zero initialization
    void* mem = already_initialized ? res->permanent_arena.base : VirtualAlloc(base_address, total_sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);//TODO(fran): use large pages

    initialize_arena(&res->permanent_arena, (u8*)mem, total_sz);
    assert(res->permanent_arena.base);
    //TODO(fran): zero memory, VirtualAlloc does it the first time, but after that it'll be filled with garbage

    zero_struct(res->global_registered_hotkeys);

    res->context_menu = nil;

    res->LanguageManager = AcquireLanguageManager();

    res->elements = nil;

    res->input.mouseP = res->input.screen_mouseP =
#ifdef DEBUG_BUILD
    { -1, -1 }; //NOTE(fran): for ease of debugging
#else
    { F32MIN, F32MIN };
#endif
}

internal void ReleaseUIState(ui_state* ui) {
    
    VirtualFree(ui->permanent_arena.base, 0, MEM_RELEASE);
    ui->permanent_arena = { 0 };

    iu::ReleaseRenderer(&ui->renderer);
}

internal void AcquireTrayIcon(ui_state* ui, ui_action on_unclick, ui_action on_unrclick)
{
    ui->tray =
    {
        .tray = OS::CreateTrayIcon(ui->wnd),
        .on_unclick = on_unclick,
        .on_unrclick = on_unrclick,
    };
}

internal void ReleaseTrayIcon(ui_tray_state* tray) //TODO(fran): this should be done automatically by Iu when the window is "destroyed"
{
    OS::DestroyTrayIcon(&tray->tray);
}

internal s8 GetUIStringStr(ui_state* ui, const ui_string& text)
{
    s8 res;
    if (text.type == ui_string_type::id)
        res = ui->LanguageManager->GetString(text.str_id);
    else if (text.type == ui_string_type::str)
        res = text.str;
    else if (text.type == ui_string_type::dynamic_id)
        res = ui->LanguageManager->GetString(text.str_dyn_id.get_str_id(text.str_dyn_id.context));
    else crash();
    return res;
}

internal void EnableRendering(ui_state* ui)
{
    ui->render_and_update_screen = true;
}
//NOTE(fran): if enable is true rendering will be done on this frame, if it is false the previous rendering state is not modified. You can only enable rendering, never disable it.
internal void EnableRendering(ui_state* ui, b32 enable)
{
    ui->render_and_update_screen |= enable;
}
internal b32 IsRenderingEnabled(ui_state* ui)
{
    b32 res = ui->render_and_update_screen;
    return res;
}

struct input_results {
    ui_element* next_hot; //Determines the element the mouse is on top of
};
internal input_results InputUpdate(ui_element* elements, ui_input* input)
{
    input_results res{ 0 };

    ui_element* element = elements;
    ui_element* next_hot = nil;

    assert(element);
    //while (element)
    {
        switch (element->type)
        {
            case ui_type::sizer:
            {
                //TODO(fran): set element as no_collision or mouse_through
                auto& data = element->data.sizer;
                auto superchild = data.childs;
                while (superchild) {
                    ui_element* child = superchild->element;
                    if (child) {
                    //while (child) {
                        next_hot = InputUpdate(child, input).next_hot;
                        if (next_hot) goto already_found; //NOTE(fran): early out since sizer elements should not overlap
                        //child = child->child;
                    }
                    superchild = superchild->next;
                }
            } break;
            case ui_type::subelement_table:
            {
                //TODO(fran): duplicate code, create a next_element() routine that gets the next superchild->element for any type of element that contains a list of elements
                auto& data = element->data.subelement_table;
                auto superchild = data.childs;
                while (superchild) {
                    ui_element* child = superchild->element;
                    if (child) {
                        //while (child) {
                        next_hot = InputUpdate(child, input).next_hot;
                        if (next_hot) goto already_found; //NOTE(fran): early out since sizer elements should not overlap
                        //child = child->child;
                    }
                    superchild = superchild->next;
                }
            } break;
            case ui_type::vpad:
            case ui_type::hpad:
            {
            } break;
            default:
            {
                if (ui_element* child = element->child) next_hot = InputUpdate(child, input).next_hot;
                if (next_hot) goto already_found;
                else if (test_pt_rc(input->mouseP, element->placement)) {
                    next_hot = element; //NOTE(fran): take the last element that matches with the mouse, a way to implement some very basic "depth filtering" taking the forward-most element
                }
            } break;
        }
        //element = element->child;
    }
already_found:
    res.next_hot = next_hot;

    return res;
}

internal void CancelInteraction(ui_state* ui) //TODO(fran): see if this makes sense, or some other concept is needed
{
    ui->interacting_with = nil;
}

internal void BeginInteraction(ui_state* ui, ui_element* next_hot, ui_input* input)
{
    EnableRendering(ui);

    ui->interacting_with = next_hot;

    auto element = ui->interacting_with;

    element->interaction_st = interaction_state::pressed;
    switch (element->type)
    {
        case ui_type::slider:
        {
            auto& data = element->data.slider;
            if (test_pt_rc(input->mouseP, data.colliders.thumb))
            {
                data.thumb_pivot = data.colliders.thumb.center();
                data.mouse_pivot = input->mouseP;
            }
            else if (test_pt_rc(input->mouseP, add_rc(data.colliders.empty_track, data.colliders.fill_track)))
            {
                v2 no_pivot = { -1,-1 };
                data.mouse_pivot = no_pivot;
                *data.value.v = data.value_for_mouseP(element->placement, input->mouseP);
            }
            else CancelInteraction(ui);
        } break;
        case ui_type::hotkey:
        {
            ui->keyboard_focus = element;
            //TODO(fran): activate blue selection drawing over the text
        } break;
        case ui_type::button:
        {
            auto& data = element->data.button;

            //TODO(fran): quick HACK: make it official whether we are currently working with left or right click
            if (IsDoubleclicked(input->keys[input_key::left_mouse]))
                safe_call(data.OnDoubleClick.action, element, data.OnDoubleClick.context);
            else 
                safe_call(data.OnClick.action, element, data.OnClick.context);
        } break;
        case ui_type::background:
        {
            auto& data = element->data.background;
            //TODO(fran): same quick HACK
            if (IsDoubleclicked(input->keys[input_key::left_mouse]))
                safe_call(data.OnDoubleClick.action, element, data.OnDoubleClick.context);
            else
                safe_call(data.OnClick.action, element, data.OnClick.context);
        } break;
    }
}

internal void EndInteraction(ui_state* ui, ui_input* input)
{
    EnableRendering(ui);

    auto element = ui->interacting_with;

    auto last_interaction_st = element->interaction_st;

    element->interaction_st = (ui->under_the_mouse == element) ? interaction_state::mouseover : interaction_state::normal;

    switch (element->type)
    {
        case ui_type::button:
        {
            auto& data = element->data.button;
            if (last_interaction_st == interaction_state::pressed)
                safe_call(data.OnUnclick.action, element, data.OnUnclick.context);
        } break;
        case ui_type::contextmenu_button:
        {
            auto& data = element->data.contextmenu_button;
            if (last_interaction_st == interaction_state::pressed)
                safe_call(data.OnUnclick.action, element, data.OnUnclick.context);
        } break;
    }

    ui->interacting_with = nil;
}

internal void ContinueInteraction(ui_state* ui, ui_input* input)
{
    EnableRendering(ui);

    auto element = ui->interacting_with;

    element->interaction_st = (ui->under_the_mouse == element) ? interaction_state::pressed : interaction_state::mouseover;

    switch (element->type)
    {
        case ui_type::slider:
        {
            auto& data = element->data.slider;
            v2 p;
            if (data.has_pivot())
                p = data.thumb_pivot + (input->mouseP - data.mouse_pivot);
            else
                p = input->mouseP;
            p.x = clamp(element->placement.left, p.x, element->placement.right());
            p.y = clamp(element->placement.top, p.y, element->placement.bottom());
            *data.value.v = data.value_for_mouseP(element->placement, p);
        } break;
    }
}

internal void UpdateUnderTheMouse(ui_state* ui, ui_element* next_hot, ui_input* input)
{
    if (ui_element* element = ui->under_the_mouse; element && element!=ui->interacting_with)
    {
        auto last_interaction_st = element->interaction_st;

        element->interaction_st = (next_hot == element) ? interaction_state::mouseover : interaction_state::normal;

        EnableRendering(ui, last_interaction_st != element->interaction_st);
    }

    //NOTE(fran): only the window directly under the mouse can change the mouse icon
    if (next_hot && ui->under_the_mouse != next_hot) //TODO(fran): extra check: while on interaction (by any iu window) we mustnt change the cursor, it must remain with the look that the element being interacted wants
    {
        assert(next_hot->cursor.type == ui_cursor_type::os); //TODO(fran): custom cursors
        OS::cursor_style cursor = next_hot->cursor.os_cursor;
#if 1
        OS::SetCursor(cursor); //NOTE(fran): WINDOWS: this only works if the ui & input threads are attached, see AttachThreadInput
#else
        OS::_SetCursor(ui->wnd, cursor);
        //if(ui->context_menu) OS::_SetCursor(ui->context_menu->wnd, cursor);

#endif
        //TODO(fran): later on we'd want more control over the cursor, probably to be handled on the on_mouseover action of each element
    }

    ui->under_the_mouse = next_hot; //TODO(fran): we should set this guy to interaction_state::mouseover if it's valid

    if (ui_element* element = ui->under_the_mouse)
    {
        switch (element->type)
        {
            case ui_type::slider:
            {
                auto data = element->data.slider;

                auto oldvalue = *data.value.v;

                f32 unit = distance(data.value.min, data.value.max) * .01f;

                f32 newvalue = clamp(data.value.min, *data.value.v + input->mouseVScroll * unit, data.value.max);

                *data.value.v = newvalue;

                EnableRendering(ui, oldvalue != newvalue);
            } break;
            case ui_type::background:
            {
                auto data = element->data.background;

                if (IsUnclicked(input->keys[input_key::right_mouse]))
                    safe_call(data.OnUnRClick.action, ui->under_the_mouse, data.OnUnRClick.context);

            } break;
        }
    }
}

internal void UpdateGlobalHotkeys(ui_state* ui, ui_input* input)
{
    if (input->global_hotkey_id != 0)
    {
        auto& [on_triggered, element, registration_time, enabled] = ui->global_registered_hotkeys[input->global_hotkey_id];
        
        //NOTE(fran): the user takes time to enter a hotkey and then release those same keys, in that time we get hotkey triggered messages from the OS! This can not be solved using wm_keyup. The problem actually also occurs on my previous win32 projects, but it seems like the delay there is much bigger (Windows inefficiencies possibly to blame) so I never noticed until now
        if (enabled || (enabled = absolute_value(EndCounter(registration_time)) > 250/*milliseconds*/)) //NOTE(fran): we check the absolute value because of the edge case where the counter overflows and wraps around
        {
            EnableRendering(ui);

            on_triggered.action(element, on_triggered.context);
        }
    }
}

internal void OnClose(ui_state* ui, ui_input* input)
{
    if (input->close)
        safe_call(ui->OnClose.action, nil, ui->OnClose.context);
}

void UnregisterGlobalHotkey(ui_state* ui, ui_hotkey hotkey)
{
    //NOTE(fran): both Register & Unregister global hotkey functions need to be called from the input thread since hotkey msgs are posted to the thread that called the functions

    struct _unreg_hk { OS::window_handle wnd; i32 id; b32 ret; } volatile unreg{ .wnd = ui->wnd, .id = hotkey.id, .ret = -10 };

    ::QueueUserAPC( //TODO(fran): in the end SendMessage might simply be a better option save for the caveat of having to define custom messages in the os code
        [](ULONG_PTR args) {
            _unreg_hk* unreg = (decltype(unreg))args; 
            unreg->ret = UnregisterSystemGlobalHotkey(unreg->wnd, unreg->id);
        }
        , iu::get_state()->io_thread, (ULONG_PTR)&unreg);
    
    while (unreg.ret == -10) {}

    if(unreg.ret)
        ui->global_registered_hotkeys[hotkey.id] = {0};
}

b32 RegisterGlobalHotkey(ui_state* ui, ui_hotkey hotkey, ui_action on_hotkey_triggered, ui_element* element)
{
    UnregisterGlobalHotkey(ui, hotkey);
    
    struct _reg_hk { OS::window_handle wnd; ui_hotkey hotkey; b32 res; } volatile reg{ .wnd = ui->wnd, .hotkey = hotkey, .res = -10 };

    ::QueueUserAPC(
        [](ULONG_PTR args) {
            _reg_hk* reg = (decltype(reg))args;
            reg->res = RegisterSystemGlobalHotkey(reg->wnd, *reg->hotkey.hk, reg->hotkey.id); //TODO(fran): we may want to leave the hwnd as nil and say that global hotkeys are gonna be iu global (inside iu_state) & outside the ui_state
        }
    , iu::get_state()->io_thread, (ULONG_PTR)&reg);

    while (reg.res == -10) {}

    if (reg.res)
        ui->global_registered_hotkeys[hotkey.id] = {.on_triggered= on_hotkey_triggered, .element=element, .registration_time=StartCounter()};
    return reg.res;
}

internal void UpdateKeyboardFocus(ui_state* ui, ui_input* input)
{
    if (ui_element* element = ui->keyboard_focus; element)
    {
        switch (element->type)
        {
            case ui_type::hotkey:
            {
                auto& data = element->data.hotkey;
                if (input->hotkey.has_hotkey())
                {
                    if (data.current_hotkey.hk->is_different(input->hotkey))
                    {
                        EnableRendering(ui);
                        //TODO(fran): flag for defining system global & application/window global hotkeys (if the window is focused (has kb input) they get triggered even if no keyboard focus is on the element)
                        *data.current_hotkey.hk = input->hotkey;
                        b32 registered = RegisterGlobalHotkey(ui, data.current_hotkey, data.on_hotkey, element);
                        
                        if (registered)
                            data.string_state = hotkey_string_state::validhk;
                        else
                            data.string_state = hotkey_string_state::invalidhk;
                    }
                }
            } break;
        }
    }
}

internal void UpdateTray(ui_state* ui, ui_input* input)
{
    if (input->tray.on_unclick)
    {
        EnableRendering(ui);
        safe_call(ui->tray.on_unclick.action, nil, ui->tray.on_unclick.context);
    }
    if (input->tray.on_unrclick)
    {
        EnableRendering(ui);
        safe_call(ui->tray.on_unrclick.action, nil, ui->tray.on_unrclick.context);
    }
}

internal void InputProcessing(ui_state* ui, ui_element* next_hot, ui_input* input)
{
    UpdateUnderTheMouse(ui, next_hot, input);
    if (ui->interacting_with)
    {
        if (IsUnclicked(input->keys[input_key::left_mouse]))
        {
            EndInteraction(ui, input);
        }
        else ContinueInteraction(ui, input);
    }
    else if (IsClicked(input->keys[input_key::left_mouse]) && next_hot)
    {
        BeginInteraction(ui, next_hot, input);
        //TODO(fran): OS::capture_mouse()
    }
    if(IsWindowVisible(ui->wnd))//TODO(fran): HACK: I had to add this condition to cover the stupid edge case where the user is registering a hotkey but does not release the keys and causes a hotkey triggered event to happen. Strange things happened in that case which I couldnt quite decipher and the end result was that the hotkey wouldnt work until the user clicked on the screen
        UpdateKeyboardFocus(ui, input);

    UpdateGlobalHotkeys(ui, input); 
    UpdateTray(ui, input); 

    OnClose(ui, input);
}

struct GetColorsForInteractionState_res { v4 fg, bk, bd; };
internal GetColorsForInteractionState_res GetColorsForInteractionState(struct button_theme::color* colors, interaction_state interaction_st )
{
    GetColorsForInteractionState_res res;
    res.fg = colors->foreground.E[(i32)interaction_st];
    res.bk = colors->background.E[(i32)interaction_st];
    res.bd = colors->border.E[(i32)interaction_st];
    return res;
}

struct SliderGetColorsForInteractionState_res { v4 track_empty, track_fill, thumb; };
internal SliderGetColorsForInteractionState_res GetColorsForInteractionState(struct slider_theme::color* colors, interaction_state interaction_st)
{
    SliderGetColorsForInteractionState_res res;
    res.track_empty = colors->track_empty.E[(i32)interaction_st];
    res.track_fill = colors->track_fill.E[(i32)interaction_st];
    res.thumb = colors->thumb.E[(i32)interaction_st];
    return res;
}

struct BkGetColorsForInteractionState_res { v4 bk, bd; };
internal BkGetColorsForInteractionState_res GetColorsForInteractionState(struct background_theme::color* colors, interaction_state interaction_st)
{
    BkGetColorsForInteractionState_res res;
    res.bk = colors->background.E[(i32)interaction_st];
    res.bd = colors->border.E[(i32)interaction_st];
    return res;
}

internal GetColorsForInteractionState_res GetColorsForInteractionState(struct hotkey_theme::color* colors, interaction_state interaction_st, hotkey_string_state string_st)
{
    GetColorsForInteractionState_res res;
    if (string_st == hotkey_string_state::placeholder)
        res.fg = colors->foreground_placeholder.E[(i32)interaction_st];
    else if (string_st == hotkey_string_state::validhk)
        res.fg = colors->foreground_validhk.E[(i32)interaction_st];
    else if (string_st == hotkey_string_state::invalidhk)
        res.fg = colors->foreground_invalidhk.E[(i32)interaction_st];
    else crash();
    //TODO(fran): also change bk and border color based on string_st
    res.bk = colors->background.E[(i32)interaction_st];
    res.bd = colors->border.E[(i32)interaction_st];
    return res;
}

internal void RenderBackground(iu::ui_renderer* r, ui_element* element, v4 bk, ui_style style)
{
    //TODO(fran): border

    switch (style)
    {
    case ui_style::rect:
    {
        Rectangle(r, element->placement, bk);
    } break;
    case ui_style::round_rect:
    {
        //TODO(fran): better radius calculation, depending on the dimensions of the button we end up with almost circles
        f32 Xradius = minimum(element->placement.w, element->placement.h) * .5f;
        f32 Yradius = Xradius;
        RoundRectangle(r, element->placement, { Xradius, Yradius }, bk);
    } break;
    case ui_style::circle:
    {
        f32 radius = minimum(element->placement.w, element->placement.h);
        Ellipse(r, element->placement.center(), { radius, radius }, bk);
    } break;
    default: { crash(); }break;
    }
}

internal rc2 GetImageRc(rc2 bounds, f32 imgW, f32 imgH)
{
    rc2 res;

    f32 aspect_ratio = imgW / imgH;
    f32 w = safe_ratio0(bounds.w, aspect_ratio);
    f32 h = bounds.h * aspect_ratio;

    res = get_centered_rc(bounds, w, h); //TODO(fran): we currently only support vertical & horizontal centering mode
    return res;
}

internal void RenderImage(iu::ui_renderer* r, ui_image image, rc2 placement, v4 col)
{
    auto renderer = r->renderer2D.deviceContext;

    D2D1_BITMAP_PROPERTIES1 props = {
        .pixelFormat = {DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, //NOTE(fran): direct2d only works with premultiplied alpha
        .dpiX = 0, //TODO(fran): dpi
        .dpiY = 0,
        .bitmapOptions = D2D1_BITMAP_OPTIONS_NONE,
        .colorContext = nil, //TODO(fran)
    };

    switch (image.type)
    {
    case ui_image_type::img:
    {
        //TODO(fran): hash table to store already generated images
        ID2D1Bitmap1* img; defer{ d3d_SafeRelease(img); };
        TESTHR(renderer->CreateBitmap({ image.img->w,image.img->h }, image.img->mem, image.img->w * (image.img->bpp / 8), props, &img)
            , "Failed to create Direct2D bitmap");

        rc2 imgrc = GetImageRc(placement, image.img->w, image.img->h);

        renderer->DrawBitmap(img, rc2_to_D2DRECT(imgrc));
    } break;
    case ui_image_type::mask:
    {
        ID2D1Bitmap1* img; defer{ d3d_SafeRelease(img); };
        TESTHR(renderer->CreateBitmap({ image.img->w,image.img->h }, image.img->mem, image.img->w * (image.img->bpp / 8), props, &img)
            , "Failed to create Direct2D bitmap");

        ID2D1SolidColorBrush* fg_br{ 0 }; defer{ d3d_SafeRelease(fg_br); };
        auto brush_props = D2D1::BrushProperties(col.a);
        renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &brush_props, &fg_br);

        auto oldmode = renderer->GetAntialiasMode(); defer{ renderer->SetAntialiasMode(oldmode); }; //NOTE(fran): FillOpacityMask requires Aliased Mode
        renderer->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

        rc2 imgrc = GetImageRc(placement, image.img->w, image.img->h);

        renderer->FillOpacityMask(img, fg_br, rc2_to_D2DRECT(imgrc));
    } break;
    case ui_image_type::render_commands:
    {
        ID2D1SolidColorBrush* fg_br{ 0 }; defer{ d3d_SafeRelease(fg_br); };
        auto brush_props = D2D1::BrushProperties(col.a);
        renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &brush_props, &fg_br);

        image.render_commands.commands(renderer, fg_br, placement, image.render_commands.context);
    } break;
    case ui_image_type::none: {} break;
#ifdef DEBUG_BUILD //TODO(fran): does adding the default case impact performance?
    default: { crash(); } break;
#endif
    }
}

internal void RenderElement(ui_state* ui, ui_element* element) 
{
    //TODO(fran): get rid of the recursion
    //TODO(fran): translation transformation before going to the render code, in order to not have to correct for the xy position (renderer->SetTransform), we gonna need to create two rc2s for each element so we can have both the transformed and non transformed rect
    while (element) //TODO(fran): should this while loop be here or in RendererDraw?
    {
        auto r = &ui->renderer;
        auto font = ui->renderer.renderer2D.font;
        
        switch (element->type)
        {
        case ui_type::button:
        {
            auto data = element->data.button;

            auto [fg, bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            assert(data.theme.dimension.border_thickness == 0);
            RenderBackground(r, element, bk, data.theme.style);

            s8 text = GetUIStringStr(ui, data.text);
                
            RenderText(r, font, text.chars, text.cnt, fg, element->placement, horz_text_align::center, vert_text_align::center, true);

            if (text.cnt == 0) //TODO(fran): buttons with text + image
            {
                rc2 img_bounds = scalefromcenter_rc(element->placement, data.image.bounds.scale_factor);
                RenderImage(r, data.image, img_bounds, fg);
            }

        } break;
        case ui_type::slider:
        {
            auto& data = element->data.slider;

            auto [track_empty_color, track_fill_color, thumb_color] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            //TODO(fran): better coloring based not only on interaction state but collision testing too, check whether the mouse is in the empty/fill/thumb region and only change the color for that one, leave the others on normal

            assert(data.theme.thumb_style == ui_style::circle);
            assert(data.theme.track_style == ui_style::round_rect);

            data.calculate_colliders(element->placement);

            sz2 radius = {
                .w = minimum(data.colliders.full_track.w, data.colliders.full_track.h) * .5f,
                .h = radius.w,
            };
            RoundRectangle(r, data.colliders.empty_track, radius, track_empty_color);

            RoundRectangle(r, data.colliders.fill_track, radius, track_fill_color);

            Ellipse(r, data.colliders.thumb.center(), data.colliders.thumb.wh * .5f, thumb_color);

        } break;
        case ui_type::sizer: //TODO(fran): we could use the sizer as a poor man's way of implementing and rendering backgrounds
        {
            auto& data = element->data.sizer;
#ifdef DEBUG_BUILD
            v4 bk_color = data.bk_color;
            v4 aura_col = V4( bk_color.rgb, bk_color.a * .2f);
            
            rc2 _aura_rect = element->placement;// inflate_rc(element->placement, 1);

            #if 1
            f32 linewidth = (element == ui->under_the_mouse) ? 3 : 1; //TODO(fran): this dont work, we need to implement reverse traversal in order to find the sizer that directly controls the under the mouse element
            RectangleOutline(r, _aura_rect, linewidth, bk_color);
            #else
            Rectangle(r, element->placement, bk_color);
            Rectangle(r, _aura_rect, aura_col);
            #endif
#endif
            ui_sizer_element* superchild = data.childs;
            while (superchild) {
                ui_element* child = superchild->element;
                while (child) {
                    RenderElement(ui, child);
                    child = child->child;
                }
                superchild = superchild->next;
            }
        } break;
        case ui_type::subelement_table:
        {
            auto& data = element->data.subelement_table;

#ifdef DEBUG_BUILD
            f32 linewidth = 1;
            v2 line_start = element->placement.xy;
            v4 bk_color = data.bk_color;
#endif

            auto superchild = data.childs;
            while (superchild) {
                ui_element* child = superchild->element;
                while (child) {
                    RenderElement(ui, child);
                    child = child->child;
                }

#ifdef DEBUG_BUILD
                //NOTE(fran): since we are doing the rendering in place part of the line will get occluded by the next child that gets rendered, not the end of the world, but should be solved & fixed
                //TODO(fran): render queue, simply for things we want rendered after everything else or after our element
                line_start.y += superchild->max_h;
                v2 line_end = { element->placement.right(), line_start.y };
                Line(r, line_start, line_end, linewidth, bk_color);
#endif

                superchild = superchild->next;
            }

#ifdef DEBUG_BUILD
            {
                linewidth *= .5f; //NOTE(fran): correction to get close to the width of the semi-occluded horizontal lines
                v2 line_start{ element->placement.x, element->placement.y };
                v2 line_end{ element->placement.x, element->placement.bottom() };

                for (auto& h : data.w_max_sizes)
                {
                    line_start.x += h;
                    line_end.x += h;
                    Line(r, line_start, line_end, linewidth, bk_color);
                }

                RectangleOutline(r, element->placement, linewidth, bk_color);
            }
#endif

        } break;
        case ui_type::hpad:
        case ui_type::vpad:
        {
#ifdef DEBUG_BUILD
            v4 bk_color = { .9f,.2f,.1f,7.f };

            f32 line_width = 1;

            v2 p1 = element->placement.xy;
            v2 p2 = element->placement.bottom_right();

            v2 p3 = element->placement.top_right();
            v2 p4 = element->placement.bottom_left();

            Line(r, p1, p2, line_width, bk_color);
            Line(r, p3, p4, line_width, bk_color);
            RectangleOutline(r, element->placement, line_width, bk_color);
#endif
        } break;
        case ui_type::hotkey:
        {
            auto data = element->data.hotkey;

            auto [fg, bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st, data.string_state);

            assert(data.theme.style == ui_style::round_rect);

            //TODO(fran): better radius calculation, depending on the dimensions we end up with almost circles
            f32 Xradius = minimum(element->placement.w, element->placement.h) * .5f;
            f32 Yradius = Xradius;
            RoundRectangle(r, element->placement, { Xradius, Yradius }, bk);

            s8 text{0};
            if (data.string_state != hotkey_string_state::placeholder)
            {
                text = stack_s(decltype(text), 128);
                OS::HotkeyToString(*data.current_hotkey.hk, &text);
            }
            else {
                text = GetUIStringStr(ui, data.placeholder_text);
            }
            
            rc2 text_rc_bk = MeasureText(font, text.chars, text.cnt, element->placement, horz_text_align::center, vert_text_align::center, true);

            sz2 avg_char = MeasureAverageTextCharacter(font);

            text_rc_bk = scalefromcenterconst_rc(text_rc_bk, avg_char);

            Xradius = minimum(text_rc_bk.w, text_rc_bk.h) * .25f;
            Yradius = Xradius;
            v4 text_rect_bk_col = bk * .5f; //TODO(fran): add to hotkey theme?
            RoundRectangle(r, text_rc_bk, { Xradius, Yradius }, text_rect_bk_col);

            RenderText(r, font, text.chars, text.cnt, fg, element->placement, horz_text_align::center, vert_text_align::center, true);

            assert(data.theme.dimension.border_thickness == 0);

        } break;
        case ui_type::background:
        {
            auto data = element->data.background;

            auto [bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            assert(data.theme.dimension.border_thickness == 0);
            RenderBackground(r, element, bk, data.theme.style);
        } break;
        case ui_type::contextmenu_button:
        {
            auto data = element->data.contextmenu_button;

            auto [fg, bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            assert(data.theme.dimension.border_thickness == 0);
            RenderBackground(r, element, bk, data.theme.style);

            //TODO(fran): either store or retrieve the placement for all subelements, instead of this HACK
            sz2 avgchar = MeasureAverageTextCharacter(font);

            f32 min_bound = minimum(element->placement.w, element->placement.h);
            rc2 img_placement{ .x=element->placement.x + avgchar.w, .y=element->placement.y, .w=min_bound, .h=min_bound };
            rc2 img_bounds = scalefromcenter_rc(img_placement, data.image.bounds.scale_factor);
            RenderImage(r, data.image, img_bounds, fg);

            s8 text = GetUIStringStr(ui, data.text);
            rc2 text_placement =
            {
                .x = img_placement.right() + avgchar.w,
                .y = element->placement.y,
                .w = distance(element->placement.right(), text_placement.x),
                .h = element->placement.h,
            };
            rc2 text_output_rc = RenderText(r, font, text.chars, text.cnt, fg, text_placement, horz_text_align::left, vert_text_align::center, true, true);

            rc2 hotkey_text_placement =
            {
                .x = text_output_rc.right(),
                .y = element->placement.y,
                .w = distance(element->placement.right(), hotkey_text_placement.x) - avgchar.w,
                .h = element->placement.h
            };
            s8 hotkey = stack_s(decltype(hotkey), 100);
            OS::HotkeyToString(data.hotkey, &hotkey);
            RenderText(r, font, hotkey.chars, hotkey.cnt, fg, hotkey_text_placement, horz_text_align::right, vert_text_align::center, true); //TODO(fran): on Windows horizontal alignment should actually be left aligned & placed after the longest text element in the context menu

        } break;
        default: crash();
        }

        element = element->child;
    }
}

internal void TESTRenderMouse(iu::ui_renderer* r, v2 mouseP)
{ //test draw mouse
    auto font = r->renderer2D.font;

    v4 mouse_col = v4{ 1,1,1,1 };

    f32 diameter = 10;
    auto mouse_rc = rc_center_diameter(mouseP, { diameter, diameter });
    Ellipse(r, mouse_rc.center(), mouse_rc.wh * .5f, mouse_col);

    utf8 mousep[20];
    auto mousepcnt = snprintf((char*)mousep, ArrayCount(mousep), "(%.2f,%.2f)", mouseP.x, mouseP.y);
    iu::RenderText(r, font, mousep, mousepcnt, mouse_col, rc2_from(mouseP, { 200,100 }), horz_text_align::left, vert_text_align::top, true);
}

internal void RenderUI(ui_state* ui)
{
    BeginRender(&ui->renderer, ui->placement.wh);
    
    ui_element* element = ui->elements;

    RenderElement(ui, element);

    if constexpr (const b32 render_mouse = false; render_mouse) TESTRenderMouse(&ui->renderer, ui->input.mouseP);

    EndRender(&ui->renderer);
}

internal f32 calc_nonclient_caption_h(f32 dpi_scaling) //TODO(fran): OS code
{
    f32 res;

    RECT testrc{ 0,0,100,100 }; 
    RECT ncrc = testrc;

    local_persistence f32 dpicorrection = OS::GetScalingForSystem(); assert(dpicorrection);
    //NOTE(fran): AdjustWindowRect retains the dpi of the system when the app started, therefore if we started on a 125% dpi system AdjustWindowRect will _always_ return the value scaled by 1.25f, and so on, in order to correct for that we retrieve the system dpi at the time of our app starting and divide by it to obtain the unscaled result
        
    //TODO(fran): AdjustWindowRectExForDpi
    AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE/*no menu*/);
    f32 h = (distance(testrc.top, ncrc.top) / dpicorrection) * dpi_scaling;

    res = h;

    return res;
}

struct measure_res {
    sz2 measurement;
    struct {
        sz2 avgchar;
    } metrics;
};
internal measure_res MeasureElementText(ui_state* ui, ui_element* elem)
{
    measure_res res;
    s8 text;
    switch (elem->type)
    {
        case ui_type::button:
        {
            text = GetUIStringStr(ui, elem->data.button.text);
        } break;
        case ui_type::sizer:
        {
            text = const_temp_s(u8"");
        } break;
        case ui_type::contextmenu_button:
        {
            auto& data = elem->data.contextmenu_button;
            //TODO(fran): subelements, we have icon+text+text. Also we should add the differentiation between MeasureElementText and a more general MeasureElement
            local_persistence utf8 _text[100];
            text = { .chars = _text, .cnt_allocd = ArrayCount(_text) };
            text += u8"____"; //space for the icon
            text += GetUIStringStr(ui, data.text);
            OS::HotkeyToString(data.hotkey, &text);
        } break;
        default: { crash(); } break;
    }

    //NOTE(fran): when text.cnt is 0 MeasureText still returns the text height for one line of text. I'd think we probably want a special condition that says if (text.cnt == 0) measurement.wh = {0,0} but currently we are actually taking advantage of this text height to resize font height dependent vertical constraint sizers

    res.measurement = MeasureText(ui->renderer.renderer2D.font, text.chars, text.cnt, { 0,0,F32MAX,F32MAX }, horz_text_align::left, vert_text_align::top).wh;

    res.metrics.avgchar = MeasureAverageTextCharacter(ui->renderer.renderer2D.font);
    return res;
}

internal f32 GetElementW(sz2 bounds, element_sizing_desc sz_desc, ui_element* elem, ui_state* ui)
{
    //horizontal
    f32 w;
    switch (sz_desc.type)
    {
        case element_sizing_type::bounds:
        {
            w = bounds.w * sz_desc.bounds.scale_factor;
        } break;
        case element_sizing_type::opposite_bounds:
        {
            w = bounds.h * sz_desc.bounds.scale_factor;
        } break;
        case element_sizing_type::font:
        {
            auto [measurement, metrics] = MeasureElementText(ui, elem);
            w = measurement.w + metrics.avgchar.w * sz_desc.font.w_extra_chars;
        } break;
        case element_sizing_type::font_clamp_to_bounds:
        {
            auto [measurement, metrics] = MeasureElementText(ui, elem);
            w = minimum(measurement.w + metrics.avgchar.w * (f32)sz_desc.font.w_extra_chars, bounds.w);
        } break;
        //TODO(fran): remaining sizing for horizontal sizers
        default: { crash(); } break;
    }
    return w;
}

internal f32 GetElementH(sz2 bounds, element_sizing_desc sz_desc, ui_element* elem, ui_state* ui, b32 resize_remaining = false, f32 remaining_sizing_h = 0.f, u32* remaining_sizing_cnt = nil, b32* skip = nil /*TODO(fran): is skipping really necessary?*/)
{
    //vertical
    f32 h;
    switch (sz_desc.type)
    {
        case element_sizing_type::bounds:
        {
            h = bounds.h * sz_desc.bounds.scale_factor;
        } break;
        case element_sizing_type::opposite_bounds:
        {
            h = bounds.w * sz_desc.bounds.scale_factor;
        } break;
        case element_sizing_type::font:
        {
            auto [measurement, metrics] = MeasureElementText(ui, elem);
            h = measurement.h * sz_desc.font.v_scale_factor;
        } break;
        case element_sizing_type::font_clamp_to_bounds:
        {
            auto [measurement, metrics] = MeasureElementText(ui, elem);
            h = minimum(measurement.h * sz_desc.font.v_scale_factor, bounds.h);
        } break;
        case element_sizing_type::os_non_client_top:
        {
            h = calc_nonclient_caption_h(ui->scaling);
        } break;
        case element_sizing_type::remaining:
        {
            if (resize_remaining)
            {
                h = remaining_sizing_h;
            }
            else
            {
            #ifdef DEBUG_BUILD //avoid runtime complains about variable not being initialized
                h = 0;
            #endif
                (*remaining_sizing_cnt)++; //IMPORTANT INFO(fran): the parenthesis are _required_, otherwise the sum is completely wrong, no clue why
                *skip = true;
            }
        } break;
        default: { crash(); } break;
    }
    return h;
}

enum class ui_subelement_type {
    text=0,
    image,
    hotkey,
};

struct ui_subelement {
    ui_subelement_type type;
    union {
        ui_string text;
        ui_image image;
        ui_hotkey hotkey;
    };
};

ui_subelement SubE(const ui_string& text) { return { .type = ui_subelement_type::text, .text = text }; }
ui_subelement SubE(const ui_image& image) { return { .type = ui_subelement_type::image, .image = image }; }
ui_subelement SubE(const ui_hotkey& hotkey) { return { .type = ui_subelement_type::hotkey, .hotkey= hotkey }; }
ui_subelement SubE(OS::hotkey_data& hotkey_data) { return { .type = ui_subelement_type::hotkey, .hotkey = {.hk=&hotkey_data, .id=0} }; }

sz2 GetSubelementWH(sz2 bounds, element_sizing_desc2 sz_desc, const ui_subelement* elem, ui_state* ui)
{
    //TODO(fran): use sz_desc
    assert(sz_desc.h.type == element_sizing_type::font);
    assert(sz_desc.w.type == element_sizing_type::font);

    sz2 res;
    switch (elem->type)
    {
        case ui_subelement_type::text:
        {
            s8 text = GetUIStringStr(ui, elem->text);

            sz2 measurement = MeasureText(ui->renderer.renderer2D.font, text.chars, text.cnt, rc2_from({ 0,0 }, bounds), horz_text_align::left, vert_text_align::top, true).wh;

            sz2 avgchar = MeasureAverageTextCharacter(ui->renderer.renderer2D.font);

            res = { measurement.w + sz_desc.w.font.w_extra_chars * avgchar.w, measurement.h * sz_desc.h.font.v_scale_factor };

        } break;
        case ui_subelement_type::image:
        {
            assert(sz_desc.h.type == element_sizing_type::font);

            s8 text = const_temp_s(u8"_");
            sz2 measurement = MeasureText(ui->renderer.renderer2D.font, text.chars, text.cnt, rc2_from({ 0,0 }, bounds), horz_text_align::left, vert_text_align::top, true).wh;
            //NOTE(fran): here measurement acts as both measurement & avgchar

            res.h = measurement.h * sz_desc.h.font.v_scale_factor;
            res.w = res.h + sz_desc.w.font.w_extra_chars * measurement.w;

        } break;
        case ui_subelement_type::hotkey:
        {
            s8 text = stack_s(s8, 100);
            OS::HotkeyToString(*elem->hotkey.hk, &text);

            sz2 measurement = MeasureText(ui->renderer.renderer2D.font, text.chars, text.cnt, rc2_from({ 0,0 }, bounds), horz_text_align::left, vert_text_align::top, true).wh;

            sz2 avgchar = MeasureAverageTextCharacter(ui->renderer.renderer2D.font);

            res = { measurement.w + sz_desc.w.font.w_extra_chars * avgchar.w, measurement.h * sz_desc.h.font.v_scale_factor };

        } break;
        default: { crash(); } break;
    }
    return res;
}

//TODO(fran): separate resize & getbottom functions with if constexpr (do_resize), we're probably gonna need to use templates: template <b32 do_resize>. Do it after the whole thing has been completely implemented and debugged, debugging templates is never fun
//TODO(fran): support for placing things inside others. That would be a good use for the 'next' pointer inside ui_element, to function as 'childs' (better change the name), and have there a sizer that receives the rectangle of the parent element as the bounds
//TODO(fran): can I somehow avoid passing the veil_ui? I simply need it to measure some fonts
internal v2 _GetElementBottomRight(ui_state* ui, const rc2 bounds, ui_element* element, b32 do_resize) 
{
    //TODO(fran): while loop over element->next in order to have individual root elements, may be better as a separate specific function, since the rectangles should be handled separately for each root

    //TODO(fran): left, right and center alignment, maybe too top and bottom

    switch (element->type) {
        case ui_type::sizer:
        {
            auto& data = element->data.sizer;
            assert(data.childs);
            assert(element->child == nil); //NOTE(fran): sizers cant have childs
            if (data.properties.is_horizontal)
            {
                v2 res = bounds.p;

                v2 xy;
                switch (data.properties.alignment)
                {
                    case sizer_alignment::left:
                    {
                        xy = bounds.p;
                    } break;
                    case sizer_alignment::center:
                    case sizer_alignment::right:
                    {
                        f32 total_w = 0;
                        ui_sizer_element* superchild = data.childs;
                        while (superchild) {
                            ui_element* child = superchild->element;
                            if(child) {
                            //while (child) {
                                f32 w = GetElementW(bounds.wh, superchild->sizing_desc, child, ui), h = bounds.h;

                                rc2 new_bounds{ 0,0,w,h };
                                total_w += _GetElementBottomRight(ui, new_bounds, child, false).x;

                                //child = child->next;
                            }
                            superchild = superchild->next;
                        }
                        if(data.properties.alignment == sizer_alignment::center)
                            xy = { bounds.x + (bounds.w - total_w) * .5f, bounds.y };
                        else  /*right aligned*/
                            xy = { bounds.x + (bounds.w - total_w), bounds.y };
                        //TODO(fran): idk whether right alignment should also incur reverse placement, aka first element is the rightmost one, for now the first element is still leftmost, maybe we could provide a flag to place reversed?
                    } break;
                    default: { crash(); } break;
                }

#ifdef DEBUG_BUILD
                v2 stored_xy = xy;
#endif

                ui_sizer_element* superchild = data.childs;
                while (superchild) 
                {
                    ui_element* child = superchild->element;
                    if(child) {
                    //while (child) {
                        //TODO(fran): not really sure if I want ui_element to also have a 'next', it seems like the usual case is the ui_sizer_element would have a 'next' for each individual element added to the sizer. Otherwise we could simply take them as possibly debug items and just take whatever size they already got and just place them next
                        f32 w = GetElementW(bounds.wh, superchild->sizing_desc, child, ui), h = bounds.h;

                        rc2 new_bounds{ xy.x,xy.y,w,h };

                        res.y = maximum(res.y, _GetElementBottomRight(ui, new_bounds, child, do_resize).y); //TODO(fran): I think this is wrong, we're possibly returning y values smaller than bounds.bottom(), a better idea would be maximum(res.y,bounds.bottom()) after the end of the loop. We could also provide the current "wrong" behaviour as an additional flag

                        xy.x += w;
                        res.x = xy.x;

                        //child = child->next;
                    }
                    superchild = superchild->next;
                }
#ifdef DEBUG_BUILD
                if(do_resize) element->placement = rc2_from(stored_xy, res - stored_xy); //IMPORTANT(fran): remember that 'res' is the xy coord for the bottom right, _not_ the width and height
#endif
                return res;
            }
            else //vertical sizer
            {

                u32 remaining_sizing_cnt = 0;
                b32 resize_remaining=false;
                u32 remaining_sizing_h = 0;
                resize_v:
                v2 xy;

                switch (data.properties.alignment)
                {
                    case sizer_alignment::top:
                    {
                        xy = bounds.p;
                    } break;
                    case sizer_alignment::center:
                    case sizer_alignment::bottom:
                    {
                        f32 total_h = 0;
                        ui_sizer_element* superchild = data.childs;
                        while (superchild) {
                            ui_element* child = superchild->element;
                            if (child) {
                                //while (child) {

                                f32 w = bounds.w, h = GetElementH(bounds.wh, superchild->sizing_desc, child, ui);

#if 0 //TODO(fran): I have the feeling I should use this path
                                rc2 new_bounds{ 0,0,w,h };
                                total_h += _GetElementBottomRight(ui, new_bounds, child, false).y;
#else
                                total_h += h;
#endif
                                //child = child->next;
                            }
                            superchild = superchild->next;
                        }
                        if (data.properties.alignment == sizer_alignment::center)
                            xy = { bounds.x, bounds.y + (bounds.h - total_h) * .5f};
                        else  /*bottom aligned*/
                            xy = { bounds.x, bounds.y + (bounds.h - total_h) };

                    } break;
                    default: { crash(); } break;
                }

#ifdef DEBUG_BUILD
                v2 stored_xy = xy;
#endif
                
                ui_sizer_element* superchild = element->data.sizer.childs;
                while (superchild) {
                    ui_element* child = superchild->element;
                    if (child) {
                    //while (child) {
                        b32 skip = false;

                        f32 w = bounds.w;
                        f32 h;

                        h = GetElementH(bounds.wh, superchild->sizing_desc, child, ui, resize_remaining, remaining_sizing_h, &remaining_sizing_cnt, &skip);
                        if (skip) goto skip_v;

                        rc2 new_bounds{ xy.x,xy.y,w,h };

                        if(do_resize) _GetElementBottomRight(ui, new_bounds, child, do_resize);

                        xy.y += h; //NOTE(fran): interstingly I dont go check my childs when looking for the bottom

                        //child = child->next;
                    }
                    skip_v:
                    superchild = superchild->next;
                }

                if (remaining_sizing_cnt && !resize_remaining)
                {
                    resize_remaining = true;
                    remaining_sizing_h = maximum((bounds.h - (xy.y-bounds.y)),0.f)/ (f32)remaining_sizing_cnt;
                    goto resize_v;
                }

                xy.x = bounds.right(); //TODO(fran): is it ok that we completely ignore the x component that our childs return?

#ifdef DEBUG_BUILD
                if (do_resize) element->placement = rc2_from(stored_xy, xy - stored_xy);
#endif

                return xy;
            }
        } break;
        case ui_type::vpad:
        {
            v2 res = { bounds.left, bounds.bottom() };
#ifdef DEBUG_BUILD
            v2 thick_res = { bounds.right(), bounds.bottom() };
            if (do_resize) element->placement = rc2_from(bounds.p, thick_res-bounds.p);
#endif
            return res;
        } break;
        case ui_type::hpad:
        {
            v2 res = { bounds.right(), bounds.top };
#ifdef DEBUG_BUILD
            v2 thick_res = { bounds.right(), bounds.bottom() };
            if (do_resize) element->placement = rc2_from(bounds.p, thick_res-bounds.p);
#endif
            return res;
        } break;
        case ui_type::subelement_table:
        {
            auto& data = element->data.subelement_table;

            v2 xy = bounds.p;

#ifdef DEBUG_BUILD
            v2 stored_xy = xy;
#endif

            ui_subelement_table_element* superchild = data.childs;
            auto& columns = data.w_max_sizes;
            columns.clear();
            columns.cnt = data.columns; assert(columns.cnt);
            while (superchild)
            {
                ui_element* child = superchild->element;
                if (child) {
                    superchild->max_h = 0;
                    switch (child->type)
                    {
                        case ui_type::contextmenu_button:
                        {
                            auto& child_data = child->data.contextmenu_button;

                            ui_subelement subelems[] = { SubE(child_data.image), SubE(child_data.text), SubE(child_data.hotkey) };
                            assert(ArrayCount(subelems) <= columns.cnt);

                            for (u32 i = 0; i < ArrayCount(subelems); i++)
                            {
                                sz2 dim = GetSubelementWH(bounds.wh, superchild->sizing_desc[i], &subelems[i], ui);
                                superchild->max_h = maximum(dim.h, superchild->max_h);
                                columns[i] = maximum(dim.w, columns[i]);
                            }
                        } break;
                        default: { crash(); } break;
                    }
                }
                superchild = superchild->next;
            }
            
            f32 w=0;
            for (auto subw : columns) w += subw;

            superchild = data.childs;
            while (superchild)
            {
                ui_element* child = superchild->element;
                if (child) {
                    
                    f32 h = superchild->max_h;

                    rc2 new_bounds{ xy.x,xy.y,w,h };

                    xy.y = maximum(xy.y, _GetElementBottomRight(ui, new_bounds, child, do_resize).y); //TODO(fran): the max check seems unnecessary at best and wrong when elements go beyond the height of the bounds
                }
                superchild = superchild->next;
            }

            xy.x += w;//NOTE(fran): only at the end do we move in x //TODO(fran): we should add extra info to the return value, eg use an rc2, so that the next guy has the knowledge of where to start depending on its situation, if it wants to go right of the element it queries for the right pos, if it wants to go below it can query that too

#ifdef DEBUG_BUILD
            if (do_resize) element->placement = rc2_from(stored_xy, xy - stored_xy);
#endif
            return xy;

        } break;
        default:
        {
            v2 xy = { bounds.x, bounds.y };
            f32 w = bounds.w, h = bounds.h;
            if (do_resize) element->placement = { xy.x, xy.y, w, h };
            xy.y += h; //move next target past ourselves
            xy.x = bounds.right();

            if (ui_element* child=element->child)
            {
                //Resize childs (childs are placed inside the bounds of the element)
                assert(child->type == ui_type::sizer);
                ui_element* parent = element;
                _GetElementBottomRight(ui, parent->placement, child, do_resize);
            }

            return xy;
        } break;
    }
}
internal v2 ResizeElement(ui_state* ui, rc2 bounds, ui_element* element)
{
    TIMEDFUNCTION();
    v2 res;
    if (ui->is_context_menu)
    {
        sz2 new_bounds = _GetElementBottomRight(ui, { 0,0,F32MAX,F32MAX }, element, true);
        if ((i32)ui->placement.w != (i32)new_bounds.w || (i32)ui->placement.h != (i32)new_bounds.h)
        {
            OS::MoveWindow(ui->wnd, rc2_from(OS::GetWindowScreenRc(ui->wnd).xy, new_bounds));
            //TODO(fran): update ui->placement?
        }
        res = new_bounds;
    }
    else res = _GetElementBottomRight(ui, bounds, element, true);
    return res;
}

internal void PrintNextHot(input_results* input_res)
{
    if (input_res->next_hot)
        switch (input_res->next_hot->type)
        {
            case ui_type::sizer: OutputDebugStringA("Sizer\n"); break;
            case ui_type::vpad: OutputDebugStringA("Vpad\n"); break;
            case ui_type::hpad: OutputDebugStringA("Hpad\n"); break;
            case ui_type::background: OutputDebugStringA("Background\n"); break;
            case ui_type::button: OutputDebugStringA("Button\n"); break;
            case ui_type::slider: OutputDebugStringA("Slider\n"); break;
            case ui_type::hotkey: OutputDebugStringA("Hotkey\n"); break;
            case ui_type::contextmenu_button: OutputDebugStringA("Context Menu Button\n"); break;
            default: crash(); break;
        }
    else  OutputDebugStringA("*None*\n");
}

internal void ScaleUIFont(iu::ui_renderer* r, f32 last_scaling, f32 new_scaling)
{
    auto font_factory = r->renderer2D.font.fontFactory;
    auto& font = r->renderer2D.font.font;
    auto font_weight = font->GetFontWeight();
    auto font_style = font->GetFontStyle();
    auto font_stretch = font->GetFontStretch();
    auto font_size = font->GetFontSize() * new_scaling / last_scaling;
    WCHAR font_name[256]; /*font_name[0] = 0;*/
    font->GetFontFamilyName(font_name,ArrayCount(font_name));
    
    d3d_SafeRelease(font);
    font_factory->CreateTextFormat(
        font_name,
        nil /*Font collection*/,
        font_weight,
        font_style,
        font_stretch,
        font_size,
        L"en-us", //TODO(fran): consider locale? (in my opinion it's just pointless)
        &font
    );
    assert(font);
}

internal void UIProcessing(ui_state* ui)
{
    ui->render_and_update_screen = false;
    
    if (ui->is_context_menu && GetForegroundWindow() != ui->wnd.hwnd) //TODO(fran): OS code
    {
        //TODO(fran): BUG: when the context menu has a function that does not change the current foreground window then it doesnt get disabled, eg when minimizing from it.
            //Solution: we can create another ui_action macro that wraps the user defined behaviour with a call to closewindow or smth like that
            //UI_ACTION_CONTEXTMENU{
            //  user_action(context);
            //  closewindow(ui->wnd);
            //}
            //Another solution: check if the ui performing the action has the iscontextmenu boolean set and always close the window after an action. Although in both cases we should check if the action opened a new subcontextmenu, in which case we dont close the window

        b32 contextmenutesting = false;
#ifdef DEBUG_BUILD
        contextmenutesting = true;
#endif
        if(OS::IsWindowVisible(ui->wnd) && !contextmenutesting) OS::HideWindow(ui->wnd);
        return; //NOTE(fran): context menus arent processed once they stop being the foreground window
    }

    //TODO(fran): (Windows) enable rendering when the mouse enters any of the 4 resize borders
    { //TODO(fran): find best place for this code, maybe it should be smth we get from the caller
        rc2 last_placement = ui->placement;
        ui->placement = OS::GetWindowRenderRc(ui->wnd);

        EnableRendering(ui, ui->placement.w != last_placement.w || ui->placement.h != last_placement.h);

        last_placement = ui->placement;
    }

    { //TODO(fran): find the best place to put this
        //TODO(fran): make this official in some better way
        
        //assert(ui->scaling == GetNewScaling(ui)); //TODO(fran): use GetNewScaling(ui), this assertion fails cause it actually updates faster than ui->scaling

        utf8 scalingstr[20]; snprintf((char*)scalingstr, ArrayCount(scalingstr), "scaling:(%.2f)\n", GetNewScaling(ui)); OutputDebugStringA((char*)scalingstr);

        if (ui->scaling != ui->_last_scaling)
        {
            EnableRendering(ui);
            ScaleUIFont(&ui->renderer, ui->_last_scaling, ui->scaling);
        }
        ui->_last_scaling = ui->scaling;
    }

    utf8 mousepstr[32]; snprintf((char*)mousepstr, ArrayCount(mousepstr), "mouseP:(%.1f,%.1f)\n", ui->input.mouseP.x, ui->input.mouseP.y); OutputDebugStringA((char*)mousepstr);

    input_results input_res = InputUpdate(ui->elements, &ui->input); //TODO(fran): retrieve the input from inside the function
    PrintNextHot(&input_res);

    InputProcessing(ui, input_res.next_hot, &ui->input);

    //TODO(fran): disable IME window

    assert(ui->elements->type == ui_type::sizer || ui->elements->type == ui_type::subelement_table);
    //TODO(fran): multiple root elements

    f32 offsetY = IsWindowMaximized(ui->wnd) ? OS::GetWindowTopMargin(ui->wnd) : 0;
    //TODO(fran): handle EnableRendering inside ResizeElement?
    ResizeElement(ui, { 0, offsetY, ui->placement.w, ui->placement.h - offsetY }, ui->elements); //TODO(fran): find correct place to do resizing, before input processing? before render? after everything?

    //TODO(fran): BUG: when resizing the window the mouse position isnt updated, which means that if some element gets resized over where the mouse last was it will change its style to mouseover
    
    if(IsRenderingEnabled(ui))
    {
        RenderUI(ui);
    #if UICOMPOSITOR
        OutputUIToWindowsCompositor(veil_ui);
    #else
        OutputToScreen(&ui->renderer);
    #endif

    }

    //TODO(fran): we may want to have a destroy method to execute before the app closes to do anything necessary, like for example unregistering global hotkeys (though I'd assume the OS does it on its own?)

    //TODO(fran): when the window is minimized animations should be paused

    //TODO(fran): change mouse icon based on interaction state, allow user to define the mouse icon to use in each case for each element

    //TODO(fran): change titlebar to inactive state when we stop being the foreground window

    //TODO(fran): additional root components, to add for example debug information like timings

    //TODO(fran): interaction passthrough, so for example we can define all elements on the title bar to be rclick_passthrough and so the right click gets sent to the background, which triggers the contextmenu

    //TODO(fran): BUG: replicate: move the mouse to the background, move the mouse to the closest nonclient border, move the mouse back to the background, the mouse icon will remain as the resizing one, since the previous and new under_the_mouse element are the same (the background), so we dont update the mouse icon
        //Solution: we need to re-update the mouse style when we go from the nonclient to the client

    //SMALL TODO(fran): tiny BUG: right click the titlebar to open the context menu, move the mouse slightly towards the menu, for one frame the mouse icon will change to the arrow with loading circle style before changing back to the arrow
}

internal void CreateOSUIElements(ui_state* ui, b32* close, ui_element* client_area)
//TODO(fran): ui_action on_close instead of b32* ?
{
    TIMEDFUNCTION();
    assert(close); //TODO(fran): allow for passing nil ptr
#ifdef OS_WINDOWS
    memory_arena* arena = &ui->permanent_arena;

    //TODO(fran): change to the inactive state

    background_theme nonclient_bk_theme =
    {
        .color =
        {
            .background =
            {
                .normal = {0.2f,0.2f,0.2f,1.0f},
                .mouseover = nonclient_bk_theme.color.background.normal,
                .pressed = nonclient_bk_theme.color.background.normal,
                .inactive = {0.35f,0.35f,0.4f,1.0f},
            },
            .border = nonclient_bk_theme.color.background,
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::rect,
    };

    button_theme base_iconbutton_theme =
    {
        .color =
        {
            .foreground =
            {
                .normal = {1.0f,1.0f,1.0f,1.0f},
                .mouseover = base_iconbutton_theme.color.foreground.normal,
                .pressed = base_iconbutton_theme.color.foreground.normal,
                .inactive = {0.35f,0.35f,0.4f,1.0f},
            },
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::round_rect,
        .font = 0,
    };

    button_theme minmax_theme =
    {
        .color =
        {
            .foreground =
            {
                .normal = {0.7f,0.7f,0.7f,1.0f},
                .mouseover = minmax_theme.color.foreground.normal,
                .pressed = minmax_theme.color.foreground.normal,
                .inactive = {0.5f,0.5f,0.5f,1.0f},
            },
            .background =
            {
                .normal = {0},
                .mouseover = {.5f,.5f,.5f,1.f},
                .pressed = {.6f,.6f,.6f,1.f},
                .inactive = {0},
            },
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::rect,
        .font = 0,
    };

    button_theme close_theme =
    {
        .color =
        {
            .foreground = minmax_theme.color.foreground,
            .background =
            {
                .normal = {0},
                .mouseover = {.7f,.0f,.0f,1.f},
                .pressed = {.6f,.0f,.0f,1.f},
                .inactive = {0},
            },
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::rect,
        .font = 0,
    };

    button_theme base_noneditabletext_theme = base_iconbutton_theme;

    element_sizing_desc full_bounds_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = 1.f},
    };

    element_sizing_desc half_bounds_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .5f},
    };
    element_sizing_desc os_nonclient_top =
    {
        .type = element_sizing_type::os_non_client_top,
    };
    element_sizing_desc icon_sizing =
    {
        .type = element_sizing_type::opposite_bounds,
        .bounds = {.scale_factor = 1.f},
    };
    element_sizing_desc noneditabletext_sizing =
    {
        .type = element_sizing_type::font,
        .font = {.w_extra_chars = 2},
    };
    element_sizing_desc minmaxclose_sizing =
    {
        .type = element_sizing_type::opposite_bounds,
        .bounds = {.scale_factor = 16.f / 9.f},
    };
    element_sizing_desc remaining_sizing =
    {
        .type = element_sizing_type::remaining,
    };

    ui_image logo_img =
    {
        .type = ui_image_type::img,
        .bounds = {.scale_factor = {.6f, .6f}},
        .img = &logo,
    };

    v2 minmaxclose_imgbounds_scale_factor{ 1.0f, 0.35f };

    ui_image minimize_img =
    {
        .type = ui_image_type::render_commands, //TODO(fran): try with a small mask (64x64)
        .bounds = {.scale_factor = minmaxclose_imgbounds_scale_factor},
        .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
            f32 l = minimum(placement.w, placement.h);
            rc2 square = get_centered_rc(placement, l, l);
            f32 line_width = 1;

            auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
            auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
            renderer2D->SetAntialiasMode(antialias_mode);

            f32 h = square.y + square.h * .5f;
            v2 p1 = {.x = square.x, .y = h };
            v2 p2 = {.x = square.right(), .y = h };
            renderer2D->DrawLine(*(D2D1_POINT_2F*)&p1, *(D2D1_POINT_2F*)&p2, brush, line_width);
        }},
    };

    ui_image maximize_img =
    {
        .type = ui_image_type::render_commands, //TODO(fran): try with a small mask (64x64)
        .bounds = {.scale_factor = minmaxclose_imgbounds_scale_factor},
        .render_commands = {.context = ui, .commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
            ui_state * ui = (decltype(ui))context;
            f32 l = minimum(placement.w, placement.h);
            rc2 square = get_centered_rc(placement, l, l);
            f32 line_width = 1.0f;

            auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
            auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
            renderer2D->SetAntialiasMode(antialias_mode); //NOTE(fran): for this 1pixel wide drawings aliased mode is best

            if (OS::IsWindowMaximized(ui->wnd))
            {
                f32 d = square.w * .25f;
                f32 half_d = d * .5f;
                square = inflate_rc(square, -d);
                rc2 square2 = translate_rc(square, half_d, -half_d);
                square = translate_rc(square, -half_d, half_d);

                square = round_rc(square);
                square2 = round_rc(square2);

                renderer2D->DrawRectangle(rc2_to_D2DRECT(square), brush, line_width);

                rc2 bounds = placement, clipout_area = square;
                { //NOTE(fran): manual clipping since Direct2D sucks hard
                    f32 correctionY = 5; //TODO(fran): quick HACK: the top of the square doesnt get rendered in aliased mode, so we had to manually enlargen the clipping region
                    rc2 top_strip =
                    {
                        .x = bounds.x,
                        .y = bounds.y - correctionY,
                        .w = bounds.w,
                        .h = distance(bounds.y, clipout_area.y) + correctionY,
                    };
                    renderer2D->PushAxisAlignedClip(rc2_to_D2DRECT(top_strip), antialias_mode);
                    defer{ renderer2D->PopAxisAlignedClip(); };

                    renderer2D->DrawRectangle(rc2_to_D2DRECT(square2), brush, line_width);
                }
                {
                    rc2 right_strip =
                    {
                        .x = clipout_area.right(),
                        .y = clipout_area.y,
                        .w = distance(bounds.right(), clipout_area.right()),
                        .h = bounds.h,
                    };
                    renderer2D->PushAxisAlignedClip(rc2_to_D2DRECT(right_strip), antialias_mode);
                    defer{ renderer2D->PopAxisAlignedClip(); };

                    renderer2D->DrawRectangle(rc2_to_D2DRECT(square2), brush, line_width);
                }
            }
            else
            {
                square = round_rc(square);
                renderer2D->DrawRectangle(rc2_to_D2DRECT(square), brush, line_width);
            }
        }},
    };

    ui_image close_img =
    {
        .type = ui_image_type::render_commands, //TODO(fran): try with a small mask (64x64)
        .bounds = {.scale_factor = minmaxclose_imgbounds_scale_factor},
        .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
            f32 l = minimum(placement.w, placement.h);
            rc2 square = get_centered_rc(placement, l, l);
            f32 line_width = 1;

            auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
            auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
            renderer2D->SetAntialiasMode(antialias_mode);

            square = round_rc(square);

            v2 p1 = square.xy;
            v2 p2 = square.bottom_right();

            v2 p3 = square.top_right();
            v2 p4 = square.bottom_left();

            renderer2D->DrawLine(*(D2D1_POINT_2F*)&p1, *(D2D1_POINT_2F*)&p2, brush, line_width);
            renderer2D->DrawLine(*(D2D1_POINT_2F*)&p3, *(D2D1_POINT_2F*)&p4, brush, line_width);
        }},
    };

    //TODO(fran): possibly a BUG: the first frame after maximizing is awful, you can see the application name in size 100 covering half your screen (idk whether this is a directx auto-stretching bug or a problem in my code)
    auto Caption_MaximizeOrRestore = UI_ACTION_LAMBDA{
        ui_state * ui = (decltype(ui))context;
        POINT screenP{ (i32)ui->input.screen_mouseP.x, (i32)ui->input.screen_mouseP.x };
        CancelInteraction(ui); //TODO(fran): same HACK from Move() code (WM_NCLBUTTONDOWN)
        PostMessage(ui->wnd.hwnd, WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(screenP.x, screenP.y));
    };

    auto Move = UI_ACTION_LAMBDA{
        ui_state * ui = (decltype(ui))context;

        POINT screenP{ (i32)ui->input.screen_mouseP.x, (i32)ui->input.screen_mouseP.x };

        CancelInteraction(ui);
        //TODO(fran): HACK: we need to immediately cancel the interaction because once WM_NCLBUTTONDOWN is sent Windows takes over and does the usual Windows things, ending with the fact that we dont get the WM_LBUTTONUP msg, therefore the interacting element is never updated and requires an extra click from the user to reset it before they can interact with anything else
            //This is in response to this BUG: the first click after window moving is finished (aka after the user releases the mouse) isnt registered

        PostMessage(ui->wnd.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(screenP.x, screenP.y));
    };

    struct contextmenu_context { ui_state* ui; b32* close; };
    auto ContextMenu = UI_ACTION_LAMBDA{
        contextmenu_context* ctx = (decltype(ctx))context;
        ui_state* ui = ctx->ui;
        b32* close = ctx->close;

        //TODO(fran): this is something the Iu createwindow handler should do for us
        if (ui->context_menu)
        {
            AcquireUIState(ui->context_menu, ui->context_menu->wnd);
        }
        else
        {
            ui->context_menu = iu::window_contextmenu();
        }

        contextmenu_button_theme contextbutton_theme =
        {
            .color =
            {
                .foreground =
                {
                    .normal = {0.7f,0.7f,0.7f,1.0f},
                    .mouseover = contextbutton_theme.color.foreground.normal,
                    .pressed = contextbutton_theme.color.foreground.normal,
                    .inactive = {0.5f,0.5f,0.5f,1.0f},
                },
                .background =
                {
                    .normal = {0.1f,0.1f,0.1f,1.f},
                    .mouseover = {.5f,.5f,.5f,1.f},
                    .pressed = contextbutton_theme.color.background.mouseover,
                    .inactive = contextbutton_theme.color.background.normal,
                },
            },
            .dimension =
            {
                .border_thickness = 0,
            },
            .style = ui_style::rect,
            .font = 0,
        };

        element_sizing_desc contextbutton_sz =
        {
            .type = element_sizing_type::font,
            .font = {.v_scale_factor = 1.f, .w_extra_chars = 2},
        };

        element_sizing_desc text_hotkey_pad_sizing =
        {
            .type = element_sizing_type::font,
            .font = {.w_extra_chars = 2},
        };

        element_sizing_desc remaining_sz =
        {
            .type = element_sizing_type::remaining,
        };

        v2 contextmenu_imgbounds_scale_factor{ 1.0f, 0.35f }; //TODO(fran): that .35f seems pointless

        #define contextmenu_img_line_width(max_l) (max_l * .25f)

        ui_image restore_img =
        {
            .type = ui_image_type::render_commands,
            .bounds = {.scale_factor = contextmenu_imgbounds_scale_factor},
            .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
                f32 l = minimum(placement.w, placement.h);
                rc2 square = get_centered_rc(placement, l, l);
                f32 line_width = contextmenu_img_line_width(l);
                line_width = round(line_width);
                f32 line_width_small = maximum(line_width * .5f, 1.f);

                //TODO(fran): FIX: does not display correctly on different resolutions

                auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
                auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
                renderer2D->SetAntialiasMode(antialias_mode);

                f32 d = square.w * .3f;
                f32 half_d = maximum(d * .5f, 1.f);
                square = inflate_rc(square, -d);
                square = translate_rc(square, -half_d, half_d);

                //IMPORTANT(fran): it seems direct2d draws outside the bounds if the line_width is bigger than one, meaning if line_width is 3 then 1.5 will be inside the bounds and 1.5 outside

                square = round_rc(square);
                line_width = round(line_width);
                line_width_small = round(line_width_small);

                v2 p1 = { square.x + square.w * .25f, square.y - line_width / 2 - line_width_small * 1.5f };
                v2 p2 = { square.right() + line_width / 2 + line_width_small * 1.5f, p1.y };

                v2 p3 = p2;
                v2 p4 = { p3.x, square.y + square.h * .75f + line_width_small / 2 };

                renderer2D->DrawRectangle(rc2_to_D2DRECT(square), brush, line_width);

                renderer2D->DrawLine(v2_to_D2DPOINT(p1), v2_to_D2DPOINT(p2), brush, line_width_small);
                renderer2D->DrawLine(v2_to_D2DPOINT(p3), v2_to_D2DPOINT(p4), brush, line_width_small);
                //TODO(fran): clean up this drawing code
            }},
        };

        ui_image minimize_img =
        {
            .type = ui_image_type::render_commands,
            .bounds = {.scale_factor = contextmenu_imgbounds_scale_factor},
            .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
                f32 l = minimum(placement.w, placement.h);
                rc2 square = get_centered_rc(placement, l, l);
                f32 line_width = contextmenu_img_line_width(l);

                auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
                auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
                renderer2D->SetAntialiasMode(antialias_mode);

                f32 h = square.y + square.h;
                v2 p1 = {.x = square.x, .y = h };
                v2 p2 = {.x = square.right(), .y = h };
                renderer2D->DrawLine(v2_to_D2DPOINT(p1), v2_to_D2DPOINT(p2), brush, line_width);
            }},
        };

        ui_image maximize_img =
        {
            .type = ui_image_type::render_commands,
            .bounds = {.scale_factor = contextmenu_imgbounds_scale_factor},
            .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
                f32 l = minimum(placement.w, placement.h);
                rc2 square = get_centered_rc(placement, l, l);
                f32 line_width = contextmenu_img_line_width(l);
                f32 line_width_small = maximum(line_width * .5f, 1.f);

                auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
                auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
                renderer2D->SetAntialiasMode(antialias_mode);

                f32 d = square.w * .25f;
                square = inflate_rc(square, -d);

                square = round_rc(square);

                renderer2D->DrawRectangle(rc2_to_D2DRECT(square), brush, line_width);
            }},
        };

        ui_image close_img =
        {
            .type = ui_image_type::render_commands, //TODO(fran): this looks like a good case to use a _mask_
            .bounds = {.scale_factor = contextmenu_imgbounds_scale_factor},
            .render_commands = {.commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
                f32 l = minimum(placement.w, placement.h);
                rc2 square = get_centered_rc(placement, l, l);
                f32 line_width = contextmenu_img_line_width(l);

                auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
                auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
                renderer2D->SetAntialiasMode(antialias_mode);

                square = round_rc(square);

                v2 p1 = square.xy;
                v2 p2 = square.bottom_right() + v2{.5f,.5f};

                v2 p3 = square.top_right() + v2{.5f,-.5f};
                v2 p4 = square.bottom_left();
                //renderer2D->FillGeometry()
                renderer2D->DrawLine(v2_to_D2DPOINT(p1), v2_to_D2DPOINT(p2), brush, line_width);
                renderer2D->DrawLine(v2_to_D2DPOINT(p3), v2_to_D2DPOINT(p4), brush, line_width);
            }},
        };

        auto KeyboardMove = UI_ACTION_LAMBDA{
            ui_state * ui = (decltype(ui))context;
            POINT screenP{ (i32)ui->input.screen_mouseP.x, (i32)ui->input.screen_mouseP.x };
            SetForegroundWindow(ui->wnd.hwnd);//NOTE(fran): required for SC_MOVE to work
            PostMessage(ui->wnd.hwnd, WM_SYSCOMMAND, SC_MOVE, MAKELPARAM(screenP.x, screenP.y));
        };

        auto KeyboardSize = UI_ACTION_LAMBDA{
            ui_state * ui = (decltype(ui))context;
            POINT screenP{ (i32)ui->input.screen_mouseP.x, (i32)ui->input.screen_mouseP.x };
            SetForegroundWindow(ui->wnd.hwnd);//NOTE(fran): required for SC_SIZE to work
            PostMessage(ui->wnd.hwnd, WM_SYSCOMMAND, SC_SIZE, MAKELPARAM(screenP.x, screenP.y));
        };

        //TODO(fran): move ui structure code outside
        memory_arena* arena = &ui->permanent_arena;

        OS::hotkey_data close_hotkey{.vk = VK_F4, .mods = MOD_ALT}; //TODO(fran): OS code

        ui_cursor Hand = {.type = ui_cursor_type::os, .os_cursor = OS::cursor_style::hand };

        ui->context_menu->elements = SubelementTable(arena, 3,
            //TODO(fran): disable Restore when not maximized & Move,Size,Maximize when maximized
            {
                .sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}},
                .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .image = restore_img, .text = {.type = ui_string_type::id, .str_id = 1}, .on_unclick = {.context = ui, .action = common_ui_actions::MaximizeOrRestore})
            },
            {.sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}}, .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .text = {.type = ui_string_type::id, .str_id = 2}, .on_unclick = {.context = ui, .action = KeyboardMove})
            },
            {.sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}}, .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .text = {.type = ui_string_type::id, .str_id = 3}, .on_unclick = {.context = ui, .action = KeyboardSize})
            },
            {.sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}}, .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .image = minimize_img, .text = {.type = ui_string_type::id, .str_id = 4}, .on_unclick = {.context = ui, .action = common_ui_actions::MinimizeOrRestore})
            },
            {.sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}}, .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .image = maximize_img, .text = {.type = ui_string_type::id, .str_id = 5}, .on_unclick = {.context = ui, .action = common_ui_actions::MaximizeOrRestore})
            },
            {.sizing = {{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz},{contextbutton_sz,contextbutton_sz}}, .element = ContextMenuButton(.arena = arena, .theme = &contextbutton_theme, .image = close_img, .text = {.type = ui_string_type::id, .str_id = 6}, .hotkey = close_hotkey, .on_unclick = {.context = close, .action = common_ui_actions::B32_Set})
            }
        );

        //IDEAs to improve element placement:
        //TODO(fran): add a general HSizer with a new property: group_sizing::max that takes the max x value that it got from its childs and uses it for all childs
            //Another idea would be a VHSizer that does its regular vertical sizing but then can also act on the horizontal component of all its childs by also using group_sizing::max
            //Also I feel we now gonna need a flag: enforce_sizing that tells the element that it must get fixed to the bounds
            //new ui_element: hotkey_button_tree: I really feel like we need to create a data structure that specifically understands this, because actually calculating the w size for all is not enough, we need to know the w size of the left aligned text, and then add to _that_ the w size of the hotkey_text
                //Now im also thinking of a table, with rows and columns, in this case n rows & 3 columns (icon - text - hotkey), at least in the sense of how to align things, obviously we still need that the 3 columns make a single ui_element, maybe in that sense then what we could do is create a table obj that receives n ui_elements that provide the amount of subelements the table requires, in this case 3, then the table can analyze all subelements of all elements and come up with the size of the element
                    //ui_subelement {type = text-icon-...}
            //Things learned: we require two separate things, there must be a clear distinction between the text on the left & the hotkey on the right, aka we cant just use a button, and we cant just use normal resizing nor a component system alone, we need to special case the working together of elements, all hotkey_buttons detected on the same z order of a sizer must be treated as a group, first its left text analyzed to find the max, and then the right text to find the total max bounds

        //TODO(fran): this may be better suited for a component system
            //I like the component system idea, we can add a structure similar to the ui_elements, eg for a 'hotkey_button' you'd have hziser -> icon + text + (hrsizer -> hotkey_text)

        //TODO(fran): 2 options: 
            // -sizing flag element_sizing_type::child_based meaning the element will ask the child for the size it wants and then use that size for itself
            // -define that right aligned sizers, when having an unbound rightmost position become left aligned sizers
            // (maybe I need both)

        //TODO(fran): a better idea instead of trying to replicate html style layouting can be to simply do it immediate mode style, if we want to create a sublayout we simply call sublayout, and then start placing stuff in the by calling other functions, eg:
            // Layout("layout")
            //    Layout("top")
            //      Button()
            //    CloseLayout()
            //    Layout("bottom")
            //      Slider()
            //    CloseLayout()
            // CloseLayout()
            // Thus avoiding the stupid extra parenthesis/brackets and making it much easier to see, visualize, understand and move things around
            // Behind the scenes storing the current layout and closing it and changing to the previous one is all very easy to do

        rc2 r = rc2_from(ui->input.screen_mouseP + v2{1.f,0.f}, {1.f,1.f}); //TODO(fran): HACK: I move the window one pixel to the side so it does not immediately collide with the first element in the context menu, on windows the first couple leftmost pixels of the menu are empty
        OS::MoveWindow(ui->context_menu->wnd, r);
        OS::ShowWindow(ui->context_menu->wnd);
        //BringWindowToTop(veil_ui->context_menu->wnd.hwnd); //TODO(fran): OS code //TODO(fran): this isnt required, but im sure we can find some edge cases where it is
        OS::SendWindowToTop(ui->context_menu->wnd);
        SetForegroundWindow(ui->context_menu->wnd.hwnd);//TODO(fran): OS code //NOTE: this is necessary because we reuse the windows, therefore the first time (when the window _is_ actually created) it automatically becomes the foreground window, but later uses of the window wont

        //TODO(fran): instead of doing a direct show it's probably better to activate some interal flag, eg: b32 active;
    };

    //TODO(fran): real non_editable_text element for the title instead of using a button
    //TODO(fran): assign system default UI font to the application title. Also idk why but the text in the title looks worse than in areas inside the "client" region
    //TODO(fran): on Windows when the window is maximized the nc arena needs to be moved down by the height of the top resize border. Currently we solve this by just hacking it and applying a Y offset before calling render

    contextmenu_context* contextmenu_ctx = push_type(arena, contextmenu_context);
    contextmenu_ctx->ui = ui;
    contextmenu_ctx->close = close;

    ui->elements = /*v*/ VSizer(arena, sizer_alignment::top,
        { .sizing = os_nonclient_top, .element = /*h*/ HSizer(arena, sizer_alignment::left,
                {.sizing = full_bounds_sizing, .element = /*nc_bk*/ Background(.arena = arena, .theme = &nonclient_bk_theme, .on_click = {.context = ui, .action = Move}, .on_doubleclick = {.context = ui, .action = Caption_MaximizeOrRestore}, .on_unrclick = {.context = contextmenu_ctx, .action = ContextMenu},
                    .child = /*nc_v*/ VSizer(arena, sizer_alignment::top,
                        {.sizing = full_bounds_sizing, .element = /*nc_h*/ HSizer(arena, sizer_alignment::left,
                            {.sizing = half_bounds_sizing, .element = /*left_placement*/ HSizer(arena, sizer_alignment::left,
                                {.sizing = icon_sizing, .element = /*logo*/Button(.arena = arena, .theme = &base_iconbutton_theme, .image = logo_img, .on_click = {.context = contextmenu_ctx, .action = ContextMenu}, .on_doubleclick = {.context = close, .action = common_ui_actions::B32_Set})},
                                {.sizing = noneditabletext_sizing, .element = /*title*/ Button(.arena = arena, .theme = &base_noneditabletext_theme, .text = {.type = ui_string_type::str, .str = const_temp_s(appname)}, .on_click = {.context = ui, .action = Move}, .on_doubleclick = {.context = ui, .action = Caption_MaximizeOrRestore})}
                            )},
                            {.sizing = half_bounds_sizing, .element = /*right_placement*/ HSizer(arena, sizer_alignment::right,
                                {.sizing = minmaxclose_sizing, .element = /*minimize*/ Button(.arena = arena, .theme = &minmax_theme, .image = minimize_img, .on_unclick = {.context = ui, .action = common_ui_actions::MinimizeOrRestore})},
                                {.sizing = minmaxclose_sizing, .element = /*maximize*/ Button(.arena = arena, .theme = &minmax_theme, .image = maximize_img, .on_unclick = {.context = ui, .action = common_ui_actions::MaximizeOrRestore})},
                                {.sizing = minmaxclose_sizing, .element = /*close*/ Button(.arena = arena, .theme = &close_theme, .image = close_img, .on_unclick = {.context = close, .action = common_ui_actions::B32_Set})}
                            )}
                        )}
                    )
                )}
        ) },
        { .sizing = remaining_sizing, .element = client_area } //TODO(fran): better if the user received where to place their ui
    );

    ui->OnClose = { .context = close, .action = common_ui_actions::B32_Set };

#else
#error Define your OS's implementation
#endif
}