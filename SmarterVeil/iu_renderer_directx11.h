#pragma once

#include <d2d1_2.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

//TODO(fran): move to the not yet existing namespace _iu
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

internal D2D1_POINT_2F v2_to_D2DPOINT(v2 v)
{
    D2D1_POINT_2F res = { .x = v.x, .y = v.y };
    return res;
}

namespace iu{

struct ui_font { //TODO(fran): no handle and simply pass in the font name & size & other props and have fonts cached so they are reused if the same set of properties was already createds
    IDWriteFactory* fontFactory;
    IDWriteTextFormat* font;
};

struct d2d_renderer {
    ID2D1Factory2* factory;
    ID2D1Device1* device;
    ID2D1DeviceContext* deviceContext;

    ID2D1Bitmap1* bitmap;

    IDWriteFactory* fontFactory;
    ui_font font;
    //TODO(fran): look at IDWriteBitmapRenderTarget
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

#define UICOMPOSITOR 0

#if UICOMPOSITOR 
struct windows_ui_compositor {
    IDCompositionDevice* device;
    IDCompositionTarget* target;
    IDCompositionVisual* visual;
};
#endif

struct ui_renderer {
    d3d11_renderer renderer3D;
    d2d_renderer renderer2D; //NOTE(fran): Direct2D uses "left-handed coordinate space" by default, aka x-axis going right and y-axis going down
};

internal void AcquireAndSetD2DBitmapAsRenderTarget(d3d11_renderer* d3d_renderer, d2d_renderer* renderer2D)
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

//TODO(fran): make this scaling thing official in a better way, we could also simply act as if scaling always started as 1.f and then on the ui processing check the new one and recreate all fonts, though that seems wasteful
internal d2d_renderer AcquireD2DRenderer(d3d11_renderer* d3d_renderer, f32 scaling)
{
    d2d_renderer res{ 0 };

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

    //NOTE(fran): Direct Write works with DIPs which are 1/96in, on the other hand, in typography Points are 1/72in
    auto Point = [](f32 DIP/*device independent pixel*/) {return DIP * (96.f / 72.f); };

    auto fontname = _OS::convert_to_s16(OS::GetUIFontName()); defer{ _OS::free_small_mem(fontname.chars); }; //TODO(fran): remove

    res.fontFactory->CreateTextFormat(
        (WCHAR*)fontname.chars,
        nil /*Font collection*/,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        Point(12.0f)* scaling,
        L"en-us", //TODO(fran): consider locale? (in my opinion it's just pointless)
        &res.font.font
    );
    res.font.fontFactory = res.fontFactory;

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


internal d3d11_renderer AcquireD3D11Renderer(HWND wnd)
{
    d3d11_renderer res{ 0 };

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

    //TODO(fran): MakeWindowAssociation

    //TODO(fran): real error handling and releaserenderer if failed, look at casey's refterm

    return res;
}

internal void ReleaseD3D11UIRenderTargets(d3d11_renderer* renderer)
{
    d3d_SafeRelease(renderer->renderTarget);
}

internal void ReleaseD3D11Renderer(d3d11_renderer* renderer)
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
internal windows_ui_compositor AcquireWindowsUICompositor(ui_state* ui)
{
    windows_ui_compositor res{ 0 };

    TESTHR(DCompositionCreateDevice(ui->renderer.dxgiDevice, __uuidof(res.device), reinterpret_cast<void**>(&res.device))
        , "Failed to create Composition Device");

    TESTHR(res.device->CreateTargetForHwnd(ui->wnd, true /*Top most*/, &res.target)
        , "Failed to create Composition Target for the window");

    TESTHR(res.device->CreateVisual(&res.visual)
        , "Failed to create Visual from Composition Device");

    TESTHR(res.visual->SetContent(ui->renderer.swapChain)
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

internal void OutputUIToWindowsCompositor(ui_state* ui)
{
    local_persistence windows_ui_compositor compositor = AcquireWindowsUICompositor(ui);
    // Make the swap chain available to the composition engine
    TESTHR(ui->renderer.swapChain->Present(1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");

    //NOTE(fran): every time we finish rendering to our d3dtexture we must call Commit on the windows compositor in order for it to use the new stuff
    //TESTHR(veil_ui->compositor.device->Commit()
    TESTHR(compositor.device->Commit()
        , "Failed to commit pending Composition commands to the Composition Device");

    //TODO(fran): find out if the swapchain and compositor need some extra command in order to force execution, in case they're just buffering everything for later
}
#else
internal void OutputToScreen(ui_renderer* renderer)
{
    TIMEDFUNCTION();

#if 0
    // Make the swap chain available to the composition engine
    TESTHR(ui->renderer.swapChain->Present(1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");
#elif 0
    //TODO(fran): HACK: we should simply check for elapsed time in the main loop, and if it is less than the refresh rate ms then wait/sleep. Only thing stopping me from doing it is I dont yet quite understand how that would affect correct vsync
    TESTHR(ui->renderer.swapChain->Present(ui->show_veil ? 0 : 1 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");
#else
    TESTHR(renderer->renderer3D.swapChain->Present(0 /*sync*/, 0 /*flags*/)
        , "Failed to present new frame to the Swapchain");
    //IMPORTANT TODO(fran): we're having problems again with too much cpu & gpu utilization
#endif 

    //NOTE(fran): every time we finish rendering to our d3dtexture we must call Commit on the windows compositor in order for it to use the new stuff
   // TESTHR(Veil->compositor.device->Commit()
   //     , "Failed to commit pending Composition commands to the Composition Device");

    //TODO(fran): find out if the swapchain and compositor need some extra command in order to force execution, in case they're just buffering everything for later
}
#endif

internal ui_renderer AcquireRenderer(OS::window_handle wnd, f32 scaling) //TODO(fran): get rid of the scaling
{
    ui_renderer res;

    res.renderer3D = iu::AcquireD3D11Renderer(wnd.hwnd);

    res.renderer2D = AcquireD2DRenderer(&res.renderer3D, scaling);

#if UICOMPOSITOR
    res.compositor = AcquireWindowsUICompositor(&res);
#endif
    return res;
}

internal void ReleaseRenderer(ui_renderer* r)
{
    ReleaseD2DRenderer(&r->renderer2D);
    ReleaseD3D11Renderer(&r->renderer3D);
    #if UICOMPOSITOR
    ReleaseWindowsUICompositor(&veil_ui->compositor);
    #endif
}

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

internal rc2 MeasureText(ui_font f, const utf8* text, u32 char_cnt, rc2 text_rect, horz_text_align h_align, vert_text_align v_align, b32 clip_to_rect = false)
{
    rc2 res;

    auto text16 = _OS::convert_to_s16({ .chars = const_cast<utf8*>(text), .cnt = char_cnt }); defer{ _OS::free_small_mem(text16.chars); }; //TODO(fran): HACK: define what we'll do with string encoding

    auto [horizontal_align, vertical_align, draw_flags] = get_dwrite_flags(h_align, v_align, clip_to_rect);

    f.font->SetTextAlignment(horizontal_align);
    f.font->SetParagraphAlignment(vertical_align);

    //TODO(fran): there's _no_ simple way to specify the draw_flags to CreateTextLayout, meaning we cant use clip_to_rect (nor other flags), the closest I think is font->SetTrimming(), look into that:
    // https://stackoverflow.com/questions/51009082/display-text-in-a-specified-rectangle-with-directwrite

    IDWriteTextLayout* layouter; defer{ d3d_SafeRelease(layouter); };
    TESTHR(f.fontFactory->CreateTextLayout((wchar_t*)text16.chars, text16.cnt, f.font, text_rect.w, text_rect.h, &layouter)
        , "Failed to create DirectWrite TextLayout");
    DWRITE_TEXT_METRICS metrics;
    layouter->GetMetrics(&metrics);

    res.left = text_rect.left + metrics.left;
    res.top = text_rect.top + metrics.top;
    res.w = metrics.width;
    //TODO(fran): look at metrics.widthIncludingTrailingWhitespace (which also apparently has a colossal bug when using DWRITE_TEXT_ALIGNMENT_TRAILING):
    //https://stackoverflow.com/questions/68034476/direct2d-widthincludingtrailingwhitespace-0-while-width-0
    res.h = metrics.height;

    return res;
}

//TODO(fran): use pointer? ui_font*
internal sz2 MeasureAverageTextCharacter(ui_font f)
{
    sz2 res;
    constexpr s8 avgchar = const_temp_s(u8"_"); //TODO(fran): we use an underscore instead of a space because space returns 0 width since we aint using trailing whitespace inside MeasureText
    res = MeasureText(f, avgchar.chars, avgchar.cnt, { 0,0,F32MAX,F32MAX }, horz_text_align::left, vert_text_align::top).wh;
    return res;
}

//TODO(fran): change to DrawText, so it's in line with DrawBackground & DrawImage
//INFO: by default it returns the received text_rect, to get the real measurement use the font_factory parameter
internal rc2 RenderText(ui_renderer* r, ui_font f, const utf8* text, u32 char_cnt, v4 color, rc2 text_rect, horz_text_align h_align, vert_text_align v_align, b32 clip_to_rect = false /*TODO(fran): use enum flag clip_to_rect*/, b32 measure = false /*TODO(fran): we may always want to measure*/)
{
    assert(f.font);
    auto renderer = r->renderer2D.deviceContext;
    auto font = f.font;

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

    rc2 res = measure ? MeasureText(f, text, char_cnt, text_rect, h_align, v_align, clip_to_rect) : text_rect;
    return res;
}

internal void BeginRender(ui_renderer* r, sz2 new_dims) //TODO(fran): better name
{
    //TODO(fran): code cleanup
    auto renderer3D = &r->renderer3D;
    auto renderer2D = &r->renderer2D;

    u32 width = new_dims.w;
    u32 height = new_dims.h;

    ReleaseD2DBitmapAndRenderTarget(renderer2D);//TODO(fran): not sure whether I need to release or not every single time, since the backbuffer will be changing on each frame

    if (width != renderer3D->currentWidth || height != renderer3D->currentHeight)
    {
        renderer3D->deviceContext->ClearState();
        ReleaseD3D11UIRenderTargets(renderer3D);
        renderer3D->deviceContext->Flush();

        if (width != 0 && height != 0)
        {
            TESTHR(renderer3D->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)
                , "Failed to resize Swapchain buffers");
            //TODO(fran): DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT flag
            //TODO(fran): clear swapchain textures (r=g=b=a=0), if that isnt already done automatically
        }

        renderer3D->currentWidth = width;
        renderer3D->currentHeight = height;
    }

    AcquireAndSetD2DBitmapAsRenderTarget(renderer3D, renderer2D);

    assert(renderer2D->bitmap); //TODO(fran): should this be allowed to fail? if so we should return false to indicate do not draw

    renderer2D->deviceContext->BeginDraw();

#ifdef DEBUG_BUILD
    v4 ClearColor = { .1f,.1f,.6f,1.f };
    renderer2D->deviceContext->Clear((D2D1_COLOR_F*)&ClearColor);
#endif
}

internal void EndRender(ui_renderer* r)
{
    TESTHR(r->renderer2D.deviceContext->EndDraw()
        , "There were errors while drawing to the Direct2D Surface");
    //TODO(fran): see what errors we care about, here's one https://docs.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1rendertarget-enddraw
}

internal void Line(ui_renderer* renderer, v2 start, v2 end, f32 line_thickness, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");

    r->DrawLine(v2_to_D2DPOINT(start), v2_to_D2DPOINT(end), br, line_thickness);
}

internal void Rectangle(ui_renderer* renderer, rc2 rc, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");

    r->FillRectangle(rc2_to_D2DRECT(rc), br);
}

internal void RectangleOutline(ui_renderer* renderer, rc2 rc, f32 line_thickness, v4 col) //TODO(fran): the outline should return the rendered rc since the border changes the input rc's size & position
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");

    r->DrawRectangle(rc2_to_D2DRECT(rc), br, line_thickness, nil /*TODO(fran): stroke style*/);
}

internal void Rectangle(ui_renderer* renderer, rc2 rc, f32 border_thickness, v4 col, v4 border_col)
{
    Rectangle(renderer, rc, col);
    RectangleOutline(renderer, rc, border_thickness, border_col);
}

internal void RoundRectangle(ui_renderer* renderer, rc2 rc, sz2 corner_radius, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");
    
    r->FillRoundedRectangle(D2D1::RoundedRect(rc2_to_D2DRECT(rc), corner_radius.w, corner_radius.h), br);
}

internal void RoundRectangleOutline(ui_renderer* renderer, rc2 rc, sz2 corner_radius, f32 line_thickness, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");
    
    r->DrawRoundedRectangle(D2D1::RoundedRect(rc2_to_D2DRECT(rc), corner_radius.w, corner_radius.h), br, line_thickness, nil);
}

internal void RoundRectangle(ui_renderer* renderer, rc2 rc, sz2 corner_radius, f32 border_thickness, v4 col, v4 border_col)
{
    RoundRectangle(renderer, rc, corner_radius, col);
    RoundRectangleOutline(renderer, rc, corner_radius, border_thickness, border_col);
}

internal void Ellipse(ui_renderer* renderer, v2 center, sz2 radius, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");
    
    r->FillEllipse(D2D1::Ellipse(v2_to_D2DPOINT(center), radius.w, radius.h), br);
}

internal void EllipseOutline(ui_renderer* renderer, v2 center, sz2 radius, f32 line_thickness, v4 col)
{
    auto r = renderer->renderer2D.deviceContext;
    ID2D1SolidColorBrush* br{ 0 }; defer{ d3d_SafeRelease(br); };
    auto br_props = D2D1::BrushProperties(col.a);
    TESTHR(r->CreateSolidColorBrush((D2D1_COLOR_F*)&col, &br_props, &br)
        , "Failed to create Direct2D brush");

    r->DrawEllipse(D2D1::Ellipse(v2_to_D2DPOINT(center), radius.w, radius.h), br, line_thickness, nil);
}

internal void Ellipse(ui_renderer* renderer, v2 center, sz2 radius, f32 border_thickness, v4 col, v4 border_col)
{
    Ellipse(renderer, center, radius, col);
    EllipseOutline(renderer, center, radius, border_thickness, border_col);
}

//TODO(fran): bitmap creation, render, rendermask

}