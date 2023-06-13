#include "../../External/ImGui/imgui.h"
#include "../../External/ImGui/imgui_impl_win32.h"
#include "../../External/ImGui/imgui_impl_dx11.h"
#include <tchar.h>
#include <iostream>
#include <string>
#include <direct.h>
#include <d3d11.h>
#include <d3dcompiler.h>


#include "gui.h"
#include "../Physics/physics.h"
#include "../../External/IBMPlexMono.h"
#include "../../resource.h"

using namespace GUI;

// Data
//static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//const int windowX = 1920+16, windowY = 1080+39;
const int windowX = 1200 + 16, windowY = 800 + 39;

float defaultFontSize = 16.0f;

static WNDCLASSEX wc;
static HWND hwnd;

static int (*mainGuiFunc)();

// Setup code, takes a function to run when doing GUI
HWND GUI::Setup(int (*OnGuiFunc)())
{
	if (OnGuiFunc != NULL)
		mainGuiFunc = OnGuiFunc;
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Cząsteczek Symulator"), NULL };
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	//wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	::RegisterClassEx(&wc);

	hwnd = ::CreateWindowW(wc.lpszClassName, _T("Cząsteczek Symulator"), (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX), 200, 100, windowX, windowY, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return NULL;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);


	g_pViewport = new D3D11_VIEWPORT();
	g_pViewport->TopLeftX = 0;
	g_pViewport->TopLeftY = 0;
	g_pViewport->Width = (FLOAT)windowX-16;
	g_pViewport->Height = (FLOAT)windowY-39;
	g_pViewport->MinDepth = 0.f;
	g_pViewport->MaxDepth = 1.f;
	g_pd3dDeviceContext->RSSetViewports(1, g_pViewport);

	aspect = g_pViewport->Width / g_pViewport->Height;


	// Compile and create vertex shader

	const float scaleX = particleScale;
	const float scale = particleScale * aspect;
	Vertex myVertices[] = {
	{ -scaleX,  scale, 0.f, {1,0,0}, {0.f, 1.f} },
	{ scaleX, -scale, 0.f, {0,1,0}, {1.0f, 0.f} },
	{ -scaleX, -scale, 0.f, {0,0,0}, { 0.f, 0.f} },
	{ -scaleX,  scale, 0.f, {1,0,0}, {0.0f, 1.f} },
	{ scaleX, scale, 0.f, {0,1,0}, {1.0f, 1.f} },
	{ scaleX, -scale, 0.f, {0,0,0}, {1.f, 0.f} },
	};

	D3D11_BUFFER_DESC vertBufDesc;
	ZeroMemory(&vertBufDesc, sizeof(vertBufDesc));
	vertBufDesc.ByteWidth = sizeof(Vertex) * _countof(myVertices);
	vertBufDesc.Usage = D3D11_USAGE_DEFAULT;
	vertBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA bufData;
	ZeroMemory(&bufData, sizeof(bufData));
	bufData.pSysMem = myVertices;

	HRESULT hr = g_pd3dDevice->CreateBuffer(&vertBufDesc, &bufData, &g_VertexBuffer);
	assert(SUCCEEDED(hr));


	// Create Instance Buffer

	D3D11_BUFFER_DESC instBufDesc;
	ZeroMemory(&instBufDesc, sizeof(instBufDesc));
	instBufDesc.ByteWidth = sizeof(Instance) * MAX_PARTICLES;
	instBufDesc.Usage = D3D11_USAGE_DYNAMIC;
	instBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instBufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//instBufDesc.StructureByteStride = 4;

	hr = g_pd3dDevice->CreateBuffer(&instBufDesc, 0, &g_InstanceBuffer);
	assert(SUCCEEDED(hr));


	// Create Constant Buffer

	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.ByteWidth = GET_ACTUAL_CB_SIZE(cb_Info);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = g_pd3dDevice->CreateBuffer(&cbDesc, 0, &g_ConstantBuffer);
	assert(SUCCEEDED(hr));


	// Compile and create vertex shader

	std::string path = "";
	char pBuf[1024];

	auto _tmp = _getcwd(pBuf, 1024);
	path = pBuf;
	path += "\\";
	std::wstring wpath = std::wstring(path.begin(), path.end());

	std::wstring vertFragPath = wpath + L"src\\Gui\\BasicVertexShader.hlsl";

	ID3D10Blob* errorBlob = 0;

	hr = D3DCompileFromFile(vertFragPath.c_str(), nullptr, nullptr, "main",
		"vs_5_0", 0, 0, &g_VertexShaderCode,
		&errorBlob);

	assert(SUCCEEDED(hr));

	hr = g_pd3dDevice->CreateVertexShader(g_VertexShaderCode->GetBufferPointer(), g_VertexShaderCode->GetBufferSize(), 0, &g_VertexShader);
	assert(SUCCEEDED(hr));

	if (errorBlob) errorBlob->Release();


	// Compile and create pixel shader

	std::wstring pixelFragPath = wpath + L"src\\Gui\\BasicPixelShader.hlsl";

	errorBlob = 0;

	hr = D3DCompileFromFile(pixelFragPath.c_str(), nullptr, nullptr, "main",
		"ps_5_0", 0, 0, &g_PixelShaderCode,
		&errorBlob);
	assert(SUCCEEDED(hr));

	hr = g_pd3dDevice->CreatePixelShader(g_PixelShaderCode->GetBufferPointer(), g_PixelShaderCode->GetBufferSize(), 0, &g_PixelShader);
	assert(SUCCEEDED(hr));

	if (errorBlob) errorBlob->Release();


	// Create input layout

	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		//{ "SV_InstanceID", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA},

		// Data from the instance buffer
		{ "I_MOTION_EQ",	0,	DXGI_FORMAT_R32G32B32A32_FLOAT,	1,	0,								D3D11_INPUT_PER_INSTANCE_DATA,	1},
		{ "I_TIME_CREATED",	0,	DXGI_FORMAT_R32_FLOAT,			1,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_INSTANCE_DATA,	1},
		{ "I_COLOR",		0,	DXGI_FORMAT_R32G32B32_FLOAT,	1,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_INSTANCE_DATA,	1}
	};

	hr = g_pd3dDevice->CreateInputLayout(inputDesc, _countof(inputDesc), g_VertexShaderCode->GetBufferPointer(), g_VertexShaderCode->GetBufferSize(), &g_InputLayout);
	assert(SUCCEEDED(hr));

