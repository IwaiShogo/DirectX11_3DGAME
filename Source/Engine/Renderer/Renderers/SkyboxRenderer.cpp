/*****************************************************************//**
 * @file	SkyboxRenderer.cpp
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
#include "SkyboxRenderer.h"
#include "Engine/Core/Application.h"

namespace Arche
{
	void SkyboxRenderer::Initialize()
	{
		auto device = Application::Instance().GetDevice();

		// 1. シェーダーコンパイル
		std::wstring shaderPath = L"Resources/Engine/Shaders/Skybox.hlsl";
		ID3DBlob* vsBlob = nullptr;
		ID3DBlob* psBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			return;
		}
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);

		hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			return;
		}
		device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

		if (vsBlob) vsBlob->Release();
		if (psBlob) psBlob->Release();
		if (errorBlob) errorBlob->Release();

		// 2. 定数バッファ
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VSConstantBuffer);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		device->CreateBuffer(&bd, nullptr, &m_cbVS);

		// 3. ステート作成
		// Depth: Z=1.0 に描画するため、LessEqual (<=) で比較し、Z書き込みはしない
		D3D11_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 書き込みなし
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;		// これが重要
		device->CreateDepthStencilState(&depthDesc, &m_depthState);

		// Rasterizer: 裏面カリングなし（内側から見るため）
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		device->CreateRasterizerState(&rasterDesc, &m_rasterizerState);

		// Sampler: リニア補間
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		device->CreateSamplerState(&sampDesc, &m_samplerState);
	}

	void SkyboxRenderer::SetTexture(std::shared_ptr<Texture> texture)
	{
		m_skyboxTexture = texture;
	}

	void SkyboxRenderer::Render(const XMMATRIX& view, const XMMATRIX& proj, const SceneEnvironment& env)
	{
		if (!m_cbVS) return;

		auto context = Application::Instance().GetContext();

		XMMATRIX viewNoTrans = view;
		viewNoTrans.r[3] = XMVectorSet(0, 0, 0, 1);

		// 定数バッファ更新
		VSConstantBuffer cb;
		cb.View = XMMatrixTranspose(viewNoTrans);
		cb.Projection = XMMatrixTranspose(proj);

		// テクスチャがあるか判定
		bool hasTexture = (m_skyboxTexture != nullptr);
		cb.UseTexture = hasTexture ? 1.0f : 0.0f;

		// 色設定
		cb.ColorTop = env.skyColorTop;
		cb.ColorHorizon = env.skyColorHorizon;
		cb.ColorBottom = env.skyColorBottom;

		context->UpdateSubresource(m_cbVS, 0, nullptr, &cb, 0, 0);

		// ステート設定
		context->OMSetDepthStencilState(m_depthState, 0);
		context->RSSetState(m_rasterizerState);

		// シェーダー設定
		context->VSSetShader(m_vs, nullptr, 0);
		context->PSSetShader(m_ps, nullptr, 0);
		context->VSSetConstantBuffers(0, 1, &m_cbVS);
		context->PSSetConstantBuffers(0, 1, &m_cbVS);

		// テクスチャ設定
		if (hasTexture)
		{
			ID3D11ShaderResourceView* srv = m_skyboxTexture->GetSRV();
			context->PSSetShaderResources(0, 1, &srv);
			context->PSSetSamplers(0, 1, &m_samplerState);
		}
		else
		{
			// テクスチャ解除
			ID3D11ShaderResourceView* nullSRV = nullptr;
			context->PSSetShaderResources(0, 1, &nullSRV);
		}

		// 描画 (36頂点)
		context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->Draw(36, 0);

		// ステート復元
		context->OMSetDepthStencilState(nullptr, 0);
		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->PSSetShaderResources(0, 1, &nullSRV); // バインド解除
	}
}