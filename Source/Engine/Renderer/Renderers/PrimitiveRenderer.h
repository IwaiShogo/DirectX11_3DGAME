/*****************************************************************//**
 * @file	PrimitiveRenderer.h
 * @brief	プリミティブ形状を描画するためのレンダラー
 * * @details
 * 箱、球、円柱、カプセル、四角錐、円錐、トーラスなどの基本形状を描画します。
 * ワイヤーフレーム表示やトランスフォーム行列による直接描画もサポートします。
 * * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
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

		// =================================================================
		//	基本描画関数 (行列指定・ワイヤーフレーム対応)
		// =================================================================
		static void DrawBox(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawSphere(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCylinder(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCapsule(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawPyramid(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCone(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawTorus(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);
		static void DrawDiamond(const XMMATRIX& world, const XMFLOAT4& color, bool wireframe = false);

		// =================================================================
		//	便利関数オーバーロード (従来のパラメータ指定形式)
		//	※内部で行列を計算して上記の基本関数を呼び出します
		// =================================================================
		static void DrawBox(const XMFLOAT3& position, const XMFLOAT3& size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawSphere(const XMFLOAT3& position, float radius, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCylinder(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCapsule(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawPyramid(const XMFLOAT3& position, const XMFLOAT3& size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawCone(const XMFLOAT3& position, float radius, float height, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawTorus(const XMFLOAT3& position, float majorRadius, float minorRadius, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);
		static void DrawDiamond(const XMFLOAT3& position, float size, const XMFLOAT4& rotation, const XMFLOAT4& color, bool wireframe = false);

		// =================================================================
		//	ユーティリティ
		// =================================================================
		// ライン描画
		static void DrawLine(const XMFLOAT3& p1, const XMFLOAT3& p2, const XMFLOAT4& color);
		// ギズモ用の矢印描画
		static void DrawArrow(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT4& color);
		// 描画モード変更
		static void SetFillMode(bool wireframe);
		// 座標軸を描画
		static void DrawAxis(float length = 5.0f);

		static ID3D11DeviceContext* GetDeviceContext() { return s_context; }

	private:
		// メッシュ生成ヘルパー
		static void CreateBoxMesh();
		static void CreateSphereMesh();
		static void CreateCylinderMesh();
		static void CreateCapsuleMesh();
		static void CreatePyramidMesh();
		static void CreateConeMesh();
		static void CreateTorusMesh();
		static void CreateDiamondMesh();

		// 円を描くヘルパー
		static void DrawCircle(const XMFLOAT3& center, float radius, const XMFLOAT4& color);

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

		// --- 各形状のバッファ ---
		// 球体
		static ComPtr<ID3D11Buffer>		s_sphereVB;
		static ComPtr<ID3D11Buffer>		s_sphereIB;
		static UINT						s_sphereIndexCount;
		// 円柱
		static ComPtr<ID3D11Buffer>		s_cylinderVB;
		static ComPtr<ID3D11Buffer>		s_cylinderIB;
		static UINT						s_cylinderIndexCount;
		// カプセル
		static ComPtr<ID3D11Buffer>		s_capsuleVB;
		static ComPtr<ID3D11Buffer>		s_capsuleIB;
		static UINT						s_capsuleIndexCount;
		// Pyramid
		static ComPtr<ID3D11Buffer>		s_pyramidVB;
		static ComPtr<ID3D11Buffer>		s_pyramidIB;
		static uint32_t					s_pyramidIndexCount;
		// Cone
		static ComPtr<ID3D11Buffer>		s_coneVB;
		static ComPtr<ID3D11Buffer>		s_coneIB;
		static uint32_t					s_coneIndexCount;
		// Torus
		static ComPtr<ID3D11Buffer>		s_torusVB;
		static ComPtr<ID3D11Buffer>		s_torusIB;
		static uint32_t					s_torusIndexCount;
		//Diamond
		static ComPtr<ID3D11Buffer>		s_diamondVB;
		static ComPtr<ID3D11Buffer>		s_diamondIB;
		static uint32_t					s_diamondIndexCount;

		static ConstantBufferData		s_cbData;

		static ComPtr<ID3D11RasterizerState>	s_rsWireframe;
		static ComPtr<ID3D11RasterizerState>	s_rsSolid;
	};

}	// namespace Arche

#endif // !___PRIMITIVE_RENDERER_H___