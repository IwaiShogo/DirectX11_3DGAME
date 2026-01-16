#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h" 
#include "Engine/Core/Time/Time.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Systems/Game/StageData.h"
#include "Sandbox/Systems/Enemy/EnemyFactory.h" 
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <algorithm>
#include <string>
#include <vector>

namespace Arche
{
	struct GameDirectorTag {};

	struct GameDirectorData
	{
		float gameTimeLimit = 5.0f; // 命の秒数
		float totalTime = 0.0f;

		int waveCount = 0;
		bool isBossActive = false;
		bool isGameOverProcessing = false; // ★追加: 多重呼び出し防止フラグ

		// UI
		Entity uiTimer = NullEntity;
		Entity uiTimerBg = NullEntity;
		Entity uiScore = NullEntity;
		Entity uiWave = NullEntity;
		Entity uiWarning = NullEntity;

		// 敵の状態追跡（ヒット確認用）
		struct EnemyCache { Entity e; int lastHp; };
		std::vector<EnemyCache> enemyCache;
	};

	class GameDirectorSystem : public ISystem
	{
	public:
		GameDirectorSystem() { m_systemName = "GameDirectorSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static GameDirectorData data;

			// 初期化
			bool init = false;
			for (auto e : reg.view<GameDirectorTag>()) { init = true; break; }
			if (!init) {
				data = GameDirectorData();
				data.gameTimeLimit = 5.0f;
				InitGame(reg, data);
				reg.emplace<GameDirectorTag>(reg.create());
			}

			float dt = Time::DeltaTime();
			data.totalTime += dt;

			// --- 1. 時間制限 (命の減少) ---
			// 既にゲームオーバー処理中なら何もしないで待つ
			if (data.isGameOverProcessing) return;

			data.gameTimeLimit -= dt;
			if (data.gameTimeLimit <= 0.0f) {
				data.gameTimeLimit = 0.0f;

				// ★修正: 一度だけ呼ぶようにする
				data.isGameOverProcessing = true;
				GameOver(reg);
				return;
			}

			// --- 2. 敵スポーン管理 (無限湧き) ---
			ManageSpawns(reg, data);

			// --- 3. 回復判定 (敵撃破 & ボスヒット) ---
			CheckRecovery(reg, data);

			// --- 4. UI更新 ---
			UpdateUI(reg, data, dt);
		}

	private:
		// ★ボス判定ヘルパー関数
		bool IsBoss(EnemyType type) {
			return type == EnemyType::Boss_Tank ||
				type == EnemyType::Boss_Prism ||
				type == EnemyType::Boss_Carrier ||
				type == EnemyType::Boss_Construct ||
				type == EnemyType::Boss_Omega;
		}

		void InitGame(Registry& reg, GameDirectorData& data)
		{
			// UI生成
			auto CreateText = [&](float x, float y, float size, XMFLOAT4 col) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { x, y, 0 };
				auto& t = reg.emplace<TextComponent>(e);
				t.fontKey = "Makinas 4 Square";
				t.fontSize = size; t.color = col; t.centerAlign = true;
				return e;
				};

			data.uiTimer = CreateText(0, 4.0f, 80.0f, { 1,0,0,1 }); // 中央上
			reg.get<TextComponent>(data.uiTimer).isBold = true;

			// タイマー背景バー
			data.uiTimerBg = reg.create();
			reg.emplace<Transform>(data.uiTimerBg).position = { 0, 4.2f, 0.1f };
			reg.get<Transform>(data.uiTimerBg).scale = { 5.0f, 1.0f, 0.1f };
			auto& g = reg.emplace<GeometricDesign>(data.uiTimerBg);
			g.shapeType = GeoShape::Cube; g.color = { 0,0,0,0.5f };

			data.uiScore = CreateText(10.0f, 4.5f, 30.0f, { 1,1,1,1 }); // 右上
			data.uiWave = CreateText(-10.0f, 4.5f, 30.0f, { 0,1,1,1 }); // 左上

			data.uiWarning = CreateText(0, 0, 100.0f, { 1,0,0,0 }); // 警告（最初透明）
			reg.get<TextComponent>(data.uiWarning).text = "WARNING\nBOSS APPROACHING";
		}

