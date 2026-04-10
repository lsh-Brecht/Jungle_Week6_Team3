#pragma once
#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Core/CoreTypes.h"

#include "RasterizerStateManager.h"
#include "DepthStencilStateManager.h"
#include "BlendStateManager.h"

class FD3DDevice
{
public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();

	void Present();
	void OnResizeViewport(int width, int height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;
	ID3D11RenderTargetView* GetFrameBufferRTV() const { return FrameBufferRTV; }
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView; }
	ID3D11ShaderResourceView* GetStencilSRV() const { return StencilSRV; }
	ID3D11RenderTargetView* GetGBufferAlbedoRTV() const { return GBufferAlbedoRTV; }
	ID3D11RenderTargetView* GetGBufferNormalRTV() const { return GBufferNormalRTV; }
	ID3D11ShaderResourceView* GetGBufferAlbedoSRV() const { return GBufferAlbedoSRV; }
	ID3D11ShaderResourceView* GetGBufferNormalSRV() const { return GBufferNormalSRV; }
	const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }
	const float* GetClearColor() const { return ClearColor; }

	void SetDepthStencilState(EDepthStencilState InState);
	void SetBlendState(EBlendState InState);
	void SetRasterizerState(ERasterizerState InState);

private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* GBufferAlbedo = nullptr;
	ID3D11RenderTargetView* GBufferAlbedoRTV = nullptr;
	ID3D11ShaderResourceView* GBufferAlbedoSRV = nullptr;
	ID3D11Texture2D* GBufferNormal = nullptr;
	ID3D11RenderTargetView* GBufferNormalRTV = nullptr;
	ID3D11ShaderResourceView* GBufferNormalSRV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	ID3D11ShaderResourceView* StencilSRV = nullptr;

	FRasterizerStateManager RasterizerStateManager;
	FDepthStencilStateManager DepthStencilStateManager;
	FBlendStateManager BlendStateManager;

	D3D11_VIEWPORT ViewportInfo = {};

	const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;
};
