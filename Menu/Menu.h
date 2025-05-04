#ifndef MENU_H
#define MENU_H

#include <d3d11.h>
#include <wincodec.h>
#include <MinHook.h>
#include <vector>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Images/Image.h"

#pragma comment(lib, "Windowscodecs.lib")

extern DWORD WINAPI EjectThread(LPVOID lpParameter);
extern HMODULE g_hModule;

namespace Menu
{
    inline ID3D11ShaderResourceView* ImageResources = nullptr;
    inline ImFont* g_FontLarge = nullptr;

    // thanks gpt
    inline bool LoadImageByMemory(
        ID3D11Device* device,
        unsigned char* image,
        size_t image_size,
        ID3D11ShaderResourceView** output)
    {
        if (!device || !image || image_size == 0 || !output)
            return false;

        HRESULT hr = S_OK;

        static IWICImagingFactory* wicFactory = nullptr;
        if (!wicFactory)
        {
            hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wicFactory));
            if (FAILED(hr))
                return false;
        }

        IWICStream* wicStream = nullptr;
        hr = wicFactory->CreateStream(&wicStream);
        if (FAILED(hr)) return false;
        wicStream->InitializeFromMemory(image, static_cast<DWORD>(image_size));

        // Decode the image
        IWICBitmapDecoder* decoder = nullptr;
        hr = wicFactory->CreateDecoderFromStream(
            wicStream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
        if (FAILED(hr)) { wicStream->Release(); return false; }

        // Grab the first frame
        IWICBitmapFrameDecode* frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) { decoder->Release(); wicStream->Release(); return false; }

        // Convert to 32bpp RGBA
        IWICFormatConverter* converter = nullptr;
        hr = wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr)) { frame->Release(); decoder->Release(); wicStream->Release(); return false; }

        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicStream->Release(); return false; }

        // Get dimensions
        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);

        // Copy pixels into a CPU buffer
        std::vector<BYTE> buffer(width * height * 4);
        hr = converter->CopyPixels(
            nullptr,
            width * 4,
            static_cast<UINT>(buffer.size()),
            buffer.data());
        if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicStream->Release(); return false; }

        // Create D3D11 texture
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = buffer.data();
        initData.SysMemPitch = width * 4;

        ID3D11Texture2D* tex = nullptr;
        hr = device->CreateTexture2D(&desc, &initData, &tex);
        if (FAILED(hr)) {
            converter->Release();
            frame->Release();
            decoder->Release();
            wicStream->Release();
            return false;
        }

        // Create a shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;

        hr = device->CreateShaderResourceView(tex, &srvDesc, output);
        tex->Release();

        // Cleanup WIC
        converter->Release();
        frame->Release();
        decoder->Release();
        wicStream->Release();

        return SUCCEEDED(hr);
    }

    inline void renderMenu()
    {
        static bool aimbotEnabled = false;
        static bool ragebotEnabled = false;
        static bool espEnabled = false;
        static bool bhopEnabled = false;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 centerPos = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f);
        ImGui::SetNextWindowPos(centerPos, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(650.0f, 500.0f), ImGuiCond_Always);

        ImGui::Begin("Aether", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        if (ImageResources)
        {
            ImGui::Image((void*)ImageResources, ImVec2(32, 32));
            ImGui::SameLine();
        }

        if (Menu::g_FontLarge)
            ImGui::PushFont(Menu::g_FontLarge);
        ImGui::Text("Aether");
        if (Menu::g_FontLarge)
            ImGui::PopFont();

        ImGui::SameLine(0.0f, 230.0f);
        float verticalOffset = 6.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);

        if (ImGui::BeginTabBar("MainTabs"))
        {
            if (ImGui::BeginTabItem("Legit"))
            {
                ImGui::Dummy(ImVec2(0, 15.0f));
                ImGui::Checkbox("Aimbot (not work)", &aimbotEnabled);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Rage"))
            {
                if (ImGui::BeginTable("General", 2, ImGuiTableFlags_None))
                {
                    ImVec2 child_size = ImVec2(0, 0);
                    ImGui::TableNextColumn();
                    float desired_padding_x = 15.0f; // padding value here lol
                    ImVec2 original_padding = ImGui::GetStyle().WindowPadding;
                    ImVec2 custom_padding = ImVec2(desired_padding_x, original_padding.y);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, custom_padding);

                    if (ImGui::BeginChild("LeftColumnChild", child_size, ImGuiChildFlags_AlwaysUseWindowPadding))
                    {
                        ImGui::Text("General");
                        ImGui::Dummy(ImVec2(0, 10.0f));
                        ImGui::Checkbox("Ragebot (not work)", &ragebotEnabled);
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, custom_padding);

                    if (ImGui::BeginChild("RightColumnChild", child_size, ImGuiChildFlags_AlwaysUseWindowPadding))
                    {
                        ImGui::Text("Exploits");
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Visuals"))
            {
                ImGui::Dummy(ImVec2(0, 15.0f));
                ImGui::Checkbox("ESP (not work)", &espEnabled);
                //ImGui::ColorPicker4("Window Background Color", (float*)&ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Misc"))
            {
                ImGui::Dummy(ImVec2(0, 15.0f));
                ImGui::Checkbox("Auto Bhop (not work)", &bhopEnabled);
                ImGui::Text("DLL Management:");
                ImGui::Separator();

                if (ImGui::Button("Eject DLL"))
                {
                    HANDLE hThread = CreateThread(NULL, 0, EjectThread, g_hModule, 0, NULL);
                    if (hThread)
                    {
                        CloseHandle(hThread);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Unloads the cheat)");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("</> Script"))
            {
                ImGui::Dummy(ImVec2(0, 15.0f));
                ImGui::Text("Coming soon...");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }
}

#endif