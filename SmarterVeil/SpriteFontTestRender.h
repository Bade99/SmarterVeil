#pragma once

#include "Directxtk11/SpriteFont.h"
#include "Directxtk11/SpriteBatch.h"

internal void SpriteFontTestRender(veil_ui_state* veil_ui) {
    //NOTE(fran): 
    //  Many issues:
    //      - works with d3d _not_ d2d, therefore I would need to huggle back and forth changing d2d and d3d to render
    //      - rendered text looks really bad
    //      - Does not allow you to load a font straight up, you must go through the MakeSpriteFont C# app to convert .ttf to .spritefont
    // Conclusion: terrible and disappointing

    local_persistence auto font = DirectX::SpriteFont(veil_ui->renderer.device,
        L"C:\\Users\\Brenda-Vero-Frank\\source\\repos\\SmarterVeil\\SmarterVeil\\Directxtk11\\arial.spritefont");
    local_persistence auto batch = DirectX::SpriteBatch(veil_ui->renderer.deviceContext);

    ID3D11Texture2D* buffer;
    TESTHR(renderer->swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buffer)
        , "Failed to retrieve Swapchain buffer");
    defer{ d3d_SafeRelease(buffer); };
    local_persistence b32 first_time = true;
    if (first_time) {
        assert(!renderer->renderTarget);
        first_time = false;
        TESTHR(renderer->device->CreateRenderTargetView((ID3D11Resource*)buffer, 0, &renderer->renderTarget)
            , "Failed to create Render target view");

        D3D11_VIEWPORT Viewport =
        {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = veil_ui->placement.w,
            .Height = veil_ui->placement.h,
        };
        renderer->deviceContext->RSSetViewports(1, &Viewport);
    }
    if (renderer->renderTarget)
        renderer->deviceContext->OMSetRenderTargets(1, &renderer->renderTarget, 0);
    else crash();

    batch.Begin(DirectX::SpriteSortMode_Immediate);
    font.DrawString(&batch, "Fuck me. Cmonn show me some texttt!!!\nFuck\nFuck\nFuck\nFuck\nFuck", DirectX::XMFLOAT2(100, 100), DirectX::Colors::White, 0, DirectX::XMFLOAT2(0, 0), 1, DirectX::SpriteEffects_None, 1);
    batch.End();
}