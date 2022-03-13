#pragma once

#include "assets/embedded_images.h"


//NOTE(fran): we'll start by using Direct2D as our UI renderer
#include <d2d1_2.h>
#include <dwrite.h>

#include <shellapi.h> // NOTIFYICONDATA (everything related to the tray icon)

#include "win32_tray.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

enum class ui_type {
    vpad=0,
    hpad,
    sizer, //NOTE(fran): strictly used for placing and resizing other non-sizer elements, they are not rendered nor collision tested
    button,
    slider,
    hotkey,
    background,
};

union ui_color_group {
    struct { v4 normal, disabled, mouseover, pressed, inactive; };
    v4 E[5];
    //NOTE(fran): mouseover will also be used in the case where the user clicks the item but then moves the mouse away from it, indicating they didnt really want to click it
};

enum class ui_style {
    round_rect, rect, circle
};

struct button_theme {
    struct color {
        ui_color_group foreground, background, border;
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
    HFONT font = 0;//TODO(fran)
};

struct background_theme {
    struct color {
        ui_color_group background, border;
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
};

struct hotkey_theme {
    struct color {
        ui_color_group foreground_placeholder, foreground_validhk, foreground_invalidhk, background, border;
        //TODO(fran): maybe the validhk is pointless, simply use normal & invalidhk
    } color;
    struct dimension {
        u32 border_thickness = 0;
    }dimension;//TODO(fran): better name, I dont like 'dimension'
    ui_style style;
    HFONT font = 0;//TODO(fran)
};

//FUTURE TODO(fran): the style flags should all be placed inside a single enum

struct slider_theme {
    struct color {
        ui_color_group track_fill, track_empty, thumb;
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

struct ui_sizer_element {
    ui_element* element; //NOTE(fran): the element itself can and will contain multiple other elements
    element_sizing_desc sizing_desc;
    ui_sizer_element* next;
};


struct sizer_props {
    b32 is_horizontal;
    enum class alignment : u8 {
        left=1<<0,
        right=1<<1,
        center=1<<2,

        top=left,//TODO(fran): can I use this ones?
        bottom=right,
    } alignment;
};

/* TODO(fran): having vertical and horizontal pads we could, for example, tabulate the placement of objects in a different axis than the containing resizer's, is that useful for anything?
struct pad_desc {
    b32 is_horizontal;//NOTE(fran): pads can be horizontal or vertical
}
*/


enum class interaction_state {
    normal=0, disabled, mouseover, pressed
};

#define _UI_ACTION_RET void
#define _UI_ACTION_ARGS (ui_element* element, void* context)
#define UI_ACTION(name) _UI_ACTION_RET (name) _UI_ACTION_ARGS
#define UI_ACTION_LAMBDA []_UI_ACTION_ARGS -> _UI_ACTION_RET
typedef UI_ACTION(action_on_unclick);
typedef UI_ACTION(action_on_unrclick);


#define BUTTON_ACTION_ON_UNCLICK(name) void(name)(ui_element* element, void* context)
#define BUTTON_ACTION_ON_UNCLICK_LAMBDA [](ui_element* element, void* context) -> void
typedef BUTTON_ACTION_ON_UNCLICK(button_action_on_unclick);

//TODO(fran): we could store a specific context param for each function inside the element object

#define BUTTON_ACTION_ON_CLICK(name) BUTTON_ACTION_ON_UNCLICK(name)
#define BUTTON_ACTION_ON_CLICK_LAMBDA BUTTON_ACTION_ON_UNCLICK_LAMBDA 
typedef BUTTON_ACTION_ON_CLICK(button_action_on_click);

#define BUTTON_ACTION_ON_DOUBLECLICK(name) BUTTON_ACTION_ON_UNCLICK(name)
#define BUTTON_ACTION_ON_DOUBLECLICK_LAMBDA BUTTON_ACTION_ON_UNCLICK_LAMBDA 
typedef BUTTON_ACTION_ON_DOUBLECLICK(button_action_on_doubleclick);

#define BACKGROUND_ACTION_ON_CLICK(name) BUTTON_ACTION_ON_UNCLICK(name)
#define BACKGROUND_ACTION_ON_CLICK_LAMBDA BUTTON_ACTION_ON_UNCLICK_LAMBDA 
typedef BACKGROUND_ACTION_ON_CLICK(background_action_on_click);

#define BACKGROUND_ACTION_ON_DOUBLECLICK(name) BUTTON_ACTION_ON_UNCLICK(name)
#define BACKGROUND_ACTION_ON_DOUBLECLICK_LAMBDA BUTTON_ACTION_ON_UNCLICK_LAMBDA 
typedef BACKGROUND_ACTION_ON_DOUBLECLICK(background_action_on_doubleclick);

#define GLOBALHOTKEY_ACTION_ON_TRIGGERED(name) void(name)(ui_element* element, void* context)
#define GLOBALHOTKEY_ACTION_ON_TRIGGERED_LAMBDA [](ui_element* element, void* context) -> void
typedef GLOBALHOTKEY_ACTION_ON_TRIGGERED(globalhotkey_action_on_triggered);

#define _UI_STRING_DYN_ID_RET u32
#define _UI_STRING_DYN_ID_ARGS (void* context)
#define UI_STRING_DYN_ID(name) _UI_STRING_DYN_ID_RET (name) _UI_STRING_DYN_ID_ARGS
#define UI_STRING_DYN_ID_LAMBDA []_UI_STRING_DYN_ID_ARGS -> _UI_STRING_DYN_ID_RET
typedef UI_STRING_DYN_ID(string_dynamic_id);

enum class ui_string_type {
    id=0, str, dynamic_id
    //TODO(fran): performance benchmark using none vs using an id equal to 0 that always maps to an empty string in hash table element 0
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
    hotkey_data* hk;
    i32 id; //NOTE(fran): unique id per hotkey
};

enum class hotkey_string_state {
    placeholder = 0, validhk, invalidhk
};

enum class ui_image_type {
    none=0,img, mask, render_commands //TODO(fran): remove 'none' once we implement the img hash table
};

#define _UI_IMAGE_RENDERCOMANDS_ARGS (ID2D1DeviceContext* renderer2D, ID2D1SolidColorBrush* brush, ui_element* element, rc2 placement, void* context)
#define UI_IMAGE_RENDERCOMANDS(name) void(name)_UI_IMAGE_RENDERCOMANDS_ARGS
#define UI_IMAGE_RENDERCOMANDS_LAMBDA []_UI_IMAGE_RENDERCOMANDS_ARGS -> void
typedef UI_IMAGE_RENDERCOMANDS(image_rendercommands);

struct ui_image {
    ui_image_type type;
    //TODO(fran): add antialias mode and pixel snapping (round_rc()) option flags
    struct {
        v2 scale_factor; //NOTE(fran): the resulting square bounds will be centered in relation to the original bounds (TODO(fran): allow for options other than centering)
    } bounds;

    union {
        const embedded_img* img;
        image_rendercommands* render_commands; //NOTE(fran): functions sort of like an svg
    };
};

struct ui_element {
    ui_type type;
    rc2 placement;
    interaction_state interaction_st;
    union {
        struct {
            sizer_props properties;
            ui_sizer_element* childs;
#ifdef DEBUG_BUILD
            v4 bk_color;
#endif
        } sizer;
        struct {
            background_theme theme;

            void* context;
            background_action_on_click* OnClick;
            background_action_on_doubleclick* OnDoubleClick;
        } background;
        struct {
            button_theme theme;

            button_action_on_unclick *OnUnclick;
            button_action_on_click *OnClick;
            button_action_on_doubleclick * OnDoubleClick;
            void* context;

            ui_string text;
            ui_image image;
        } button;
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
                    p = 1-percentage_between(placement.top, mouseP.y, placement.bottom());
                res = lerp(this->value.min, p, this->value.max);
                return res;
            }
        } slider;
        struct {
            hotkey_theme theme;
            ui_string placeholder_text;
            hotkey_string_state string_state;
            ui_hotkey current_hotkey;
            globalhotkey_action_on_triggered* on_hotkey;
            void* context;
        } hotkey;
    } data;
    ui_element* child;//TODO(fran): maybe change to childs, elements that will be placed inside the parent

    ui_element* AddChild(ui_element* child) {
        assert(this->child == nil);
        assert(child->type == ui_type::sizer);
        this->child = child;
        return this;
    }
};

struct d2d_renderer {
    ID2D1Factory2* factory;
    ID2D1Device1* device;
    ID2D1DeviceContext* deviceContext;

    ID2D1Bitmap1* bitmap;

    IDWriteFactory* fontFactory;
    IDWriteTextFormat* font;
    //TODO(fran): look at IDWriteBitmapRenderTarget
};

struct d3d11_ui_renderer {
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;

    IDXGIDevice* dxgiDevice;
    IDXGIFactory2* dxgiFactory;
    IDXGISwapChain1* swapChain;

    ID3D11Buffer* constantBuffer;
    ID3D11PixelShader* pixelShader;
    ID3D11VertexShader* vertexShader;

    ID3D11RenderTargetView* renderTarget;

    u32 currentWidth;
    u32 currentHeight;
};

#define UICOMPOSITOR 0

#if UICOMPOSITOR 
struct windows_ui_compositor {
    IDCompositionDevice* device;
    IDCompositionTarget* target;
    IDCompositionVisual* visual;
};
#endif

enum class input_key_state : u8 {//NOTE(fran): all the states are mutually exclusive
    doubleclicked = (1 << 0), //NOTE(fran): double click is an annoying feature, we'll simply ignore it as much as we can for now
    clicked = (1 << 1),
    pressed = (1 << 2),
    unclicked = (1 << 3),
    //TODO(fran): analyze times between clicks to see if it's worth it to register multiple clicks per run
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

struct user_input {
    v2 mouseP;//INFO(fran): mouse coordinates must be relative to the window area (client area on Windows) and with x-axis going right and y-axis going down
    v2 screen_mouseP;
    input_key_state keys[2];
    f32 mouseVScroll; //NOTE(fran): indicates the number of units to scroll (eg a value of 1 means to scroll 1 unit), the unit will be determined by whoever handles the scroll
    //TODO(fran): some mice, like the G502 Hero that Im using, allow horizontal scrolling (by moving the scroll wheel to the sides), how do they do it & can we handle that case?
    //TODO(fran): on Windows mouseVScroll is positive when the user moves the finger upwards through the wheel, & negative when moving the finger downwards, idk how we should do it in other OSs

    hotkey_data hotkey;
    i32 global_hotkey_id; //System wide hotkey, received from the OS at any time

    struct {
        b32 on_unclick, on_unrclick;
        //TODO(fran): store mouseP
    } tray;
};

struct ui_tray_state {
    struct {
        NOTIFYICONDATAW notification; //TODO(fran): OS independent (generic) version
    }impl;
    action_on_unclick* on_unclick;
    void* on_unclick_context;
    action_on_unrclick* on_unrclick;
    void* on_unrclick_context;
};

struct veil_ui_state {
    HWND wnd;
    u64 main_thread_id;//TODO(fran): the thread id can probably be gotten from the hwnd
    rc2 placement;
    f32 scaling; //eg on windows takes the form of dpi scaling //TODO(fran): in order to be able to handle Ctrl+Mousewheel we'd need two scaling values, one for the wheel and another for dpi, or maybe they override each other?
    b32 render_and_update_screen;

    ui_element* elements;
    memory_arena permanent_arena;//NOTE(fran): UI elements are placed here
    //memory_arena one_frame_arena;//NOTE(fran):

    d3d11_ui_renderer renderer;
    d2d_renderer renderer2D;

    language_manager LanguageManager;

    ui_element* interacting_with;
    ui_element* under_the_mouse;
    ui_element* keyboard_focus;

    b32 quit;

    f32 threshold;
    //f32 brightness;
    struct {
        f32 threshold_min;
        f32 threshold_max;
    };

    b32 show_veil;

    hotkey_data show_ui_hotkey;

    struct {
        globalhotkey_action_on_triggered* action;
        ui_element* element;
        void* context;
        i64 registration_time; //TODO(fran): look for a better alternative, we should accept the hotkey once the buttons that were pressed to register it are unpressed
        b32 enabled; //TODO(fran): I could instead set registration_time to negative and use that as the enabled flag
    } global_registered_hotkeys[64];

    user_input* input;//TODO(fran): make this official, we probably want to store the whole struct inside here

    struct {
        v2 last_mouse_pivot;
        v2 mouse_pivot;
    }os_movewindow;

