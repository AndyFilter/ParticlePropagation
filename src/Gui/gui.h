#pragma once

#include <d3d11.h>
#include <vector>
#include "../Physics/physics_definitions.h"

namespace GUI
{
	HWND Setup(int (*OnGuiFunc)());
	int DrawGui() noexcept;
	void Destroy() noexcept;
	void LoadFonts(float fontSizeMultiplier = 1.f);

	const float particleScale = 0.002;

	// Data
	inline IDXGISwapChain* g_pSwapChain = NULL;
	inline ID3D11Device* g_pd3dDevice = NULL;
	inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
	inline std::vector<IDXGIOutput*> vOutputs;
	inline std::vector<IDXGIAdapter*> vAdapters;
	inline D3D11_VIEWPORT* g_pViewport;
	inline bool (*onExitFunc)();

	inline ID3D11Buffer* g_VertexBuffer;
	inline ID3D11Buffer* g_InstanceBuffer;
	inline ID3D11Buffer* g_ConstantBuffer;
	inline ID3D10Blob* g_VertexShaderCode, * g_PixelShaderCode;
	inline ID3D11VertexShader* g_VertexShader;
	inline ID3D11PixelShader* g_PixelShader;
	inline ID3D11InputLayout* g_InputLayout;

	inline UINT *VSyncFrame = nullptr;
	inline float aspect = 0;

	struct Instance
	{
		//float motionEquation[4];
		Physics::MotionEquation motionEquation;
		float timeCreated = 0;
		float color[3] = { 1,1,1 };
	};

	struct Vertex
	{
		float Position[3];
		float Color[3];
		float UV[2];
	};

	struct cb_Info
	{
		double physTime;
		float simSize[2];
	};

	#define GET_ACTUAL_CB_SIZE(size) (sizeof(size) + (16 - (sizeof(size) % 16)))
}

#define TOOLTIP(...)					\
   	if (ImGui::IsItemHovered())			\
		ImGui::SetTooltip(__VA_ARGS__); \