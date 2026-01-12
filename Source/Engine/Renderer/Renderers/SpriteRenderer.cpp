/*****************************************************************//**
 * @file	SpriteRenderer.cpp
 * @brief	2D専用のレンダラークラスの実装
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Config.h"

namespace Arche
{
	// 静的メンバ定義
	ID3D11Device* SpriteRenderer::s_device = nullptr;
	ID3D11DeviceContext* SpriteRenderer::s_context = nullptr;
	float SpriteRenderer::s_screenWidth = 0.0f;
	float SpriteRenderer::s_screenHeight = 0.0f;
	ComPtr<ID3D11VertexShader> SpriteRenderer::s_vs = nullptr;
	ComPtr<ID3D11PixelShader> SpriteRenderer::s_ps = nullptr;
	ComPtr<ID3D11InputLayout> SpriteRenderer::s_inputLayout = nullptr;
	ComPtr<ID3D11Buffer> SpriteRenderer::s_vertexBuffer = nullptr;
	ComPtr<ID3D11Buffer> SpriteRenderer::s_constantBuffer = nullptr;
	ComPtr<ID3D11RasterizerState> SpriteRenderer::s_rs2D = nullptr;
	ComPtr<ID3D11DepthStencilState> SpriteRenderer::s_ds2D = nullptr;
	ComPtr<ID3D11BlendState> SpriteRenderer::s_blendState = nullptr;
	ComPtr<ID3D11SamplerState> SpriteRenderer::s_samplerState = nullptr;
	SpriteRenderer::ConstantBufferData SpriteRenderer::s_cbData = {};

	struct Vertex2D {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	void SpriteRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float w, float h)
	{
		s_device = device;
		s_context = context;
		s_screenWidth = w;
		s_screenHeight = h;

		// 1. シェーダーコンパイル (パスに注意)
		ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

		HRESULT hr = D3DCompileFromFile(L"Resources/Engine/Shaders/Sprite.hlsl", nullptr, nullptr, "VS", "vs_5_0", flags, 0, &vsBlob, &errorBlob);
		if (FAILED(hr)) throw std::runtime_error("Failed to compile Sprite VS");
		s_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);

		hr = D3DCompileFromFile(L"Resources/Engine/Shaders/Sprite.hlsl", nullptr, nullptr, "PS", "ps_5_0", flags, 0, &psBlob, &errorBlob);
		if (FAILED(hr)) throw std::runtime_error("Failed to compile Sprite PS");
		s_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ps);

		// 2. 入力レイアウト (Pos + UV)
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		s_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout);

		// 3. 頂点バッファ (動的に書き換えるのでDYNAMIC)
		// 四角形 (TriangleStripなら4頂点で済む)
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex2D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		s_device->CreateBuffer(&bd, nullptr, &s_vertexBuffer);

		// 4. 定数バッファ
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBufferData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		s_device->CreateBuffer(&bd, nullptr, &s_constantBuffer);

		// 5. ブレンドステート (透過有効)
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		s_device->CreateBlendState(&blendDesc, &s_blendState);

		// 6. サンプラー (リニア補間)
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		s_device->CreateSamplerState(&samplerDesc, &s_samplerState);

		// 7. ラスタライザー
		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_NONE;
		rd.FillMode = D3D11_FILL_SOLID;
		rd.DepthClipEnable = FALSE;
		s_device->CreateRasterizerState(&rd, &s_rs2D);

		// 深度ステンシルステート
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = FALSE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;

		hr = s_device->CreateDepthStencilState(&dsd, &s_ds2D);
		if (FAILED(hr)) throw std::runtime_error("Failed to create 2D DepthStencil State");
	}

	void SpriteRenderer::Shutdown()
	{
		s_vs.Reset();
		s_ps.Reset();
		s_inputLayout.Reset();
		s_vertexBuffer.Reset();
		s_constantBuffer.Reset();
		s_rs2D.Reset();
		s_ds2D.Reset();
		s_blendState.Reset();
		s_samplerState.Reset();

		s_device = nullptr;
		s_context = nullptr;
	}

	void SpriteRenderer::Begin()
	{
		// 2D用のパイプライン設定
		s_context->IASetInputLayout(s_inputLayout.Get());
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // ストリップ

		UINT stride = sizeof(Vertex2D);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_vertexBuffer.GetAddressOf(), &stride, &offset);

		s_context->VSSetShader(s_vs.Get(), nullptr, 0);
		s_context->PSSetShader(s_ps.Get(), nullptr, 0);

		s_context->VSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());

		// サンプラーセット
		s_context->PSSetSamplers(0, 1, s_samplerState.GetAddressOf());

		// ブレンドステートセット
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		s_context->OMSetBlendState(s_blendState.Get(), blendFactor, 0xffffffff);

		// 2D正射影行列 (左上0,0 ～ 右下W,H)
		// Z範囲は 0.0～1.0
		float w = static_cast<float>(Config::SCREEN_WIDTH);
		float h = static_cast<float>(Config::SCREEN_HEIGHT);
		s_cbData.projection = XMMatrixTranspose(XMMatrixOrthographicLH(w, h, 0.0f, 100.0f));

		s_context->RSSetState(s_rs2D.Get());
		s_context->OMSetDepthStencilState(s_ds2D.Get(), 0);
	}

	void SpriteRenderer::Draw(Texture* texture, const XMMATRIX& worldMatrix, const XMFLOAT4& color) {
		if (!texture)
		{
			return;
		}

		// 頂点データの更新 (4頂点)
		D3D11_MAPPED_SUBRESOURCE ms;
		if (SUCCEEDED(s_context->Map(s_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		{
			Vertex2D* v = (Vertex2D*)ms.pData;

			float left = 0.0f;
			float right = 1.0f;
			float top = 1.0f;
			float bottom = 0.0f;

			// TriangleStripの順序: 左上 -> 右上 -> 左下 -> 右下
			v[0] = { XMFLOAT3(0, 0, 0), XMFLOAT2(0, 1) }; // 左上
			v[1] = { XMFLOAT3(1, 0, 0), XMFLOAT2(1, 1) }; // 右上
			v[2] = { XMFLOAT3(0, 1, 0), XMFLOAT2(0, 0) }; // 左下
			v[3] = { XMFLOAT3(1, 1, 0), XMFLOAT2(1, 0) }; // 右下
			s_context->Unmap(s_vertexBuffer.Get(), 0);
		}

		// テクスチャセット
		s_context->PSSetShaderResources(0, 1, texture->srv.GetAddressOf());

		// 定数バッファ更新
		s_cbData.world = XMMatrixTranspose(worldMatrix);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		// 描画
		s_context->Draw(4, 0);
	}

}	// namespace Arche