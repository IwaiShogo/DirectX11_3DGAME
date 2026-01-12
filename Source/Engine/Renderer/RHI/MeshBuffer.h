
#ifndef ___MESH_BUFFER_H___
#define ___MESH_BUFFER_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{

	class MeshBuffer
	{
	public:
		// 頂点データの定義（アニメーション対応）
		struct Vertex {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT3 normal;
			DirectX::XMFLOAT2 uv;
			DirectX::XMFLOAT4 color;
			DirectX::XMFLOAT4 weight;	// ボーンウェイト
			uint32_t index[4];			// ボーンインデックス (uint4として扱う)
		};

		struct Description {
			void* pVtx;			// 頂点データへのポインタ
			UINT vtxSize;		// 1頂点のサイズ (sizeof(Vertex))
			UINT vtxCount;		// 頂点数
			void* pIdx;			// インデックスデータへのポインタ (オプション)
			UINT idxSize;		// インデックスのサイズ (2 or 4)
			UINT idxCount;		// インデックス数
			D3D11_PRIMITIVE_TOPOLOGY topology;
			bool isWrite;		// CPU書き込みを行うか (Dynamic)
		};

	public:
		MeshBuffer();
		~MeshBuffer();

		// 作成
		HRESULT Create(const Description& desc);

		// 描画
		void Draw();

		// CPUからの書き込み (Dynamicの場合のみ)
		HRESULT Write(void* pVtx);

	private:
		HRESULT CreateVertexBuffer(const void* pVtx, UINT size, UINT count, bool isWrite);
		HRESULT CreateIndexBuffer(const void* pIdx, UINT size, UINT count);

	private:
		ComPtr<ID3D11Buffer> m_pVtxBuffer;
		ComPtr<ID3D11Buffer> m_pIdxBuffer;
		Description m_desc;
	};

}	// namespace Arche

#endif // !___MESH_BUFFER_H___