#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h" // 遷移用
#include "Engine/Core/Time/Time.h"
#include "Engine/Renderer/Text/TextRenderer.h"

#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Systems/Game/StageData.h"
#include "Sandbox/Systems/Enemy/EnemyFactory.h" // Factoryを使用
#include "Sandbox/Components/Player/PlayerTime.h"

#include <algorithm>
#include <string>

namespace Arche
{
	class GameDirectorSystem : public ISystem
	{
	public:
		GameDirectorSystem()
		{
			m_systemName = "GameDirectorSystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			// 1. 初期化（シーンロード直後）
			if (!m_isInitialized)
			{
				InitializeGame(registry);
				m_isInitialized = true;
			}

			float dt = Time::DeltaTime();
			m_gameTimer += dt;

			// 2. 敵の出現処理
			ProcessSpawns(registry);

			// 3. UI演出更新
			UpdateUI(registry, dt);

			// 4. クリア判定
			CheckClearCondition(registry);
		}

	private:
		bool m_isInitialized = false;
		float m_gameTimer = 0.0f;

		// ステージデータ
		std::vector<SpawnEvent> m_spawnEvents;
		int m_nextEventIndex = 0;

		// UIエンティティ
		Entity m_waveText = NullEntity;
		Entity m_statusText = NullEntity;
		Entity m_warningText = NullEntity;
		float m_warningTimer = 0.0f;

		// ------------------------------------------------------------
		// ゲーム開始時のセットアップ
		// ------------------------------------------------------------
		void InitializeGame(Registry& reg)
		{
			// 選択されたステージIDを取得 (デフォルトは1)
			int stageId = GameSession::selectedStageId;
			if (stageId == 0) stageId = 1;

			// ステージデータを取得して時間順にソート
			m_spawnEvents = StageDataProvider::GetStageData(stageId);
			std::sort(m_spawnEvents.begin(), m_spawnEvents.end(), [](const SpawnEvent& a, const SpawnEvent& b) {
				return a.time < b.time;
				});

			m_nextEventIndex = 0;
			m_gameTimer = 0.0f;

			// UI作成と開始演出
			CreateUI(reg);
			ShowWaveText(reg, "STAGE " + std::to_string(stageId) + " START", 3.0f);

			Logger::Log("Game Started: Stage " + std::to_string(stageId));
		}

		// ------------------------------------------------------------
		// 敵のスポーン管理
		// ------------------------------------------------------------
		void ProcessSpawns(Registry& reg)
		{
			// プレイヤーの位置を取得（敵をプレイヤー周辺に出現させるため）
			XMVECTOR playerPos = XMVectorZero();
			bool pFound = false;
			for (auto e : reg.view<PlayerTime, Transform>()) {
				playerPos = XMLoadFloat3(&reg.get<Transform>(e).position);
				pFound = true;
				break;
			}
			if (!pFound) return; // プレイヤーがいなければスキップ

			// 未消化のイベントを確認
			while (m_nextEventIndex < m_spawnEvents.size())
			{
				const auto& evt = m_spawnEvents[m_nextEventIndex];

				// 時間になったら出現
				if (m_gameTimer >= evt.time)
				{
					// 出現座標の計算 (プレイヤー中心の円周上)
					float rad = XMConvertToRadians(evt.angle);
					float x = XMVectorGetX(playerPos) + sinf(rad) * evt.dist;
					float z = XMVectorGetZ(playerPos) + cosf(rad) * evt.dist;

					// ボスなら警告演出
					if (IsBoss(evt.type)) ShowWarning(reg);

					// ★EnemyFactoryを使って敵を生成
					EnemyFactory::Spawn(reg, evt.type, { x, 0.0f, z });

					m_nextEventIndex++;
				}
				else
				{
					break; // まだ時間のイベントではない
				}
			}
		}

