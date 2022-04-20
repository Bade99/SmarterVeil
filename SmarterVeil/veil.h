
#define ShowErrorMsg(msg) MessageBoxA(0,msg,"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND);

#ifdef DEBUG_BUILD
//TODO(fran): if possible explain error
#define TESTHR(code,msg) if (HRESULT hr = (code); FAILED(hr)) {OutputDebugStringA("[ERROR] ");OutputDebugStringA(msg);OutputDebugStringA("\n");DebugBreak();}
#else
#define TESTHR(code,msg) if (HRESULT hr = (code); FAILED(hr)) {MessageBoxA(0,msg,"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND); crash();}
#endif

#define d3d_SafeRelease(obj) if(obj){ obj->Release(); obj = nil; }

// Wrapper for the horrible QueryInterface API
//TODO(fran): macro
//IMPORTANT: _must_ use ampersand (&) for second parameter: As(d3dDevice, &dxgiDevice);
template<typename Origin, typename Dest>
internal HRESULT As(Origin * o, Dest * d)
{
    return o->QueryInterface(__uuidof(Dest), reinterpret_cast<void**>(d));
}

#include <malloc.h>  // _alloca
#include <stdio.h> // snprintf
#include <d3d11.h>
//#include <d3d11_2.h>
#include <dxgi1_3.h> // version 1.3 is needed for DXGI_CREATE_FACTORY_DEBUG
#include <dcomp.h>

#include "veil_vs.h" // veil_vs_bytecode
#include "veil_ps.h" // veil_ps_bytecode
#include "iu.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dcomp")
#pragma comment (lib, "dxguid") // IID_ID3D11Texture2D

struct veil_start_data {
    OS::window_handle veil_wnd;
    ui_state* veil_ui_base_state;
};

struct d3d11_renderer {
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

//NOTE(fran): d3d11 constant buffers sizes are required to be multiples of 16bytes
//TODO(fran): I dont really need alignment in 16s, just sizes multiples of 16 (maybe use #pragma pack?)
struct alignas(16) renderer_const_buffer {
    f32 threshold;
    f32 brightness;
    //TODO(fran): all floats can be packed inside a 32bit u32
};
static_assert((sizeof(renderer_const_buffer) % 16) == 0);

struct d3d11_desktop_duplication {
    IDXGIOutputDuplication* duplicator;
    ID3D11Device* linked_device; //NOFREE: this device belongs to the d3d11_renderer, do _not_ release it
    //ID3D11Texture2D* new_desktop_texture;// m_AcquiredDesktopImage;
    u32 output_number;
    DXGI_OUTPUT_DESC output_description;
};

struct windows_compositor {
    IDCompositionDevice* device;
    IDCompositionTarget* target;
    IDCompositionVisual* visual;
};

struct veil_ui_state /* : ui_state */ {
    ui_state* _ui; //TODO(fran): try to integrate this again inside veil_ui_state like we did with inheritance

    b32 quit;

    f32 threshold;
    //f32 brightness;
    struct {
        f32 threshold_min;
        f32 threshold_max;
    };

    b32 show_veil;

    OS::hotkey_data show_ui_hotkey;
};

struct veil_state {
    OS::window_handle wnd;
    
    d3d11_renderer renderer;
    d3d11_desktop_duplication desktop_duplication;
    windows_compositor compositor;

    u32 locking_wait_ms;

