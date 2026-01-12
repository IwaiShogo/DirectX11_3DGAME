#include "Engine/pch.h"
#include "MeshBuffer.h"
#include "Engine/Core/Application.h" // Device取得用

namespace Arche
{

	using namespace Arche; // Applicationクラスのある名前空間

	MeshBuffer::MeshBuffer() : m_desc{} {}
	MeshBuffer::~MeshBuffer() {}

	HRESULT MeshBuffer::Create(const Description& desc)
	{
		HRESULT hr = E_FAIL;

		// 頂点バッファ作成
		hr = CreateVertexBuffer(desc.pVtx, desc.vtxSize, desc.vtxCount, desc.isWrite);
		if (FAILED(hr)) return hr;

		// インデックスバッファ作成
		if (desc.pIdx) {
			hr = CreateIndexBuffer(desc.pIdx, desc.idxSize, desc.idxCount);
			if (FAILED(hr)) return hr;
		}

		m_desc = desc;
		// ポインタメンバは保持しない（参照先が消える可能性があるため）
		m_desc.pVtx = nullptr;
		m_desc.pIdx = nullptr;

		return S_OK;
	}

	void MeshBuffer::Draw()
	{
		auto context = Application::Instance().GetContext();

		UINT stride = m_desc.vtxSize;
		UINT offset = 0;

		context->IASetPrimitiveTopology(m_desc.topology);
		context->IASetVertexBuffers(0, 1, m_pVtxBuffer.GetAddressOf(), &stride, &offset);

		if (m_desc.idxCount > 0)
		{
			DXGI_FORMAT format = (m_desc.idxSize == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
			context->IASetIndexBuffer(m_pIdxBuffer.Get(), format, 0);
			context->DrawIndexed(m_desc.idxCount, 0, 0);
		}
		else
		{
			context->Draw(m_desc.vtxCount, 0);
		}
	}

	HRESULT MeshBuffer::Write(void* pVtx)
	{
		if (!m_desc.isWrite) return E_FAIL;

		auto context = Application::Instance().GetContext();
		D3D11_MAPPED_SUBRESOURCE mapResource;

		HRESULT hr = context->Map(m_pVtxBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
		if (SUCCEEDED(hr))
		{
			memcpy(mapResource.pData, pVtx, m_desc.vtxCount * m_desc.vtxSize);
			context->Unmap(m_pVtxBuffer.Get(), 0);
		}
		return hr;
	}

	HRESULT MeshBuffer::CreateVertexBuffer(const void* pVtx, UINT size, UINT count, bool isWrite)
	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.ByteWidth = size * count;
		bufDesc.Usage = isWrite ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.CPUAccessFlags = isWrite ? D3D11_CPU_ACCESS_WRITE : 0;

		D3D11_SUBRESOURCE_DATA subResource = {};
		subResource.pSysMem = pVtx;

		return Application::Instance().GetDevice()->CreateBuffer(&bufDesc, &subResource, &m_pVtxBuffer);
	}

	HRESULT MeshBuffer::CreateIndexBuffer(const void* pIdx, UINT size, UINT count)
	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.ByteWidth = size * count;
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA subResource = {};
		subResource.pSysMem = pIdx;

		return Application::Instance().GetDevice()->CreateBuffer(&bufDesc, &subResource, &m_pIdxBuffer);
	}
}