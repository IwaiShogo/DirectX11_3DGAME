#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h" 
#include "Engine/Core/Time/Time.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Systems/Game/StageData.h"
#include "Sandbox/Systems/Enemy/EnemyFactory.h" 
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <algorithm>
#include <string>
#include <vector>
#include <queue>

namespace Arche
{
	struct GameDirectorTag {};

	struct WaveDef {
		EnemyType type; int count; float interval; bool isBoss;
	};

	struct GameDirectorData
	{
		float totalTime = 0.0f;
		bool isGameOverProcessing = false;
		bool isPaused = false;

		// ステージ管理
		int currentStage = 1;

		// ウェーブ
		std::queue<WaveDef> currentWaveQueue;
		std::vector<std::vector<WaveDef>> stageWaves;
		int currentWaveIndex = 0;
		float spawnTimer = 0.0f;
		bool isWaveClear = false;
		float waveIntervalTimer = 0.0f;

		// UI
		Entity pauseOverlay = NullEntity;
		Entity pauseText = NullEntity;
		Entity guideText = NullEntity;

		// 敵管理用キャッシュ
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

				if (GameSession::selectedStageId == 0) GameSession::selectedStageId = 1;
				data.currentStage = GameSession::selectedStageId;

				InitStageData(data);

				GameSession::killCount = 0;
				GameSession::currentWave = 1;
				GameSession::totalWaves = (int)data.stageWaves.size();

				StartNextWave(data, 0);

				for (auto e : reg.view<PlayerTime>()) { reg.get<PlayerTime>(e).currentTime = 5.0f; }

				InitPauseUI(reg, data);
				AudioManager::Instance().PlayBGM("bgm_game.wav", 0.2f);
				reg.emplace<GameDirectorTag>(reg.create());
			}

			// --- ポーズ処理 ---
			if (Input::GetButtonDown(Button::Start) || Input::GetKeyDown('P') || Input::GetKeyDown(VK_ESCAPE)) {
				TogglePause(reg, data);
			}
			if (data.isPaused) {
				Time::timeScale = 0.0f;
				UpdatePauseMenu(reg, data);
				return;
			}
			else {
				bool lowTime = false;
				for (auto e : reg.view<PlayerTime>()) {
					if (reg.get<PlayerTime>(e).currentTime <= 1.0f) lowTime = true;
				}
				Time::timeScale = lowTime ? 0.5f : 1.0f;
			}

			float dt = Time::DeltaTime();
			data.totalTime += dt;

			// --- プレイヤー時間管理 ---
			if (!data.isGameOverProcessing) {
				for (auto e : reg.view<PlayerTime>()) {
					auto& pt = reg.get<PlayerTime>(e);
					pt.currentTime -= dt;
					if (pt.currentTime <= 0.0f) {
						pt.currentTime = 0.0f; pt.isDead = true;
						data.isGameOverProcessing = true;
						GameOver(reg);
						return;
					}
				}
			}

