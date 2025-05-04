#include <Windows.h>
#include <d3d11.h>
#include <MinHook.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <thread>

#include "Menu/Menu.h"
static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* view = nullptr;
static HWND g_hwnd = nullptr;
void* origin_present = nullptr;
HMODULE g_hModule = NULL;
bool g_unloading = false;
void* present_hook_target = nullptr;

using Present =  HRESULT (__stdcall*)(IDXGISwapChain* , UINT,UINT);

WNDPROC origin_wndProc;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT __stdcall WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) { return true; }
	return CallWindowProc(origin_wndProc, hwnd, uMsg, wParam, lParam);
}

bool inited = false;

long __stdcall my_present(IDXGISwapChain* _this, UINT a, UINT b)
{
	if (g_unloading)
	{
		return ((Present)origin_present)(_this, a, b);
	}

	if (!inited)
	{
		_this->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
		g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

		DXGI_SWAP_CHAIN_DESC sd;
		_this->GetDesc(&sd);
		g_hwnd = sd.OutputWindow;

		ID3D11Texture2D* buf{};
		_this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
		g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &view);
		buf->Release();

		origin_wndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(g_hwnd);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

		Menu::LoadImageByMemory(g_pd3dDevice, aetherLogo, sizeof(aetherLogo), &Menu::ImageResources);

		inited = true;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 10.0f;
	style.WindowPadding = ImVec2(15, 15);
	style.TabBarBorderSize = 0.0f;
	style.TabRounding = 0.0f;
	style.ItemInnerSpacing.x = 10.0f;
	style.TabBorderSize = 5.0f;

	ImFontConfig fontConfig;
	fontConfig.MergeMode = true;
	fontConfig.PixelSnapH = true;
	fontConfig.OversampleH = 3;
	fontConfig.OversampleV = 3;

	auto& colors = style.Colors;
	float colR = 139.0f / 255.0f;
	float colG = 168.0f / 255.0f;
	float colB = 179.0f / 255.0f;
	float colA = 255.0f / 255.0f;

	colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_TabHovered] = ImVec4(colR * 0.85f, colG * 0.85f, colB * 0.85f, colA);
	colors[ImGuiCol_TabActive] = ImVec4(colR, colG , colB, 0.1f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(colR * 0.8f, colG * 0.8f, colB * 0.8f, colA * 0.8f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(colR * 0.5f, colG * 0.5f, colB * 0.5f, colA * 0.6f);
	colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
	colors[ImGuiCol_Button] = ImVec4(colR * 0.6f, colG * 0.6f, colB * 0.6f, colA * 0.7f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.0f, 0.0f, colA * 0.7f);
	colors[ImGuiCol_FrameBg] = ImVec4(colR * 0.6f, colG * 0.6f, colB * 0.6f, colA * 0.7f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(colR * 0.6f, colG * 0.6f, colB * 0.6f, colA * 0.7f);
	colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	colors[ImGuiCol_WindowBg] = ImVec4(45.0f/255.0f, 53.0f/255.0f, 60.0f/255.0f, 0.95f);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	ImFont* fontRegular = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\segoeui.ttf", 17.0f
	);

	ImFont* fontLarge = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\segoeui.ttf", 32.0f
	);

	ImGui_ImplDX11_CreateDeviceObjects();

	Menu::g_FontLarge = fontLarge;

	if (!g_unloading)
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		static bool show_menu = true;
		if (GetAsyncKeyState(VK_INSERT) & 1)
		{
			show_menu = !show_menu;
		}

		if (show_menu)
		{
			Menu::renderMenu();
		}

		ImGui::EndFrame();
		ImGui::Render();
		g_pd3dContext->OMSetRenderTargets(1, &view, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	return ((Present)origin_present)(_this, a, b);
}


DWORD create(void*)
{
	const unsigned level_count = 2;
	D3D_FEATURE_LEVEL levels[level_count] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	auto hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		levels,
		level_count,
		D3D11_SDK_VERSION,
		&sd,
		&g_pSwapChain,
		&g_pd3dDevice,
		nullptr,
		nullptr);

	if (g_pSwapChain)
	{
		auto vtable_ptr = (void***)(g_pSwapChain);
		auto vtable = *vtable_ptr;
		auto present = vtable[8];
		present_hook_target = present;
		MH_Initialize();
		MH_CreateHook(present_hook_target, my_present, &origin_present);
		MH_EnableHook(present_hook_target);
		g_pd3dDevice->Release();
		g_pSwapChain->Release();
	}

	return 0;
}

static BOOL __stdcall DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		g_hModule = hModule;
		DisableThreadLibraryCalls(hModule);
		CreateThread(NULL, 0, create, NULL, 0, NULL);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		g_unloading = true;
	}

    return TRUE;
}

DWORD WINAPI EjectThread(LPVOID lpParameter)
{
	g_unloading = true;

	Sleep(500);

	if (g_hwnd && origin_wndProc)
		SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)origin_wndProc);

	if (inited)
	{
		if (g_pd3dDevice && g_pd3dContext)
			ImGui_ImplDX11_Shutdown();

		if (g_hwnd)
			ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
	}

	if (present_hook_target)
	{
		MH_DisableHook(present_hook_target);
		MH_RemoveHook(present_hook_target);
	}

	MH_Uninitialize();

	if (view)
	{
		view->Release();
		view = nullptr;
	}

	if (lpParameter)
	{
		FreeLibraryAndExitThread((HMODULE)lpParameter, 0);
	}
	else
	{
		ExitThread(0);
	}
	return 0;
}
