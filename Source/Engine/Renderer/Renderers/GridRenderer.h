/*****************************************************************//**
 * @file	GridRenderer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___GRID_RENDERER_H___
#define ___GRID_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/RHI/MeshBuffer.h"

namespace Arche
{
	class GridRenderer
	{
	public:
		void Initialize();
		void Render(const XMMATRIX& view, const XMMATRIX& proj, const XMFLOAT3& cameraPos, float farPlane);

	private:
		struct VSConstantBuffer
		{
			XMMATRIX View;
			XMMATRIX Projection;
			XMFLOAT3 CameraPos;
			float Near;
			float Far;
			XMFLOAT3 Padding;
		};

		struct GridConstantBuffer
		{
			XMMATRIX InverseViewProj;
		};

		ID3D11VertexShader* m_vs = nullptr;
		ID3D11PixelShader* m_ps = nullptr;
		ID3D11Buffer* m_cbVS = nullptr;
		ID3D11Buffer* m_cbGrid = nullptr;

		ID3D11BlendState* m_blendState = nullptr;
		ID3D11DepthStencilState* m_depthState = nullptr;
		ID3D11RasterizerState* m_rasterizerState = nullptr;
	};
}

#endif // !___GRID_RENDERER_H___