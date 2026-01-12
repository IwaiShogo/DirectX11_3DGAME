/*****************************************************************//**
 * @file	BillboardRenderer.h
 * @brief	3D空間に2D描画をする
 *
 * @details
 *
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___BILLBOARD_RENDERER_H___
#define ___BILLBOARD_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/RHI/Texture.h"

namespace Arche
{

	class ARCHE_API BillboardRenderer
	{
	public:
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

		static void Shutdown();

		static void Begin(const XMMATRIX& view, const XMMATRIX& projection);

		// 中心座標、幅、高さ、色を指定して描画
		static void Draw(Texture* texture, const XMFLOAT3& position, float width, float height, const XMFLOAT4& color = { 1,1,1,1 });

	private:
		static ID3D11DeviceContext* s_context;
		static ID3D11Device* s_device;

		static ComPtr<ID3D11VertexShader> s_vs;
		static ComPtr<ID3D11PixelShader> s_ps;
		static ComPtr<ID3D11InputLayout> s_inputLayout;
		static ComPtr<ID3D11Buffer> s_constantBuffer;
		static ComPtr<ID3D11Buffer> s_vertexBuffer; // 四角形の頂点用
		static ComPtr<ID3D11SamplerState> s_samplerState;
		static ComPtr<ID3D11RasterizerState> s_rsBillboard;
		static ComPtr<ID3D11BlendState> s_blendState; // 半透明用

		struct ConstantBufferData
		{
			XMMATRIX world;
			XMMATRIX view;
			XMMATRIX projection;
			XMFLOAT4 color;
		};
		static ConstantBufferData s_cbData;
	};

}	// namespace Arche

#endif // !___BILLBOARD_RENDERER_H___