    ui_tray_state tray;
    
#if UICOMPOSITOR
    windows_ui_compositor compositor;
#endif
};

//TODO(fran): store pointer to last element instead of iterating to the end all the time
#define GetOnePastLast(elements) while (*elements) elements = &((*elements)->next);

internal ui_element* CreateSizer(memory_arena* arena, b32 is_horizontal, enum sizer_props::alignment alignment)
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::sizer;
    elem->placement = { 0 };
    elem->child = nil;
    elem->data.sizer.childs = nil;
    elem->data.sizer.properties.alignment = alignment;
    elem->data.sizer.properties.is_horizontal = is_horizontal;
#ifdef DEBUG_BUILD
    elem->data.sizer.bk_color = { random01(),random01(),random01(),1.f };
#endif
    return elem;
}
internal ui_element* CreateHSizer(memory_arena* arena, enum sizer_props::alignment alignment)
{
    return CreateSizer(arena, true, alignment);
}
internal ui_element* CreateVSizer(memory_arena* arena, enum sizer_props::alignment alignment = sizer_props::alignment::top)
{
    return CreateSizer(arena, false, alignment);
}

//TODO(fran): a way to define single values, for example in the case of functions, we wanna just add the ones each particular element will want. Do this either by setFunc...() or with a single structure that contains all the params, where the user can do {.action_click=...,}
internal ui_element* CreateButton(memory_arena* arena, button_theme* theme, ui_string text, void* context, button_action_on_unclick on_unclick, button_action_on_click on_click=nil) 
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::button;
    elem->placement = {0};
    elem->child = nil;
    elem->data.button.theme = *theme; //TODO(fran): this could be changed to be a straight pointer, and allow for quick and easy realtime update of all elements sharing the same theme

    elem->data.button.OnUnclick = on_unclick;
    elem->data.button.OnClick = on_click;
    elem->data.button.context = context;
    elem->data.button.text = text;

    return elem;
}

struct button_creation_args {
    memory_arena* arena;
    button_theme* theme;
    ui_string text;
    ui_image image;
    void* context;
    button_action_on_click* on_click;
    button_action_on_unclick* on_unclick;
    button_action_on_doubleclick* on_doubleclick;
    ui_element* child;
};
internal ui_element* Button(const button_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::button;
    elem->placement = { 0 };
    elem->child = args.child;
    elem->data.button.theme = *args.theme; //TODO(fran): this could be changed to be a straight pointer, and allow for quick and easy realtime update of all elements sharing the same theme

    elem->data.button.text = args.text;
    elem->data.button.image = args.image;
    elem->data.button.context = args.context;
    elem->data.button.OnClick = args.on_click;
    elem->data.button.OnUnclick = args.on_unclick;
    elem->data.button.OnDoubleClick = args.on_doubleclick;

    return elem;
}

internal ui_element* CreateBackground(memory_arena* arena, background_theme* theme, void* context, background_action_on_click on_click, ui_element* child = nil)
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::background;
    elem->placement = { 0 };
    elem->child = child;
    elem->data.background.theme = *theme;
    elem->data.background.context = context;
    elem->data.background.OnClick = on_click;

    return elem;
}

struct background_creation_args {
    memory_arena* arena;
    background_theme* theme;
    void* context;
    background_action_on_click* on_click;
    background_action_on_doubleclick* on_doubleclick;
    ui_element* child;
};
internal ui_element* Background(const background_creation_args& args)
{
    ui_element* elem = push_type(args.arena, ui_element);
    elem->type = ui_type::background;
    elem->placement = { 0 };
    elem->child = args.child;
    elem->data.background.theme = *args.theme;
    elem->data.background.context = args.context;
    elem->data.background.OnClick = args.on_click;
    elem->data.background.OnDoubleClick = args.on_doubleclick;

    return elem;
}

internal ui_element* CreateSlider(memory_arena* arena, slider_theme* theme, slider_value value) 
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::slider;
    elem->placement = { 0 };
    elem->child = nil;
    elem->data.slider.theme = *theme;
    elem->data.slider.value = value;

    return elem;
}

internal ui_element* CreateVPad(memory_arena* arena) 
{
    //TODO(fran): assign a font to the pad, so it can decide its padding based on that
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::vpad;
    elem->placement = { 0 };
    elem->child = nil;

    return elem;
}

internal ui_element* CreateHPad(memory_arena* arena) 
{
    //TODO(fran): assign a font to the pad, so it can decide its padding based on that
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::hpad;
    elem->placement = { 0 };
    elem->child = nil;

    return elem;
}

internal ui_element* CreateHotkey(memory_arena* arena, hotkey_theme* theme, hotkey_data* hotkey_value, ui_string placeholder_text, void* context, globalhotkey_action_on_triggered* on_hotkey) 
{
    ui_element* elem = push_type(arena, ui_element);
    elem->type = ui_type::hotkey;
    elem->placement = { 0 };
    elem->child = nil;
    elem->data.hotkey.theme = *theme;
    elem->data.hotkey.on_hotkey = on_hotkey;
    elem->data.hotkey.context = context;
    elem->data.hotkey.placeholder_text = placeholder_text;
    elem->data.hotkey.string_state = hotkey_string_state::placeholder;

    local_persistence i32 hotkey_id = 1; //NOTE(fran): id 0 is invalid
    elem->data.hotkey.current_hotkey.id = hotkey_id++;
    assert(hotkey_id <= ArrayCount(veil_ui_state::global_registered_hotkeys));
    elem->data.hotkey.current_hotkey.hk = hotkey_value;//TODO(fran): start with a hotkey and try to register it

    return elem;
}

/*
internal ui_element* AddElementAtEnd(ui_element** root, ui_element* element) {
    ui_element** elements = root;
    GetOnePastLast(elements);
    *elements = element;
    return *elements;
}
*/

internal void SizerAddElement(memory_arena* arena, ui_element* sizer, ui_element* new_elem, element_sizing_desc sizing_desc)
{
    assert(sizer->type == ui_type::sizer);

    ui_sizer_element** elements = &sizer->data.sizer.childs;
    GetOnePastLast(elements);

    *elements = push_type(arena, ui_sizer_element);
    (*elements)->element = new_elem;
    (*elements)->sizing_desc = sizing_desc;
    (*elements)->next = nil;
}

//TODO(fran): when the window is minimized animations should be paused
internal void MinimizeWindow(HWND wnd) //TODO(fran): OS code
{
    ShowWindow(wnd, SW_MINIMIZE);
    //SetForegroundWindow(nil);
    //SetFocus(nil);
}

internal void MaximizeWindow(HWND wnd) //TODO(fran): OS code
{
    ShowWindow(wnd, SW_MAXIMIZE);
}

internal void RestoreWindow(HWND wnd) //TODO(fran): OS code
{
    ShowWindow(wnd, SW_RESTORE);
}

internal b32 IsWindowMaximized(HWND wnd) //TODO(fran): OS code
{
    b32 res = IsZoomed(wnd);
    return res;
}

internal void CancelInteraction(veil_ui_state* veil_ui) //TODO(fran): see if this makes sense, or some other concept is needed //TODO(fran): join with the rest of the Interaction functions once the background resizing hack is dealt with
{
    veil_ui->interacting_with = nil;
}

internal rc2 get_centered_rc(rc2 bounds, f32 w, f32 h)
{
    //Generates a new rectangle of width w and height h and centered in relation to bounds
    rc2 res =
    {
        .x = bounds.x + (bounds.w - w) * .5f,
        .y = bounds.y + (bounds.h - h) * .5f,
        .w = w,
        .h = h,
    };
    return res;
}

internal D2D1_RECT_F rc2_to_D2DRECT(rc2 rc)
{
    D2D1_RECT_F res =
    {
        .left = rc.left,
        .top = rc.top,
        .right = rc.right(),
        .bottom = rc.bottom(),
    };
    return res;
}

internal void PushInverseAxisAlignedClip(ID2D1DeviceContext* renderer2D, rc2 bounds, rc2 clipout_area /*do not render inside here*/, D2D1_ANTIALIAS_MODE antialias_mode)
{
    //NOTE(fran): we assume the clipout_area is always contained within the bounds

    //TODO(fran): make sure the edges are correctly left in/out

    rc2 strips[4] =
    {
        { //left strip
            .x = bounds.x,
            .y = bounds.y,
            .w = distance(bounds.x, clipout_area.x),
            .h = bounds.h,
        },
        { //top strip
            .x = bounds.x,
            .y = bounds.y,
            .w = bounds.w,
            .h = distance(bounds.y, clipout_area.y),
        },
        { //right strip
            .x = clipout_area.right(),
            .y = bounds.y,
            .w = distance(bounds.right(), clipout_area.right()),
            .h = bounds.h,
        },
        { //bottom strip
            .x = bounds.x,
            .y = clipout_area.bottom(),
            .w = bounds.w,
            .h = distance(bounds.bottom(), clipout_area.bottom()),
        },
    };
    for (u32 i = 0; i < 4; i++) 
        renderer2D->PushAxisAlignedClip(rc2_to_D2DRECT(strips[i]), antialias_mode);
    //TODO(fran): this doesnt work, each push is subtractive, after the 4 pushes the rendering area is simply null, how can direct2d clipping be so limited?
        // I find it amazing that Direct2D clipping is so bad, you can _not_ create an exclusion zone (a rect where no rendering should happen), nor add multiple AxisAlignedClips to create a draw region around the excluded one

}

internal void PopInverseAxisAlignedClip(ID2D1DeviceContext* renderer2D)
{
    for (u32 i = 0; i < 4; i++) renderer2D->PopAxisAlignedClip();
}

namespace common_ui_actions
{
    auto OnOffVeil = UI_ACTION_LAMBDA{
    b32 * show_veil = (decltype(show_veil))context;
    *show_veil = !(*show_veil);
    };

    auto MinimizeRestoreVeilUI = UI_ACTION_LAMBDA{
        //TODO(fran): add the minimize/restore to tray code to the future animation system, currently it stalls the thread (via Sleep) until it finishes minimizing/restoring
        //TODO(fran): check for active tray icon, otherwise do normal minimize
        veil_ui_state* veil_ui = (decltype(veil_ui))context;
        if (!IsWindowVisible(veil_ui->wnd)) { //window is minimized
            RestoreWndFromTray(veil_ui->wnd);
            //TODO(fran): the veil could be occluded, we should check that the veil is on top too
        }
        else {
            //TODO(fran): the window could be occluded (in which case we want to SW_SHOW), there doesnt seem to be an easy way to know whether your window is actually visible to the user
            MinimizeWndToTray(veil_ui->wnd);
        }
    };
}

