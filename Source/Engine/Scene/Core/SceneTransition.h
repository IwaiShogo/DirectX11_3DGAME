/*****************************************************************//**
 * @file	SceneTransition.h
 * @brief	シーン遷移の基底クラスと、標準的なフェード
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SCENE_TRANSITION_H___
#define ___SCENE_TRANSITION_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Config.h"
#include "Engine/Core/Graphics/Graphics.h"

namespace Arche
{
	// 標準エフェクトの基底クラス（インターフェース）
	// これを継承して新しいフェード効果を作ります。
	// ============================================================
	class ISceneTransition
	{
	public:
		enum class Phase { Out, Loading, WaitAsync, In, Finished };

		virtual ~ISceneTransition() = default;

		// 開始時に一度だけ呼ばれる
		virtual void Start() { m_phase = Phase::Out; m_timer = 0.0f; };

		// 更新処理（trueを返すと遷移終了）
		virtual bool Update(float dt) = 0;

		// 描画処理
		virtual void Render() = 0;

		// 遷移のフェーズ（Out: 暗くなる -> Load -> In: 明るくなる）
		Phase GetPhase() const { return m_phase; }
		void SetPhase(Phase p) { m_phase = p; }

		// タイマーリセット用
		void ResetTimer() { m_timer = 0.0f; }

	protected:
		Phase m_phase = Phase::Out;
		float m_duration = 0.5f;
		float m_timer = 0.0f;

		// ヘルパー: 画面全体を覆う行列
		XMMATRIX GetFullScreenMatrix(float offsetX = 0.0f, float offsetY = 0.0f, bool flipY = false)
		{
			float w = (float)Config::SCREEN_WIDTH;
			float h = (float)Config::SCREEN_HEIGHT;

			XMMATRIX mat = XMMatrixScaling(w, h, 1.0f) * XMMatrixTranslation(-w * 0.5f + offsetX, -h * 0.5f + offsetY, 0.0f);
			
			if (flipY)
			{
				mat *= XMMatrixRotationX(XM_PI);
			}

			return mat;
		}
	};

	// 瞬時切り替え
	// ============================================================
	class ImmediateTransition : public ISceneTransition
	{
	public:
		bool Update(float dt) override
		{
			// 全てのフェーズを一瞬で終わらせる
			if (m_phase == Phase::Out) return true; // ロードへ
			if (m_phase == Phase::In) m_phase = Phase::Finished;
			return false;
		}
		void Render() override {}	// 何も描画しない
	};

	// 標準的なフェード（黒味フェード）
	// ============================================================
	class FadeTransition : public ISceneTransition
	{
	public:
		// コンストラクタで色や時間を指定可能に
		FadeTransition(float duration = 0.5f, const XMFLOAT4& color = { 0,0,0,1 })
			: m_color(color), m_alpha(0.0f) { m_duration = duration; }

		void Start() override
		{
			ISceneTransition::Start();
			m_alpha = 0.0f;
		}

		bool Update(float dt) override
		{
			m_timer += dt;
			float progress = std::clamp(m_timer / m_duration, 0.0f, 1.0f);

			if (m_phase == Phase::Out)
			{
				// フェードアウト（透明 -> 色）
				m_alpha = progress;
				if (progress >= 1.0f) return true;	// Phase終了通知
			}
			else if (m_phase == Phase::In)
			{
				// フェードイン（色 -> 透明）
				m_alpha = 1.0f - progress;
				if (progress >= 1.0f) m_phase = Phase::Finished;
			}
			else
			{
				m_alpha = 1.0f;
			}

			return false;
		}

		void Render() override
		{
			auto tex = ResourceManager::Instance().GetTexture("White");
			if (!tex) return;

			SpriteRenderer::Begin();
			XMFLOAT4 col = m_color;
			col.w = m_alpha;
			std::stringstream ss;
			ss << "Fade Alpha: " << m_alpha << "\n";
			OutputDebugStringA(ss.str().c_str());
			SpriteRenderer::Draw(tex.get(), GetFullScreenMatrix(), col);
		}

	private:
		float m_alpha;
		XMFLOAT4 m_color;
	};

	// スライド遷移（横から黒幕が来る）
	// ============================================================
	class SlideTransition : public ISceneTransition
	{
	public:
		SlideTransition(float duration = 0.5f) { m_duration = duration; }

		bool Update(float dt) override
		{
			m_timer += dt;
			float progress = std::clamp(m_timer / m_duration, 0.0f, 1.0f);
			// イージング関数 (SmoothStep)
			float t = progress * progress * (3.0f - 2.0f * progress);

			float screenW = (float)Config::SCREEN_WIDTH;

			if (m_phase == Phase::Out)
			{
				// 右から左へ入ってくる
				m_posX = screenW - (screenW * t);
				if (progress >= 1.0f) { m_posX = 0; return true; }
			}
			else if (m_phase == Phase::In)
			{
				// さらに左へ抜けていく
				m_posX = 0 - (screenW * t);
				if (progress >= 1.0f) { m_phase = Phase::Finished; }
			}
			else
			{
				m_posX = 0; // 画面を覆ったまま待機
			}
			return false;
		}

		void Render() override
		{
			auto tex = ResourceManager::Instance().GetTexture("White");
			if (!tex) return;

			SpriteRenderer::Begin();

			// X方向に移動させる行列
			float w = (float)Config::SCREEN_WIDTH;
			float h = (float)Config::SCREEN_HEIGHT;

			// 黒色で描画
			SpriteRenderer::Draw(tex.get(), GetFullScreenMatrix(m_posX, 0), { 0, 0, 0, 1 });
		}

	private:
		float m_posX = 0.0f;
	};

	// ============================================================
	// 4. クロスフェード
	// ============================================================
	class CrossDissolveTransition : public ISceneTransition
	{
	public:
		CrossDissolveTransition(float duration = 1.0f)
			: m_alpha(0.0f) { m_duration = duration; }

		void Start() override
		{
			ISceneTransition::Start();
			m_alpha = 0.0f;
			Graphics::CopyBackBufferToTexture("LastFrame");
		}

		bool Update(float dt) override
		{
			if (m_phase == Phase::Out)
			{
				return true; // 即座にロードへ
			}
			else if (m_phase == Phase::In)
			{
				m_timer += dt;
				float progress = std::clamp(m_timer / m_duration, 0.0f, 1.0f);
				m_alpha = 1.0f - progress;

				if (progress >= 1.0f) m_phase = Phase::Finished;
			}
			return false;
		}

		void Render() override
		{
			if (m_phase == Phase::In)
			{
				auto tex = ResourceManager::Instance().GetTexture("LastFrame");
				if (!tex) tex = ResourceManager::Instance().GetTexture("White"); // 代用

				SpriteRenderer::Begin();

				XMFLOAT4 col = { 1,1,1, m_alpha };
				if (tex->filepath == "System::White" || tex->filepath.find("White") != std::string::npos) col = { 0,0,0, m_alpha }; // 代用の場合は黒

				SpriteRenderer::Draw(tex.get(), GetFullScreenMatrix(0, 0, true), col);
			}
		}
	private:
		float m_alpha = 1.0f;
	};

	// ============================================================
	// 5. サークルワイプ (円形遷移)
	// ※ "Hole.png" (周囲が黒、中央が透明の画像) が必要
	// ============================================================
	class CircleWipeTransition : public ISceneTransition
	{
	public:
		CircleWipeTransition(float duration = 0.5f) { m_duration = duration; }

		bool Update(float dt) override
		{
			m_timer += dt;
			float progress = std::clamp(m_timer / m_duration, 0.0f, 1.0f);

			// 画面を覆うのに必要なスケール（画面対角線の半分以上）
			float maxScale = 2.5f;

			if (m_phase == Phase::Out)
			{
				// 円が小さくなっていく (1 -> 0)
				m_scale = maxScale * (1.0f - progress);
				if (progress >= 1.0f) { m_scale = 0.0f; return true; }
			}
			else if (m_phase == Phase::In)
			{
				// 円が大きくなっていく (0 -> 1)
				m_scale = maxScale * progress;
				if (progress >= 1.0f) m_phase = Phase::Finished;
			}
			else m_scale = 0.0f;

			return false;
		}

		void Render() override
		{
			auto tex = ResourceManager::Instance().GetTexture("Hole");
			if (!tex) tex = ResourceManager::Instance().GetTexture("White");

			SpriteRenderer::Begin();

			float w = (float)Config::SCREEN_WIDTH;
			float h = (float)Config::SCREEN_HEIGHT;

			XMMATRIX mat = XMMatrixScaling(w * m_scale, h * m_scale, 1.0f) * XMMatrixTranslation(-w * m_scale * 0.5f, -h * m_scale * 0.5f, 0.0f);

			XMFLOAT4 color = { 1,1,1,1 };
			if (tex->filepath.find("White") != std::string::npos) color = { 0,0,0,1 };

			SpriteRenderer::Draw(tex.get(), mat, color);

			if (m_scale < 1.0f && tex->filepath.find("White") != std::string::npos)
			{
				auto white = ResourceManager::Instance().GetTexture("White");
			}
		}

	private:
		float m_scale = 1.0f;
	};

}

#endif // !___SCENE_TRANSITION_H___