			UpdateWave(reg, data, dt);
			CheckRecoveryAndBossKill(reg, data);
		}

	private:
		void InitStageData(GameDirectorData& data)
		{
			data.stageWaves.clear();
			auto Add = [&](const std::vector<WaveDef>& w) { data.stageWaves.push_back(w); };

			if (data.currentStage == 1)
			{
				Add({ {EnemyType::Zako_Cube, 40, 0.15f, false} });
				Add({ {EnemyType::Zako_Speed, 25, 0.2f, false}, {EnemyType::Zako_Cube, 20, 0.5f, false} });
				Add({ {EnemyType::Boss_Tank, 1, 0.0f, true}, {EnemyType::Zako_Cube, 30, 1.0f, false} });
			}
			else if (data.currentStage == 2)
			{
				Add({ {EnemyType::Zako_Shield, 25, 0.3f, false}, {EnemyType::Zako_Sniper, 15, 0.5f, false} });
				Add({ {EnemyType::Zako_Armored, 20, 0.5f, false} });
				Add({ {EnemyType::Zako_Speed, 30, 0.1f, false}, {EnemyType::Zako_Sniper, 20, 0.5f, false} });
				Add({ {EnemyType::Boss_Prism, 1, 0.0f, true}, {EnemyType::Zako_Sniper, 10, 2.0f, false} });
			}
			else if (data.currentStage == 3)
			{
				Add({ {EnemyType::Zako_Kamikaze, 50, 0.1f, false} });
				Add({ {EnemyType::Zako_Speed, 30, 0.2f, false}, {EnemyType::Zako_Kamikaze, 20, 0.3f, false} });
				Add({ {EnemyType::Zako_Shield, 20, 0.4f, false}, {EnemyType::Zako_Sniper, 20, 0.4f, false} });
				Add({ {EnemyType::Zako_Cube, 60, 0.1f, false} });
				Add({ {EnemyType::Boss_Carrier, 1, 0.0f, true}, {EnemyType::Zako_Kamikaze, 40, 1.0f, false} });
			}
			else if (data.currentStage == 4)
			{
				Add({ {EnemyType::Zako_Armored, 25, 0.3f, false} });
				Add({ {EnemyType::Zako_Speed, 60, 0.1f, false} });
				Add({ {EnemyType::Zako_Shield, 25, 0.3f, false}, {EnemyType::Zako_Sniper, 25, 0.3f, false} });
				Add({ {EnemyType::Zako_Kamikaze, 40, 0.1f, false}, {EnemyType::Zako_Cube, 40, 0.1f, false} });
				Add({ {EnemyType::Zako_Armored, 15, 0.5f, false}, {EnemyType::Zako_Speed, 30, 0.2f, false} });
				Add({ {EnemyType::Boss_Construct, 1, 0.0f, true}, {EnemyType::Boss_Tank, 1, 5.0f, true} });
			}
			else if (data.currentStage == 5)
			{
				Add({ {EnemyType::Boss_Omega, 1, 0.0f, true}, {EnemyType::Zako_Cube, 60, 1.0f, false} });
				Add({ {EnemyType::Boss_Titan, 1, 0.0f, true}, {EnemyType::Zako_Kamikaze, 30, 1.0f, false}, {EnemyType::Zako_Sniper, 20, 2.0f, false} });
			}
		}

		void StartNextWave(GameDirectorData& data, int waveIndex) {
			if (waveIndex >= data.stageWaves.size()) return;
			data.currentWaveIndex = waveIndex;
			GameSession::currentWave = waveIndex + 1;
			data.isWaveClear = false; data.spawnTimer = 0.0f;
			while (!data.currentWaveQueue.empty()) data.currentWaveQueue.pop();
			for (const auto& def : data.stageWaves[waveIndex]) {
				for (int i = 0; i < def.count; ++i) data.currentWaveQueue.push({ def.type, 1, def.interval, def.isBoss });
			}
		}

		void UpdateWave(Registry& reg, GameDirectorData& data, float dt) {
			if (!data.currentWaveQueue.empty()) {
				data.spawnTimer -= dt;
				if (data.spawnTimer <= 0.0f) {
					do {
						const auto& wave = data.currentWaveQueue.front();
						SpawnEnemy(reg, data, wave.type);
						data.spawnTimer = wave.interval;
						data.currentWaveQueue.pop();
					} while (!data.currentWaveQueue.empty() && data.spawnTimer <= 0.0f);
				}
			}

			int enemyCount = 0;
			for (auto e : reg.view<EnemyStats>()) enemyCount++;

			if (data.currentWaveQueue.empty() && enemyCount == 0) {
				if (!data.isWaveClear) {
					data.isWaveClear = true;
					data.waveIntervalTimer = 1.0f; // 待ち時間短縮
					AddTime(reg, 5.0f);
				}
			}

			if (data.isWaveClear) {
				data.waveIntervalTimer -= dt;
				if (data.waveIntervalTimer <= 0.0f) {
					if (data.currentWaveIndex + 1 < data.stageWaves.size()) {
						StartNextWave(data, data.currentWaveIndex + 1);
					}
					else {
						GameSession::isGameClear = true;
						AudioManager::Instance().StopBGM();
						SceneManager::Instance().LoadScene("Resources/Game/Scenes/ResultScene.json", new FadeTransition(1.0f, { 1,1,1,1 }));
					}
				}
			}
		}

		void SpawnEnemy(Registry& reg, GameDirectorData& data, EnemyType type) {
			XMFLOAT3 spawnPos = { 0, 0, 0 };
			float fieldRadius = (data.currentStage == 5) ? 35.0f : 25.0f;

			if (IsBoss(type)) {
				spawnPos = { 0, 0, fieldRadius - 5.0f };
			}
			else {
				float spawnDist = fieldRadius + 10.0f;
				float angle = (float)(rand() % 360) * (3.14159f / 180.0f);
				spawnPos.x = cosf(angle) * spawnDist;
				spawnPos.z = sinf(angle) * spawnDist;
				spawnPos.y = 0.0f;
			}
			EnemyFactory::Spawn(reg, type, spawnPos);
		}

		void CheckRecoveryAndBossKill(Registry& reg, GameDirectorData& data) {
			// 現在生存している敵をリスト化（ループ中の削除を避けるため、まずコピーを取る）
			std::vector<Entity> currentEnemies;
			for (auto e : reg.view<EnemyStats>()) {
				currentEnemies.push_back(e);
			}

			// --- 撃破カウントの更新 ---
			for (auto& c : data.enemyCache) {
				bool found = false;
				for (auto e : currentEnemies) {
					if (e == c.e) { found = true; break; }
				}
				if (!found) GameSession::killCount++;
			}

			// --- ダメージ/ボス回復ボーナスの判定 ---
			for (auto e : currentEnemies) {
				if (!reg.valid(e)) continue;
				auto& s = reg.get<EnemyStats>(e);
				int lastHp = s.maxHp;
				for (auto& c : data.enemyCache) {
					if (c.e == e) { lastHp = c.lastHp; break; }
				}
				if (s.hp < lastHp && IsBoss(s.type)) AddTime(reg, 0.2f);
			}

			// --- ボス撃破時の即クリア判定 ---
			bool waveHasBoss = false;
			if (data.currentWaveIndex < data.stageWaves.size()) {
				for (auto& w : data.stageWaves[data.currentWaveIndex]) {
					if (w.isBoss) waveHasBoss = true;
				}
			}

			if (waveHasBoss) {
				int bossCount = 0;
				for (auto e : currentEnemies) {
					if (IsBoss(reg.get<EnemyStats>(e).type)) bossCount++;
				}

				// ボスが全滅し、生成キューも空の場合
				if (bossCount == 0 && data.currentWaveQueue.empty() && !data.isWaveClear) {

					// ★安全な削除：viewループ内ではなく、別のリスト(targets)に集めてから消す
					std::vector<Entity> targets;
					for (auto e : reg.view<EnemyStats>()) {
						targets.push_back(e);
					}

					for (auto e : targets) {
						if (reg.valid(e)) DestroyRecursive(reg, e);
					}

					while (!data.currentWaveQueue.empty()) data.currentWaveQueue.pop();

					data.isWaveClear = true;
					data.waveIntervalTimer = 1.0f;
					AddTime(reg, 10.0f);
				}
			}

			// --- キャッシュの更新 ---
			data.enemyCache.clear();
			// 最新の生存状況を反映
			for (auto e : reg.view<EnemyStats>()) {
				data.enemyCache.push_back({ e, (int)reg.get<EnemyStats>(e).hp });
			}
		}

		void DestroyRecursive(Registry& reg, Entity e) {
			if (reg.has<Relationship>(e)) {
				for (auto child : reg.get<Relationship>(e).children)
					if (reg.valid(child)) DestroyRecursive(reg, child);
			}
			reg.destroy(e);
		}

		bool IsBoss(EnemyType type) {
			return type == EnemyType::Boss_Tank || type == EnemyType::Boss_Prism ||
				type == EnemyType::Boss_Carrier || type == EnemyType::Boss_Construct ||
				type == EnemyType::Boss_Omega || type == EnemyType::Boss_Titan;
		}

		void AddTime(Registry& reg, float amount) { for (auto e : reg.view<PlayerTime>()) { auto& pt = reg.get<PlayerTime>(e); pt.currentTime += amount; if (pt.currentTime > pt.maxTime)pt.currentTime = pt.maxTime; } }
		void GameOver(Registry& reg) { GameSession::isGameClear = false; SceneManager::Instance().LoadScene("Resources/Game/Scenes/ResultScene.json", new FadeTransition(1.0f, { 0,0,0,1 })); }

		Entity CreateText(Registry& reg, std::string txt, float x, float y, float size) {
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = { x,y,0 };
			auto& t = reg.emplace<TextComponent>(e);
			t.text = txt; t.fontSize = size; t.fontKey = "Makinas 4 Square"; t.centerAlign = true;
			return e;
		}

		void InitPauseUI(Registry& reg, GameDirectorData& data) {
			// 背景オーバーレイ
			data.pauseOverlay = reg.create();
			reg.emplace<Transform>(data.pauseOverlay).scale = { 0,0,0 };
			auto& g = reg.emplace<GeometricDesign>(data.pauseOverlay);
			g.shapeType = GeoShape::Cube; g.color = { 0,0,0,0.8f };

			// テキスト (サイズ縮小: 80 -> 50)
			data.pauseText = CreateText(reg, "PAUSE", 0, 1.0f, 50.0f);
			reg.get<Transform>(data.pauseText).scale = { 0,0,0 };

			// ガイドテキスト (サイズ縮小: 40 -> 25)
			data.guideText = CreateText(reg, "A: RESUME  B: TITLE", 0, -1.0f, 25.0f);
			reg.get<Transform>(data.guideText).scale = { 0,0,0 };
		}

		void TogglePause(Registry& reg, GameDirectorData& data) {
			data.isPaused = !data.isPaused;
			float s = data.isPaused ? 1.0f : 0.0f;
			Entity cam = NullEntity; for (auto e : reg.view<Camera>()) { cam = e; break; }
			if (reg.valid(cam)) {
				auto& ct = reg.get<Transform>(cam);
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(ct.rotation.x, ct.rotation.y, ct.rotation.z);
				XMVECTOR pos = XMLoadFloat3(&ct.position) + XMVector3TransformCoord({ 0,0,5 }, rot);
				XMFLOAT3 p; XMStoreFloat3(&p, pos);

				// UI位置・サイズ調整
				if (reg.valid(data.pauseOverlay)) {
					auto& t = reg.get<Transform>(data.pauseOverlay);
					// サイズを小さく (16x9 -> 8x4.5)
					t.position = p; t.rotation = ct.rotation; t.scale = { 8.0f * s, 4.5f * s, 0.1f * s };
				}
				if (reg.valid(data.pauseText)) {
					auto& t = reg.get<Transform>(data.pauseText);
					XMVECTOR tp = pos + XMVector3TransformCoord({ 0, 0.5f, -0.1f }, rot);
					XMStoreFloat3(&t.position, tp); t.rotation = ct.rotation; t.scale = { s,s,s };
				}
				if (reg.valid(data.guideText)) {
					auto& t = reg.get<Transform>(data.guideText);
					XMVECTOR gp = pos + XMVector3TransformCoord({ 0, -0.5f, -0.1f }, rot);
					XMStoreFloat3(&t.position, gp); t.rotation = ct.rotation; t.scale = { s,s,s };
				}
			}
		}

		void UpdatePauseMenu(Registry& reg, GameDirectorData& data) {
			if (Input::GetButtonDown(Button::A) || Input::GetKeyDown(VK_RETURN)) TogglePause(reg, data);
			else if (Input::GetButtonDown(Button::B) || Input::GetKeyDown(VK_BACK)) {
				Time::timeScale = 1.0f;
				AudioManager::Instance().StopBGM();
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/TitleScene.json", new FadeTransition(1.0f, { 0,0,0,1 }));
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::GameDirectorSystem, "GameDirectorSystem")