    veil_ui_state ui;
};

internal d3d11_renderer AcquireD3D11Renderer(HWND wnd)
{
    d3d11_renderer res{ 0 };

    TESTHR(D3D11CreateDevice(nullptr,       // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                            // Module
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,   //Support for d2d
        nullptr, 0,                         // Highest available feature level
        D3D11_SDK_VERSION,
        &res.device,
        nullptr,                            // Actual feature level
        &res.deviceContext)                 // Device context
        , "Failed to create Direct3D device");

    TESTHR(As(res.device, &res.dxgiDevice), "Failed to QueryInterface for DXGI Device");
    
    u32 dxgiFactoryflags = 0;
#ifdef DEBUG_BUILD
    dxgiFactoryflags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    TESTHR(CreateDXGIFactory2(
        dxgiFactoryflags
        , __uuidof(res.dxgiFactory), reinterpret_cast<void**>(&res.dxgiFactory))
        , "Failed to create DXGIFactory2");

    RECT wndRc; GetClientRect(wnd, &wndRc);
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = { 0 };
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapchainDesc.Width = RECTW(wndRc);
    swapchainDesc.Height = RECTH(wndRc);

    TESTHR(res.dxgiFactory->CreateSwapChainForComposition(res.dxgiDevice, &swapchainDesc
        , nil /* Don’t restrict */, &res.swapChain)
        , "Failed to create Swapchain for composition");
    //TODO(fran): clear swapchain textures (r=g=b=a=0), if that isnt already done automatically

    //TODO(fran): look at IDXGISwapChain2_GetFrameLatencyWaitableObject

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

    //TODO(fran): real error handling and releaserenderer if failed, look at casey's refterm

    return res;
}

internal void ReleaseD3D11RenderTargets(d3d11_renderer* renderer)
{
    d3d_SafeRelease(renderer->renderTarget);
}

internal void ReleaseD3D11Renderer(d3d11_renderer* renderer) 
{
    ReleaseD3D11RenderTargets(renderer);

    d3d_SafeRelease(renderer->dxgiFactory);
    d3d_SafeRelease(renderer->swapChain);
    d3d_SafeRelease(renderer->dxgiDevice);
    d3d_SafeRelease(renderer->deviceContext);
    d3d_SafeRelease(renderer->device);

    d3d11_renderer ZeroRenderer = { 0 };
    *renderer = ZeroRenderer;
}

internal d3d11_desktop_duplication AcquireD3D11DesktopDuplication(const d3d11_renderer* linked_renderer) 
{
    d3d11_desktop_duplication res {0};

    res.output_number = 0; //TODO(fran): apparently you need to capture each monitor separately, if there's more than one

    res.linked_device = linked_renderer->device;
    //res.linked_device->AddRef(); // Take a reference on the device //TODO(fran): probably remove this

    // Get DXGI adapter
    IDXGIAdapter* DxgiAdapter {0};
    TESTHR(linked_renderer->dxgiDevice->GetParent(__uuidof(DxgiAdapter), reinterpret_cast<void**>(&DxgiAdapter))
        , "Failed to get parent DXGI Adapter");
    defer{ d3d_SafeRelease(DxgiAdapter); };
    
    // Get output
    IDXGIOutput* DxgiOutput {0};
    TESTHR(DxgiAdapter->EnumOutputs(res.output_number, &DxgiOutput)
        , "Failed to get specified output");
    defer{ d3d_SafeRelease(DxgiOutput); };
    //IMPORTANT(fran): if the adapter came from a device created using D3D_DRIVER_TYPE_WARP, then the adapter has no outputs, so an error is returned
    
    DxgiOutput->GetDesc(&res.output_description);

    // QI for Output 1
    IDXGIOutput1* DxgiOutput1 {0};
    TESTHR(As(DxgiOutput,&DxgiOutput1)
        , "Failed to QueryInterface for DxgiOutput1");
    defer{ d3d_SafeRelease(DxgiOutput1); };


    // Create desktop duplication
    TESTHR(DxgiOutput1->DuplicateOutput(res.linked_device, &res.duplicator)
        , "Failed to duplicate desktop output");
    //NOTE(fran): hresult could be equal to DXGI_ERROR_NOT_CURRENTLY_AVAILABLE, which means that the max number of apps using Desktop Duplication API was reached, so the user should try closing one and then try again, this probably has not happened to anyone ever in the history of Windows

    return res;
}

internal void ReleaseD3D11DesktopDuplication(d3d11_desktop_duplication* desktop_duplication)
{
    //desktop_duplication->duplicator->ReleaseFrame();
    d3d_SafeRelease(desktop_duplication->duplicator);

    d3d11_desktop_duplication ZeroDuplicator = { 0 };
    *desktop_duplication = ZeroDuplicator;
}

//NOTE: 'only mouse' updates do not count as new images
//FREE: the returned ID3D11Texture2D _must_ be released by the caller
internal ID3D11Texture2D* GetNewDesktopImage(d3d11_desktop_duplication* desktop_duplication, u32 locking_wait_ms)
{
    TIMEDFUNCTION();

    ID3D11Texture2D* res{0}; //New desktop texture

    IDXGIResource* DesktopResource {0};
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

    //TODO(fran): check if calling this from the start, even when we dont have a frame, can cause problems 
    desktop_duplication->duplicator->ReleaseFrame();//release previous frame, after this the previously acquired ID3D11Texture2D will be invalid and unusable by the gpu

    HRESULT hr = desktop_duplication->duplicator->AcquireNextFrame(
        locking_wait_ms, //TODO(fran): allows to specify a wait time, if a new img is not present it'll wait x milliseconds and check again. May be useful?
        &FrameInfo,
        &DesktopResource);
    defer{ d3d_SafeRelease(DesktopResource); };
    if (FAILED(hr))
    {
        if (hr != DXGI_ERROR_WAIT_TIMEOUT)
            ShowErrorMsg("Failed to get new desktop image");
        return res;
    }

    // QI for IDXGIResource
    TESTHR(As(DesktopResource, &res)
        , "Failed to QueryInterface for ID3D11Texture2D from acquired IDXGIResource");

    //TODO(fran): Im pretty sure this will still count mouse pointer _shape_ changes as image changes, but since the mouse shape doesnt change too often it isnt really a performance/effiency concern, if it were check here for a solution
    //https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/ns-dxgi1_2-dxgi_outdupl_frame_info
    bool only_mouse_update = FrameInfo.AccumulatedFrames==0 && FrameInfo.TotalMetadataBufferSize==0 && FrameInfo.LastPresentTime.QuadPart==0;

    if (only_mouse_update) d3d_SafeRelease(res);

    return res;
}

internal windows_compositor AcquireWindowsCompositor(const veil_state* Veil)
{
    windows_compositor res {0};

    TESTHR(DCompositionCreateDevice(Veil->renderer.dxgiDevice, __uuidof(res.device), reinterpret_cast<void**>(&res.device))
        , "Failed to create Composition Device");

    TESTHR(res.device->CreateTargetForHwnd(Veil->wnd.hwnd, true /*Top most*/, &res.target)
        , "Failed to create Composition Target for the window");

    TESTHR(res.device->CreateVisual(&res.visual)
        , "Failed to create Visual from Composition Device");

    TESTHR(res.visual->SetContent(Veil->renderer.swapChain)
        , "Failed to set Composition Visual's content to the Swapchain");

    TESTHR(res.target->SetRoot(res.visual)
        , "Failed to set new Composition Visual as new root object of the Composition Target");

    return res;
}

internal void ReleaseWindowsCompositor(windows_compositor* compositor) 
{
    d3d_SafeRelease(compositor->visual);
    d3d_SafeRelease(compositor->target);
    d3d_SafeRelease(compositor->device);
    //TODO(fran): do I need to remove something prior to releasing?
}

internal void CreateVeilUIElements(veil_ui_state* veil_ui)
{
    TIMEDFUNCTION();
    button_theme base_button_theme =
    {
        .color =
        {
            .foreground =
            {
                .normal = {1.f,1.0f,1.0f,1.0f},
                .disabled = {0.2f,0.2f,0.2f,1.0f},
                .mouseover = {0.9f,0.9f,0.9f,1.0f},
                .pressed = {0.8f,0.8f,0.8f,1.0f},
            },
            .background =
            {
                .normal = {0.0f,0.6f,0.8f,1.0f},
                .disabled = {.0f,.2f,.0f,1.0f},
                .mouseover = V4(base_button_theme.color.background.normal.xyz * .95f,1.0f),
                .pressed = V4(base_button_theme.color.background.mouseover.xyz * .95f,1.0f),
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
                .normal = {0.8f,0.8f,0.8f,1.f},
                //.mouseover = {0.85f,0.85f,0.85f,1.f},
                .mouseover = base_slider_theme.color.track_fill.normal,
                .pressed = base_slider_theme.color.track_fill.mouseover,
            },
            .track_empty =
            {
                .normal = {0.3f,0.3f,0.3f,1.f},
                //.mouseover = {0.35f,0.35f,0.35f,1.f},
                .mouseover = base_slider_theme.color.track_empty.normal,
                .pressed = base_slider_theme.color.track_empty.mouseover,
            },
            .thumb =
            {
                .normal = {0.6f,0.6f,0.6f,1.f},
                .mouseover = {0.65f,0.65f,0.65f,1.f},
                .pressed = base_slider_theme.color.thumb.mouseover,
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
        .bounds = {.scale_factor = (1.f / 3.f)},
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
    element_sizing_desc threequarters_bounds_sz =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .9f},
    };

    memory_arena* arena = &veil_ui->_ui->permanent_arena;

    element_sizing_desc TEST_button_w_sizing =
    {
        .type = element_sizing_type::bounds,
        .bounds = {.scale_factor = .35f},
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
        .font = {.v_scale_factor = 11},
    };

    element_sizing_desc horizontal_constraint_sz =
    {
        .type = element_sizing_type::font_clamp_to_bounds,
        //.bounds = {.scale_factor = 1.f},
        .font = {.w_extra_chars = 75},
    };

    ui_string on_off_text =
    {
        .type = ui_string_type::dynamic_id,
        .str_dyn_id = {
            .context = &veil_ui->show_veil,
            .get_str_id = UI_STRING_DYN_ID_LAMBDA{
                b32 * show_veil = (decltype(show_veil))context;
                return 50u + *show_veil;
            },
        },
    };

    ui_cursor Hand = { .type = ui_cursor_type::os, .os_cursor = OS::cursor_style::hand };
    ui_cursor Text = { .type = ui_cursor_type::os, .os_cursor = OS::cursor_style::text };

    ui_element* layout = VSizer(arena, sizer_alignment::top,
        { .sizing = full_bounds_sizing, .element = HSizer(arena, sizer_alignment::left,
            {.sizing = full_bounds_sizing, .element = Background(.arena = arena, .theme = &bk_theme,
                .child = VSizer(arena, sizer_alignment::center,
                    {.sizing = vertical_constraint_sz, .element = HSizer(arena, sizer_alignment::center,
                        {.sizing = horizontal_constraint_sz, .element = VSizer(arena, sizer_alignment::top,
                            {.sizing = vertical_trio_sizing, .element = /*top third*/ HSizer(arena, sizer_alignment::center,
                                {.sizing = TEST_full_slider, .element = Slider(.arena = arena, .theme = &base_slider_theme, .value = {&veil_ui->threshold,veil_ui->threshold_min,veil_ui->threshold_max})}
                            )},
                            {.sizing = vertical_trio_sizing, .element = /*mid_third*/ HSizer(arena, sizer_alignment::right,
                                {.sizing = TEST_button_w_sizing, .element = VSizer(arena, sizer_alignment::top, {.sizing = threequarters_bounds_sz, .element = Button(.arena = arena, .theme = &base_button_theme, .text = on_off_text, .cursor = Hand, .on_unclick = {.context = &veil_ui->show_veil, .action = common_ui_actions::B32_Flip})})},
                                {.sizing = TEST_filler_pad, .element = HPad(arena)}
                            )},
                            {.sizing = vertical_trio_sizing, .element = /*bot_third*/ HSizer(arena, sizer_alignment::center,
                                {.sizing = hotkey_sizing, .element = VSizer(arena, sizer_alignment::center, {.sizing = threequarters_bounds_sz, .element = Hotkey(.arena = arena, .theme = &base_hotkey_theme, .hotkey_value = &veil_ui->show_ui_hotkey,.placeholder_text = {.type = ui_string_type::id, .str_id = 52u }, .cursor = Text, .on_hotkey = {.context = veil_ui->_ui, .action = common_ui_actions::MinimizeOrRestore})})}
                            )}
                        )}
                    )}
                )
            )}
        ) }
    );

    CreateOSUIElements(veil_ui->_ui, &veil_ui->quit, layout);
}

void CreateVeilUITray(veil_ui_state* veil_ui)
{
    ui_action on_unclick = { .context = &veil_ui->show_veil, .action = common_ui_actions::B32_Flip };
    ui_action on_unrclick = { .context = veil_ui->_ui, .action = common_ui_actions::MinimizeOrRestore };
    AcquireTrayIcon(veil_ui->_ui, on_unclick, on_unrclick);
}

internal void AcquireVeilUIState(veil_ui_state* res, ui_state* veil_ui_base_state)
{
    res->_ui = veil_ui_base_state;

    //TODO(fran): retrieve settings from save file, metaprogramming
    res->threshold = .5f;//TODO(fran): what if the user only changed the threshold?

    res->threshold_min = 0.1f;
    res->threshold_max = 1.0f;

    //res->brightness = .6f;

    res->quit = false;

    res->show_veil = true;

    res->show_ui_hotkey = { 0 };

    rc2 desktop_rc = OS::GetMouseMonitorRc();
    
    f32 units = 28.f;
    //f32 _units_to_pixels = GetSystemMetrics(SM_CXMENUCHECK);
    f32 units_to_pixels = OS::GetSystemFontMetrics().h * OS::GetScalingForRc(desktop_rc) / OS::GetScalingForSystem();
    i32 wnd_w = units_to_pixels * units;
    i32 wnd_h = wnd_w * 9.5f / 16.f;

    rc2 wnd_rc = get_centered_rc(desktop_rc, wnd_w, wnd_h);

#ifdef DEBUG_BUILD
    wnd_rc.x -= (distance(wnd_rc.x, desktop_rc.x) - 100);
#endif

    OS::MoveWindow(res->_ui->wnd, wnd_rc);
    OS::ShowWindow(res->_ui->wnd);

    CreateVeilUIElements(res);
    
    CreateVeilUITray(res);
}

internal void ReleaseVeilUIState(veil_ui_state* veil_ui) {
    ReleaseTrayIcon(&veil_ui->_ui->tray);
}

internal void OutputToWindowsCompositor(veil_state* Veil) 
{
    // Make the swap chain available to the composition engine
    TESTHR(Veil->renderer.swapChain->Present(1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");

    //NOTE(fran): every time we finish rendering to our d3dtexture we must call Commit on the windows compositor in order for it to use the new stuff
    TESTHR(Veil->compositor.device->Commit()
        , "Failed to commit pending Composition commands to the Composition Device");

    //TODO(fran): find out if the swapchain and compositor need some extra command in order to force execution, in case they're just buffering everything for later
}

internal void RendererDraw(veil_state* Veil, ID3D11Texture2D* new_desktop_texture, u32 width, u32 height) 
{
    d3d11_renderer* renderer = &Veil->renderer;

    // resize RenderView to match window size
    if (width != renderer->currentWidth || height != renderer->currentHeight)
    {
        renderer->deviceContext->ClearState();
        ReleaseD3D11RenderTargets(renderer);
        renderer->deviceContext->Flush();

        if (width != 0 && height != 0)
        {
            TESTHR(renderer->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)
                , "Failed to resize Swapchain buffers");
            //TODO(fran): DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT flag
            //TODO(fran): clear swapchain textures (r=g=b=a=0), if that isnt already done automatically

            ID3D11Texture2D* buffer;
            TESTHR(renderer->swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buffer)
                , "Failed to retrieve Swapchain buffer");
            defer{ d3d_SafeRelease(buffer); };
            
            TESTHR(renderer->device->CreateRenderTargetView((ID3D11Resource*)buffer, 0, &renderer->renderTarget)
                , "Failed to create Render target view");

            D3D11_VIEWPORT viewport =
            {
                .TopLeftX = (f32)0,
                .TopLeftY = (f32)0,
                .Width =    (f32)width,
                .Height =   (f32)height,
            };
            renderer->deviceContext->RSSetViewports(1, &viewport);
            renderer->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        }

        renderer->currentWidth = width;
        renderer->currentHeight = height;
    }

    if (renderer->renderTarget) 
    {
        D3D11_MAPPED_SUBRESOURCE MappedConstantBuffer;
        TESTHR(renderer->deviceContext->Map((ID3D11Resource*)renderer->constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedConstantBuffer)
            , "Failed to map the Constant Buffer");
        {
            f32 brightness_min = .05f, brightness_max = 1.f;
            f32 t = 1.f-percentage_between(Veil->ui.threshold_min, Veil->ui.threshold, Veil->ui.threshold_max);
            auto EaseIn = [](f32 t /*[0.0,1.0]*/) { return t * t; };
            auto EaseOut = [](f32 t /*[0.0,1.0]*/) { return 1.f - squared(squared(t-1.f)); };

            auto EaseOutExp = [](f32 t  /*[0.0,1.0]*/) { return t == 1.f ? 1.f : 1.f - pow(2, -10 * t); };

            auto EaseOutSuperExp = [](f32 t  /*[0.0,1.0]*/) {
                return clamp(0.0f, 1.014508f - (1.01935502f / (1 + pow(t / 0.04550539f, 1.37152f))), 1.f);
            };

            //TODO(fran): I still dont like any of these, ultimately the final answer seems to be that just applying a fixed opacity to the whole screen is better as it maintains everything looking consistent.
                //We could still try to provide both sliders (threshold & brightness) and see how that looks, it does allow for more control

            f32 brightness = lerp(brightness_min, EaseOutSuperExp(t), brightness_max);
            //TODO(fran): brightness is quite unintuitive, the bigger the value the darker the screen gets
            //NOTE(fran): since brightness makes the screen darker as it goes up we want to use a non linear EaseOut blend to clamp more values together at the top end since human eyes are more sensitive to darkness than brightness
            renderer_const_buffer constData =
            {
                .threshold = Veil->ui.threshold,
                .brightness = brightness,
            };
            memcpy(MappedConstantBuffer.pData, &constData, sizeof(constData));
        }
        renderer->deviceContext->Unmap((ID3D11Resource*)renderer->constantBuffer, 0);
        
        ID3D11ShaderResourceView* new_desktop_texture_view {0};
        TESTHR(renderer->device->CreateShaderResourceView((ID3D11Resource*)new_desktop_texture, nil, &new_desktop_texture_view)
            , "Failed to create Shader Resource View for New desktop texture");
        defer{ d3d_SafeRelease(new_desktop_texture_view); };

        ID3D11Texture2D* previous_veil_texture;//NOTE(fran): this texture is read only
        TESTHR(renderer->swapChain->GetBuffer(1, IID_ID3D11Texture2D, (void**)&previous_veil_texture)
            , "Failed to retrieve secondary Swapchain buffer");
        defer{ d3d_SafeRelease(previous_veil_texture); };

        ID3D11ShaderResourceView* previous_veil_texture_view {0};
        TESTHR(renderer->device->CreateShaderResourceView((ID3D11Resource*)previous_veil_texture, nil, &previous_veil_texture_view)
            , "Failed to create Shader Resource View for Previous veil texture");
        defer{ d3d_SafeRelease(previous_veil_texture_view); };

        ID3D11ShaderResourceView* Resources[] = { new_desktop_texture_view, previous_veil_texture_view };

        renderer->deviceContext->OMSetRenderTargets(1, &renderer->renderTarget, 0);
        renderer->deviceContext->PSSetConstantBuffers(0, 1, &renderer->constantBuffer);
        renderer->deviceContext->PSSetShaderResources(0, ARRAYSIZE(Resources), Resources);
        renderer->deviceContext->VSSetShader(renderer->vertexShader, 0, 0);
        renderer->deviceContext->PSSetShader(renderer->pixelShader, 0, 0);
        renderer->deviceContext->Draw(4, 0);
    }
}

internal void VeilProcessing(veil_start_data* start_data)
{
    veil_state* Veil = (decltype(Veil))alloca(sizeof(*Veil)); zero_struct(*Veil);
    Veil->wnd = start_data->veil_wnd;
    Veil->renderer = AcquireD3D11Renderer(start_data->veil_wnd.hwnd /*TODO(fran): OS agnostic*/); defer{ ReleaseD3D11Renderer(&Veil->renderer); };
    Veil->desktop_duplication = AcquireD3D11DesktopDuplication(&Veil->renderer); defer{ ReleaseD3D11DesktopDuplication(&Veil->desktop_duplication); };
    Veil->compositor = AcquireWindowsCompositor(Veil); defer{ ReleaseWindowsCompositor(&Veil->compositor); };
    Veil->locking_wait_ms = (u32)(1000.f / (f32)OS::GetRefreshRateHz(Veil->wnd)); //TODO(fran): maybe v-sync is a better option?

    AcquireVeilUIState(&Veil->ui, start_data->veil_ui_base_state); defer{ ReleaseVeilUIState(&Veil->ui); };
    //TODO(fran): handle refresh rate changes, probably from D3D since the wndproc doesnt seem to get any special notification about it

    f64 IgnoreUpdateTimer = 0;//ms
    bool IgnoreUpdate = false;

    //INFO(fran): 
    //Conclusions on the gpu spikes when hiding the veil:
    // + TODO(fran): For starters having multiple d3d devices presenting means that we are stalling execution until the next frame before we would want to, when the ui presents it stalls, then we continue to the veil, which presents too and stalls again. This ends up causing the veil to be off by a couple of frames which is very noticeable and terrible, in sharp contrast with when the veil is on its own, in which case it is unnoticeable. Should we have a different thread for each d3d presentation device?

    while (!Veil->ui.quit)
    {
        TIMEDBLOCK(MainLoop);
        TIMERSTART(complete_cycle_elapsed);

        auto old_show_veil = Veil->ui.show_veil;

        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) //NOTE(fran): handling for the veil, which is just a standard hwnd not handled by Iu
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        iu::update_and_render();

        //ProcessMessages(Veil);
        //SUPER TODO(fran): I have no clue why when the veil is hidden "ProcessMessages(Veil)" spikes GPU usage like crazy, to 50% if it's just hidden, and 20% if it's hidden and we check "if (Veil->ui.show_veil)" before doing render & update to compositor

        if (old_show_veil != Veil->ui.show_veil)
        {
            if (Veil->ui.show_veil) OS::ShowWindow(Veil->wnd);
            else OS::HideWindow(Veil->wnd);
            //Make sure the ui remains on top of the veil
            if (Veil->ui.show_veil && OS::IsWindowVisible(Veil->ui._ui->wnd)/*not minimized*/)
                OS::SendWindowToTop(Veil->ui._ui->wnd);
        }
        //TODO(fran): when re-showing the veil it's topmost state is updated and sent to the top, which causes the veil ui to be occluded

        //TODO(fran): reassert that the window is still topmost, it often happens that any random new app that opens does so on top of our window. Also see what we can do about fullscreen windows that instantly minimize when another window is clicked, maybe our trying to go topmost will minimize it?

        //TODO(fran): while desktop duplication is active the computer cannot go to sleep, find a way to detect sleep requests and disable duplication

        //TODO(fran): handle rotated desktops

        //TODO(fran): once I turn off my screen the veil will keep failing to get the next desktop img when the screen turns back on

        if (Veil->ui.show_veil) //TODO(fran): true veil processing stop, possibly requiring to release all desktop duplication objects, I dont want to simply stop calling GetNewDesktopImage because Windows will still store more update regions, idk how much extra memory that will entail (TODO(fran): check that), also (embarrassingly) we are using it as our frame timer
        {
            TIMEDBLOCK(VeilOutputToCompositor);

            ID3D11Texture2D* new_desktop_texture = GetNewDesktopImage(&Veil->desktop_duplication, Veil->locking_wait_ms);
            defer{ d3d_SafeRelease(new_desktop_texture); };

            IgnoreUpdate = IgnoreUpdateTimer > 5000.f;
            if (IgnoreUpdate) IgnoreUpdateTimer = 0;
            //TODO(fran): ROBUSTNESS: check that the time between updates was short (eg <100ms) before deciding to ignore, do not ignore a new frame if the previous one came much earlier cause it'd be much more probable that we werent the ones who caused it

            if (new_desktop_texture && !IgnoreUpdate)
            {
                //RECT wndRc; GetClientRect(Veil->wnd, &wndRc);
                rc2 wnd_rc = OS::GetWindowRenderRc(Veil->wnd);

                RendererDraw(Veil, new_desktop_texture, wnd_rc.w, wnd_rc.h);
                OutputToWindowsCompositor(Veil);
            }
        }
        auto dt = TIMEREND(complete_cycle_elapsed);
        IgnoreUpdateTimer += dt;

        if (!Veil->ui.show_veil && !Veil->ui._ui->render_and_update_screen /*TODO(fran): HACK: find out how to sync everything correctly*/ && dt < (f32)Veil->locking_wait_ms)
            Sleep(Veil->locking_wait_ms - (u32)dt);
        //TODO(fran): if we really wanted to, after the sleep we can spinlock if our desired ms is not met

    }
}