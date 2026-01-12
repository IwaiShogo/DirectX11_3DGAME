#include "Engine/pch.h"
#include "Graphics.h"
#include "Engine/Renderer/RHI/Texture.h"
#include "Engine/Resource/ResourceManager.h"

namespace Arche
{
	ID3D11Device* Graphics::s_device = nullptr;
	ID3D11DeviceContext* Graphics::s_context = nullptr;
	IDXGISwapChain* Graphics::s_swapChain = nullptr;
	ID3D11ShaderResourceView* Graphics::s_captureSource = nullptr;

	void Graphics::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain)
	{
		s_device = device;
		s_context = context;
		s_swapChain = swapChain;
		s_captureSource = nullptr;
	}

	void Graphics::Shutdown()
	{
		s_device = nullptr;
		s_context = nullptr;
		s_swapChain = nullptr;
		s_captureSource = nullptr;
	}

	void Graphics::SetCaptureSource(ID3D11ShaderResourceView* srv)
	{
		s_captureSource = srv;
	}

	void Graphics::CopyBackBufferToTexture(const std::string& resourceKey)
	{
		if (!s_device || !s_context) return;

		ID3D11Texture2D* srcTexture = nullptr;
		bool needReleaseSrc = false; // GetBufferなどで参照カウントが増えた場合のみReleaseするフラグ

		// ------------------------------------------------------------
		// A. 指定されたキャプチャソース（RenderTarget）がある場合
		// ------------------------------------------------------------
		if (s_captureSource)
		{
			ID3D11Resource* res = nullptr;
			s_captureSource->GetResource(&res); // 参照カウント増
			if (res)
			{
				// QueryInterfaceを使わずキャスト（作成元がTexture2Dである前提）
				// 安全に行うなら res->QueryInterface(...) ですが、SRVの元は基本Texture2Dです
				srcTexture = static_cast<ID3D11Texture2D*>(res);
				needReleaseSrc = true;
			}
		}
		// ------------------------------------------------------------
		// B. ない場合（Releaseビルドなど）、バックバッファを使用
		// ------------------------------------------------------------
		else if (s_swapChain)
		{
			HRESULT hr = s_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&srcTexture);
			if (SUCCEEDED(hr))
			{
				needReleaseSrc = true;
			}
		}

		// ソースが取得できなければ終了
		if (!srcTexture) return;

		// 1. ソース情報の取得（サイズ等）
		D3D11_TEXTURE2D_DESC srcDesc;
		srcTexture->GetDesc(&srcDesc);

		// 2. コピー先テクスチャの作成
		D3D11_TEXTURE2D_DESC copyDesc = srcDesc;
		copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // シェーダーリソースとしてバインド
		copyDesc.Usage = D3D11_USAGE_DEFAULT;
		copyDesc.CPUAccessFlags = 0;
		copyDesc.MiscFlags = 0; // RenderTargetなどは不要

		ID3D11Texture2D* copyTexture = nullptr;
		HRESULT hr = s_device->CreateTexture2D(&copyDesc, nullptr, &copyTexture);

		if (FAILED(hr))
		{
			if (needReleaseSrc) srcTexture->Release();
			return;
		}

		// 3. コピー実行 (GPU上での転送)
		s_context->CopyResource(copyTexture, srcTexture);

		// ソースはもう不要なので解放
		if (needReleaseSrc) srcTexture->Release();

		// 4. SRV (Shader Resource View) の作成
		ID3D11ShaderResourceView* srv = nullptr;
		hr = s_device->CreateShaderResourceView(copyTexture, nullptr, &srv);

		// SRV作成後はテクスチャ本体のポインタ参照を下げておく（SRVが内部で保持するため）
		copyTexture->Release();

		if (SUCCEEDED(hr))
		{
			// 5. Arche::Texture オブジェクトを作成して登録
			auto newTexture = std::make_shared<Texture>();

			newTexture->width = srcDesc.Width;
			newTexture->height = srcDesc.Height;
			newTexture->filepath = resourceKey; // キーを識別子として保持
			newTexture->srv.Attach(srv);		// ComPtrに所有権譲渡

			// ResourceManagerに登録（上書き）
			ResourceManager::Instance().AddResource(resourceKey, newTexture);
		}
	}
}