#pragma region style
	// Własny styl
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.ChildRounding = 6.0f;
	style.FrameRounding = 5.0f;
	style.PopupRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.FrameBorderSize = 0.f;
	style.PopupBorderSize = 0.f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Separator] = ImVec4(0.96f, 0.66f, 0.72f, 0.63f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.84f, 0.58f, 0.63f, 0.78f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.89f, 0.61f, 0.66f, 0.87f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.96f, 0.66f, 0.72f, 1.00f);
	colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.49f, 0.50f, 0.85f, 0.61f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.49f, 0.50f, 0.85f, 0.79f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.49f, 0.50f, 0.85f, 0.95f);
	colors[ImGuiCol_Header] = ImVec4(0.49f, 0.50f, 0.85f, 0.45f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.49f, 0.50f, 0.85f, 0.58f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.49f, 0.50f, 0.85f, 0.81f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.02f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.94f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.24f, 0.59f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.29f, 0.29f, 0.29f, 0.65f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.32f, 0.32f, 0.71f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.91f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.56f, 0.68f, 0.96f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.03f, 0.03f, 0.03f, 0.51f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.81f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.33f, 0.76f, 0.91f, 0.96f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.86f, 1.00f, 1.00f);
#pragma endregion

	return hwnd;
}

void GUI::LoadFonts(float fontSizeMultiplier)
{
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig config;
	config.OversampleH = 2 * fontSizeMultiplier;
	config.OversampleV = 2 * fontSizeMultiplier;

	static ImWchar ranges[] = { 0x1, 0x17F, 0x2070, 0x2089, 0 };
	//static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };

	auto defFont = io.Fonts->AddFontFromMemoryCompressedBase85TTF(IBMPlexMono_compressed_data_base85, 16, &config, ranges);//io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 13, &config);
	//auto defFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 13, &config, ranges);
	io.FontDefault = defFont;

	io.Fonts->Build();
}