internal void CreateOSUIElements(veil_ui_state* veil_ui, ui_element* client_area)
{
    //TODO(fran): add background that handles window movement
#ifdef OS_WINDOWS
    memory_arena* arena = &veil_ui->permanent_arena;

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
        .bounds= {.scale_factor = 1.f},
    };
    element_sizing_desc noneditabletext_sizing =
    {
        .type = element_sizing_type::font,
        .font = {.w_extra_chars = 2},
    };
    element_sizing_desc minmaxclose_sizing =
    {
        .type = element_sizing_type::opposite_bounds,
        .bounds = {.scale_factor = 16.f/9.f},
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
        .render_commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
            f32 l = minimum(placement.w, placement.h);
            rc2 square = get_centered_rc(placement, l, l);
            f32 line_width = 1;

            auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
            auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
            renderer2D->SetAntialiasMode(antialias_mode);

            f32 h = square.y + square.h * .5f;
            v2 p1 = { .x = square.x, .y = h };
            v2 p2 = { .x = square.right(), .y = h };
            renderer2D->DrawLine(*(D2D1_POINT_2F*)&p1, *(D2D1_POINT_2F*)&p2, brush, line_width);
        },
    };

    ui_image maximize_img =
    {
        .type = ui_image_type::render_commands, //TODO(fran): try with a small mask (64x64)
        .bounds = {.scale_factor = minmaxclose_imgbounds_scale_factor},
        .render_commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
            veil_ui_state* veil_ui = (decltype(veil_ui))context;
            f32 l = minimum(placement.w, placement.h);
            rc2 square = get_centered_rc(placement, l, l);
            f32 line_width = 1.0f;

            auto oldaliasmode = renderer2D->GetAntialiasMode(); defer{ renderer2D->SetAntialiasMode(oldaliasmode); };
            auto antialias_mode = D2D1_ANTIALIAS_MODE_ALIASED;
            renderer2D->SetAntialiasMode(antialias_mode); //NOTE(fran): for this 1pixel wide drawings aliased mode is best

            if (IsWindowMaximized(veil_ui->wnd))
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
        },
    };

    ui_image close_img =
    {
        .type = ui_image_type::render_commands, //TODO(fran): try with a small mask (64x64)
        .bounds = {.scale_factor = minmaxclose_imgbounds_scale_factor},
        .render_commands = UI_IMAGE_RENDERCOMANDS_LAMBDA{
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
        },
    };

    //TODO(fran): possibly a BUG: the first frame after maximizing is awful, you can see the application name in size 100 covering half your screen (idk whether this is a directx auto-stretching bug or a problem in my code)
    auto Caption_MaximizeOrRestore = UI_ACTION_LAMBDA{
        veil_ui_state* veil_ui = (decltype(veil_ui))context;
        POINT screenP{ (i32)veil_ui->input->mouseP.x, (i32)veil_ui->input->mouseP.x };
        MapWindowPoints(veil_ui->wnd, HWND_DESKTOP, &screenP, 1);
        CancelInteraction(veil_ui); //TODO(fran): same HACK from Move() code (WM_NCLBUTTONDOWN)
        PostMessage(veil_ui->wnd, WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(screenP.x, screenP.y));
    };

    auto MaximizeOrRestore = UI_ACTION_LAMBDA{
        veil_ui_state * veil_ui = (decltype(veil_ui))context;
        if (IsWindowMaximized(veil_ui->wnd)) RestoreWindow(veil_ui->wnd);
        else MaximizeWindow(veil_ui->wnd);
    };

    auto Move = UI_ACTION_LAMBDA{
        veil_ui_state * veil_ui = (decltype(veil_ui))context;

        POINT screenP{ (i32)veil_ui->input->mouseP.x, (i32)veil_ui->input->mouseP.x };
        MapWindowPoints(veil_ui->wnd, HWND_DESKTOP, &screenP, 1);

        CancelInteraction(veil_ui);
        //TODO(fran): HACK: we need to immediately cancel the interaction because once WM_NCLBUTTONDOWN is sent Windows takes over and does the usual Windows things, ending with the fact that we dont get the WM_LBUTTONUP msg, therefore the interacting element is never updated and requires an extra click from the user to reset it before they can interact with anything else
            //This is in response to this BUG: the first click after window moving is finished (aka after the user releases the mouse) isnt registered

        PostMessage(veil_ui->wnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(screenP.x, screenP.y));
    };

    auto Quit = UI_ACTION_LAMBDA{
        b32* quit = (decltype(quit))context;
        *quit = true;
    };

    ui_element* layout = CreateVSizer(arena);

    ui_element* h_placement = CreateHSizer(arena, sizer_props::alignment::left);

    ui_element* nc_layout = CreateVSizer(arena);
    
    ui_element* nc_bk = Background({ .arena=arena, .theme=&nonclient_bk_theme, .context=veil_ui, .on_click = Move, .on_doubleclick=Caption_MaximizeOrRestore, .child=nc_layout });

    ui_element* nc_h = CreateHSizer(arena, sizer_props::alignment::left);
    SizerAddElement(arena, nc_layout, nc_h, full_bounds_sizing);

    SizerAddElement(arena, h_placement, nc_bk, full_bounds_sizing);

    ui_element* left_placement = CreateHSizer(arena, sizer_props::alignment::left);
    ui_element* right_placement = CreateHSizer(arena, sizer_props::alignment::right);
    SizerAddElement(arena, nc_h, left_placement, half_bounds_sizing);
    SizerAddElement(arena, nc_h, right_placement, half_bounds_sizing);

    auto LogoQuit = UI_ACTION_LAMBDA{ ((veil_ui_state*)context)->quit = true; };

    //TODO(fran): action functions as member objects common to all ui_elements
    SizerAddElement(arena, left_placement, 
        Button({.arena=arena, .theme=&base_iconbutton_theme, .image=logo_img, .context=veil_ui, .on_doubleclick= LogoQuit /*TODO(fran): on click open right click menu*/ }), icon_sizing);

    //TODO(fran): real non editable text
    //TODO(fran): assign system default UI font to this application title
    SizerAddElement(arena, left_placement, CreateButton(arena, &base_noneditabletext_theme, {.type=ui_string_type::str, .str=const_temp_s(appname)}, nil, nil, nil), noneditabletext_sizing);

    SizerAddElement(arena, right_placement, Button({ .arena=arena, .theme=&minmax_theme, .image= minimize_img, .context=veil_ui, .on_unclick= common_ui_actions::MinimizeRestoreVeilUI }), minmaxclose_sizing);

    SizerAddElement(arena, right_placement, Button({.arena=arena, .theme=&minmax_theme, .image= maximize_img, .context=veil_ui, .on_unclick=MaximizeOrRestore}), minmaxclose_sizing);
    //TODO(fran): on Windows when the window is maximized the nc arena needs to be moved down by the height of the top resize border

    SizerAddElement(arena, right_placement, Button({ .arena = arena, .theme = &close_theme, .image = close_img, .context = &veil_ui->quit, .on_unclick = Quit}), minmaxclose_sizing);

    SizerAddElement(arena, layout, h_placement, os_nonclient_top);
    SizerAddElement(arena, layout, client_area, remaining_sizing);

    veil_ui->elements = layout; //TODO(fran): better if the user received where to place their ui
#else
#error Define your OS's implementation
#endif
}

internal void CreateVeilUIElements(veil_ui_state* veil_ui) 
{
    button_theme base_button_theme =
    {
        .color =
        {
            .foreground =
            {
                .normal = {1.f,.0f,.0f,1.0f},
                .disabled = {.2f,.0f,.0f,1.0f},
                .mouseover = {.8f,.0f,.0f,1.0f},
                .pressed = {.5f,.0f,.0f,1.0f},
            },
            .background =
            {
                .normal = {.0f,1.0f,.0f,1.0f},
                .disabled = {.0f,.2f,.0f,1.0f},
                .mouseover = {.0f,.8f,.0f,1.0f},
                .pressed = {.0f,.5f,.0f,1.0f},
            },
            .border = base_button_theme.color.background,
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::round_rect,
        .font = 0,
    };

    slider_theme base_slider_theme =
    {
        .color =
        {
            .track_fill =
            {
                .normal = {.0f,0.5f,0.f,1.f},
                .disabled = {1.f,0.f,0.f,1.f},
                .mouseover = {.0f,.7f,0.f,1.f},
                .pressed = {.0f,.9f,0.f,1.f},
            },
            .track_empty =
            {
                .normal = {1.f,0.f,0.f,1.f},
                .disabled = {1.f,0.f,0.f,1.f},
                .mouseover = {1.f,0.f,0.f,1.f},
                .pressed = {1.f,0.f,0.f,1.f},
            },
            .thumb =
            {
                .normal = {.0f,0.f,0.5f,1.f},
                .disabled = {.0f,.0f,0.1f,1.f},
                .mouseover = {.0f,0.f,0.7f,1.f},
                .pressed = {1.f,0.f,0.9f,1.f},
            },
        },
        .dimension = 
        {
            .track_thickness = .3f,
            .thumb_thickness = .7f,
        },
        .thumb_style = ui_style::circle,
        .track_style = ui_style::round_rect,
    };
    
    hotkey_theme base_hotkey_theme =
    {
        .color =
        {
            .foreground_placeholder =
            {
                .normal = {1.f,1.0f,1.0f,1.0f},
                .disabled = {0.2f,0.2f,0.2f,1.0f},
                .mouseover = {0.9f,0.9f,0.9f,1.0f},
                .pressed = {0.8f,0.8f,0.8f,1.0f},
            },
            .foreground_validhk =
            {
                .normal = {0.0f,1.0f,0.0f,1.0f},
                .disabled = {0.0f,0.2f,0.0f,1.0f},
                .mouseover = {0.0f,0.9f,0.0f,1.0f},
                .pressed = {0.0f,0.8f,0.0f,1.0f},
            },
            .foreground_invalidhk =
            {
                .normal = {1.f,0.0f,0.0f,1.0f},
                .disabled = {0.2f,0.0f,0.0f,1.0f},
                .mouseover = {0.9f,0.0f,0.0f,1.0f},
                .pressed = {0.8f,0.0f,0.0f,1.0f},
            },
            .background =
            {
                .normal = {0.3f,0.3f,0.25f,1.0f},
                .disabled = {0.2f,0.2f,0.2f,1.0f},
                .mouseover = {0.3f,.3f,.25f,1.0f},
                .pressed = {.3f,.3f,.3f,1.0f},
            },
            .border = base_hotkey_theme.color.background,
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::round_rect,
        .font = 0,
    };
    background_theme bk_theme =
    {
        .color =
        {
            .background =
            {
                .normal = {0.4f,0.4f,0.4f,1.0f},
                .mouseover = bk_theme.color.background.normal,
                .pressed = bk_theme.color.background.normal,
                .inactive = {0.35f,0.35f,0.4f,1.0f},
            },
            .border = bk_theme.color.background,
        },
        .dimension =
        {
            .border_thickness = 0,
        },
        .style = ui_style::rect,
    };

    element_sizing_desc base_button_w_sizing =
    { 
        .type = element_sizing_type::font, 
        .font = {.w_extra_chars = 2},
    };

    element_sizing_desc vertical_trio_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = (1.f/3.f)},
    };

    element_sizing_desc empty_pad_sizing =
    {
        .type = element_sizing_type::font,
        .font = {0},
    };

    element_sizing_desc remaining_sizing =
    {
        .type = element_sizing_type::remaining,
    };

    element_sizing_desc hotkey_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .7f},
    };
    element_sizing_desc full_bounds_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = 1.f},
    };

    memory_arena* arena = &veil_ui->permanent_arena;

    ui_element* layout = CreateVSizer(arena); //TODO(fran): initializer list or ... initialization in order to be able to construct the whole ui inside the same parenthesis

#if 0
    ui_element* top = CreateHSizer(arena, sizer_alignment::left);
    SizerAddElement(arena, top, CreateVPad(arena), empty_pad_sizing); //TODO(fran): do I need to add this empty element or will the hsizer automatically return something useful on its own?

    ui_element* middle = CreateHSizer(arena, sizer_alignment::left);
    SizerAddElement(arena, middle, CreateSlider(arena, &base_slider_theme), remaining_sizing);
    SizerAddElement(arena, middle, CreateButton(arena, &base_button_theme), base_button_w_sizing);


    ui_element* bottom = CreateHSizer(arena, sizer_alignment::center);
    SizerAddElement(arena, bottom, CreateHotkey(arena, &base_button_theme), hotkey_sizing);

    SizerAddElement(arena, layout, top, vertical_trio_sizing);
    SizerAddElement(arena, layout, middle, vertical_trio_sizing);
    SizerAddElement(arena, layout, bottom, vertical_trio_sizing);

    //AddButton(&veil_ui->elements, &veil_ui->permanent_arena, base_button_theme);
    //AddButton(&veil_ui->elements, &veil_ui->permanent_arena, base_button_theme);
    //AddButton(&veil_ui->elements, &veil_ui->permanent_arena, base_button_theme);
