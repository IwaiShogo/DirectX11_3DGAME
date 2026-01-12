/*****************************************************************//**
 * @file	SpriteRenderer.h
 * @brief	2D描画専用のレンダラークラス
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SPRITE_RENDERER_H___
#define ___SPRITE_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/RHI/Texture.h"

namespace Arche
{

	class ARCHE_API SpriteRenderer
	{
	public:
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, float screenW, float screenH);

		// 終了処理
		static void Shutdown();

		// 描画開始
		static void Begin();

		// サイズ指定版
		static void Draw(Texture* texture, const XMMATRIX& worldMatrix, const XMFLOAT4& color = { 1, 1, 1, 1 });

	private:
		static ID3D11Device*		s_device;
		static ID3D11DeviceContext*	s_context;
		static float s_screenWidth;
		static float s_screenHeight;

		static ComPtr<ID3D11VertexShader>		s_vs;
		static ComPtr<ID3D11PixelShader>		s_ps;
		static ComPtr<ID3D11InputLayout>		s_inputLayout;
		static ComPtr<ID3D11Buffer>				s_vertexBuffer;
		static ComPtr<ID3D11Buffer>				s_constantBuffer;
		static ComPtr<ID3D11RasterizerState>	s_rs2D;
		static ComPtr<ID3D11DepthStencilState>	s_ds2D;

		// 透過処理用
		static ComPtr<ID3D11BlendState>		s_blendState;
		// サンプラー（テクスチャの貼り方設定）
		static ComPtr<ID3D11SamplerState>	s_samplerState;

		struct ConstantBufferData {
			XMMATRIX world;
			XMMATRIX projection;
			XMFLOAT4 color;
		};
		static ConstantBufferData s_cbData;
	};

}

#endif // !___SPRITE_RENDERER_H___