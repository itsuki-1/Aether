#ifndef MENU_H
#define MENU_H

#include <d3d11.h>
#include <wincodec.h>
#include <MinHook.h>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
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

    inline void ToggleButton(const char* str_id, bool* v)
    {
        float height = ImGui::GetFrameHeight() * 0.9f;
        float width = height * 1.55f;
        float radius = height * 0.50f;

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton(str_id, ImVec2(width, height));
        if (ImGui::IsItemClicked())
        {
            *v = !*v;
        }

        ImGuiID id = ImGui::GetID(str_id);
        ImGuiStorage* storage = ImGui::GetStateStorage();
        float t = storage->GetFloat(id, *v ? 1.0f : 0.0f);
        float ANIM_SPEED = 0.085f;
        if (*v && t < 1.0f)
        {
            t += ImGui::GetIO().DeltaTime / ANIM_SPEED;
            if (t > 1.0f) t = 1.0f;
        }
        else if (!*v && t > 0.0f)
        {
            t -= ImGui::GetIO().DeltaTime / ANIM_SPEED;
            if (t < 0.0f) t = 0.0f;
        }
        storage->SetFloat(id, t);

        const ImVec4 color_on = ImVec4(166.0f / 255.0f, 164.0f / 255.0f, 244.0f / 255.0f, 1.0f);
        const ImVec4 color_off_hover = ImVec4(0.78f, 0.78f, 0.78f, 1.0f);
        const ImVec4 color_off_normal = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

        ImU32 col_bg;
        if (ImGui::IsItemHovered())
            col_bg = ImGui::GetColorU32(ImLerp(color_off_hover, color_on, t));
        else
            col_bg = ImGui::GetColorU32(ImLerp(color_off_normal, color_on, t));

        draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
        draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
    }

    inline bool CustomSliderInt(const char* label, int* v, int v_min, int v_max)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems || !window)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = ImGui::CalcItemWidth();

        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        const float grab_radius = 8.0f;
        const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + grab_radius * 2.0f));
        const ImRect total_bb(frame_bb.Min, ImVec2(frame_bb.Max.x, frame_bb.Max.y + style.ItemInnerSpacing.y + label_size.y));

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(frame_bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
        const bool active = held;

        if (active)
        {
            ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            float clicked_t = (mouse_pos.x - frame_bb.Min.x) / w;
            clicked_t = ImSaturate(clicked_t);
            int new_value = static_cast<int>(roundf(ImLerp((float)v_min, (float)v_max, clicked_t)));
            if (*v != new_value)
            {
                *v = new_value;
                ImGui::MarkItemEdited(id);
            }
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImU32 track_color = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        const ImU32 active_track_color = ImGui::GetColorU32(ImVec4(166.0f / 255.0f, 164.0f / 255.0f, 244.0f / 255.0f, 1.0f));
        const ImU32 grab_color = ImGui::GetColorU32(ImVec4(166.0f / 255.0f, 164.0f / 255.0f, 244.0f / 255.0f, 1.0f));
        const ImU32 halo_color = ImGui::GetColorU32(ImVec4(166.0f / 255.0f, 164.0f / 255.0f, 244.0f / 255.0f, 0.2f));

        const float halo_radius = 16.0f;
        const float track_height = 8.0f;
        const float slider_y = frame_bb.GetCenter().y;
        float t = (float)(*v - v_min) / (float)(v_max - v_min);
        float grab_x = frame_bb.Min.x + t * w;
        ImVec2 grab_pos = ImVec2(grab_x, slider_y);
        float rounding = track_height * 0.5f;
        draw_list->AddRectFilled(
            ImVec2(frame_bb.Min.x, slider_y - track_height * 0.5f),
            ImVec2(frame_bb.Max.x, slider_y + track_height * 0.5f),
            track_color,
            rounding
        );

        if (grab_x > frame_bb.Min.x)
        {
            draw_list->AddRectFilled(
                ImVec2(frame_bb.Min.x, slider_y - track_height * 0.5f),
                ImVec2(grab_x, slider_y + track_height * 0.5f),
                active_track_color,
                rounding
            );
        }

        if (active || hovered)
        {
            draw_list->AddCircleFilled(grab_pos, halo_radius, halo_color, 24);

            char value_buf[32];
            snprintf(value_buf, sizeof(value_buf), "%d", *v);
            ImVec2 text_size = ImGui::CalcTextSize(value_buf);
            ImVec2 tooltip_pos = ImVec2(grab_pos.x - text_size.x * 0.5f, grab_pos.y + grab_radius + 4.0f);

            ImVec2 tooltip_min = ImVec2(tooltip_pos.x - 4, tooltip_pos.y - 4);
            ImVec2 tooltip_max = ImVec2(tooltip_pos.x + text_size.x + 4, tooltip_pos.y + text_size.y + 4);
            draw_list->AddRectFilled(tooltip_min, tooltip_max, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);

            draw_list->AddText(tooltip_pos, ImGui::GetColorU32(ImGuiCol_Text), value_buf);
        }

        draw_list->AddCircleFilled(grab_pos, grab_radius, grab_color, 24);
        draw_list->AddText(ImVec2(total_bb.Min.x, frame_bb.Max.y + style.ItemInnerSpacing.y), ImGui::GetColorU32(ImGuiCol_Text), label);

        return active;
    }

    inline void renderMenu()
    {
        static bool aimbotEnabled = false;
        static bool ragebotEnabled = false;
        static bool espEnabled = false;
        static bool bhopEnabled = false;
        static bool maxJumpEnabled = false;
        static int maxJump = 1;
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

                ImGui::Text("Aimbot (not work)");
                ImGui::SameLine();
                Menu::ToggleButton("##AimbotToggle", &aimbotEnabled);

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Rage"))
            {
                if (ImGui::BeginTable("General", 2, ImGuiTableFlags_None))
                {
                    ImVec2 child_size = ImVec2(0, 0);
                    ImGui::TableNextColumn();
                    float desired_padding_x = 15.0f;
                    ImVec2 original_padding = ImGui::GetStyle().WindowPadding;
                    ImVec2 custom_padding = ImVec2(desired_padding_x, original_padding.y);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, custom_padding);

                    if (ImGui::BeginChild("LeftColumnChild", child_size, ImGuiChildFlags_AlwaysUseWindowPadding))
                    {
                        ImGui::Text("General");
                        ImGui::Dummy(ImVec2(0, 10.0f));
                        ImGui::Text("Ragebot (not work)");
                        ImGui::SameLine();
                        Menu::ToggleButton("##RagebotToggle", &ragebotEnabled);
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

                // --- REPLACED CHECKBOX ---
                ImGui::Text("ESP (not work)");
                ImGui::SameLine();
                Menu::ToggleButton("##ESPToggle", &espEnabled);

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Misc"))
            {
                ImGui::Dummy(ImVec2(0, 15.0f));
                ImGui::Text("Auto Bhop (not work)");
                ImGui::SameLine();
                Menu::ToggleButton("##BhopToggle", &bhopEnabled);

                if (bhopEnabled)
                {
                    ImGui::Text("Limit Max Jump");
                    ImGui::SameLine();
                    Menu::ToggleButton("##MaxJumpToggle", &maxJumpEnabled);

                    if (maxJumpEnabled)
                    {
                        ImGui::Indent();
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(166.0f / 255.0f, 164.0f / 255.0f, 244.0f / 255.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(99.0f / 255.0f, 93.0f / 255.0f, 247.0f / 255.0f, 1.0f));
                        ImGui::Dummy(ImVec2(0, 10.0f));
                        ImGui::SetNextItemWidth(250.0f);
                        CustomSliderInt("", &maxJump, 1, 10);

                        ImGui::PopStyleColor(2);
                        ImGui::Unindent();
                    }
                }
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