#else //simpler ui for debugging
    element_sizing_desc TEST_button_w_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .45f},
    };
    element_sizing_desc TEST_filler_pad =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .1f},
    };

    element_sizing_desc TEST_full_slider =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .9f},
    };

    //TODO(fran): maintain aspect ratio between vertical_constraint_sz sizer & horizontal_constraint_sz sizer for any size of the window? (may be a little too much, and probably not really necessary)
    element_sizing_desc vertical_constraint_sz =
    {
        .type = element_sizing_type::font_clamp_to_bounds,
        //.bounds = {.scale_factor = 1.f},
        .font = {.v_scale_factor=11},
    };

    element_sizing_desc horizontal_constraint_sz =
    {
        .type = element_sizing_type::font_clamp_to_bounds,
        //.bounds = {.scale_factor = 1.f},
        .font = {.w_extra_chars = 75},
    };

    ui_element* h_layout = CreateHSizer(arena, sizer_props::alignment::left);
    SizerAddElement(arena, layout, h_layout, full_bounds_sizing);

    ui_element* constrained_layout_v = CreateVSizer(arena, sizer_props::alignment::center);
    ui_element* constrained_layout_h = CreateHSizer(arena, sizer_props::alignment::center);

    ui_element* bk = CreateBackground(arena, &bk_theme, nil, nil, constrained_layout_v);
    SizerAddElement(arena, h_layout, bk, full_bounds_sizing);

    SizerAddElement(arena, constrained_layout_v, constrained_layout_h, vertical_constraint_sz);

    ui_element* internal_layout = CreateVSizer(arena);
    SizerAddElement(arena, constrained_layout_h, internal_layout, horizontal_constraint_sz);

    ui_element* top_third = CreateHSizer(arena, sizer_props::alignment::center);
    SizerAddElement(arena, top_third, CreateSlider(arena, &base_slider_theme, {&veil_ui->threshold,veil_ui->threshold_min,veil_ui->threshold_max}), TEST_full_slider);

    ui_element* mid_third = CreateHSizer(arena, sizer_props::alignment::right);
    SizerAddElement(arena, mid_third,
        CreateButton(arena, &base_button_theme, 
            { 
                .type = ui_string_type::dynamic_id, 
                .str_dyn_id = {
                    .context = &veil_ui->show_veil, 
                    .get_str_id = UI_STRING_DYN_ID_LAMBDA{
                        b32 * show_veil = (decltype(show_veil))context; 
                        return 50u + *show_veil;
                    },
                },
            },
            & veil_ui->show_veil,
            common_ui_actions::OnOffVeil),
        TEST_button_w_sizing);
    SizerAddElement(arena, mid_third, CreateHPad(arena), TEST_filler_pad);

    ui_element* bot_third = CreateHSizer(arena, sizer_props::alignment::center);
    SizerAddElement(arena, bot_third, CreateHotkey(arena, &base_hotkey_theme, &veil_ui->show_ui_hotkey,{ .type = ui_string_type::id, .str_id = 52u }, veil_ui,
        common_ui_actions::MinimizeRestoreVeilUI), hotkey_sizing);

    SizerAddElement(arena, internal_layout, top_third, vertical_trio_sizing);
    SizerAddElement(arena, internal_layout, mid_third, vertical_trio_sizing);
    SizerAddElement(arena, internal_layout, bot_third, vertical_trio_sizing);
#endif

#if 0
    veil_ui->elements = layout;
#else
    CreateOSUIElements(veil_ui, layout);
#endif
}

internal void AcquireAndSetD2DBitmapAsRenderTarget(d3d11_ui_renderer* d3d_renderer, d2d_renderer* renderer2D)
{
    // Retrieve the swap chain's back buffer
    IDXGISurface2* dxgiSurface{ 0 }; defer{ d3d_SafeRelease(dxgiSurface); };
    TESTHR(d3d_renderer->swapChain->GetBuffer(0 /*index*/, __uuidof(dxgiSurface), reinterpret_cast<void**>(&dxgiSurface))
        , "Failed to retrieve DXGI Surface (Backbuffer)");
    // Create a Direct2D bitmap that points to the swap chain surface
    D2D1_BITMAP_PROPERTIES1 d2dBitmapProperties = {};
    d2dBitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;// D2D1_ALPHA_MODE_PREMULTIPLIED;
    d2dBitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d2dBitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    TESTHR(renderer2D->deviceContext->CreateBitmapFromDxgiSurface(dxgiSurface, d2dBitmapProperties, &renderer2D->bitmap)
        , "Failed to create Bitmap from DXGI Surface");
    // Point the device context to the bitmap for rendering
    renderer2D->deviceContext->SetTarget(renderer2D->bitmap);
}

internal const wchar_t* win32_GetUIFontName() {
    //Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
    //-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
    //-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
    //-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

    HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); };

    struct search_font {
        const wchar_t** fontname_options;
        u32 fontname_cnt;
        u32 best_match;
        const wchar_t* match;
    };
    
    local_persistence const wchar_t* default_font = L"";
    local_persistence const wchar_t* requested_fontnames[] = { L"Segoe UI", L"Arial Unicode MS", L"Microsoft YaHei", L"Microsoft YaHei UI", L"Microsoft JhengHei", L"Microsoft JhengHei UI" };

    search_font request = {
        .fontname_options = requested_fontnames,
        .fontname_cnt = ArrayCount(requested_fontnames),
        .best_match = U32MAX,
        .match = default_font,
    };

    //TODO(fran): this is stupid, there must be a way to question about a specific font without having to go through all of them. Also, can we do this with DirectWrite instead of Gdi?
    EnumFontFamiliesExW(dc, nil
        , [](const LOGFONTW* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)->int 
        {
            auto req = ((search_font*)lParam);
            for (u32 i = 0; i < req->fontname_cnt; i++)
            {
                if (_wcsicmp(req->fontname_options[i], lpelfe->lfFaceName) == 0) {
                    if (i < req->best_match) 
                    {
                        req->best_match = i;
                        req->match = req->fontname_options[i];
                        if (i == 0) return false;
                    }
                }
            }
            return true;
        }
    , (LPARAM)&request, 0);

    return request.match;
}

internal d2d_renderer AcquireD2DRenderer(d3d11_ui_renderer* d3d_renderer)
{
    d2d_renderer res{0};

    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_NONE };
#ifdef DEBUG_BUILD
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    TESTHR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &res.factory)
        , "Failed to create Direct2D Factory");

    // Create the Direct2D device that links back to the Direct3D device
    TESTHR(res.factory->CreateDevice(d3d_renderer->dxgiDevice, &res.device)
        , "Failed to create Direct2D Device");
    // Create the Direct2D device context that is the actual render target and exposes drawing commands
    TESTHR(res.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &res.deviceContext)
        , "Failed to create Direct2D Device Context");

    AcquireAndSetD2DBitmapAsRenderTarget(d3d_renderer, &res);

    TESTHR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(res.fontFactory), (IUnknown**)&res.fontFactory)
        , "Failed to create DirectWrite Font Factory");

    //NOTE(fran): Direct Write work with DIPs which are 1/96in, on the other hand, in typography Points are 1/72in
    auto Point = [](f32 DIP/*device independent pixel*/) {return DIP * (96.f / 72.f); };

    res.fontFactory->CreateTextFormat(
        win32_GetUIFontName(),
        nil /*Font collection*/,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        Point(12.0f),
        L"en-us", //TODO(fran): consider locale? (in my opinion it's just pointless)
        &res.font
    );

    //TODO(fran): call this function again whenever we need to reinitialize the d3d surfaces
    return res;
}

internal void ReleaseD2DBitmapAndRenderTarget(d2d_renderer* renderer2D) {
    renderer2D->deviceContext->SetTarget(nil);
    d3d_SafeRelease(renderer2D->bitmap);
}

internal void ReleaseD2DRenderer(d2d_renderer* renderer)
{
    ReleaseD2DBitmapAndRenderTarget(renderer);
    d3d_SafeRelease(renderer->deviceContext);
    d3d_SafeRelease(renderer->device);
    d3d_SafeRelease(renderer->factory);

    *renderer = { 0 };
}


internal d3d11_ui_renderer AcquireD3D11UIRenderer(HWND wnd)
{
    d3d11_ui_renderer res{ 0 };

    u32 deviceflags = D3D11_CREATE_DEVICE_BGRA_SUPPORT /*Support for d2d*/ | D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef DEBUG_BUILD
    deviceflags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    TESTHR(D3D11CreateDevice(nullptr,       // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                            // Module
        deviceflags,
        nullptr, 0,                         // Highest available feature level
        D3D11_SDK_VERSION,
        &res.device,
        nullptr,                            // Actual feature level
        &res.deviceContext)                 // Device context
        , "Failed to create Direct3D device");

    //TODO(fran): if failed use D3D_DRIVER_TYPE_WARP

    TESTHR(As(res.device, &res.dxgiDevice), "Failed to QueryInterface for DXGI Device");

    u32 dxgiFactoryflags = 0;
#ifdef DEBUG_BUILD
    dxgiFactoryflags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    TESTHR(CreateDXGIFactory2(
        dxgiFactoryflags
        , __uuidof(res.dxgiFactory), reinterpret_cast<void**>(&res.dxgiFactory))
        , "Failed to create DXGIFactory2");

    RECT wndRc; GetClientRect(wnd, &wndRc); //TODO(fran): check this values get updated in time, the first time through
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = { 0 };
    //swapchainDesc.Scaling = DXGI_SCALING_NONE; //NOTE(fran): determines scaling when backbuffer size is different from window/output size
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;// DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;// DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapchainDesc.Width = RECTW(wndRc);
    swapchainDesc.Height = RECTH(wndRc);

#if UICOMPOSITOR
    TESTHR(res.dxgiFactory->CreateSwapChainForComposition(res.dxgiDevice, &swapchainDesc
        , nil /* Don’t restrict */, &res.swapChain)
        , "Failed to create Swapchain for composition");
#else
    TESTHR(res.dxgiFactory->CreateSwapChainForHwnd(res.dxgiDevice, wnd, &swapchainDesc
        , nil, nil/* Don’t restrict */, &res.swapChain)
        , "Failed to create Swapchain for composition");
    //TODO(fran): is it ok to use the dxgiDevice, or should I use another one?
#endif

    //TODO(fran): look at IDXGISwapChain2_GetFrameLatencyWaitableObject

    /*
    D3D11_BUFFER_DESC ConstantBufferDesc =
    {
        .ByteWidth = sizeof(renderer_const_buffer),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    TESTHR(res.device->CreateBuffer(&ConstantBufferDesc, 0, &res.constantBuffer)
        , "Failed to create Constant Buffer");

    res.device->CreatePixelShader(veil_ps_bytecode, sizeof(veil_ps_bytecode), 0, &res.pixelShader);
    res.device->CreateVertexShader(veil_vs_bytecode, sizeof(veil_vs_bytecode), 0, &res.vertexShader);
    */

    //TODO(fran): MakeWindowAssociation

    //TODO(fran): real error handling and releaserenderer if failed, look at casey's refterm

    return res;
}

internal void ReleaseD3D11UIRenderTargets(d3d11_ui_renderer* renderer)
{
    d3d_SafeRelease(renderer->renderTarget);
}

internal void ReleaseD3D11UIRenderer(d3d11_ui_renderer* renderer)
{
    ReleaseD3D11UIRenderTargets(renderer);

    d3d_SafeRelease(renderer->dxgiFactory);
    d3d_SafeRelease(renderer->swapChain);
    d3d_SafeRelease(renderer->dxgiDevice);
    d3d_SafeRelease(renderer->deviceContext);
    d3d_SafeRelease(renderer->device);

    *renderer = { 0 };
}

#if UICOMPOSITOR
internal windows_ui_compositor AcquireWindowsUICompositor(veil_ui_state* veil_ui)
{
    windows_ui_compositor res{ 0 };

    TESTHR(DCompositionCreateDevice(veil_ui->renderer.dxgiDevice, __uuidof(res.device), reinterpret_cast<void**>(&res.device))
        , "Failed to create Composition Device");

    TESTHR(res.device->CreateTargetForHwnd(veil_ui->wnd, true /*Top most*/, &res.target)
        , "Failed to create Composition Target for the window");

    TESTHR(res.device->CreateVisual(&res.visual)
        , "Failed to create Visual from Composition Device");

    TESTHR(res.visual->SetContent(veil_ui->renderer.swapChain)
        , "Failed to set Composition Visual's content to the Swapchain");

    TESTHR(res.target->SetRoot(res.visual)
        , "Failed to set new Composition Visual as new root object of the Composition Target");

    return res;
}

internal void ReleaseWindowsUICompositor(windows_ui_compositor* compositor)
{
    d3d_SafeRelease(compositor->visual);
    d3d_SafeRelease(compositor->target);
    d3d_SafeRelease(compositor->device);
    //TODO(fran): do I need to remove something prior to releasing?
}

