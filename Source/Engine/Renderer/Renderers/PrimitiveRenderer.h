/*****************************************************************//**
 * @file	PrimitiveRenderer.h
 * @brief	箱を描画するための簡易クラス
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/23	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___PRIMITIVE_RENDERER_H___
#define ___PRIMITIVE_RENDERER_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{

	class ARCHE_API PrimitiveRenderer
	{
	public:
		/**
		 * @brief	初期化
		 * @param	device	デバイス
		 * @param	context	コンテキスト
		 */
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

		static void Shutdown();

		/**
		 * @brief	描画開始（カメラ行列をリセット）
		 * @param	view		ビュー行列
		 * @param	projection	プロジェクション行列
		 */
		static void Begin(const XMMATRIX& view, const XMMATRIX& projection);

		// ボックス描画
		static void DrawBox(const XMFLOAT3& position, const XMFLOAT3& size, const XMFLOAT4& rotation, const XMFLOAT4& color);
		// 球体描画
		static void DrawSphere(const XMFLOAT3& position, float radius, const XMFLOAT4& color);
		// カプセル描画
		static void DrawCapsule(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color);
		// 円柱描画
		static void DrawCylinder(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color);
		// ライン描画
		static void DrawLine(const XMFLOAT3& p1, const XMFLOAT3& p2, const XMFLOAT4& color);

		// ギズモ用の矢印描画
		static void DrawArrow(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT4& color);

		// 描画モード変更
		static void SetFillMode(bool wireframe);
		// グリッドと軸を描画
		//static void DrawGrid(float spacing = 1.0f, int lines = 10);
		static void DrawAxis(float length = 5.0f);

		static ID3D11DeviceContext* GetDeviceContext() { return s_context; }

	private:
		// 円を描くヘルパー
		static void DrawCircle(const XMFLOAT3& center, float radius, const XMFLOAT4& color);
		// メッシュ用ヘルパー
		static void CreateCylinderMesh();
		static void CreateCapsuleMesh();

	private:
		struct ConstantBufferData
		{
			XMMATRIX world;
			XMMATRIX view;
			XMMATRIX projection;
			XMFLOAT4 color;
		};

		// 静的メンバ変数
		static ID3D11Device* s_device;
		static ID3D11DeviceContext* s_context;

		static ComPtr<ID3D11VertexShader>	s_vs;
		static ComPtr<ID3D11PixelShader>	s_ps;
		static ComPtr<ID3D11InputLayout>	s_inputLayout;
		static ComPtr<ID3D11Buffer>			s_vertexBuffer;
		static ComPtr<ID3D11Buffer>			s_indexBuffer;
		static ComPtr<ID3D11Buffer>			s_constantBuffer;
		static ComPtr<ID3D11Buffer>			s_lineVertexBuffer;
		static ComPtr<ID3D11DepthStencilState> s_depthState;
		// 球体用のバッファ
		static ComPtr<ID3D11Buffer>		s_sphereVB;
		static ComPtr<ID3D11Buffer>		s_sphereIB;
		static UINT						s_sphereIndexCount;
		// 円柱用バッファ
		static ComPtr<ID3D11Buffer> s_cylinderVB;
		static ComPtr<ID3D11Buffer> s_cylinderIB;
		static UINT s_cylinderIndexCount;
		// カプセル用バッファ
		static ComPtr<ID3D11Buffer> s_capsuleVB;
		static ComPtr<ID3D11Buffer> s_capsuleIB;
		static UINT s_capsuleIndexCount;

		static ConstantBufferData	s_cbData;

		static ComPtr<ID3D11RasterizerState>	s_rsWireframe;
		static ComPtr<ID3D11RasterizerState>	s_rsSolid;
	};

}	// namespace Arche

#endif // !___PRIMITIVE_RENDERER_H___