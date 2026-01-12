/*****************************************************************//**
 * @file	SceneManager.cpp
 * @brief	シーンマネージャー
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{
	SceneManager* SceneManager::s_instance = nullptr;

	SceneManager::SceneManager()
	{
		assert(s_instance == nullptr && "SceneManagerは既に存在します。");
		s_instance = this;
	}

	SceneManager::~SceneManager()
	{
		m_world.clearSystems();
		if (s_instance == this) s_instance = nullptr;
	}

	SceneManager& SceneManager::Instance()
	{
		assert(s_instance != nullptr);
		return *s_instance;
	}

	void SceneManager::Initialize()
	{
	}

	void SceneManager::Update()
	{
		float dt = Time::DeltaTime();

		// 通常のシーン更新（ロード待ち以外なら動かす）
		// ※ロード中もアニメーションさせたい場合はここを調整
		if (m_transition == nullptr || m_transition->GetPhase() != ISceneTransition::Phase::WaitAsync)
		{
			m_world.Tick(m_context.editorState);
		}

		// 遷移エフェクト更新
		if (m_transition)
		{
			// フェード更新。trueが帰ってきたらそのフェーズ完了
			if (m_transition->Update(dt))
			{
				auto phase = m_transition->GetPhase();

				// 1. フェードアウト完了 -> ロード開始
				if (phase == ISceneTransition::Phase::Out)
				{
					if (m_isAsyncLoading)
					{
						// --- 非同期ロード開始 ---
						m_transition->SetPhase(ISceneTransition::Phase::WaitAsync);
						m_transition->ResetTimer();

						// A. シーンファイルから必要なアセットを自動収集
						std::vector<std::string> models, textures, sounds;
						SceneSerializer::CollectAssets(m_nextScenePath, models, textures, sounds);

						// B. ResourceManagerに一括リクエスト
						auto& rm = ResourceManager::Instance();
						for (const auto& path : models)	  rm.LoadModelAsync(path);
						for (const auto& path : textures) rm.LoadTextureAsync(path);
						for (const auto& path : sounds)	  rm.LoadSoundAsync(path);

						Logger::Log("Async Load Requested for: " + m_nextScenePath);
					}
					else
					{
						// 同期ロード
						m_transition->SetPhase(ISceneTransition::Phase::Loading);
						m_transition->ResetTimer();
						PerformLoad(m_nextScenePath);
						m_transition->SetPhase(ISceneTransition::Phase::In);
						m_transition->ResetTimer();
					}
				}
			}

			// 2. 非同期ロード待ち
			if (m_transition->GetPhase() == ISceneTransition::Phase::WaitAsync)
			{
				auto& rm = ResourceManager::Instance();

				// 進捗率の更新
				if (m_currentAsyncOp)
				{
					m_currentAsyncOp->progress = rm.GetProgress();
				}

				// ロード完了チェック
				if (!rm.IsLoading())
				{
					// ロードが終わったので、実際にシーンを構築（リソースはキャッシュにあるので一瞬）
					PerformLoad(m_nextScenePath);

					if (m_currentAsyncOp)
					{
						m_currentAsyncOp->progress = 1.0f;
						m_currentAsyncOp->isDone = true;
					}

					// フェードイン開始
					m_transition->SetPhase(ISceneTransition::Phase::In);
					m_transition->ResetTimer();

					Logger::Log("Async Load Completed.");
				}
			}

			// 3. 終了判定
			if (m_transition->GetPhase() == ISceneTransition::Phase::Finished)
			{
				m_transition = nullptr;
				m_isAsyncLoading = false;
				m_currentAsyncOp = nullptr;
				m_nextScenePath = "";
			}
		}
	}

	void SceneManager::Render()
	{
		// 1. 通常のシーン描画
		m_world.Render(m_context);

		// 2. ローディングバーの描画 (非同期ロード中のみ)
		if (m_isAsyncLoading && m_currentAsyncOp)
		{
			// UI描画用にレンダラーステートをセット
			SpriteRenderer::Begin();

			// システムの白テクスチャを取得
			auto tex = ResourceManager::Instance().GetTexture("White");

			if (tex)
			{
				float w = static_cast<float>(Config::SCREEN_WIDTH);
				float h = static_cast<float>(Config::SCREEN_HEIGHT);
				float progress = m_currentAsyncOp->progress;

				// バーの設定
				float barWidth = w * 0.8f;
				float barHeight = 20.0f;
				float barY = -h * 0.4f; // 画面下の方 (座標系は中心0,0と仮定)

				// A. 背景 (グレー)
				{
					XMMATRIX mat = XMMatrixScaling(barWidth, barHeight, 1.0f) *
						XMMatrixTranslation(0.0f, barY, 0.0f);
					SpriteRenderer::Draw(tex.get(), mat, { 0.2f, 0.2f, 0.2f, 1.0f });
				}

				// B. 進捗 (白)
				{
					// 左端基準で伸びるように位置調整
					float currentW = barWidth * progress;
					float offsetX = (currentW - barWidth) * 0.5f;

					XMMATRIX mat = XMMatrixScaling(currentW, barHeight, 1.0f) *
						XMMatrixTranslation(offsetX, barY, 0.0f);
					SpriteRenderer::Draw(tex.get(), mat, { 1.0f, 1.0f, 1.0f, 1.0f });
				}
			}
		}

		// 3. トランジション（フェードなど）描画
		if (m_transition) m_transition->Render();
	}

	void SceneManager::Render(World* targetWorld)
	{
		if (!targetWorld) return;
		targetWorld->Render(m_context);
	}

	// 同期ロード
	void SceneManager::LoadScene(const std::string& filepath, ISceneTransition* transition)
	{
		if (m_transition || m_isAsyncLoading) return;

		m_nextScenePath = filepath;
		m_isAsyncLoading = false;

		if (transition == nullptr) m_transition = std::make_unique<FadeTransition>(0.5f);
		else m_transition.reset(transition);

		m_transition->Start();
		Logger::Log("Transition Started (Sync): " + filepath);
	}

	// 非同期ロード
	std::shared_ptr<AsyncOperation> SceneManager::LoadSceneAsync(const std::string& filepath, ISceneTransition* transition)
	{
		if (m_transition || m_isAsyncLoading) return nullptr;

		m_nextScenePath = filepath;
		m_isAsyncLoading = true;
		m_currentAsyncOp = std::make_shared<AsyncOperation>();
		m_currentAsyncOp->progress = 0.0f;
		m_currentAsyncOp->isDone = false;

		if (transition == nullptr) m_transition = std::make_unique<FadeTransition>(0.5f);
		else m_transition.reset(transition);

		m_transition->Start();
		Logger::Log("Transition Started (Async): " + filepath);

		return m_currentAsyncOp;
	}

	// 同期ロード用ヘルパー
	void SceneManager::PerformLoad(const std::string& path)
	{
		m_world.clearSystems();
		m_world.clearEntities();

		// ここで呼び出すLoadSceneは、内部でResourceManager::GetModelなどを呼ぶが、
		// 既にAsyncLoadでキャッシュに乗っているため、ディスク読み込みは発生しない（高速）
		SceneSerializer::LoadScene(m_world, path);

		m_currentScenePath = path;
		m_isDirty = false;

		Logger::Log("Scene Loaded: " + m_currentScenePath);
	}

}	// namespace Arche