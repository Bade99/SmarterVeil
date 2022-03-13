
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

#define MovWindow(wnd, rc) MoveWindow(wnd, rc.left, rc.top, RECTW(rc), RECTH(rc), false)

internal u32 GetRefreshRateHz(HWND wnd) //TODO(fran): OS code
{
    //TODO(fran): this may be simpler with GetDeviceCaps
    u32 res = 60;
    HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
    if (mon) {
        MONITORINFOEX nfo;
        nfo.cbSize = sizeof(nfo);
        if (GetMonitorInfo(mon, &nfo)) {
            DEVMODE devmode;
            devmode.dmSize = sizeof(devmode);
            devmode.dmDriverExtra = 0;
            if (EnumDisplaySettings(nfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode)) {
                res = devmode.dmDisplayFrequency;
            }
        }
    }
    return res;
}

#include <malloc.h>  // _alloca
#include <stdio.h> // snprintf
#include <d3d11.h>
//#include <d3d11_2.h>
#include <dxgi1_3.h> // version 1.3 is needed for DXGI_CREATE_FACTORY_DEBUG
#include <dcomp.h>
#include <timeapi.h> //TIMECAPS, timeBeginPeriod

#include "veil_vs.h" // veil_vs_bytecode
#include "veil_ps.h" // veil_ps_bytecode
#include "veil_ui.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dcomp")
#pragma comment (lib, "dxguid") // IID_ID3D11Texture2D
#pragma comment (lib, "winmm") // timeapi.h


struct veil_start_data {
    HWND veil_wnd;
    HWND veil_ui_wnd;
    u64 main_thread_id;
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

struct veil_state {
    HWND wnd;

    veil_ui_state ui;
    
    d3d11_renderer renderer;
    d3d11_desktop_duplication desktop_duplication;
    windows_compositor compositor;