internal void OutputUIToWindowsCompositor(veil_ui_state* veil_ui)
{
    local_persistence windows_ui_compositor compositor = AcquireWindowsUICompositor(veil_ui);
    // Make the swap chain available to the composition engine
    TESTHR(veil_ui->renderer.swapChain->Present(1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");

    //NOTE(fran): every time we finish rendering to our d3dtexture we must call Commit on the windows compositor in order for it to use the new stuff
    //TESTHR(veil_ui->compositor.device->Commit()
    TESTHR(compositor.device->Commit()
        , "Failed to commit pending Composition commands to the Composition Device");

    //TODO(fran): find out if the swapchain and compositor need some extra command in order to force execution, in case they're just buffering everything for later
}
#else
internal void OutputToScreen(veil_ui_state* veil_ui)
{
    TIMEDFUNCTION();

#if 0
    // Make the swap chain available to the composition engine
    TESTHR(veil_ui->renderer.swapChain->Present(1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");
#else
    //TODO(fran): HACK: we should simply check for elapsed time in the main loop, and if it is less than the refresh rate ms then wait/sleep. Only thing stopping me from doing it is I dont yet quite understand how that would affect correct vsync
    TESTHR(veil_ui->renderer.swapChain->Present( veil_ui->show_veil ? 0:1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");
#endif 

    //NOTE(fran): every time we finish rendering to our d3dtexture we must call Commit on the windows compositor in order for it to use the new stuff
   // TESTHR(Veil->compositor.device->Commit()
   //     , "Failed to commit pending Composition commands to the Composition Device");

    //TODO(fran): find out if the swapchain and compositor need some extra command in order to force execution, in case they're just buffering everything for later
}
#endif

internal void AcquireLanguageManager(language_manager* res)
{
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

#define WM_TRAY (WM_USER + 101)
internal void AcquireTrayIcon(ui_tray_state* res, HWND hwnd, /*const*/ veil_ui_state* veil_ui)
{
    assert(veil_ui->wnd == hwnd);
    //TODO(fran): OS code
    //TODO(fran): later we may want to return or receive some object that contains all the properties of the tray element, eg actions on the different clicks, and the right click menu
    //NOTE(fran): realistically one application always has at most one tray icon, we wont bother with handling multiple ones
    
    HINSTANCE instance = GetModuleHandleW(nil);
    HICON tray_icon = nil;
    i32 SmallIconX = GetSystemMetrics(SM_CXSMICON);//TODO(fran): GetSystemMetricsForDpi?
    tray_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, SmallIconX, SmallIconX, 0);
    assert(tray_icon);

    //TODO(fran): this may not work on Windows XP: https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
    NOTIFYICONDATAW notification =
    {
        .cbSize = sizeof(notification),
        .hWnd = hwnd,
        .uID = 1,
        .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,//TODO(fran): check this
        .uCallbackMessage = WM_TRAY,
        .hIcon = tray_icon,

#ifdef appnameL
#error Macro already defined elsewhere
#endif
#define __appnameL(name) L##name
#define ___appnameL(name) __appnameL(#name)
#define ____appnameL(name) ___appnameL(name)
#define appnameL ____appnameL(_appname)
        .szTip = appnameL, //NOTE(fran): must be less than 128 chars
#undef appnameL
#undef __appnameL
#undef ___appnameL
#undef ____appnameL

        .dwState = NIS_SHAREDICON,//TODO(fran): check this
        .dwStateMask = NIS_SHAREDICON,
        .szInfo = 0,
        .uVersion = NOTIFYICON_VERSION_4,//INFO(fran): this changes the message format, but not when nif_showtip is enabled, I think
        .szInfoTitle = 0,
        .dwInfoFlags = NIIF_NONE,
        //.guidItem = ,
        .hBalloonIcon = NULL,
    };

    b32 ret = Shell_NotifyIconW(NIM_ADD, &notification);
    assert(ret);

    *res =
    {
        .impl = {.notification = notification},
        .on_unclick = common_ui_actions::OnOffVeil,
        .on_unclick_context = &veil_ui->show_veil,
        .on_unrclick = common_ui_actions::MinimizeRestoreVeilUI,
        .on_unrclick_context = veil_ui,
    };

}

internal void ReleaseTrayIcon(ui_tray_state* tray)
{
    BOOL ret = Shell_NotifyIconW(NIM_DELETE, &tray->impl.notification);
    DestroyIcon(tray->impl.notification.hIcon);
}

internal void AcquireVeilUIState(veil_ui_state* res, HWND veil_ui_wnd, u64 main_thread_id)
{
    res->wnd = veil_ui_wnd;
    res->main_thread_id = main_thread_id;
    //TODO(fran): retrieve settings from save file, metaprogramming
    res->threshold = .5f;//TODO(fran): what if the user only changed the threshold?

    res->threshold_min = 0.2f;
    res->threshold_max = 1.0f;

    //res->brightness = .6f;
    res->scaling = 1.f;

    res->quit = false;

    res->show_veil = true;

    res->show_ui_hotkey = { 0 };

    res->interacting_with = nil;
    res->under_the_mouse = nil;
    res->keyboard_focus = nil;

    f32 units = 29.f;
    f32 units_to_pixels = GetSystemMetrics(SM_CXMENUCHECK); //TODO(fran): less terrible way to calculate the conversion
    i32 wnd_w = units_to_pixels * units;
    i32 wnd_h = wnd_w * 9.5f / 16.f;

    RECT DesktopRect; GetWindowRect(GetDesktopWindow(), &DesktopRect);
    i32 wnd_x = RECTW(DesktopRect) / 2 - wnd_w / 2;
    i32 wnd_y = RECTH(DesktopRect) / 2 - wnd_h / 2;

#ifdef DEBUG_BUILD
    wnd_x = wnd_y = 100;
#endif

    MoveWindow(res->wnd, wnd_x, wnd_y, wnd_w, wnd_h, false);
    ShowWindow(res->wnd, SW_SHOW);

    res->placement =
    {
        .x = (f32)wnd_x,
        .y = (f32)wnd_y,
        .w = (f32)wnd_w,
        .h = (f32)wnd_h,
        //TODO(fran): make sure this does not include the nonclient area
    };

    res->render_and_update_screen = true; //TODO(fran): do this same thing on WM_SIZE or whatever

#ifdef DEBUG_BUILD
    LPVOID base_address = (LPVOID)Terabytes(1); //Fixed address for ease of debugging
#else
    LPVOID base_address = 0;
#endif

    u32 total_sz = Megabytes(1);

    initialize_arena(
        &res->permanent_arena,
        (u8*)VirtualAlloc(base_address, total_sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE),//TODO(fran): use large pages
        total_sz
    );
    assert(res->permanent_arena.base);

    res->renderer = AcquireD3D11UIRenderer(res->wnd);

    res->renderer2D = AcquireD2DRenderer(&res->renderer);

#if UICOMPOSITOR
    res.compositor = AcquireWindowsUICompositor(&res);
#endif

    AcquireLanguageManager(&res->LanguageManager);

    zero_struct(res->global_registered_hotkeys);

    res->elements = nil;
    CreateVeilUIElements(res);

    //TODO(fran): dynamic UI string flag, in order to avoid storing state, that way we can turn on/off the veil from here without having to update the button's ui_string id

    AcquireTrayIcon(&res->tray, veil_ui_wnd, res);
}

internal void ReleaseVeilUIState(veil_ui_state* veil_ui) {
    VirtualFree(veil_ui->permanent_arena.base, 0, MEM_RELEASE);
    veil_ui->permanent_arena = { 0 };

    ReleaseD2DRenderer(&veil_ui->renderer2D);

    ReleaseD3D11UIRenderer(&veil_ui->renderer);

    ReleaseTrayIcon(&veil_ui->tray);

#if UICOMPOSITOR
    ReleaseWindowsUICompositor(&veil_ui->compositor);
#endif
}

internal void EnableRendering(veil_ui_state* veil_ui)
{
    veil_ui->render_and_update_screen = true;
}
internal b32 IsRenderingEnabled(veil_ui_state* veil_ui)
{
    b32 res = veil_ui->render_and_update_screen;
    return res;
}

struct input_results {
    ui_element* next_hot; //Determines the element the mouse is on top of
};
internal input_results InputUpdate(ui_element* elements, user_input* input)
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
                ui_sizer_element* superchild = element->data.sizer.childs;
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

#define safe_call(funcptr, ...) if(funcptr) funcptr(__VA_ARGS__); else 0

internal void BeginInteraction(veil_ui_state* veil_ui, ui_element* next_hot, user_input* input)
{
    EnableRendering(veil_ui);

    veil_ui->interacting_with = next_hot;

    auto element = veil_ui->interacting_with;

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
            else CancelInteraction(veil_ui);
        } break;
        case ui_type::hotkey:
        {
            veil_ui->keyboard_focus = element;
            //TODO(fran): activate blue selection drawing over the hotkey text
        } break;
        case ui_type::button:
        {
            auto& data = element->data.button;

            //TODO(fran): quick HACK: made it official whether we are currently working with left or right click
            if (IsDoubleclicked(input->keys[input_key::left_mouse]))
                safe_call(data.OnDoubleClick, element, data.context);
            else 
                safe_call(data.OnClick, element, data.context);
        } break;
        case ui_type::background:
        {
            auto& data = element->data.background;
            //TODO(fran): same quick HACK
            if (IsDoubleclicked(input->keys[input_key::left_mouse]))
                safe_call(data.OnDoubleClick, element, data.context);
            else
                safe_call(data.OnClick, element, data.context);
        } break;
    }
}

internal void EndInteraction(veil_ui_state* veil_ui, user_input* input)
{
    EnableRendering(veil_ui);

    auto element = veil_ui->interacting_with;

    auto last_interaction_st = element->interaction_st;

    element->interaction_st = (veil_ui->under_the_mouse == element) ? interaction_state::mouseover : interaction_state::normal;

    switch (element->type)
    {
        case ui_type::button:
        {
            auto& data = element->data.button;
            if (last_interaction_st == interaction_state::pressed)
                safe_call(data.OnUnclick, element, data.context);
        } break;
    }

    veil_ui->interacting_with = nil;
}

internal void ContinueInteraction(veil_ui_state* veil_ui, user_input* input)
{
    EnableRendering(veil_ui);

    auto element = veil_ui->interacting_with;

    element->interaction_st = (veil_ui->under_the_mouse == element) ? interaction_state::pressed : interaction_state::mouseover;

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

internal void UpdateUnderTheMouse(veil_ui_state* veil_ui, ui_element* next_hot, user_input* input)
{
    //TODO(fran): handle EnableRendering
    if (ui_element* element = veil_ui->under_the_mouse; element && element!=veil_ui->interacting_with)
    {
        auto last_interaction_st = element->interaction_st;

        element->interaction_st = (next_hot == element) ? interaction_state::mouseover : interaction_state::normal;

        if (last_interaction_st != element->interaction_st) EnableRendering(veil_ui);
    }
    veil_ui->under_the_mouse = next_hot; //TODO(fran): we should set this guy to interaction_state::mouseover if it's valid

    if (ui_element* element = veil_ui->under_the_mouse)
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

                if(oldvalue != newvalue) EnableRendering(veil_ui);
            } break;
        }
    }
}

internal void UpdateGlobalHotkeys(veil_ui_state* veil_ui, user_input* input)
{
    if (input->global_hotkey_id != 0)
    {
        auto& [func, element, context, registration_time, enabled] = veil_ui->global_registered_hotkeys[input->global_hotkey_id];
        
        //NOTE(fran): the user takes time to enter a hotkey and then release those same keys, in that time we get hotkey triggered messages from the OS! This can not be solved using wm_keyup. The problem actually also occurs on my previous win32 projects, but it seems like the delay there is much bigger (Windows inefficiencies possibly to blame) so I never noticed until now
        if (enabled || (enabled=EndCounter(registration_time) > 250/*milliseconds*/))
        {
            EnableRendering(veil_ui);

            func(element, context);
        }
    }
}

void UnregisterGlobalHotkey(veil_ui_state* veil_ui, ui_hotkey hotkey)
{
    b32 res = UnregisterHotKey(nil, hotkey.id); //TODO(fran): OS code
    if(res)
        veil_ui->global_registered_hotkeys[hotkey.id] = {0};
}

b32 RegisterGlobalHotkey(veil_ui_state* veil_ui, ui_hotkey hotkey, globalhotkey_action_on_triggered* action, ui_element* element, void* context)
{
    UnregisterGlobalHotkey(veil_ui, hotkey);
    b32 res = RegisterHotKey(nil, hotkey.id, hotkey.hk->mods | MOD_NOREPEAT, hotkey.hk->vk); //TODO(fran): OS code
    //NOTE(fran): if we dont associate the hotkey with any hwnd then it just sends the msg to the current thread, also documentation says it can not associate the hotkey with a wnd created from another thread
    if (res)
        veil_ui->global_registered_hotkeys[hotkey.id] = {.action=action, .element=element, .context=context, .registration_time=StartCounter()};
    return res;
}

internal void UpdateKeyboardFocus(veil_ui_state* veil_ui, user_input* input)
{
    if (ui_element* element = veil_ui->keyboard_focus; element)
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
                        EnableRendering(veil_ui);
                        //TODO(fran): flag for defining global & non global hotkeys (also later we'll probably want to have application global hotkeys in the sense they get triggered even if no keyboard focus is on the element, but that is application code, not OS)
                        *data.current_hotkey.hk = input->hotkey;
                        b32 registered = RegisterGlobalHotkey(veil_ui, data.current_hotkey, data.on_hotkey, element, data.context);
                        
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

internal void UpdateTray(veil_ui_state* veil_ui, user_input* input)
{
    if (input->tray.on_unclick)
    {
        EnableRendering(veil_ui);
        safe_call(veil_ui->tray.on_unclick, nil, veil_ui->tray.on_unclick_context);
    }
    if (input->tray.on_unrclick)
    {
        EnableRendering(veil_ui);
        safe_call(veil_ui->tray.on_unrclick, nil, veil_ui->tray.on_unrclick_context);
    }
}

internal void InputProcessing(veil_ui_state* veil_ui, ui_element* next_hot, user_input* input)
{
    UpdateUnderTheMouse(veil_ui, next_hot, input);
    if (veil_ui->interacting_with)
    {
        if (IsUnclicked(input->keys[input_key::left_mouse]))
        {
            EndInteraction(veil_ui, input);
        }
        else ContinueInteraction(veil_ui, input);
    }
    else if (IsClicked(input->keys[input_key::left_mouse]) && next_hot)
    {
        BeginInteraction(veil_ui, next_hot, input);
        //TODO(fran): OS::capture_mouse()
    }
    if(IsWindowVisible(veil_ui->wnd))//TODO(fran): HACK: I had to add this condition to cover the stupid edge case where the user is registering a hotkey but does not release the keys and causes a hotkey triggered event to happen. Strange things happened in that case which I couldnt quite decipher and the end result was that the hotkey wouldnt work until the user clicked on the screen
        UpdateKeyboardFocus(veil_ui, input);

    UpdateGlobalHotkeys(veil_ui, input); 
    UpdateTray(veil_ui, input); 
}

enum class horz_text_align {
    left = 0, center, right, justified
};

enum class vert_text_align {
    top = 0, center, bottom
};

struct dwrite_flags {
    DWRITE_TEXT_ALIGNMENT horizontal_align;
    DWRITE_PARAGRAPH_ALIGNMENT vertical_align;
    D2D1_DRAW_TEXT_OPTIONS draw_flags;
};
internal dwrite_flags get_dwrite_flags(horz_text_align h_align, vert_text_align v_align, b32 clip_to_rect)
{
    dwrite_flags res;
    DWRITE_TEXT_ALIGNMENT horizontal_align_map[4]{
    DWRITE_TEXT_ALIGNMENT_LEADING,
    DWRITE_TEXT_ALIGNMENT_CENTER,
    DWRITE_TEXT_ALIGNMENT_TRAILING,
    DWRITE_TEXT_ALIGNMENT_JUSTIFIED };

    res.horizontal_align = horizontal_align_map[(int)h_align];

    DWRITE_PARAGRAPH_ALIGNMENT vertical_align_map[3]{
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        DWRITE_PARAGRAPH_ALIGNMENT_FAR };
    res.vertical_align = vertical_align_map[(int)v_align];

    res.draw_flags =
        D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | //TODO(fran): from what I read, as an in between browsers allow only quarter pixel snappings, I doubt that can be configured with DirectWrite though
         (clip_to_rect ? D2D1_DRAW_TEXT_OPTIONS_CLIP : (D2D1_DRAW_TEXT_OPTIONS)0)
        | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT //TODO(fran): make sure this is what I think it is
        | D2D1_DRAW_TEXT_OPTIONS_DISABLE_COLOR_BITMAP_SNAPPING; //TODO(fran): test how this looks on & off
    
    return res;
}

internal s8 GetUIStringStr(veil_ui_state* veil_ui, const ui_string& text)
{
    s8 res;
    if (text.type == ui_string_type::id)
        res = veil_ui->LanguageManager.GetString(text.str_id);
    else if (text.type == ui_string_type::str)
        res = text.str;
    else if (text.type == ui_string_type::dynamic_id)
        res = veil_ui->LanguageManager.GetString(text.str_dyn_id.get_str_id(text.str_dyn_id.context));
    else crash();
    return res;
}

internal rc2 MeasureText(IDWriteFactory* font_factory, IDWriteTextFormat* font, const utf8* text, u32 char_cnt, rc2 text_rect, horz_text_align h_align, vert_text_align v_align, b32 clip_to_rect = false)
{
    rc2 res;

    auto text16 = _OS::convert_to_s16({ .chars = const_cast<utf8*>(text), .cnt = char_cnt }); defer{ _OS::free_small_mem(text16.chars); }; //TODO(fran): HACK: define what we'll do with string encoding

    auto [horizontal_align, vertical_align, draw_flags] = get_dwrite_flags(h_align, v_align, clip_to_rect);

    font->SetTextAlignment(horizontal_align);
    font->SetParagraphAlignment(vertical_align);

    //TODO(fran): there's _no_ simple way to specify the draw_flags to CreateTextLayout, meaning we cant use clip_to_rect (nor other flags), the closest I think is font->SetTrimming(), look into that:
    // https://stackoverflow.com/questions/51009082/display-text-in-a-specified-rectangle-with-directwrite

    IDWriteTextLayout* layouter; defer{ d3d_SafeRelease(layouter); };
    TESTHR(font_factory->CreateTextLayout((wchar_t*)text16.chars, text16.cnt, font, text_rect.w, text_rect.h, &layouter)
        , "Failed to create DirectWrite TextLayout");
    DWRITE_TEXT_METRICS metrics;
    layouter->GetMetrics(&metrics);

    res.left = text_rect.left + metrics.left;
    res.top  = text_rect.top  + metrics.top;
    res.w = metrics.width; 
    //TODO(fran): look at metrics.widthIncludingTrailingWhitespace (which also apparently has a colossal bug when using DWRITE_TEXT_ALIGNMENT_TRAILING):
    //https://stackoverflow.com/questions/68034476/direct2d-widthincludingtrailingwhitespace-0-while-width-0
    res.h = metrics.height;

    return res;
}

internal void RenderText(ID2D1DeviceContext* renderer, IDWriteTextFormat* font, const utf8* text, u32 char_cnt, v4 color, rc2 text_rect,  horz_text_align h_align, vert_text_align v_align, b32 clip_to_rect = false) 
{
    assert(font);

    auto [horizontal_align, vertical_align, draw_flags] = get_dwrite_flags(h_align, v_align, clip_to_rect);

    font->SetTextAlignment(horizontal_align);

    font->SetParagraphAlignment(vertical_align);

    //TODO(fran): renderer->SetTextRenderingParams()
    //NOTE(fran): renderer->SetTextAntialiasMode() is set to D2D1_TEXT_ANTIALIAS_MODE_DEFAULT which defaults to cleartype
    text_rect.w += 1; //TODO(fran): HACK: we need to add this correction because MeasureText has no way (that I found) to specify the measuring mode, it's using DWRITE_MEASURING_MODE_NATURAL but I've changed this text rendering code to use DWRITE_MEASURING_MODE_GDI_NATURAL
    D2D1_RECT_F drawrect = rc2_to_D2DRECT(text_rect);

    ID2D1SolidColorBrush* fg_brush{ 0 }; defer{ d3d_SafeRelease(fg_brush); };
    auto fg_brush_props = D2D1::BrushProperties(color.a);
    TESTHR(renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&color, &fg_brush_props, &fg_brush)
        , "Failed to create Direct2D brush, pathetic");

    auto text16 = _OS::convert_to_s16({ .chars = const_cast<utf8*>(text), .cnt = char_cnt }); defer{ _OS::free_small_mem(text16.chars); }; //TODO(fran): HACK: define what we'll do with string encoding

    renderer->DrawTextW((wchar_t*)text16.chars, text16.cnt, font, drawrect, fg_brush, draw_flags, DWRITE_MEASURING_MODE_GDI_NATURAL); //NOTE(fran): DWRITE_MEASURING_MODE_GDI_NATURAL is the best
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

internal void DrawBackground(ID2D1DeviceContext* renderer, ui_element* element, v4 bk, ui_style style)
{
    //TODO(fran): border
    ID2D1SolidColorBrush* bk_brush{ 0 }; defer{ d3d_SafeRelease(bk_brush); };

    auto bk_brush_props = D2D1::BrushProperties(bk.a);
    TESTHR(renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&bk, &bk_brush_props, &bk_brush)
        , "Failed to create Direct2D brush");

    switch (style)
    {
    case ui_style::rect:
    {
        renderer->FillRectangle(rc2_to_D2DRECT(element->placement), bk_brush);
    } break;
    case ui_style::round_rect:
    {
        //TODO(fran): better radius calculation, depending on the dimensions of the button we end up with almost circles
        f32 Xradius = minimum(element->placement.w, element->placement.h) * .5f;
        f32 Yradius = Xradius;
        D2D1_ROUNDED_RECT roundrect = { rc2_to_D2DRECT(element->placement), Xradius, Yradius };
        renderer->FillRoundedRectangle(roundrect, bk_brush);
    } break;
    case ui_style::circle:
    {
        f32 r = minimum(element->placement.w, element->placement.h);
        D2D1_ELLIPSE circle = D2D1::Ellipse({ element->placement.centerX(), element->placement.centerY() }, r, r);
        renderer->FillEllipse(circle, bk_brush);
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

internal sz2 MeasureAverageTextCharacter(IDWriteFactory* font_factory, IDWriteTextFormat* font)
{
    sz2 res;
    constexpr s8 avgchar = const_temp_s(u8"_"); //TODO(fran): we use an underscore instead of a space because space returns 0 width since we aint using trailing whitespace inside MeasureText
    res = MeasureText(font_factory, font, avgchar.chars, avgchar.cnt, { 0,0,F32MAX,F32MAX }, horz_text_align::left, vert_text_align::top).wh;
    return res;
}

internal void RenderElement(veil_ui_state* veil_ui, ui_element* element) 
{
    //TODO(fran): get rid of the recursion
    //TODO(fran): translation transformation before going to the render code, in order to not have to correct for the xy position (renderer->SetTransform), we gonna need to create two rc2s for each element so we can have both the transformed and non transformed rect
    while (element) //TODO(fran): should this while loop be here on in RendererDraw?
    {
        D2D1_RECT_F drawrect = rc2_to_D2DRECT(element->placement);
        auto renderer = veil_ui->renderer2D.deviceContext;
        auto font_factory = veil_ui->renderer2D.fontFactory;
        
        switch (element->type)
        {
        case ui_type::button:
        {
            auto data = element->data.button;

            auto [fg, bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            assert(data.theme.dimension.border_thickness == 0);
            DrawBackground(renderer, element, bk, data.theme.style);

            s8 text = GetUIStringStr(veil_ui, data.text);
                
            RenderText(renderer, veil_ui->renderer2D.font, text.chars, text.cnt, fg, element->placement, horz_text_align::center, vert_text_align::center, true);

            if (text.cnt == 0) //TODO(fran): buttons with text + image
            {
                D2D1_BITMAP_PROPERTIES1 props = {
                    .pixelFormat = {DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, //NOTE(fran): direct2d only works with premultiplied alpha
                    .dpiX = 0, //TODO(fran): dpi
                    .dpiY = 0,
                    .bitmapOptions = D2D1_BITMAP_OPTIONS_NONE,
                    .colorContext = nil, //TODO(fran)
                };

                rc2 img_bounds = scalefromcenter_rc(element->placement, data.image.bounds.scale_factor);
                switch (data.image.type)
                {
                    case ui_image_type::img:
                    {
                        //TODO(fran): hash table to store already generated images
                        ID2D1Bitmap1* img; defer{ d3d_SafeRelease(img); };
                        TESTHR(renderer->CreateBitmap({ data.image.img->w,data.image.img->h }, data.image.img->mem, data.image.img->w * (data.image.img->bpp / 8), props, &img)
                            , "Failed to create Direct2D bitmap");

                        rc2 imgrc = GetImageRc(img_bounds, data.image.img->w, data.image.img->h);

                        renderer->DrawBitmap(img, rc2_to_D2DRECT(imgrc));
                    } break;
                    case ui_image_type::mask:
                    { 
                        ID2D1Bitmap1* img; defer{ d3d_SafeRelease(img); };
                        TESTHR(renderer->CreateBitmap({ data.image.img->w,data.image.img->h }, data.image.img->mem, data.image.img->w* (data.image.img->bpp / 8), props, & img)
                            , "Failed to create Direct2D bitmap");

                        ID2D1SolidColorBrush* fg_br{ 0 }; defer{ d3d_SafeRelease(fg_br); };
                        v4 col = fg;
                        auto brush_props = D2D1::BrushProperties(col.a);
                        renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &brush_props, &fg_br);

                        auto oldmode = renderer->GetAntialiasMode(); defer{ renderer->SetAntialiasMode(oldmode); }; //NOTE(fran): FillOpacityMask requires Aliased Mode
                        renderer->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

                        rc2 imgrc = GetImageRc(img_bounds, data.image.img->w, data.image.img->h);

                        renderer->FillOpacityMask(img, fg_br, rc2_to_D2DRECT(imgrc));
                    } break;
                    case ui_image_type::render_commands:
                    {
                        ID2D1SolidColorBrush* fg_br{ 0 }; defer{ d3d_SafeRelease(fg_br); };
                        v4 col = fg;
                        auto brush_props = D2D1::BrushProperties(col.a);
                        renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &brush_props, &fg_br);

                        data.image.render_commands(renderer, fg_br, element, img_bounds, data.context); //TODO(fran): separate contexts
                    } break;
                    case ui_image_type::none: {} break;
                #ifdef DEBUG_BUILD //TODO(fran): does adding the default case impact performance?
                    default: { crash(); } break;
                #endif
                }
            }

        } break;
        case ui_type::slider:
        {
            auto& data = element->data.slider;

            ID2D1SolidColorBrush* TrackEmptyBrush{ 0 }; defer{ d3d_SafeRelease(TrackEmptyBrush); };
            ID2D1SolidColorBrush* TrackFillBrush{ 0 }; defer{ d3d_SafeRelease(TrackFillBrush); };
            ID2D1SolidColorBrush* ThumbBrush{ 0 }; defer{ d3d_SafeRelease(ThumbBrush); };

            v4 track_empty_color = data.theme.color.track_empty.normal;
            v4 track_fill_color = data.theme.color.track_fill.normal;
            v4 thumb_color = data.theme.color.thumb.normal;

            auto brush_props = D2D1::BrushProperties(track_empty_color.a);
            renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&track_empty_color, &brush_props, &TrackEmptyBrush);
            brush_props = D2D1::BrushProperties(track_fill_color.a);
            renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&track_fill_color, &brush_props, &TrackFillBrush);
            brush_props = D2D1::BrushProperties(thumb_color.a);
            renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&thumb_color, &brush_props, &ThumbBrush);

            assert(data.theme.thumb_style == ui_style::circle);
            assert(data.theme.track_style == ui_style::round_rect);

            data.calculate_colliders(element->placement);

            f32 Xradius = minimum(data.colliders.full_track.w, data.colliders.full_track.h) * .5f;
            f32 Yradius = Xradius;
            D2D1_ROUNDED_RECT roundrect = { rc2_to_D2DRECT(data.colliders.empty_track), Xradius, Yradius };
            renderer->FillRoundedRectangle(roundrect, TrackEmptyBrush);

            roundrect.rect = rc2_to_D2DRECT(data.colliders.fill_track);
            renderer->FillRoundedRectangle(roundrect, TrackFillBrush);

            D2D1_ELLIPSE ellipse = D2D1::Ellipse({ data.colliders.thumb.centerX(), data.colliders.thumb.centerY() }, data.colliders.thumb.w * .5f, data.colliders.thumb.h * .5f);
            renderer->FillEllipse(ellipse, ThumbBrush);

        } break;
        case ui_type::sizer: //TODO(fran): we could use the sizer as a poor man's way of implementing and rendering backgrounds
        {
            auto data = element->data.sizer;
#ifdef DEBUG_BUILD
            ID2D1SolidColorBrush* d2dBrush{ 0 }; defer{ d3d_SafeRelease(d2dBrush); };
            ID2D1SolidColorBrush* auraBrush{ 0 }; defer{ d3d_SafeRelease(auraBrush); };
            v4 bk_color = data.bk_color;
            auto bk_brush_props = D2D1::BrushProperties(bk_color.a);
            TESTHR(veil_ui->renderer2D.deviceContext->CreateSolidColorBrush((D2D1_COLOR_F*)&bk_color, &bk_brush_props, &d2dBrush)
                , "Failed to create Direct2D brush");
            auto aura_brush_props = D2D1::BrushProperties(bk_color.a*.2f);
            //NOTE(fran): im beyond amazed that you need to pass the alpha as a _separate_ property, it does not care about the color's alpha
            TESTHR(veil_ui->renderer2D.deviceContext->CreateSolidColorBrush((D2D1_COLOR_F*)&bk_color, &aura_brush_props, &auraBrush)
                , "Failed to create Direct2D brush");
            
            rc2 _aura_rect = element->placement;// inflate_rc(element->placement, 1);

            #if 1
            f32 linewidth = (element == veil_ui->under_the_mouse) ? 3 : 1; //TODO(fran): this dont work, we need to implement reverse traversal in order to find the sizer that directly controls the under the mouse element
            renderer->DrawRectangle(rc2_to_D2DRECT(_aura_rect), d2dBrush, linewidth);
            #else
            D2D1_RECT_F aura_rect = rc2_to_D2DRECT(_aura_rect);
            veil_ui->renderer2D.deviceContext->FillRectangle(&drawrect, d2dBrush);
            veil_ui->renderer2D.deviceContext->FillRectangle(&aura_rect, auraBrush);
            #endif
#endif
            ui_sizer_element* superchild = element->data.sizer.childs;
            while (superchild) {
                ui_element* child = superchild->element;
                while (child) {
                    RenderElement(veil_ui, child);
                    child = child->child;
                }
                superchild = superchild->next;
            }
        } break;
        case ui_type::hpad:
        case ui_type::vpad:
        {
#ifdef DEBUG_BUILD
            ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
            v4 bk_color = { .9f,.2f,.1f,7.f };
            auto bk_brush_props = D2D1::BrushProperties(bk_color.a);
            TESTHR(renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&bk_color, &bk_brush_props, &br)
                , "Failed to create Direct2D brush");

            f32 line_width = 1;

            v2 p1 = element->placement.xy;
            v2 p2 = element->placement.bottom_right();

            v2 p3 = element->placement.top_right();
            v2 p4 = element->placement.bottom_left();

            renderer->DrawLine(*(D2D1_POINT_2F*)&p1, *(D2D1_POINT_2F*)&p2, br, line_width);
            renderer->DrawLine(*(D2D1_POINT_2F*)&p3, *(D2D1_POINT_2F*)&p4, br, line_width);
            renderer->DrawRectangle(drawrect, br, line_width);
#endif
        } break;
        case ui_type::hotkey:
        {
            auto data = element->data.hotkey;

            ID2D1SolidColorBrush* bk_brush{ 0 }; defer{ d3d_SafeRelease(bk_brush); };
            auto [fg, bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st, data.string_state);

            auto bk_brush_props = D2D1::BrushProperties(bk.a);
            TESTHR(veil_ui->renderer2D.deviceContext->CreateSolidColorBrush((D2D1_COLOR_F*)&bk, &bk_brush_props, &bk_brush)
                , "Failed to create Direct2D brush");

            assert(data.theme.style == ui_style::round_rect);

            //TODO(fran): better radius calculation, depending on the dimensions we end up with almost circles
            f32 Xradius = minimum(element->placement.w, element->placement.h) * .5f;
            f32 Yradius = Xradius;
            D2D1_ROUNDED_RECT roundrect = { rc2_to_D2DRECT(element->placement), Xradius, Yradius };
            renderer->FillRoundedRectangle(roundrect, bk_brush);

            s8 text{0};
            utf8 hotkeystr[128];
            if (data.string_state != hotkey_string_state::placeholder)
            {
                text = { .chars = hotkeystr, .cnt = 0, .cnt_allocd = ArrayCount(hotkeystr) };
                OS::HotkeyToString(*data.current_hotkey.hk, &text);
            }
            else {
                if (data.placeholder_text.type == ui_string_type::id)
                    text = veil_ui->LanguageManager.GetString(data.placeholder_text.str_id);
                else
                    text = data.placeholder_text.str;
            }
            
            rc2 text_rc_bk = MeasureText(font_factory, veil_ui->renderer2D.font, text.chars, text.cnt, element->placement, horz_text_align::center, vert_text_align::center, true);
            //TODO(fran): expand the rectangle half a character on each side

            sz2 avg_char = MeasureAverageTextCharacter(font_factory, veil_ui->renderer2D.font);

            text_rc_bk = scalefromcenterconst_rc(text_rc_bk, avg_char);

            Xradius = minimum(text_rc_bk.w, text_rc_bk.h) * .25f;
            Yradius = Xradius;
            roundrect = { rc2_to_D2DRECT(text_rc_bk), Xradius, Yradius };
            v4 text_rect_bk_col = bk * .5f; //TODO(fran): add to hotkey theme?
            ID2D1SolidColorBrush* text_bk_brush{ 0 }; defer{ d3d_SafeRelease(text_bk_brush); };
            auto text_bk_brush_props = D2D1::BrushProperties(text_rect_bk_col.a);
            TESTHR(veil_ui->renderer2D.deviceContext->CreateSolidColorBrush((D2D1_COLOR_F*)&text_rect_bk_col, &text_bk_brush_props, &text_bk_brush)
                , "Failed to create Direct2D brush");
            renderer->FillRoundedRectangle(roundrect, text_bk_brush);

            RenderText(renderer, veil_ui->renderer2D.font, text.chars, text.cnt, fg, element->placement, horz_text_align::center, vert_text_align::center, true);

            assert(data.theme.dimension.border_thickness == 0);

        } break;
        case ui_type::background:
        {
            auto data = element->data.background;

            auto [bk, bd] = GetColorsForInteractionState(&data.theme.color, element->interaction_st);
            assert(data.theme.dimension.border_thickness == 0);
            DrawBackground(renderer, element, bk, data.theme.style);
        } break;
        default: crash();
        }

        element = element->child;
    }
}

internal void RendererDraw(veil_ui_state* veil_ui, v2 mouseP)
{
    //NOTE(fran): Direct2D uses "left-handed coordinate space" by default, aka x-axis going right and y-axis going down
    d3d11_ui_renderer* renderer = &veil_ui->renderer;
    d2d_renderer* renderer2D = &veil_ui->renderer2D;

    u32 width = veil_ui->placement.w;
    u32 height = veil_ui->placement.h;

    ReleaseD2DBitmapAndRenderTarget(renderer2D);//TODO(fran): not sure whether I need to release or not every single time, since the backbuffer will be changing on each frame
    // resize RenderView to match window size

    if (width != renderer->currentWidth || height != renderer->currentHeight)
    {
        renderer->deviceContext->ClearState();
        ReleaseD3D11UIRenderTargets(renderer);
        renderer->deviceContext->Flush();

        if (width != 0 && height != 0)
        {
            TESTHR(renderer->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)
                , "Failed to resize Swapchain buffers");
            //TODO(fran): DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT flag
            //TODO(fran): clear swapchain textures (r=g=b=a=0), if that isnt already done automatically
        }

        renderer->currentWidth = width;
        renderer->currentHeight = height;
    }

    AcquireAndSetD2DBitmapAsRenderTarget(renderer, renderer2D);
    if (renderer2D->bitmap)
    {
        //TODO(fran): i really prefer the concept of a render queue where on InputUpdate we send all the draw commands for each element, and here we simply process them, instead of coming here to analyze the results from InputUpdate, also the renderer should be completely independent from the rest of the code

        veil_ui->renderer2D.deviceContext->BeginDraw();
#ifdef DEBUG_BUILD
        v4 ClearColor = { .1f,.1f,.6f,1.f };
        veil_ui->renderer2D.deviceContext->Clear((D2D1_COLOR_F*)&ClearColor);
#endif
        ui_element* element = veil_ui->elements;
        
        RenderElement(veil_ui, element);

        if constexpr (const b32 render_mouse = false; render_mouse)
        { //test draw mouse
            auto renderer = veil_ui->renderer2D.deviceContext;
            ID2D1SolidColorBrush* mouse_br{ 0 }; defer{ d3d_SafeRelease(mouse_br); };

            v4 mouse_col = v4{1,1,1,1};
            
            auto brush_props = D2D1::BrushProperties(mouse_col.a);
            renderer->CreateSolidColorBrush((D2D1_COLOR_F*)&mouse_col, &brush_props, &mouse_br);
            f32 diameter = 10;
            auto mouse_rc = rc_center_diameter( mouseP, { diameter, diameter });
            D2D1_ELLIPSE ellipse = D2D1::Ellipse({ mouse_rc.centerX(), mouse_rc.centerY() }, mouse_rc.w * .5f, mouse_rc.h * .5f);
            renderer->FillEllipse(ellipse, mouse_br);

            utf8 mousep[20];
            auto mousepcnt = snprintf((char*)mousep, ArrayCount(mousep), "(%.2f,%.2f)", mouseP.x, mouseP.y);
            RenderText(renderer, veil_ui->renderer2D.font, mousep, mousepcnt, mouse_col, rc2_from(mouseP, {200,100}), horz_text_align::left, vert_text_align::top, true);
        }

        TESTHR(veil_ui->renderer2D.deviceContext->EndDraw()
            , "There were errors while drawing to the Direct2D Surface");
        //TODO(fran): see what errors we care about, here's one https://docs.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-enddraw
    }

}

