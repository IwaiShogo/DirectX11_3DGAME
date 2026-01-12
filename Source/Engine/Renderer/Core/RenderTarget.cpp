/*****************************************************************//**
 * @file	RenderTarget.cpp
 * @brief	シーンビュー、ゲームビューの実体
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Core/RenderTarget.h"

namespace Arche
{
	void RenderTarget::Create(ID3D11Device* device, int width, int height)
	{
		// サイズが変わっていない、かつ作成済みなら何もしない
		if (m_texture && m_width == width && m_height == height) return;

		m_width = width;
		m_height = height;

		// リソース解放
		m_texture.Reset();
		m_rtv.Reset();
		m_srv.Reset();
		m_dsv.Reset();

		// 1. テクスチャ作成
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		device->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());

		// 2. RTV作成
		device->CreateRenderTargetView(m_texture.Get(), nullptr, m_rtv.GetAddressOf());

		// 3. SRV作成 (ImGui用)
		device->CreateShaderResourceView(m_texture.Get(), nullptr, m_srv.GetAddressOf());

		// 4. 深度バッファ作成
		D3D11_TEXTURE2D_DESC dsDesc = desc;
		dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ComPtr<ID3D11Texture2D> depthTex;
		device->CreateTexture2D(&dsDesc, nullptr, depthTex.GetAddressOf());
		device->CreateDepthStencilView(depthTex.Get(), nullptr, m_dsv.GetAddressOf());
	}

	void RenderTarget::Bind(ID3D11DeviceContext* context)
	{
		if (!m_rtv || !m_dsv) return;

		D3D11_VIEWPORT vp = {};
		vp.Width = (float)m_width;
		vp.Height = (float)m_height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		context->RSSetViewports(1, &vp);

		context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
	}

	void RenderTarget::Clear(ID3D11DeviceContext* context, float r, float g, float b, float a)
	{
		if (!m_rtv || !m_dsv) return;

		float color[] = { r, g, b, a };
		context->ClearRenderTargetView(m_rtv.Get(), color);
		context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

}	// namespace Arche