    u32 locking_wait_ms;
};

internal void ProcessMessages(veil_state* Veil)
{
    TIMEDFUNCTION();
    MSG msg;
    //IMPORTANT(fran): PeekMessageW _needs_ to be called with an HWND parameter of 0 in order to receive both window messages (eg WM_SIZE, ...) and _thread_ messages (eg WM_QUIT! which otherwise is only sometimes received if using a specific hwnd value for PeekMessageW)
    //INFO(fran): it may in the future be important to know that apparently (saythe docs) PeekMessageW does not remove WM_PAINT messages, the only way those are removed is by doing painting on the wndproc, that could mean that if we stopped answering to WM_PAINT our message queue would become crowded with old WM_PAINT messages

    local_persistence user_input ui_input;
    
    ui_input.hotkey={ 0 };
    ui_input.global_hotkey_id = 0;//TODO(fran): we could also send the full info, id+vk+mods
    ui_input.mouseVScroll = 0;

    for (i32 i = 0; i < ArrayCount(ui_input.keys); i++)
        if (ui_input.keys[i] == input_key_state::clicked || ui_input.keys[i] == input_key_state::doubleclicked)
            ui_input.keys[i] = input_key_state::pressed;
    
    ui_input.tray.on_unclick = false;
    ui_input.tray.on_unrclick = false;

    //TODO(fran): while resizing the window with the mouse we enter WM_NCLBUTTONDOWN and seem to be getting hooked from someone else who 'handles' the sizing effect by stretching our window, this is probably some Directx thing I need to set up correctly, we need to get rid of this, otherwise real resizing doesnt occur until the user releases the mouse
        //TODO(fran): messages are going straight to the wndproc, bypassing this
    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
    {
        //auto _msgName = msgToString(msg.message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");

        //TODO(fran): handle WM_ENDSESSION?

        switch (msg.message)
        {
            case WM_QUIT:
            {
                Veil->ui.quit = true;
            } break;
            //NOTE(fran): no WM_DISPLAYCHANGE msg is sent when the _refresh rate_ is changed
            default:
            {
                //TODO(fran): msg.hwnd is null when clicking inside the client area of the window, we cant know which window is being addressed if we had multiple ones (all this because PostThreadMessageW doesnt let you pass the hwnd, is there a way to pass it?)
                //if (msg.hwnd == Veil->ui.wnd) { //Check for interesting messages to the UI
                    switch (msg.message) 
                    {
                        case WM_DPICHANGED:
                        {
                            Veil->ui.scaling = (f32)LOWORD(msg.wParam) / 96.f /*aka USER_DEFAULT_SCREEN_DPI */;
                            RECT suggested_window = *(RECT*)msg.lParam;
                            MovWindow(Veil->wnd, suggested_window); //TODO(fran): test the rcs we get, I've seen that for many applications this suggested new window is terrible, both in position & size, see if we can come up with a better suggestion
                            Veil->ui.render_and_update_screen = true;
                        } break;
                        case WM_MOUSEMOVE:
                        {
                            POINT p{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
                            POINT screenP = p; 
                            MapWindowPoints(Veil->ui.wnd, HWND_DESKTOP, &screenP, 1);

                            ui_input.mouseP = v2_from(p.x, p.y);

                            ui_input.screen_mouseP = v2_from(screenP.x, screenP.y);
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

                TranslateMessage(&msg); //TODO(fran): are this two doing anything?
                DispatchMessageW(&msg);//send msg to wndproc
            } break;
        }
    }
    VeilUIProcessing(&Veil->ui, &ui_input);
}

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

    TESTHR(res.device->CreateTargetForHwnd(Veil->wnd, true /*Top most*/, &res.target)
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
    veil_state* Veil = (decltype(Veil))alloca(sizeof(*Veil)); //TODO(fran): zero initialize
    Veil->wnd = start_data->veil_wnd;
    AcquireVeilUIState(&Veil->ui, start_data->veil_ui_wnd, start_data->main_thread_id); defer{ ReleaseVeilUIState(&Veil->ui); };
    Veil->renderer = AcquireD3D11Renderer(start_data->veil_wnd); defer{ ReleaseD3D11Renderer(&Veil->renderer); };
    Veil->desktop_duplication = AcquireD3D11DesktopDuplication(&Veil->renderer); defer{ ReleaseD3D11DesktopDuplication(&Veil->desktop_duplication); };
    Veil->compositor = AcquireWindowsCompositor(Veil); defer{ ReleaseWindowsCompositor(&Veil->compositor); };
    Veil->locking_wait_ms = (u32)(1000.f/(f32)GetRefreshRateHz(Veil->wnd)); //TODO(fran): maybe v-sync is a better option?
    //TODO(fran): handle refresh rate changes, probably from D3D since the wndproc doesnt seem to get any special notification about it

    f64 IgnoreUpdateTimer = 0;//ms
    bool IgnoreUpdate = false;

    //INFO(fran): 
    //Conclusions on the gpu spikes when hiding the veil:
    // + TODO(fran): For starters having multiple d3d devices presenting means that we are stalling execution until the next frame before we would want to, when the ui presents it stalls, then we continue to the veil, which presents too and stalls again. This ends up causing the veil to be off by a couple of frames which is very noticeable and terrible, in sharp contrast with when the veil is on its own, in which case it is unnoticeable (TODO(fran): now I cant replicate it, and both cases look similar :c). Should we have a different thread for each d3d presentation device?

    u32 desired_scheduler_ms = 1;
    TIMECAPS timecap;
    if (timeGetDevCaps(&timecap, sizeof(timecap)) == MMSYSERR_NOERROR)
        desired_scheduler_ms = maximum(desired_scheduler_ms, timecap.wPeriodMin);
    //TODO(fran): if we really wanted to we can spinlock if our desired ms is not met
    timeBeginPeriod(desired_scheduler_ms); defer{ timeEndPeriod(desired_scheduler_ms); };
    //TODO(fran): from what I understand, older systems would require calls to timeBeginPeriod and timeEndPeriod each time we use Sleep/other timers, because originally it was a system global thing

    while (!Veil->ui.quit) 
    {
        TIMEDBLOCK(MainLoop);
        TIMERSTART(complete_cycle_elapsed);

        auto old_show_veil = Veil->ui.show_veil;
        
        ProcessMessages(Veil);
        //SUPER TODO(fran): I have no clue why when the veil is hidden "ProcessMessages(Veil)" spikes GPU usage like crazy, to 50% if it's just hidden, and 20% if it's hidden and we check "if (Veil->ui.show_veil)" before doing render & update to compositor
        
        if (old_show_veil != Veil->ui.show_veil)
        {
            ShowWindow(Veil->wnd, Veil->ui.show_veil ? SW_SHOW : SW_HIDE);
            //Make sure the ui remains on top of the veil
            if(Veil->ui.show_veil && IsWindowVisible(Veil->ui.wnd)/*not minimized*/) //TODO(fran): OS code, also im not sure IsWindowVisible is completely foolproof
                SetWindowPos(Veil->ui.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE|  SWP_NOACTIVATE);
        }
        //TODO(fran): when re-showing the veil it's topmost state is updated and sent to the top, which causes the veil ui to be occluded

        //TODO(fran): reassert that the window is still topmost, it often happens that any random new app that opens does so on top of our window. Also see what we can do about fullscreen windows that instantly minimize when another window is clicked, maybe our trying to go topmost will minimize it?

        //TODO(fran): while desktop duplication is active the computer cannot go to sleep, find a way to detect sleep requests and disable duplication

        //TODO(fran): handle rotated desktops

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
                RECT wndRc; GetClientRect(Veil->wnd, &wndRc);

                RendererDraw(Veil, new_desktop_texture, RECTW(wndRc), RECTH(wndRc));
                OutputToWindowsCompositor(Veil);
            }
        }
        auto dt = TIMEREND(complete_cycle_elapsed);
        IgnoreUpdateTimer += dt;

        if (!Veil->ui.show_veil && !Veil->ui.render_and_update_screen && dt < (f32)Veil->locking_wait_ms)
            Sleep(Veil->locking_wait_ms - (u32)dt);

        OutputDebugStringA("-----------\n");
    }
}