internal rc2 RECT_to_rc2(RECT r)
{
    rc2 res =
    {
        .x = (f32)r.left,
        .y = (f32)r.top,
        .w = (f32)RECTW(r),
        .h = (f32)RECTH(r),
    };
    return res;
}

internal f32 calc_nonclient_caption_h() //TODO(fran): OS code
{
    f32 res;

    RECT testrc{ 0,0,100,100 }; 
    RECT ncrc = testrc;
    //TODO(fran): dependent on the dpi of the specific window
    AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE);//no menu
    f32 h = distance(testrc.top, ncrc.top);

    res = h;

    return res;
}

struct measure_res {
    sz2 measurement;
    struct {
        sz2 avgchar;
    } metrics;
};
internal measure_res MeasureElementText(veil_ui_state* veil_ui, ui_element* elem)
{
    measure_res res;
    s8 text;
    switch (elem->type)
    {
        case ui_type::button:
        {
            text = GetUIStringStr(veil_ui, elem->data.button.text);
        } break;
        case ui_type::sizer:
        {
            text = const_temp_s(u8"");
        } break;
        default: { crash(); } break;
    }

    //NOTE(fran): when text.cnt is 0 MeasureText still returns the text height for one line of text. I'd think we probably want a special condition that says if (text.cnt == 0) measurement.wh = {0,0} but currently we are actually taking advantage of this text height to resize font height dependent vertical constraint sizers

    res.measurement = MeasureText(veil_ui->renderer2D.fontFactory, veil_ui->renderer2D.font, text.chars, text.cnt, { 0,0,F32MAX,F32MAX }, horz_text_align::left, vert_text_align::top).wh;

    res.metrics.avgchar = MeasureAverageTextCharacter(veil_ui->renderer2D.fontFactory, veil_ui->renderer2D.font);
    return res;
}

