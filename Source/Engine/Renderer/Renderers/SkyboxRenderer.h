/*****************************************************************//**
 * @file	SkyboxRenderer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SKYBOX_RENDERER_H___
#define ___SKYBOX_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/RHI/Texture.h"
#include "Engine/Scene/SceneEnvironment.h"

namespace Arche
{
	class SkyboxRenderer
	{
	public:
		void Initialize();
		void Render(const XMMATRIX& view, const XMMATRIX& proj, const SceneEnvironment& env);

		// スカイボックス用のテクスチャを設定 (.dds推奨)
		void SetTexture(std::shared_ptr<Texture> texture);

	private:
		struct VSConstantBuffer
		{
			XMMATRIX View;
			XMMATRIX Projection;
			XMFLOAT3 CameraPos;
			float UseTexture;

			// グラデーション色
			XMFLOAT4 ColorTop;
			XMFLOAT4 ColorHorizon;
			XMFLOAT4 ColorBottom;
		};

		ID3D11VertexShader* m_vs = nullptr;
		ID3D11PixelShader* m_ps = nullptr;
		ID3D11Buffer* m_cbVS = nullptr;

		ID3D11DepthStencilState* m_depthState = nullptr;
		ID3D11RasterizerState* m_rasterizerState = nullptr;
		ID3D11SamplerState* m_samplerState = nullptr;

		std::shared_ptr<Texture> m_skyboxTexture = nullptr;
	};
}

#endif // !___SKYBOX_RENDERER_H___