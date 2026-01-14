/*****************************************************************//**
 * @file	PrimitiveRenderer.cpp
 * @brief	プリミティブ描画クラスの実装
 *********************************************************************/

 // ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Renderers/PrimitiveRenderer.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace Arche
{
	// 静的メンバ変数初期化
	ID3D11Device* PrimitiveRenderer::s_device = nullptr;
	ID3D11DeviceContext* PrimitiveRenderer::s_context = nullptr;

	ComPtr<ID3D11VertexShader>	PrimitiveRenderer::s_vs = nullptr;
	ComPtr<ID3D11PixelShader>	PrimitiveRenderer::s_ps = nullptr;
	ComPtr<ID3D11InputLayout>	PrimitiveRenderer::s_inputLayout = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_vertexBuffer = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_indexBuffer = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_constantBuffer = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_lineVertexBuffer = nullptr;
	ComPtr<ID3D11DepthStencilState> PrimitiveRenderer::s_depthState = nullptr;

	// 各形状のバッファ
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_sphereVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_sphereIB = nullptr;
	UINT						PrimitiveRenderer::s_sphereIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_cylinderVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_cylinderIB = nullptr;
	UINT						PrimitiveRenderer::s_cylinderIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_capsuleVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_capsuleIB = nullptr;
	UINT						PrimitiveRenderer::s_capsuleIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_pyramidVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_pyramidIB = nullptr;
	uint32_t					PrimitiveRenderer::s_pyramidIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_coneVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_coneIB = nullptr;
	uint32_t					PrimitiveRenderer::s_coneIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_torusVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_torusIB = nullptr;
	uint32_t					PrimitiveRenderer::s_torusIndexCount = 0;

	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_diamondVB = nullptr;
	ComPtr<ID3D11Buffer>		PrimitiveRenderer::s_diamondIB = nullptr;
	uint32_t					PrimitiveRenderer::s_diamondIndexCount = 0;

	PrimitiveRenderer::ConstantBufferData	PrimitiveRenderer::s_cbData = {};

	ComPtr<ID3D11RasterizerState>	PrimitiveRenderer::s_rsWireframe = nullptr;
	ComPtr<ID3D11RasterizerState>	PrimitiveRenderer::s_rsSolid = nullptr;

	static const float PI = 3.1415926535f;

	struct Vertex
	{
		XMFLOAT3 position;
	};

	void PrimitiveRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		s_device = device;
		s_context = context;

		// 1. シェーダーコンパイル
		ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;

		// Vertex Shader
		HRESULT hr = D3DCompileFromFile(L"Resources/Engine/Shaders/DebugPrimitive.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			throw std::runtime_error("Failed to compile VS");
		}
		s_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);

		// Pixel Shader
		hr = D3DCompileFromFile(L"Resources/Engine/Shaders/DebugPrimitive.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			throw std::runtime_error("Failed to compile PS");
		}
		s_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ps);

		// 2. 入力レイアウト
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		s_device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout);

		// 3. 各メッシュ生成
		CreateBoxMesh();
		CreateSphereMesh();
		CreateCylinderMesh();
		CreateCapsuleMesh();
		CreatePyramidMesh();
		CreateConeMesh();
		CreateTorusMesh();
		CreateDiamondMesh();

		// 5. 定数バッファ
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBufferData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		s_device->CreateBuffer(&bd, nullptr, &s_constantBuffer);

		// 線描画用の動的頂点バッファ
		bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * 2;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		s_device->CreateBuffer(&bd, nullptr, &s_lineVertexBuffer);

		// 深度ステンシルステート
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_LESS;
		s_device->CreateDepthStencilState(&dsd, &s_depthState);

		// ラスタライザステート
		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_NONE;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = TRUE;

		rd.FillMode = D3D11_FILL_WIREFRAME;
		s_device->CreateRasterizerState(&rd, &s_rsWireframe);

		rd.FillMode = D3D11_FILL_SOLID;
		s_device->CreateRasterizerState(&rd, &s_rsSolid);
	}

	void PrimitiveRenderer::Shutdown()
	{
		s_vs.Reset(); s_ps.Reset(); s_inputLayout.Reset();
		s_vertexBuffer.Reset(); s_indexBuffer.Reset();
		s_constantBuffer.Reset(); s_lineVertexBuffer.Reset();
		s_depthState.Reset();
		s_rsWireframe.Reset(); s_rsSolid.Reset();

		// メッシュ解放
		s_sphereVB.Reset(); s_sphereIB.Reset();
		s_cylinderVB.Reset(); s_cylinderIB.Reset();
		s_capsuleVB.Reset(); s_capsuleIB.Reset();
		s_pyramidVB.Reset(); s_pyramidIB.Reset();
		s_coneVB.Reset(); s_coneIB.Reset();
		s_torusVB.Reset(); s_torusIB.Reset();
		s_diamondVB.Reset(); s_diamondIB.Reset();

		s_device = nullptr;
		s_context = nullptr;
	}

	void PrimitiveRenderer::Begin(const XMMATRIX& view, const XMMATRIX& projection)
	{
		s_context->IASetInputLayout(s_inputLayout.Get());
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->OMSetDepthStencilState(s_depthState.Get(), 0);

		s_context->VSSetShader(s_vs.Get(), nullptr, 0);
		s_context->PSSetShader(s_ps.Get(), nullptr, 0);
		s_context->VSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());
		s_context->PSSetConstantBuffers(0, 1, s_constantBuffer.GetAddressOf());

		s_cbData.view = XMMatrixTranspose(view);
		s_cbData.projection = XMMatrixTranspose(projection);
	}

	void PrimitiveRenderer::SetFillMode(bool wireframe)
	{
		s_context->RSSetState(wireframe ? s_rsWireframe.Get() : s_rsSolid.Get());
	}

	// =================================================================
	//	基本描画関数 (XMMATRIX版)
	// =================================================================

	// Box
	void PrimitiveRenderer::DrawBox(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_vertexBuffer.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(36, 0, 0);

		if (wireframe) SetFillMode(false); // Restore
	}

	// Sphere
	void PrimitiveRenderer::DrawSphere(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_sphereVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_sphereIB.Get(), DXGI_FORMAT_R16_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_sphereIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// Cylinder
	void PrimitiveRenderer::DrawCylinder(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_cylinderVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_cylinderIB.Get(), DXGI_FORMAT_R16_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_cylinderIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// Capsule
	void PrimitiveRenderer::DrawCapsule(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_capsuleVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_capsuleIB.Get(), DXGI_FORMAT_R16_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_capsuleIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// Pyramid
	void PrimitiveRenderer::DrawPyramid(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_pyramidVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_pyramidIB.Get(), DXGI_FORMAT_R32_UINT, 0); // 32bit index for robust
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_pyramidIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// Cone
	void PrimitiveRenderer::DrawCone(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_coneVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_coneIB.Get(), DXGI_FORMAT_R32_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_coneIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// Torus
	void PrimitiveRenderer::DrawTorus(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);

		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_torusVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_torusIB.Get(), DXGI_FORMAT_R32_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_torusIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	void PrimitiveRenderer::DrawDiamond(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe)
	{
		SetFillMode(wireframe);
		s_cbData.world = XMMatrixTranspose(world);
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_diamondVB.GetAddressOf(), &stride, &offset);
		s_context->IASetIndexBuffer(s_diamondIB.Get(), DXGI_FORMAT_R32_UINT, 0);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		s_context->DrawIndexed(s_diamondIndexCount, 0, 0);

		if (wireframe) SetFillMode(false);
	}

	// =================================================================
	//	便利関数オーバーロード実装 (行列計算 -> 基本関数呼び出し)
	// =================================================================

	void PrimitiveRenderer::DrawBox(const XMFLOAT3& position, const XMFLOAT3& size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		XMMATRIX world = XMMatrixScaling(size.x, size.y, size.z) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawBox(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawSphere(const XMFLOAT3& position, float radius, const XMFLOAT4& color, bool wireframe)
	{
		float scale = radius * 2.0f; // 単位球(半径0.5)を使用しているため
		XMMATRIX world = XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawSphere(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawCylinder(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		// 半径はXY、高さはY軸
		float rScale = radius * 2.0f; // 単位円柱(半径0.5)想定なら
		// メッシュは半径1(直径2)ならradius倍、半径0.5(直径1)ならradius*2倍。
		// CreateCylinderMeshの実装を見ると半径1(cos/sin)なので直径2。
		// つまり scale = radius でOK。
		XMMATRIX world = XMMatrixScaling(radius, height, radius) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawCylinder(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawCapsule(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		// カプセルは構造が特殊なため、単純なスケーリングでは歪む可能性がありますが、
		// 今回は単一メッシュ描画に統合するため、スケール行列で近似します。
		// CreateCapsuleMeshは半径0.5、全高1.0で作られています。

		XMVECTOR q = XMLoadFloat4(&rotation);
		// X/Zは半径(0.5->radius なので radius*2倍)、Yは高さ(1.0->height なので height倍)
		XMMATRIX world = XMMatrixScaling(radius * 2.0f, height, radius * 2.0f) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawCapsule(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawPyramid(const XMFLOAT3& position, const XMFLOAT3& size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		XMMATRIX world = XMMatrixScaling(size.x, size.y, size.z) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawPyramid(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawCone(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		XMMATRIX world = XMMatrixScaling(radius, height, radius) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawCone(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawTorus(const XMFLOAT3& position, float majorRadius, float minorRadius, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		// CreateTorusMesh は固定サイズで作るため、スケーリングで半径を調整するのは難しい（太さも変わる）。
		// ここでは全体スケールとして扱うか、動的生成が必要だが、簡易的にMajorRadiusに合わせてスケールする。
		// 基準サイズが Major=1.0 と仮定。
		float scale = majorRadius;
		XMMATRIX world = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawTorus(world, color, wireframe);
	}

	void PrimitiveRenderer::DrawDiamond(const XMFLOAT3& position, float size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe)
	{
		XMVECTOR q = XMLoadFloat4(&rotation);
		XMMATRIX world = XMMatrixScaling(size, size, size) * XMMatrixRotationQuaternion(q) * XMMatrixTranslation(position.x, position.y, position.z);
		DrawDiamond(world, color, wireframe);
	}

	// =================================================================
	//	ライン・ユーティリティ
	// =================================================================

	void PrimitiveRenderer::DrawLine(const XMFLOAT3& p1, const XMFLOAT3& p2, const XMFLOAT4& color)
	{
		s_cbData.world = XMMatrixIdentity();
		s_cbData.color = color;
		s_context->UpdateSubresource(s_constantBuffer.Get(), 0, nullptr, &s_cbData, 0, 0);

		D3D11_MAPPED_SUBRESOURCE ms;
		if (SUCCEEDED(s_context->Map(s_lineVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		{
			Vertex* v = (Vertex*)ms.pData;
			v[0].position = p1;
			v[1].position = p2;
			s_context->Unmap(s_lineVertexBuffer.Get(), 0);
		}

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		s_context->IASetVertexBuffers(0, 1, s_lineVertexBuffer.GetAddressOf(), &stride, &offset);
		s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		s_context->Draw(2, 0);
	}

	void PrimitiveRenderer::DrawArrow(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT4& color)
	{
		DrawLine(start, end, color);
		float boxSize = 0.1f;
		// 簡易的に終点にボックス
		DrawBox(end, XMFLOAT3(boxSize, boxSize, boxSize), XMFLOAT4(0, 0, 0, 1), color, false);
	}

	void PrimitiveRenderer::DrawAxis(float length)
	{
		DrawLine(XMFLOAT3(0, 0, 0), XMFLOAT3(length, 0, 0), XMFLOAT4(1, 0, 0, 1));
		DrawLine(XMFLOAT3(0, 0, 0), XMFLOAT3(0, length, 0), XMFLOAT4(0, 1, 0, 1));
		DrawLine(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, length), XMFLOAT4(0, 0, 1, 1));
	}

	void PrimitiveRenderer::DrawCircle(const XMFLOAT3& center, float radius, const XMFLOAT4& color)
	{
		const int div = 16;
		const float dAngle = XM_2PI / div;
		for (int i = 0; i < div; ++i)
		{
			float a1 = i * dAngle;
			float a2 = (i + 1) * dAngle;
			XMFLOAT3 p1 = { center.x + cosf(a1) * radius, center.y, center.z + sinf(a1) * radius };
			XMFLOAT3 p2 = { center.x + cosf(a2) * radius, center.y, center.z + sinf(a2) * radius };
			DrawLine(p1, p2, color);
		}
	}

	// =================================================================
	//	メッシュ生成実装
	// =================================================================

	void PrimitiveRenderer::CreateBoxMesh()
	{
		float hs = 0.5f;
		Vertex vertices[] = {
			{ XMFLOAT3(-hs, -hs, -hs) }, { XMFLOAT3(-hs, +hs, -hs) },
			{ XMFLOAT3(+hs, +hs, -hs) }, { XMFLOAT3(+hs, -hs, -hs) },
			{ XMFLOAT3(-hs, -hs, +hs) }, { XMFLOAT3(-hs, +hs, +hs) },
			{ XMFLOAT3(+hs, +hs, +hs) }, { XMFLOAT3(+hs, -hs, +hs) },
		};
		// Box Vertex Buffer
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(Vertex) * 8;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = vertices;
		s_device->CreateBuffer(&bd, &initData, &s_vertexBuffer);

		uint16_t indices[] = {
			0, 1, 2, 0, 2, 3, // Front
			4, 6, 5, 4, 7, 6, // Back
			1, 5, 6, 1, 6, 2, // Top
			0, 3, 7, 0, 7, 4, // Bottom
			0, 4, 5, 0, 5, 1, // Left
			3, 2, 6, 3, 6, 7  // Right
		};
		// Box Index Buffer
		bd.ByteWidth = sizeof(indices);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		initData.pSysMem = indices;
		s_device->CreateBuffer(&bd, &initData, &s_indexBuffer);
	}

	void PrimitiveRenderer::CreateSphereMesh()
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		int div = 16;
		float radius = 0.5f;

		for (int y = 0; y <= div; ++y) {
			float v = (float)y / div;
			float pitch = (v - 0.5f) * XM_PI;
			for (int x = 0; x <= div; ++x) {
				float u = (float)x / div;
				float yaw = u * XM_2PI;
				vertices.push_back({ { radius * cos(pitch) * cos(yaw), radius * sin(pitch), radius * cos(pitch) * sin(yaw) } });
			}
		}
		for (int y = 0; y < div; ++y) {
			for (int x = 0; x < div; ++x) {
				int i0 = y * (div + 1) + x;
				int i1 = i0 + 1;
				int i2 = (y + 1) * (div + 1) + x;
				int i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i2); indices.push_back(i1); indices.push_back(i3);
			}
		}
		s_sphereIndexCount = (UINT)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_sphereVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint16_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_sphereIB);
	}

	void PrimitiveRenderer::CreateCylinderMesh()
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		int slices = 16;
		// Top Center
		vertices.push_back({ {0, 0.5f, 0} });
		for (int i = 0; i <= slices; ++i) {
			float theta = (float)i / slices * XM_2PI;
			vertices.push_back({ {cosf(theta), 0.5f, sinf(theta)} });
		}
		// Bottom Center
		int botCenter = (int)vertices.size();
		vertices.push_back({ {0, -0.5f, 0} });
		for (int i = 0; i <= slices; ++i) {
			float theta = (float)i / slices * XM_2PI;
			vertices.push_back({ {cosf(theta), -0.5f, sinf(theta)} });
		}
		// Indices
		for (int i = 1; i <= slices; ++i) {
			indices.push_back(0); indices.push_back(i + 1); indices.push_back(i);
		}
		int botStart = botCenter + 1;
		for (int i = 0; i < slices; ++i) {
			indices.push_back(botCenter); indices.push_back(botStart + i); indices.push_back(botStart + i + 1);
		}
		for (int i = 1; i <= slices; ++i) {
			int t1 = i, t2 = i + 1;
			int b1 = botStart + (i - 1), b2 = botStart + i;
			indices.push_back(t1); indices.push_back(b1); indices.push_back(t2);
			indices.push_back(t2); indices.push_back(b1); indices.push_back(b2);
		}
		s_cylinderIndexCount = (UINT)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_cylinderVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint16_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_cylinderIB);
	}

	void PrimitiveRenderer::CreateCapsuleMesh()
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		int slices = 16;
		int stacks = 16;
		float radius = 0.5f;
		float cylinderH = 0.5f;

		for (int j = 0; j <= stacks; ++j) {
			float v = (float)j / stacks;
			float phi = v * XM_PI;
			float y = -cosf(phi) * radius;
			if (j < stacks / 2) y -= cylinderH * 0.5f;
			else if (j > stacks / 2) y += cylinderH * 0.5f;

			float r = sinf(phi) * radius;
			for (int i = 0; i <= slices; ++i) {
				float theta = (float)i / slices * XM_2PI;
				vertices.push_back({ { r * cosf(theta), y, r * sinf(theta) } });
			}
		}
		int ringVerts = slices + 1;
		for (int j = 0; j < stacks; ++j) {
			for (int i = 0; i < slices; ++i) {
				int i0 = j * ringVerts + i;
				int i1 = i0 + 1;
				int i2 = (j + 1) * ringVerts + i;
				int i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
				indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
			}
		}
		s_capsuleIndexCount = (UINT)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_capsuleVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint16_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_capsuleIB);
	}

	void PrimitiveRenderer::CreatePyramidMesh()
	{
		// 底面(四角) + 4つの側面(三角)
		// フラットシェーディングにするため頂点は共有しない方が綺麗だが、簡易的に共有させる
		// 頂点: Top(0,0.5,0), Bottom(+-0.5, -0.5, +-0.5)
		std::vector<Vertex> vertices = {
			{{0, 0.5f, 0}}, // 0: Top
			{{-0.5f, -0.5f, -0.5f}}, // 1
			{{0.5f, -0.5f, -0.5f}},	 // 2
			{{0.5f, -0.5f, 0.5f}},	 // 3
			{{-0.5f, -0.5f, 0.5f}}	 // 4
		};
		// インデックス
		std::vector<uint32_t> indices = {
			0, 2, 1,  0, 3, 2,	0, 4, 3,  0, 1, 4, // Sides
			1, 2, 3,  1, 3, 4					   // Bottom
		};
		s_pyramidIndexCount = (uint32_t)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_pyramidVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint32_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_pyramidIB);
	}

	void PrimitiveRenderer::CreateConeMesh()
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		int slices = 16;
		float height = 1.0f;
		float radius = 0.5f;

		// 0: Top
		vertices.push_back({ {0, height / 2, 0} });
		// Base Vertices
		for (int i = 0; i < slices; ++i) {
			float theta = (float)i / slices * XM_2PI;
			vertices.push_back({ {radius * cosf(theta), -height / 2, radius * sinf(theta)} });
		}
		// Base Center
		vertices.push_back({ {0, -height / 2, 0} });
		int baseCenter = (int)vertices.size() - 1;

		// Sides
		for (int i = 0; i < slices; ++i) {
			int p1 = i + 1;
			int p2 = (i + 1) % slices + 1; // Wrap around (Note: index 1 to slices)
			// Adjust p2 index logic:
			// Indices are 1..slices. So if i=slices-1, p2 should be 1.
			if (i == slices - 1) p2 = 1;
			else p2 = i + 2;

			indices.push_back(0); indices.push_back(p2); indices.push_back(p1);
			// Bottom cap
			indices.push_back(baseCenter); indices.push_back(p1); indices.push_back(p2);
		}
		s_coneIndexCount = (uint32_t)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_coneVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint32_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_coneIB);
	}

	void PrimitiveRenderer::CreateTorusMesh()
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		int rowCount = 16;
		int colCount = 24;
		float tubeR = 0.2f;
		float mainR = 0.5f;

		for (int i = 0; i <= colCount; ++i) {
			float currColAngle = XM_2PI * i / colCount;
			float cosCol = cosf(currColAngle);
			float sinCol = sinf(currColAngle);

			for (int j = 0; j <= rowCount; ++j) {
				float currRowAngle = XM_2PI * j / rowCount;
				float cosRow = cosf(currRowAngle);
				float sinRow = sinf(currRowAngle);

				float x = (mainR + tubeR * cosRow) * cosCol;
				float y = tubeR * sinRow;
				float z = (mainR + tubeR * cosRow) * sinCol;
				vertices.push_back({ {x, y, z} });
			}
		}
		int vertsPerRow = rowCount + 1;
		for (int i = 0; i < colCount; ++i) {
			for (int j = 0; j < rowCount; ++j) {
				int i0 = i * vertsPerRow + j;
				int i1 = i0 + 1;
				int i2 = (i + 1) * vertsPerRow + j;
				int i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i2); indices.push_back(i1); indices.push_back(i3);
			}
		}
		s_torusIndexCount = (uint32_t)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_torusVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint32_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_torusIB);
	}

	void PrimitiveRenderer::CreateDiamondMesh()
	{
		// 正八面体 (6頂点, 8面)
	// 上下(Y)、前後(Z)、左右(X) の頂点
		float s = 0.5f; // 半径
		std::vector<Vertex> vertices = {
			{{ 0,  s,  0}}, // 0: Top
			{{ 0, -s,  0}}, // 1: Bottom
			{{-s,  0,  0}}, // 2: Left
			{{ s,  0,  0}}, // 3: Right
			{{ 0,  0,  s}}, // 4: Front
			{{ 0,  0, -s}}	// 5: Back
		};

		// 8つの三角形
		std::vector<uint32_t> indices = {
			0,4,3,	0,3,5,	0,5,2,	0,2,4, // 上半分
			1,3,4,	1,5,3,	1,2,5,	1,4,2  // 下半分
		};
		s_diamondIndexCount = (uint32_t)indices.size();

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
		D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
		s_device->CreateBuffer(&bd, &initData, &s_diamondVB);

		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = sizeof(uint32_t) * (UINT)indices.size();
		initData.pSysMem = indices.data();
		s_device->CreateBuffer(&bd, &initData, &s_diamondIB);
	}

}	// namespace Arche