		void ManageSpawns(Registry& reg, GameDirectorData& data)
		{
			int enemyCount = 0;
			for (auto e : reg.view<EnemyStats>()) enemyCount++;

			int stageId = GameSession::selectedStageId;
			if (stageId == 0) stageId = 1;

			// ボス戦かどうか (ステージ5)
			bool isBossStage = (stageId == 5);

			// 常に敵を補充 (無限湧き)
			int maxEnemies = 5 + stageId * 2;
			if (enemyCount < maxEnemies)
			{
				if (isBossStage && !data.isBossActive) {
					// ボススポーン
					EnemyFactory::Spawn(reg, EnemyType::Boss_Omega, { 0,0,10 });
					data.isBossActive = true;
					ShowWarning(reg, data);
				}
				else if (!data.isBossActive) {
					// ザコスポーン
					float ang = (float)(rand() % 360);
					float dist = 15.0f + (rand() % 10);
					float x = cosf(ang) * dist; float z = sinf(ang) * dist;

					// EnemyType修正済
					EnemyType type = EnemyType::Zako_Cube;
					if (stageId >= 2 && rand() % 3 == 0) type = EnemyType::Zako_Armored;
					if (stageId >= 3 && rand() % 3 == 0) type = EnemyType::Zako_Speed;

					EnemyFactory::Spawn(reg, type, { x, 0, z });
				}
			}
		}

		void CheckRecovery(Registry& reg, GameDirectorData& data)
		{
			// 現在の敵リストを取得
			std::vector<Entity> currentEnemies;
			for (auto e : reg.view<EnemyStats>()) currentEnemies.push_back(e);

			// 1. 撃破判定 (キャッシュにあって現在ない = 倒された)
			for (auto& cache : data.enemyCache) {
				bool found = false;
				for (auto e : currentEnemies) { if (e == cache.e) { found = true; break; } }

				if (!found) {
					// 撃破！時間回復
					AddTime(data, 2.0f); // ザコ撃破+2秒
					GameSession::lastScore += 100;
				}
			}

			// 2. ボスヒット判定 (HPが減った)
			for (auto e : currentEnemies) {
				auto& stats = reg.get<EnemyStats>(e);
				// キャッシュから前回HPを探す
				int lastHp = stats.maxHp; // 初期値
				for (auto& cache : data.enemyCache) { if (cache.e == e) { lastHp = cache.lastHp; break; } }

				if (stats.hp < lastHp) {
					// ダメージを受けた！
					if (IsBoss(stats.type)) {
						AddTime(data, 0.5f); // ボスに攻撃当てるたびに0.5秒回復
					}
				}
			}

			// キャッシュ更新
			data.enemyCache.clear();
			for (auto e : currentEnemies) {
				GameDirectorData::EnemyCache c;
				c.e = e;
				c.lastHp = (int)reg.get<EnemyStats>(e).hp;
				data.enemyCache.push_back(c);
			}
		}

		void AddTime(GameDirectorData& data, float amount)
		{
			data.gameTimeLimit += amount;
			if (data.gameTimeLimit > 15.0f) data.gameTimeLimit = 15.0f; // 上限
		}

		void ShowWarning(Registry& reg, GameDirectorData& data)
		{
			if (reg.valid(data.uiWarning)) {
				reg.get<TextComponent>(data.uiWarning).color.w = 1.0f;
			}
		}

		void GameOver(Registry& reg)
		{
			GameSession::isGameClear = false; // 失敗
			SceneManager::Instance().LoadScene("Resources/Game/Scenes/ResultScene.json", new FadeTransition(1.0f, { 0,0,0,1 }));
		}

		void UpdateUI(Registry& reg, GameDirectorData& data, float dt)
		{
			// タイマー更新
			if (reg.valid(data.uiTimer)) {
				auto& t = reg.get<TextComponent>(data.uiTimer);
				t.text = std::to_string(data.gameTimeLimit).substr(0, 4); // "5.00"

				// 色変化
				if (data.gameTimeLimit < 2.0f) {
					t.color = { 1, 0, 0, 1 }; // 赤
					t.fontSize = 90.0f + sinf(data.totalTime * 20.0f) * 10.0f; // 激しく鼓動
				}
				else {
					t.color = { 0, 1, 1, 1 }; // シアン
					t.fontSize = 80.0f;
				}
			}

			if (reg.valid(data.uiScore)) {
				reg.get<TextComponent>(data.uiScore).text = "SCORE: " + std::to_string(GameSession::lastScore);
			}

			// 警告フェードアウト
			if (reg.valid(data.uiWarning)) {
				auto& col = reg.get<TextComponent>(data.uiWarning).color;
				if (col.w > 0) {
					col.w -= dt * 0.5f;
					// 点滅
					if ((int)(data.totalTime * 10) % 2 == 0) col.w *= 0.5f;
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::GameDirectorSystem, "GameDirectorSystem")