int GUI::DrawGui() noexcept
{
	static bool showMainWindow = true;
	static bool done = false;

	MSG msg;
	Physics::isSimPaused = false;
	while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
		    done = true;
		if(msg.message == WM_MOVE)
			Physics::isSimPaused = true;
	}
	if (done)
		return 1;


	D3D11_MAPPED_SUBRESOURCE resource;
	g_pd3dDeviceContext->Map(g_InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);

	Physics::is_accessing = true;
	Instance* instances = (Instance*)resource.pData;//new Instance[Physics::particles.size()];
	//memset(instances, 0, sizeof(Instance) * 1);
	for (int i = 0; i < Physics::particles.size(); i++)
	{
		auto& p = Physics::particles[i];
		instances[i].motionEquation = p.me;
		instances[i].timeCreated = p.timeCreated;
	}
	Physics::is_accessing = false;

	//memcpy(resource.pData, instances, Physics::particles.size() * sizeof(Instance));
	g_pd3dDeviceContext->Unmap(g_InstanceBuffer, 0);


	// Update Constant Buffer
	g_pd3dDeviceContext->Map(g_ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	cb_Info* cbData = (cb_Info*)resource.pData;
	cbData->physTime = Physics::simulationTime;
	memcpy(&cbData->simSize, &Physics::boundsExtend, sizeof(float) * 2);
	g_pd3dDeviceContext->Unmap(g_ConstantBuffer, 0);


	// Need to update Vertex Buffer after a resize!


	const float clear_color_with_alpha[4] = { 0.07f,0.07f,0.07f,1 };
	g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);

	// Set all the buffers and shaders
	g_pd3dDeviceContext->IASetInputLayout(g_InputLayout);
	g_pd3dDeviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//UINT vbStride = sizeof(Vertex), vbOffset = 0;
	UINT Strides[] = { sizeof(Vertex), sizeof(Instance) };
	UINT Offsets[] = { 0, 0 };
	ID3D11Buffer* Buffers[] = { g_VertexBuffer, g_InstanceBuffer };
	g_pd3dDeviceContext->IASetVertexBuffers(0, _countof(Strides), Buffers, Strides, Offsets);
	//g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_VertexBuffer, &vbStride, &vbOffset);
	g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &g_ConstantBuffer);
	g_pd3dDeviceContext->VSSetShader(g_VertexShader, 0, 0);
	g_pd3dDeviceContext->PSSetShader(g_PixelShader, 0, 0);

	g_pd3dDeviceContext->DrawInstanced(6, Physics::particles.size(), 0, 0);
	//g_pd3dDeviceContext->Draw(6, 0);


	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	auto size = ImGui::GetIO().DisplaySize;
	//size = ImGui::GetWindowSize();
	ImGui::SetNextWindowSize({ (float)size.x, (float)size.y });
	ImGui::SetNextWindowPos({ 0,0 });
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
	ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.07f,0.07f,0.07f,0 });
	ImGui::Begin("Window", &showMainWindow, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	// Call the GUI function in main file.
	if (int mainGui = mainGuiFunc())
		return mainGui;

	ImGui::End();

#ifdef _DEBUG 
	ImGui::ShowDemoWindow();
#endif // DEBUG

	// Rendering
	ImGui::Render();
	//const float clear_color_with_alpha[4] = { 0.07f,0.07f,0.07f,1 };
	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
	//g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	//g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);


	g_pSwapChain->Present(VSyncFrame ? *VSyncFrame : 0, 0);
	return 0;
}

void GUI::Destroy() noexcept
{
	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0; // 60
	sd.BufferDesc.RefreshRate.Denominator = 0; // 1
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

#ifdef _DEBUG
	IDXGIDevice* pDXGIDevice = nullptr;
	auto hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);

	IDXGIAdapter* pDXGIAdapter = nullptr;
	hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);

	DXGI_ADAPTER_DESC desc;
	pDXGIAdapter->GetDesc(&desc);

	wprintf(L"Selected Adapter: %s\n", desc.Description);
#endif

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	g_pSwapChain->SetFullscreenState(FALSE, NULL);
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (!pBackBuffer)
		ExitProcess(1);
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		Physics::isSimPaused = true;
		if ((UINT)LOWORD(lParam) > 0) {
			Physics::boundsExtend.x = (UINT)LOWORD(lParam);
			Physics::boundsExtend.y = (UINT)HIWORD(lParam);
		}
		else
			Physics::isSimPlaying = false;
		Physics::was_simulation_changed = true;
		Physics::was_size_changed = true;
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			if (g_pViewport) {
				g_pViewport->Width = (UINT)LOWORD(lParam);
				g_pViewport->Height = (UINT)HIWORD(lParam);
				g_pd3dDeviceContext->RSSetViewports(1, g_pViewport);
			}
			CreateRenderTarget();
		}
		return 0;
	case WM_MOVE:
		Physics::isSimPaused = true;
		break;
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
		lpMMI->ptMinTrackSize.x = windowX;
		lpMMI->ptMinTrackSize.y = windowY;
		break;
	}
	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE)
		{
			if (onExitFunc)
			{
				if (onExitFunc())
					break;
				else
					return 0;
			}
			else
				break;
		}
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		if (onExitFunc)
		{
			if (onExitFunc())
			{
				::PostQuitMessage(0);
				return 0;
			}
		}
		else
		{
			::PostQuitMessage(0);
			return 0;
		}
	}


	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