		// ------------------------------------------------------------
		// クリア判定
		// ------------------------------------------------------------
		void CheckClearCondition(Registry& reg)
		{
			// 全てのイベントを消化済みか？
			if (m_nextEventIndex >= m_spawnEvents.size())
			{
				// 画面内の敵の数をカウント
				int enemyCount = (int)reg.view<EnemyStats>().size();

				// 敵が0になり、かつ最後の敵が出現してから少し時間が経っている場合
				if (enemyCount == 0 && m_gameTimer > 2.0f)
				{
					// クリア処理（重複防止のためフラグ管理などを推奨）
					if (!GameSession::isGameClear) // 簡易チェック
					{
						GameSession::isGameClear = true;
						Logger::Log("STAGE CLEAR!");

						// リザルトシーンへ遷移
						SceneManager::Instance().LoadScene("Resources/Game/Scenes/ResultScene.json", new FadeTransition(1.0f, { 1,1,1,1 }));
					}
				}
			}
		}

		bool IsBoss(EnemyType t) {
			// 列挙型の値でボス判定（Boss_Tank以降はボス）
			return t >= EnemyType::Boss_Tank;
		}

		// ------------------------------------------------------------
		// UI関連
		// ------------------------------------------------------------
		void CreateUI(Registry& reg)
		{
			// 中央のメッセージ（WAVE STARTなど）
			m_waveText = reg.create();
			reg.emplace<Transform>(m_waveText).position = { 0, 1.0f, 0 };
			auto& t = reg.emplace<TextComponent>(m_waveText);
			t.fontKey = "Consolas";
			t.fontSize = 80.0f;
			t.centerAlign = true;
			t.color = { 0, 1, 1, 0 }; // 最初は透明

			// 右上の情報表示（残りウェーブ数など）
			m_statusText = reg.create();
			reg.emplace<Transform>(m_statusText).position = { 8.0f, 4.0f, 0 };
			auto& st = reg.emplace<TextComponent>(m_statusText);
			st.fontKey = "Consolas";
			st.fontSize = 24.0f;
			st.color = { 1, 1, 1, 1 };
			st.centerAlign = true; // 右寄せっぽく配置

			// ボス警告（赤文字）
			m_warningText = reg.create();
			reg.emplace<Transform>(m_warningText).position = { 0, 0, 0 };
			auto& wt = reg.emplace<TextComponent>(m_warningText);
			wt.text = "WARNING\nBOSS APPROACHING";
			wt.fontKey = "Consolas";
			wt.fontSize = 100.0f;
			wt.centerAlign = true;
			wt.color = { 1, 0, 0, 0 };
			wt.isBold = true;
		}

		void ShowWaveText(Registry& reg, std::string text, float duration)
		{
			if (reg.valid(m_waveText)) {
				auto& t = reg.get<TextComponent>(m_waveText);
				t.text = text;
				t.color.w = 1.0f; // 表示
			}
		}

		void ShowWarning(Registry& reg)
		{
			m_warningTimer = 4.0f; // 4秒間点滅
		}

		void UpdateUI(Registry& reg, float dt)
		{
			// WaveTextのフェードアウト
			if (reg.valid(m_waveText)) {
				auto& col = reg.get<TextComponent>(m_waveText).color;
				if (col.w > 0) col.w -= dt * 0.5f; // ゆっくり消える
			}

			// Warningの点滅処理
			if (m_warningTimer > 0.0f)
			{
				m_warningTimer -= dt;
				if (reg.valid(m_warningText)) {
					// 高速点滅
					float alpha = (int)(m_warningTimer * 6.0f) % 2 == 0 ? 1.0f : 0.0f;
					reg.get<TextComponent>(m_warningText).color.w = alpha;
				}
			}
			else {
				if (reg.valid(m_warningText)) reg.get<TextComponent>(m_warningText).color.w = 0.0f;
			}

			// ステータス更新（残り敵数）
			if (reg.valid(m_statusText)) {
				int enemies = (int)reg.view<EnemyStats>().size();
				int eventsLeft = (int)(m_spawnEvents.size() - m_nextEventIndex);

				std::string s = "ENEMIES: " + std::to_string(enemies) + "\nRESERVE: " + std::to_string(eventsLeft);
				reg.get<TextComponent>(m_statusText).text = s;
			}
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::GameDirectorSystem, "GameDirectorSystem")
