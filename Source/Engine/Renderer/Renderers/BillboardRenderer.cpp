/*****************************************************************//**
 * @file	BillboardRenderer.cpp
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
#include "Engine/Renderer/Renderers/BillboardRenderer.h"

namespace Arche
{
	ID3D11DeviceContext* BillboardRenderer::s_context = nullptr;
	ID3D11Device* BillboardRenderer::s_device = nullptr;

	ComPtr<ID3D11VertexShader> BillboardRenderer::s_vs = nullptr;
	ComPtr<ID3D11PixelShader> BillboardRenderer::s_ps = nullptr;
	ComPtr<ID3D11InputLayout> BillboardRenderer::s_inputLayout = nullptr;
	ComPtr<ID3D11Buffer> BillboardRenderer::s_constantBuffer = nullptr;
	ComPtr<ID3D11Buffer> BillboardRenderer::s_vertexBuffer = nullptr; // 四角形の頂点用
	ComPtr<ID3D11SamplerState> BillboardRenderer::s_samplerState = nullptr;
	ComPtr<ID3D11RasterizerState> BillboardRenderer::s_rsBillboard = nullptr;
	ComPtr<ID3D11BlendState> BillboardRenderer::s_blendState = nullptr; // 半透明用
	BillboardRenderer::ConstantBufferData BillboardRenderer::s_cbData = {};

	struct BillboardVertex {
		XMFLOAT3 pos; // オフセット位置 (-0.5 ~ 0.5)
		XMFLOAT2 uv;
	};

	void BillboardRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		s_device = device;
		s_context = context;

		// 1. シェーダーコンパイル (Billboard.hlsl)
		ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

		HRESULT hr = D3DCompileFromFile(L"Resources/Engine/Shaders/Billboard.hlsl", nullptr, nullptr, "VS", "vs_5_0", flags, 0, &vsBlob, &errorBlob);
		if (FAILED(hr)) throw std::runtime_error("Failed to compile Billboard VS");
		s_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);

		hr = D3DCompileFromFile(L"Resources/Engine/Shaders/Billboard.hlsl", nullptr, nullptr, "PS", "ps_5_0", flags, 0, &psBlob, &errorBlob);
		if (FAILED(hr)) throw std::runtime_error("Failed to compile Billboard PS");
		s_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ps);

		// 2. 入力レイアウト
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		s_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout);

		// 3. 定数バッファ
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBufferData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		s_device->CreateBuffer(&bd, nullptr, &s_constantBuffer);

		// 4. 頂点バッファ (単位正方形: 中心基準)
		BillboardVertex vertices[] = {
			{ XMFLOAT3(-0.5f, 0.5f, 0),	 XMFLOAT2(0, 0) }, // 左上
			{ XMFLOAT3(0.5f, 0.5f, 0),	 XMFLOAT2(1, 0) }, // 右上
			{ XMFLOAT3(-0.5f, -0.5f, 0), XMFLOAT2(0, 1) }, // 左下
			{ XMFLOAT3(0.5f, -0.5f, 0), XMFLOAT2(1, 1) }, // 右下
		};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(BillboardVertex) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vertices;
		s_device->CreateBuffer(&bd, &initData, &s_vertexBuffer);

		// 5. サンプラー & ラスタライザ & ブレンド
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		s_device->CreateSamplerState(&sd, &s_samplerState);

		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_NONE; // 両面描画
		rd.FillMode = D3D11_FILL_SOLID;
		s_device->CreateRasterizerState(&rd, &s_rsBillboard);

		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		s_device->CreateBlendState(&blendDesc, &s_blendState);
	}

	void BillboardRenderer::Shutdown()
	{
		s_vs.Reset();
		s_ps.Reset();
		s_inputLayout.Reset();
		s_constantBuffer.Reset();
		s_vertexBuffer.Reset();
		s_samplerState.Reset();
		s_rsBillboard.Reset();
		s_blendState.Reset();

		s_device = nullptr;
		s_context = nullptr;
	}

	void BillboardRenderer::Begin(const XMMATRIX& view, const XMMATRIX& projection) {
		s_context->IASetInputLayout(s_inputLayout.Get());
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		UINT stride = sizeof(BillboardVertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_vertexBuffer.GetAddressOf(), &stride, &offset);

		s_context->VSSetShader(s_vs.Get(), nullptr, 0);
		s_context->PSSetShader(s_ps.Get(), nullptr, 0);

		s_context->VSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetSamplers(0, 1, s_samplerState.GetAddressOf());

		s_context->RSSetState(s_rsBillboard.Get());

		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		s_context->OMSetBlendState(s_blendState.Get(), blendFactor, 0xffffffff);

		// ビュー・プロジェクション行列セット
		s_cbData.view = XMMatrixTranspose(view);
		s_cbData.projection = XMMatrixTranspose(projection);
	}

	void BillboardRenderer::Draw(Texture* texture, const XMFLOAT3& position, float width, float height, const XMFLOAT4& color) {
		if (!texture) return;

		// 1. ワールド行列（位置とサイズのみ。回転はシェーダーがやる）
		// スケール行列で幅と高さを設定
		XMMATRIX S = XMMatrixScaling(width, height, 1.0f);
		XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
		XMMATRIX world = S * T;

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		// 2. テクスチャセット
		s_context->PSSetShaderResources(0, 1, texture->srv.GetAddressOf());

		// 3. 描画
		s_context->Draw(4, 0);
	}

}	// namespace Arche