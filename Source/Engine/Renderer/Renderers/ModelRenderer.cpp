/*****************************************************************//**
 * @file	ModelRenderer.cpp
 * @brief	モデルを描画するクラス
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/26	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "ModelRenderer.h"
#include "Engine/Renderer/RHI/MeshBuffer.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	ID3D11Device* ModelRenderer::s_device = nullptr;
	ID3D11DeviceContext* ModelRenderer::s_context = nullptr;
	ComPtr<ID3D11VertexShader> ModelRenderer::s_vs = nullptr;
	ComPtr<ID3D11PixelShader> ModelRenderer::s_ps = nullptr;
	ComPtr<ID3D11InputLayout> ModelRenderer::s_inputLayout = nullptr;
	ComPtr<ID3D11Buffer> ModelRenderer::s_constantBuffer = nullptr;
	ComPtr<ID3D11Buffer> ModelRenderer::s_lightConstantBuffer = nullptr;
	ComPtr<ID3D11SamplerState> ModelRenderer::s_samplerState = nullptr;
	ComPtr<ID3D11RasterizerState> ModelRenderer::s_rsSolid = nullptr;
	ComPtr<ID3D11ShaderResourceView> ModelRenderer::s_whiteTexture = nullptr;
	ModelRenderer::CBData ModelRenderer::s_cbData = {};
	ModelRenderer::SceneLightCBData ModelRenderer::s_lightData = {};
	ComPtr<ID3D11ShaderResourceView> ModelRenderer::s_shadowSRV = nullptr;
	ComPtr<ID3D11SamplerState> ModelRenderer::s_shadowSampler = nullptr;
	XMMATRIX ModelRenderer::s_lightView = XMMatrixIdentity();
	XMMATRIX ModelRenderer::s_lightProj = XMMatrixIdentity();

	void ModelRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		s_device = device;
		s_context = context;

		// --- シェーダーパスの定義 ---
		// ワイド文字列リテラルで定義
		const std::wstring vsPath = L"Resources/Engine/Shaders/Standard.hlsl";
		const std::wstring psPath = L"Resources/Engine/Shaders/Standard.hlsl";

		// --- ファイル存在チェック (デバッグ用) ---
		// 現在のカレントディレクトリとファイルの有無を確認
		if (!std::filesystem::exists(vsPath))
		{
			std::string currentDir = std::filesystem::current_path().string();
			std::string errorMsg = "Shader File NOT FOUND!\nPath: Resources/Engine/Shaders/Standard.hlsl\nCurrent Directory: " + currentDir;

			// 致命的エラーとしてログ出力＆メッセージボックス
			Logger::LogError(errorMsg);
			MessageBoxA(nullptr, errorMsg.c_str(), "Shader Error", MB_ICONERROR | MB_OK);
			return; // 初期化中断
		}

		// 1. シェーダーコンパイル
		ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		flags |= D3DCOMPILE_DEBUG;
#endif

		// VS
		HRESULT hr = D3DCompileFromFile(vsPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", flags, 0, &vsBlob, &errorBlob);
		if (FAILED(hr)) {
			std::string errorMsg = "Failed to compile VS.\n";
			if (errorBlob) errorMsg += (char*)errorBlob->GetBufferPointer();
			Logger::LogError(errorMsg);
			OutputDebugStringA(errorMsg.c_str());
			return;
		}
		s_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);

		// PS
		hr = D3DCompileFromFile(psPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", flags, 0, &psBlob, &errorBlob);
		if (FAILED(hr)) {
			std::string errorMsg = "Failed to compile PS.\n";
			if (errorBlob) errorMsg += (char*)errorBlob->GetBufferPointer();
			Logger::LogError(errorMsg);
			OutputDebugStringA(errorMsg.c_str());
			return;
		}
		s_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ps);

		// 2. 入力レイアウト
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	 0, 0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",	  0, DXGI_FORMAT_R32G32B32_FLOAT,	 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WEIGHT",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "INDEX",	  0, DXGI_FORMAT_R32G32B32A32_UINT,	 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		s_device->CreateInputLayout(layout, 6, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout);

		// 3. 定数バッファ
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(CBData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		s_device->CreateBuffer(&bd, nullptr, &s_constantBuffer);

		bd.ByteWidth = sizeof(SceneLightCBData);
		s_device->CreateBuffer(&bd, nullptr, &s_lightConstantBuffer);

		// 4. サンプラー & ラスタライザ
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		s_device->CreateSamplerState(&sd, &s_samplerState);

		// 影用サンプラー
		D3D11_SAMPLER_DESC shadowSd = {};
		shadowSd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // 比較フィルタ
		shadowSd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER; // 範囲外は影にしない（白）
		shadowSd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowSd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowSd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 深度比較
		shadowSd.BorderColor[0] = 1.0f;
		shadowSd.BorderColor[1] = 1.0f;
		shadowSd.BorderColor[2] = 1.0f;
		shadowSd.BorderColor[3] = 1.0f;
		s_device->CreateSamplerState(&shadowSd, &s_shadowSampler);

		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_BACK;
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = TRUE;
		rd.MultisampleEnable = TRUE;
		// 左手系・右手系の違いでカリングが逆になる場合があるため、表示がおかしい場合はここを CULL_NONE にして確認してください
		// rd.CullMode = D3D11_CULL_NONE; 
		s_device->CreateRasterizerState(&rd, &s_rsSolid);

		CreateWhiteTexture();
	}

	void ModelRenderer::Shutdown()
	{
		s_vs.Reset(); s_ps.Reset(); s_inputLayout.Reset();
		s_constantBuffer.Reset(); s_samplerState.Reset(); s_rsSolid.Reset();
		s_whiteTexture.Reset();
		s_lightConstantBuffer.Reset();
		s_shadowSRV.Reset();
		s_shadowSampler.Reset();
	}

	void ModelRenderer::Begin(const XMMATRIX& view, const XMMATRIX& projection, const XMFLOAT3& lightDir, const XMFLOAT3& lightColor)
	{
		s_context->IASetInputLayout(s_inputLayout.Get());
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->RSSetState(s_rsSolid.Get());

		s_context->VSSetShader(s_vs.Get(), nullptr, 0);
		s_context->PSSetShader(s_ps.Get(), nullptr, 0);

		s_context->VSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetConstantBuffers(1, 1, s_lightConstantBuffer.GetAddressOf());
		s_context->PSSetSamplers(0, 1, s_samplerState.GetAddressOf());

		s_cbData.view = XMMatrixTranspose(view);
		s_cbData.projection = XMMatrixTranspose(projection);
		s_cbData.lightDir = XMFLOAT4(lightDir.x, lightDir.y, lightDir.z, 0);
		s_cbData.lightColor = XMFLOAT4(lightColor.x, lightColor.y, lightColor.z, 1);

		if (s_shadowSRV)
		{
			s_context->PSSetShaderResources(1, 1, s_shadowSRV.GetAddressOf());
			s_context->PSSetSamplers(1, 1, s_shadowSampler.GetAddressOf());
		}
		// 定数バッファへセット
		s_cbData.lightView = XMMatrixTranspose(s_lightView);
		s_cbData.lightProj = XMMatrixTranspose(s_lightProj);
	}

	void ModelRenderer::SetSceneLights(const XMFLOAT3& ambientColor, float ambientIntensity, const std::vector<PointLightData>& lights)
	{
		s_lightData.ambientColor = ambientColor;
		s_lightData.ambientIntensity = ambientIntensity;

		// ライト数を制限（最大32）
		int count = (int)lights.size();
		if (count > 32) count = 32;
		s_lightData.pointLightCount = count;

		// データをコピー
		for (int i = 0; i < count; ++i)
		{
			s_lightData.pointLights[i] = lights[i];
		}

		// 定数バッファ更新
		s_context->UpdateSubresource(s_lightConstantBuffer.Get(), 0, nullptr, &s_lightData, 0, 0);
	}

	void ModelRenderer::SetShadowMap(ID3D11ShaderResourceView* srv)
	{
		s_shadowSRV = srv;
	}

	void ModelRenderer::SetLightMatrix(const XMMATRIX& view, const XMMATRIX& proj)
	{
		s_lightView = view;
		s_lightProj = proj;
	}

	void ModelRenderer::Draw(std::shared_ptr<Model> model, const XMFLOAT3& pos, const XMFLOAT3& scale, const XMFLOAT3& rot)
	{
		if (!model) return;

		// ワールド行列作成
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX R = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
		XMMATRIX world = S * R * T;
		
		Draw(model, world);
	}

	void ModelRenderer::Draw(std::shared_ptr<Model> model, const DirectX::XMMATRIX& worldMatrix)
	{
		if (!model) return;

		// ワールド行列更新
		s_cbData.world = XMMatrixTranspose(worldMatrix);

		// メッシュごとの描画
		const auto& meshes = model->GetMeshes();
		const auto& nodes = model->GetNodes();

		for (const auto& mesh : meshes)
		{
			// --- ボーン行列の更新 ---
			if (!mesh.bones.empty()) {
				s_cbData.hasAnimation = 1;
				for (int i = 0; i < 200; ++i) s_cbData.boneTransforms[i] = XMMatrixIdentity();

				for (size_t b = 0; b < mesh.bones.size() && b < 200; ++b)
				{
					const auto& bone = mesh.bones[b];
					XMMATRIX m = bone.invOffset * nodes[bone.index].mat;
					s_cbData.boneTransforms[b] = XMMatrixTranspose(m);
				}
			}
			else {
				s_cbData.hasAnimation = 0;
			}

			// マテリアル設定
			ID3D11ShaderResourceView* srv = s_whiteTexture.Get();
			if (mesh.materialID >= 0) {
				const auto* mat = model->GetMaterial(mesh.materialID);
				if (mat && mat->pTexture && mat->pTexture->srv) {
					srv = mat->pTexture->srv.Get();
				}
				if (mat) s_cbData.materialColor = mat->diffuse;
				else s_cbData.materialColor = { 1,1,1,1 };
			}
			
			// 定数バッファ更新
			s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

			// テクスチャセット
			s_context->PSSetShaderResources(0, 1, &srv);

			// 描画
			if (mesh.pMesh) mesh.pMesh->Draw();
		}
	}

	void ModelRenderer::CreateWhiteTexture()
	{
		uint32_t pixel = 0xFFFFFFFF;
		D3D11_SUBRESOURCE_DATA initData = { &pixel, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = 1; desc.Height = 1; desc.MipLevels = 1; desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		ComPtr<ID3D11Texture2D> tex;
		s_device->CreateTexture2D(&desc, &initData, &tex);
		s_device->CreateShaderResourceView(tex.Get(), nullptr, &s_whiteTexture);
	}
}