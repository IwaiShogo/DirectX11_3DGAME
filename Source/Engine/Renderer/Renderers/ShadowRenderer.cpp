/*****************************************************************//**
 * @file	ShadowRenderer.cpp
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
#include "ShadowRenderer.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	ID3D11Device* ShadowRenderer::s_device = nullptr;
	ID3D11DeviceContext* ShadowRenderer::s_context = nullptr;
	ComPtr<ID3D11VertexShader> ShadowRenderer::s_vs = nullptr;
	ComPtr<ID3D11PixelShader> ShadowRenderer::s_ps = nullptr;
	ComPtr<ID3D11InputLayout> ShadowRenderer::s_inputLayout = nullptr;
	ComPtr<ID3D11Buffer> ShadowRenderer::s_constantBuffer = nullptr;
	ShadowRenderer::CBData ShadowRenderer::s_cbData = {};

	void ShadowRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		s_device = device;
		s_context = context;

		std::wstring shaderPath = L"Resources/Engine/Shaders/ShadowMap.hlsl";

		// コンパイル & シェーダー作成
		ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		flags |= D3DCOMPILE_DEBUG;
#endif
		// VS
		if (FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", flags, 0, &vsBlob, &errBlob))) {
			if (errBlob) Logger::LogError((char*)errBlob->GetBufferPointer());
			return;
		}
		s_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);

		// PS (空の関数)
		if (FAILED(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", flags, 0, &psBlob, &errBlob))) {
			if (errBlob) Logger::LogError((char*)errBlob->GetBufferPointer());
			return;
		}
		s_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ps);

		// Input Layout
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	 0, 0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",	  0, DXGI_FORMAT_R32G32B32_FLOAT,	 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WEIGHT",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "INDEX",	  0, DXGI_FORMAT_R32G32B32A32_UINT,	 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		s_device->CreateInputLayout(layout, 6, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout);

		// Constant Buffer
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(CBData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		s_device->CreateBuffer(&bd, nullptr, &s_constantBuffer);
	}

	void ShadowRenderer::Shutdown()
	{
		s_vs.Reset(); s_ps.Reset(); s_inputLayout.Reset(); s_constantBuffer.Reset();
	}

	void ShadowRenderer::Begin(const XMMATRIX& lightView, const XMMATRIX& lightProj)
	{
		s_context->IASetInputLayout(s_inputLayout.Get());
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		s_context->VSSetShader(s_vs.Get(), nullptr, 0);
		// 深度のみならPSはnullptrでも良いが、今回は空のPSを使用
		s_context->PSSetShader(s_ps.Get(), nullptr, 0);

		s_context->VSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());

		// 共通データのセット
		s_cbData.view = XMMatrixTranspose(lightView);
		s_cbData.projection = XMMatrixTranspose(lightProj);
	}

	void ShadowRenderer::Draw(std::shared_ptr<Model> model, const DirectX::XMMATRIX& worldMatrix)
	{
		if (!model) return;

		s_cbData.world = XMMatrixTranspose(worldMatrix);

		const auto& meshes = model->GetMeshes();
		const auto& nodes = model->GetNodes();

		for (const auto& mesh : meshes)
		{
			// アニメーション更新
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

			// バッファ更新 & 描画
			s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);
			if (mesh.pMesh) mesh.pMesh->Draw();
		}
	}
}