//TODO(fran): scaling
//TODO(fran): separate resize & getbottom functions with if constexpr (do_resize), we're probably gonna need to use templates: template <b32 do_resize>. Do it after the whole thing has been completely implemented and debugged, debugging templates is never fun
//TODO(fran): support for placing things inside others. That would be a good use for the 'next' pointer inside ui_element, to function as 'childs' (better change the name), and have there a sizer that receives the rectangle of the parent element as the bounds
//TODO(fran): can I somehow avoid passing the veil_ui? I simply need it to measure some fonts
internal v2 _GetElementBottomRight(veil_ui_state* veil_ui, const rc2 bounds, ui_element* element, b32 do_resize) 
{
    //TODO(fran): while loop over element->next in order to have individual root elements, may be better as a separate specific function, since the rectangles should be handled separately for each root

    //TODO(fran): left, right and center alignment, maybe too top and bottom

    switch (element->type) {
        case ui_type::sizer:
        {
            auto data = element->data.sizer;
            assert(data.childs);
            assert(element->child == nil); //NOTE(fran): sizers cant have childs
            if (data.properties.is_horizontal)
            {
                v2 res = bounds.p;

                v2 xy;
                switch (data.properties.alignment)
                {
                    case sizer_props::alignment::left:
                    {
                        xy = bounds.p;
                    } break;
                    case sizer_props::alignment::center:
                    case sizer_props::alignment::right:
                    {
                        f32 total_w = 0;
                        ui_sizer_element* superchild = data.childs;
                        while (superchild) {
                            ui_element* child = superchild->element;
                            if(child) {
                            //while (child) {
                                f32 w, h = bounds.h;
                                if (superchild->sizing_desc.type == element_sizing_type::bounds)
                                    w = bounds.w * superchild->sizing_desc.bounds.scale_factor;
                                else if (superchild->sizing_desc.type == element_sizing_type::opposite_bounds)
                                    w = bounds.h * superchild->sizing_desc.bounds.scale_factor;
                                else if (superchild->sizing_desc.type == element_sizing_type::font_clamp_to_bounds)
                                {
                                    auto [measurement, metrics] = MeasureElementText(veil_ui, child);
                                    w = measurement.w + metrics.avgchar.w * superchild->sizing_desc.font.w_extra_chars;
                                    w = minimum(w, bounds.w);
                                }
                                else crash();

                                rc2 new_bounds{ 0,0,w,h };
                                total_w += _GetElementBottomRight(veil_ui, new_bounds, child, false).x;

                                //child = child->next;
                            }
                            superchild = superchild->next;
                        }
                        if(data.properties.alignment == sizer_props::alignment::center)
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
                        f32 w, h = bounds.h;
                        if (superchild->sizing_desc.type == element_sizing_type::bounds)
                            w = bounds.w * superchild->sizing_desc.bounds.scale_factor;
                        else if (superchild->sizing_desc.type == element_sizing_type::opposite_bounds)
                            w = bounds.h * superchild->sizing_desc.bounds.scale_factor;
                        else if (superchild->sizing_desc.type == element_sizing_type::font)
                        {
                            auto [measurement, metrics] = MeasureElementText(veil_ui, child);
                            w = measurement.w + metrics.avgchar.w * superchild->sizing_desc.font.w_extra_chars;
                        }
                        else if (superchild->sizing_desc.type == element_sizing_type::font_clamp_to_bounds)
                        {
                            auto [measurement, metrics] = MeasureElementText(veil_ui, child);
                            w = measurement.w + metrics.avgchar.w * superchild->sizing_desc.font.w_extra_chars;
                            w = minimum(w, bounds.w);
                        }
                        else crash(); //TODO(fran): move to separate function

                        rc2 new_bounds{ xy.x,xy.y,w,h };

                        res.y = maximum(res.y, _GetElementBottomRight(veil_ui, new_bounds, child, do_resize).y); //TODO(fran): I think this is wrong, we're possibly returning y values smaller than bounds.bottom(), a better idea would be maximum(res.y,bounds.bottom()) after the end of the loop. We could also provide the current "wrong" behaviour as an additional flag

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
                    case sizer_props::alignment::top:
                    {
                        xy = bounds.p;
                    } break;
                    case sizer_props::alignment::center:
                    {
                        f32 total_h = 0;
                        ui_sizer_element* superchild = element->data.sizer.childs;
                        while (superchild) {
                            ui_element* child = superchild->element;
                            if (child) {
                                //while (child) {

                                f32 w = bounds.w;
                                f32 h;

                                if (superchild->sizing_desc.type == element_sizing_type::bounds)
                                    h = bounds.h * superchild->sizing_desc.bounds.scale_factor;
                                else if (superchild->sizing_desc.type == element_sizing_type::opposite_bounds)
                                    h = bounds.w * superchild->sizing_desc.bounds.scale_factor;
                                else if (superchild->sizing_desc.type == element_sizing_type::os_non_client_top)
                                    h = calc_nonclient_caption_h();
                                else if (superchild->sizing_desc.type == element_sizing_type::font_clamp_to_bounds)
                                {
                                    auto [measurement, metrics] = MeasureElementText(veil_ui, child);
                                    h = measurement.h * superchild->sizing_desc.font.v_scale_factor;
                                    h = minimum(h, bounds.h);
                                }
                                else crash();

#if 0 //TODO(fran): I have the feeling I should use this path
                                rc2 new_bounds{ 0,0,w,h };
                                total_h += _GetElementBottomRight(veil_ui, new_bounds, child, false).y;
#else
                                total_h += h;
#endif
                                //child = child->next;
                            }
                            superchild = superchild->next;
                        }

                        xy = { bounds.x, bounds.y + (bounds.h - total_h) * .5f};

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

                        f32 w = bounds.w;
                        f32 h;

                        if (superchild->sizing_desc.type == element_sizing_type::bounds)
                            h = bounds.h * superchild->sizing_desc.bounds.scale_factor;
                        else if (superchild->sizing_desc.type == element_sizing_type::opposite_bounds)
                            h = bounds.w * superchild->sizing_desc.bounds.scale_factor;
                        else if (superchild->sizing_desc.type == element_sizing_type::os_non_client_top)
                            h = calc_nonclient_caption_h();
                        else if (superchild->sizing_desc.type == element_sizing_type::remaining)
                        {
                            if (resize_remaining)
                            {
                                h = remaining_sizing_h;
                            }
                            else
                            {
                                remaining_sizing_cnt++;
                                goto skip_v;
                            }
                        }
                        else if (superchild->sizing_desc.type == element_sizing_type::font_clamp_to_bounds)
                        {
                            auto [measurement, metrics] = MeasureElementText(veil_ui, child);
                            h = measurement.h * superchild->sizing_desc.font.v_scale_factor;
                            h = minimum(h, bounds.h);
                        }
                        else crash();

                        rc2 new_bounds{ xy.x,xy.y,w,h };

                        if(do_resize) _GetElementBottomRight(veil_ui, new_bounds, child, do_resize);

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
                _GetElementBottomRight(veil_ui, parent->placement, child, do_resize);
            }

            return xy;
        } break;
    }
}
internal v2 ResizeElement(veil_ui_state* veil_ui, rc2 bounds, ui_element* element)
{
    TIMEDFUNCTION();
    return _GetElementBottomRight(veil_ui, bounds, element, true);
}
//internal v2 GetElementBottomRight(veil_ui_state* veil_ui, rc2 bounds, ui_element* element)
//{
//    return _GetElementBottomRight(veil_ui, bounds, element, false);
//}

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
            default: crash(); break;
        }
}

internal void VeilUIProcessing(veil_ui_state* veil_ui, user_input* input) 
{
    veil_ui->render_and_update_screen = false;

    veil_ui->input = input;

    //Temporary Arena initialization
    initialize_arena(&veil_ui->LanguageManager.temp_string_arena, veil_ui->LanguageManager._temp_string_arena, sizeof(veil_ui->LanguageManager._temp_string_arena));

    //TODO(fran): (Windows) enable rendering when the mouse enters any of the 4 resize borders

    input_results input_res = InputUpdate(veil_ui->elements, input);
    PrintNextHot(&input_res);

    InputProcessing(veil_ui, input_res.next_hot, input);

    //TODO(fran): disable IME window

    { //TODO(fran): find best place for this code, maybe it should be smth we get from the caller
        local_persistence rc2 last_placement{0};
        RECT veilrc; GetClientRect(veil_ui->wnd, &veilrc);
        veil_ui->placement =
        {
            .x = (f32)veilrc.left,
            .y = (f32)veilrc.top,
            .w = (f32)RECTW(veilrc),
            .h = (f32)RECTH(veilrc),
            //TODO(fran): remove non client area
        };

        if (veil_ui->placement.w != last_placement.w || veil_ui->placement.h != last_placement.h)
            EnableRendering(veil_ui);

        last_placement = veil_ui->placement;
    }

    assert(veil_ui->elements->type == ui_type::sizer);
    //TODO(fran): multiple root elements

    f32 offsetY = IsWindowMaximized(veil_ui->wnd) ? GetWindowTopMargin(veil_ui->wnd) : 0;
    //TODO(fran): handle EnableRendering inside ResizeElement?
    ResizeElement(veil_ui, { 0, offsetY, veil_ui->placement.w, veil_ui->placement.h - offsetY }, veil_ui->elements); //TODO(fran): find correct place to do resizing, before input proccesing? before render? after everything?

    //TODO(fran): BUG: when resizing the window the mouse position isnt updated, which means that if some element gets resized over where the mouse last was it will change its style to mouseover
    
    if(IsRenderingEnabled(veil_ui))
    {
        RendererDraw(veil_ui, input->mouseP);
    #if UICOMPOSITOR
        OutputUIToWindowsCompositor(veil_ui);
    #else
        OutputToScreen(veil_ui);
    #endif
    }

    //TODO(fran): we may want to have a destroy method to execute before the app closes to do anything necessary, like for example unregistering global hotkeys (though I'd assume the OS does it on its own?)
}