/*****************************************************************//**
 * @file	TextRenderer.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Config.h"

namespace Arche
{
	// 静的メンバ定義
	ID3D11Device* TextRenderer::s_device = nullptr;
	ID3D11DeviceContext* TextRenderer::s_context = nullptr;
	ComPtr<ID2D1Factory> TextRenderer::s_d2dFactory;
	ComPtr<ID2D1SolidColorBrush> TextRenderer::s_brush;
	std::unordered_map<ID3D11RenderTargetView*, ComPtr<ID2D1RenderTarget>> TextRenderer::s_d2dTargets;

	void TextRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		s_device = device;
		s_context = context;

		// FontManager初期化
		FontManager::Instance().Initialize();

		// D2Dファクトリの取得
		s_d2dFactory = FontManager::Instance().GetD2DFactory();
	}

	void TextRenderer::Shutdown()
	{
		s_d2dTargets.clear();
		s_brush.Reset();
		s_d2dFactory.Reset();

		s_device = nullptr;
		s_context = nullptr;
	}

	void TextRenderer::ClearCache()
	{
		// ターゲットとブラシを破棄
		s_d2dTargets.clear();
		s_brush.Reset();
	}

	void TextRenderer::Draw(Registry& registry, const XMMATRIX& view, const XMMATRIX& projection, ID3D11RenderTargetView* rtv)
	{
		// RTVが指定されていない場合、現在バインドされている物を取得
		ComPtr<ID3D11RenderTargetView> currentRTV;
		if (!rtv)
		{
			ComPtr<ID3D11DepthStencilView> dsv;
			s_context->OMGetRenderTargets(1, currentRTV.GetAddressOf(), dsv.GetAddressOf());
			rtv = currentRTV.Get();
		}

		if (!rtv) return;

		// D2Dレンダーターゲット取得
		ID2D1RenderTarget* d2dRT = GetD2DRenderTarget(rtv);
		if (!d2dRT) return;

		// --------------------------------------------------------
		// 描画開始
		// --------------------------------------------------------
		d2dRT->BeginDraw();

		// ブラシ作成（まだ無ければ）
		if (!s_brush) {
			d2dRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), s_brush.GetAddressOf());
		}

		// ビューポートサイズ取得
		D2D1_SIZE_F rtSize = d2dRT->GetSize();

		// 2D UI用のスケーリング比率（Config::SCREEN_WIDTH基準）
		float ratioX = rtSize.width / (float)Config::SCREEN_WIDTH;
		float ratioY = rtSize.height / (float)Config::SCREEN_HEIGHT;

		// 3D投影用のビューポート定義
		D3D11_VIEWPORT vp;
		vp.Width = rtSize.width;
		vp.Height = rtSize.height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		// --------------------------------------------------------
		// ECSループ
		// --------------------------------------------------------
		registry.view<TextComponent>().each([&](Entity e, TextComponent& text)
			{
				if (text.text.empty() || text.color.w <= 0.0f) return;

				D2D1::Matrix3x2F finalMat = D2D1::Matrix3x2F::Identity();
				bool shouldDraw = true;

				// =================================================================
				// パターンA: 2D UI (Transform2D)
				// =================================================================
				if (registry.has<Transform2D>(e))
				{
					auto& t2d = registry.get<Transform2D>(e);
					// UISystemで計算済みの行列を取得 (Y-Up, Center Origin)
					D2D1::Matrix3x2F worldMat = D2D1::Matrix3x2F(
						t2d.worldMatrix._11, t2d.worldMatrix._12,
						t2d.worldMatrix._21, t2d.worldMatrix._22,
						t2d.worldMatrix._31, t2d.worldMatrix._32
					);

					// オフセット加算 (Y-UpなのでYはそのまま加算)
					worldMat = worldMat * D2D1::Matrix3x2F::Translation(text.offset.x, text.offset.y);

					// 座標系変換: UISystem(Center, Y-Up) -> Direct2D(TopLeft, Y-Down)
					float screenHalfW = Config::SCREEN_WIDTH * 0.5f;
					float screenHalfH = Config::SCREEN_HEIGHT * 0.5f;

					// 1. テキスト自体の反転 (全体座標系反転による文字の裏返り防止)
					D2D1::Matrix3x2F textFlip = D2D1::Matrix3x2F::Scale(1.0f, -1.0f);

					// 2. 座標系の変換 (Y軸反転 + 中心移動)
					D2D1::Matrix3x2F screenTransform = D2D1::Matrix3x2F::Scale(1.0f, -1.0f) * D2D1::Matrix3x2F::Translation(screenHalfW, screenHalfH);

					// 3. ビューポート拡縮
					D2D1::Matrix3x2F scaleToViewport = D2D1::Matrix3x2F::Scale(ratioX, ratioY);

					// 行列合成: [TextFlip] -> [World] -> [CoordConv] -> [ViewportScale]
					finalMat = textFlip * worldMat * screenTransform * scaleToViewport;
				}
				// =================================================================
				// パターンB: 3D Billboard (Transform)
				// =================================================================
				else if (registry.has<Transform>(e))
				{
					auto& t3d = registry.get<Transform>(e);
					XMVECTOR worldPos = XMLoadFloat3(&t3d.position);

					// 必要なら3D空間オフセットを加算 (text.offsetを3Dオフセットとして扱う場合)
					// worldPos = XMVectorAdd(worldPos, XMVectorSet(text.offset.x, text.offset.y, 0, 0));

					// 3D座標 -> スクリーン座標 への変換
					// vp.Width/Height が実際のRTサイズなので、戻り値は実際のピクセル座標
					XMVECTOR screenPos = XMVector3Project(worldPos, 0, 0, vp.Width, vp.Height, 0, 1, projection, view, XMMatrixIdentity());

					// カメラの後ろにある場合は描画しない
					float depth = XMVectorGetZ(screenPos);
					if (depth < 0.0f || depth > 1.0f)
					{
						shouldDraw = false;
					}
					else
					{
						// スクリーン座標を取得 (Direct2Dと同じ左上原点)
						float x = XMVectorGetX(screenPos);
						float y = XMVectorGetY(screenPos);

						// 配置 (2Dオフセットがある場合はピクセル単位でずらす)
						// ※3D投影は既に実サイズなので scaleToViewport は掛けない
						finalMat = D2D1::Matrix3x2F::Translation(x + text.offset.x, y + text.offset.y);
					}
				}
				// =================================================================
				// パターンC: Transformなし (固定配置)
				// =================================================================
				else
				{
					D2D1::Matrix3x2F scaleToViewport = D2D1::Matrix3x2F::Scale(ratioX, ratioY);
					finalMat = D2D1::Matrix3x2F::Translation(text.offset.x, text.offset.y) * scaleToViewport;
				}

				if (!shouldDraw) return;

				// --- 描画実行 ---
				d2dRT->SetTransform(finalMat);

				// ブラシ色設定
				s_brush->SetColor(D2D1::ColorF(text.color.x, text.color.y, text.color.z, text.color.w));

				// テキストフォーマット取得
				std::wstring wFontName = std::wstring(text.fontKey.begin(), text.fontKey.end());
				if (wFontName.empty()) wFontName = L"Meiryo";

				DWRITE_FONT_WEIGHT weight = text.isBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
				DWRITE_FONT_STYLE style = text.isItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

				auto format = FontManager::Instance().GetTextFormat(
					StringId(text.fontKey),
					wFontName,
					text.fontSize,
					weight,
					style
				);

				if (format)
				{
					// 文字列変換
					int size_needed = MultiByteToWideChar(CP_UTF8, 0, text.text.c_str(), (int)text.text.size(), NULL, 0);
					std::wstring wText(size_needed, 0);
					MultiByteToWideChar(CP_UTF8, 0, text.text.c_str(), (int)text.text.size(), &wText[0], size_needed);
					if (!wText.empty() && wText.back() == L'\0') wText.pop_back();

					// レイアウト枠
					float layoutW = rtSize.width * 2.0f; // 十分な広さを確保
					float layoutH = rtSize.height * 2.0f;
					D2D1_RECT_F layoutRect;

					if (text.centerAlign)
					{
						layoutRect = D2D1::RectF(-layoutW * 0.5f, -layoutH * 0.5f, layoutW * 0.5f, layoutH * 0.5f);
						format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
						format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
					}
					else
					{
						layoutRect = D2D1::RectF(0.0f, 0.0f, layoutW, layoutH);
						format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
						format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
					}

					d2dRT->DrawText(
						wText.c_str(),
						(UINT32)wText.length(),
						format.Get(),
						layoutRect,
						s_brush.Get()
					);
				}
			});

		// 変換行列をリセット
		d2dRT->SetTransform(D2D1::Matrix3x2F::Identity());
		d2dRT->EndDraw();
	}

	ID2D1RenderTarget* TextRenderer::GetD2DRenderTarget(ID3D11RenderTargetView* rtv)
	{
		if (s_d2dTargets.find(rtv) != s_d2dTargets.end()) {
			return s_d2dTargets[rtv].Get();
		}

		ComPtr<ID3D11Resource> resource;
		rtv->GetResource(&resource);
		if (!resource) return nullptr;

		ComPtr<IDXGISurface> surface;
		resource.As(&surface);
		if (!surface) return nullptr;

		D2D1_RENDER_TARGET_PROPERTIES props =
			D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
				96.0f, 96.0f
			);

		ComPtr<ID2D1RenderTarget> target;
		HRESULT hr = FontManager::Instance().GetD2DFactory()->CreateDxgiSurfaceRenderTarget(
			surface.Get(),
			&props,
			&target
		);

		if (SUCCEEDED(hr))
		{
			target->SetDpi(96.0f, 96.0f);
			s_d2dTargets[rtv] = target;
			return target.Get();
		}
		return nullptr;
	}
}