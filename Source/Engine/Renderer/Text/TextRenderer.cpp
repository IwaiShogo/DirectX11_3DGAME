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

	void TextRenderer::Draw(Registry& registry, ID3D11RenderTargetView* rtv)
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

		// ビューポートサイズ取得（解像度依存の配置のため）
		D2D1_SIZE_F rtSize = d2dRT->GetSize();

		// ビューポート比率計算
		float viewportWidth = rtSize.width;
		float viewportHeight = rtSize.height;

		float ratioX = viewportWidth / (float)Config::SCREEN_WIDTH;
		float ratioY = viewportHeight / (float)Config::SCREEN_HEIGHT;

		// ECSからTextComponentを持つエンティティを走査
		registry.view<TextComponent>().each([&](Entity e, TextComponent& text)
			{
				// Transform2Dの取得
				D2D1::Matrix3x2F worldMat = D2D1::Matrix3x2F::Identity();

				if (registry.has<Transform2D>(e))
				{
					auto& t2d = registry.get<Transform2D>(e);
					auto& m = t2d.worldMatrix;

					worldMat = D2D1::Matrix3x2F(m._11, m._12, m._21, m._22, m._31, m._32);
				}
				else
				{
					worldMat = D2D1::Matrix3x2F::Translation(text.offset.x, text.offset.y);
				}

				// 座標系変換
				float screenHalfW = Config::SCREEN_WIDTH * 0.5f;
				float screenHalfH = Config::SCREEN_HEIGHT * 0.5f;

				// 行列の平行移動成分を取り出し、変換
				float worldX = worldMat.dx;
				float worldY = worldMat.dy;

				float d2dX = worldX + screenHalfW;
				float d2dY = screenHalfH - worldY; // Y-Up -> Y-Down

				D2D1::Matrix3x2F screenTransform = D2D1::Matrix3x2F::Scale(1.0f, -1.0f, D2D1::Point2F(0, 0)) * D2D1::Matrix3x2F::Translation(screenHalfW, screenHalfH);
				D2D1::Matrix3x2F finalMat = worldMat * screenTransform;

				// 色設定
				s_brush->SetColor(D2D1::ColorF(text.color.x, text.color.y, text.color.z, text.color.w));

				// テキストフォーマット取得
				std::wstring wFontName = std::wstring(text.fontKey.begin(), text.fontKey.end());
				if (wFontName.empty()) wFontName = L"Meiryo"; // デフォルト

				DWRITE_FONT_WEIGHT weight = text.isBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
				DWRITE_FONT_STYLE style = text.isItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

				// フォント作成 (StringIdなど使っている場合は適宜修正)
				auto format = FontManager::Instance().GetTextFormat(
					StringId(text.fontKey), // キー
					wFontName,
					text.fontSize,
					weight,
					style
				);

				if (format)
				{
					// 文字列変換 (UTF8 -> Wide)
					int size_needed = MultiByteToWideChar(CP_UTF8, 0, text.text.c_str(), (int)text.text.size(), NULL, 0);
					std::wstring wText(size_needed, 0);
					MultiByteToWideChar(CP_UTF8, 0, text.text.c_str(), (int)text.text.size(), &wText[0], size_needed);

					if (!wText.empty() && wText.back() == L'\0') wText.pop_back();

					D2D1::Matrix3x2F textFlip = D2D1::Matrix3x2F::Scale(1.0f, -1.0f);
					finalMat = textFlip * finalMat;

					// 描画領域
					float layoutW = viewportWidth * 2.0f;
					float layoutH = viewportHeight * 2.0f;
					D2D1_RECT_F layoutRect;

					// 配置設定
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

					// 画面比率（ウィンドウサイズ対応）
					D2D1::Matrix3x2F scaleToViewport = D2D1::Matrix3x2F::Scale(ratioX, ratioY);

					// 最終行列 = ワールド行列(Logic) * ビューポート拡縮
					d2dRT->SetTransform(finalMat * scaleToViewport);

					// 描画
					d2dRT->DrawText(
						wText.c_str(),
						(UINT32)wText.length(),
						format.Get(),
						layoutRect,
						s_brush.Get()
					);
				}
			});

		// 変換行列を戻す
		d2dRT->SetTransform(D2D1::Matrix3x2F::Identity());

		d2dRT->EndDraw();
	}

	ID2D1RenderTarget* TextRenderer::GetD2DRenderTarget(ID3D11RenderTargetView* rtv)
	{
		// キャッシュにあれば返す
		if (s_d2dTargets.find(rtv) != s_d2dTargets.end()) {
			return s_d2dTargets[rtv].Get();
		}

		// なければ作成 (DXGI Surface経由)
		ComPtr<ID3D11Resource> resource;
		rtv->GetResource(&resource);
		if (!resource) return nullptr;

		ComPtr<IDXGISurface> surface;
		resource.As(&surface);
		if (!surface) return nullptr;

		// DXGIサーフェスのプロパティ
		D2D1_RENDER_TARGET_PROPERTIES props =
			D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
				96.0f, 96.0f // DPI
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