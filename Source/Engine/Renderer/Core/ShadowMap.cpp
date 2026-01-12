/*****************************************************************//**
 * @file	ShadowMap.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "ShadowMap.h"

namespace Arche
{
	ShadowMap::~ShadowMap()
	{
		m_texture.Reset();
		m_dsv.Reset();
		m_srv.Reset();
	}

	void ShadowMap::Initialize(ID3D11Device* device, float width, float height)
	{
		m_width = width;
		m_height = height;

		// 1. テクスチャの作成 (Typelessで作成し、DSVとSRVでフォーマットを使い分ける)
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = (UINT)width;
		texDesc.Height = (UINT)height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // 深度24bit + ステンシル8bit
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		device->CreateTexture2D(&texDesc, nullptr, m_texture.GetAddressOf());

		// 2. DSV (書き込み用)
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(m_texture.Get(), &dsvDesc, m_dsv.GetAddressOf());

		// 3. SRV (読み込み用)
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // 深度部分だけを赤成分(R)として読む
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srv.GetAddressOf());

		// 4. ビューポート設定
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = width;
		m_viewport.Height = height;
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;
	}

	void ShadowMap::Begin(ID3D11DeviceContext* context)
	{
		// レンダーターゲットを解除し、深度バッファのみをセット
		// (カラー出力は不要なため、RTVはnullptr)
		ID3D11RenderTargetView* nullRTV = nullptr;
		context->OMSetRenderTargets(0, &nullRTV, m_dsv.Get());

		// 深度クリア
		context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		// ビューポート設定
		context->RSSetViewports(1, &m_viewport);
	}

	void ShadowMap::End(ID3D11DeviceContext* context)
	{
		// 特に処理は不要だが、SRVとして使うためにバインド解除が必要な場合に備える
		// (今回はRenderSystem側でRTVを再設定することで自動的に解除されるため空でOK)
	}
}