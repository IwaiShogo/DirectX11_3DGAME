/*****************************************************************//**
 * @file	GridRenderer.cpp
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
#include "GridRenderer.h"
#include "Engine/Core/Application.h"

namespace Arche
{
	void GridRenderer::Initialize()
	{
		auto device = Application::Instance().GetDevice();

		// 1. シェーダーコンパイル (埋め込みではなくファイル読み込み)
		// ※パスはプロジェクト構成に合わせて調整してください
		//	 ビルド後に Resources/Engine/Shaders に配置される前提です
		std::wstring shaderPath = L"Resources/Engine/Shaders/InfiniteGrid.hlsl";

		ID3DBlob* vsBlob = nullptr;
		ID3DBlob* psBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		// VS
		HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			return;
		}
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);

		// PS
		hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			return;
		}
		device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

		vsBlob->Release();
		psBlob->Release();
		if (errorBlob) errorBlob->Release();

		// 2. 定数バッファ作成
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VSConstantBuffer);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		device->CreateBuffer(&bd, nullptr, &m_cbVS);

		bd.ByteWidth = sizeof(GridConstantBuffer);
		device->CreateBuffer(&bd, nullptr, &m_cbGrid);

		// 3. ステート作成
		// Blend State (Alpha Blend)
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		device->CreateBlendState(&blendDesc, &m_blendState);

		// Depth State (Z-Write OFF, Z-Test ON)
		// グリッドは半透明なのでデプスには書き込まないが、手前の物体には隠れるべき
		D3D11_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 書き込みなし
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		device->CreateDepthStencilState(&depthDesc, &m_depthState);

		// Rasterizer (Culling OFF)
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = FALSE;
		device->CreateRasterizerState(&rasterDesc, &m_rasterizerState);
	}

	void GridRenderer::Render(const XMMATRIX& view, const XMMATRIX& proj, const XMFLOAT3& cameraPos, float farPlane)
	{
		auto context = Application::Instance().GetContext();

		// 定数バッファ更新
		VSConstantBuffer cb;
		cb.View = XMMatrixTranspose(view);
		cb.Projection = XMMatrixTranspose(proj);
		cb.CameraPos = cameraPos;
		cb.Near = 0.1f;
		cb.Far = farPlane;
		context->UpdateSubresource(m_cbVS, 0, nullptr, &cb, 0, 0);

		GridConstantBuffer gcb;
		XMMATRIX viewProj = view * proj;
		XMVECTOR det;
		gcb.InverseViewProj = XMMatrixTranspose(XMMatrixInverse(&det, viewProj));
		context->UpdateSubresource(m_cbGrid, 0, nullptr, &gcb, 0, 0);

		// ステート設定
		context->OMSetBlendState(m_blendState, nullptr, 0xFFFFFFFF);
		context->OMSetDepthStencilState(m_depthState, 0);
		context->RSSetState(m_rasterizerState);

		// シェーダー設定
		context->VSSetShader(m_vs, nullptr, 0);
		context->PSSetShader(m_ps, nullptr, 0);
		context->VSSetConstantBuffers(0, 1, &m_cbVS);
		context->VSSetConstantBuffers(1, 1, &m_cbGrid);
		context->PSSetConstantBuffers(0, 1, &m_cbVS);

		// 頂点バッファなしで6頂点描画（Fullscreen Triangle x 2相当）
		context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->Draw(3, 0);

		// ステート復元（必要であれば）
		context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		context->OMSetDepthStencilState(nullptr, 0); // デフォルトに戻す
	}
}