#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Resource/PrefabManager.h"

#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <random>

namespace Arche
{
	class EnemySpawnSystem : public ISystem
	{
	public:
		EnemySpawnSystem()
		{
			m_systemName = "EnemySpawnSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 1. スポーンタイマー更新
			m_timer += dt;
			if (m_timer < m_spawnInterval) return;

			// 2. プレイヤーの位置を取得（中心にするため）
			XMVECTOR centerPos = XMVectorZero();
			bool playerFound = false;

			auto view = registry.view<Tag, Transform>();
			for (auto e : view)
			{
				if (view.get<Tag>(e).tag == "Player")
				{
					centerPos = XMLoadFloat3(&view.get<Transform>(e).position);
					playerFound = true;
					break;
				}
			}

			if (!playerFound) return;

			// 3. 敵の数を制限（増えすぎ防止）
			// 現在の敵の数をカウント
			int enemyCount = 0;
			auto enemyView = registry.view<EnemyStats>();
			for (auto e : enemyView) enemyCount++;

			if (enemyCount < m_maxEnemies)
			{
				SpawnEnemy(registry, centerPos);
				m_timer = 0.0f; // タイマーリセット

				// 時間経過で少しずつ間隔を短くする（難易度上昇）
				if (m_spawnInterval > 0.5f) m_spawnInterval -= 0.01f;
			}
		}

	private:
		// パラメータ
		float m_timer = 0.0f;
		float m_spawnInterval = 3.0f; // 最初は3秒ごとに湧く
		int m_maxEnemies = 50;		  // 画面内に最大50体まで
		float m_spawnRadius = 20.0f;  // プレイヤーから20m離れた位置に湧く

		void SpawnEnemy(Registry& registry, XMVECTOR centerPos)
		{
			// ランダムな位置を計算
			// (乱数生成器はstaticにして使い回す)
			static std::random_device rd;
			static std::mt19937 gen(rd());
			static std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI); // 0 ~ 360度

			float angle = distAngle(gen);

			// 円周上の座標を求める (Yは0固定)
			float x = cosf(angle) * m_spawnRadius;
			float z = sinf(angle) * m_spawnRadius;
			XMVECTOR offset = XMVectorSet(x, 0.0f, z, 0.0f);

			XMVECTOR spawnPos = centerPos + offset;
			XMFLOAT3 pos;
			XMStoreFloat3(&pos, spawnPos);

			std::string prefabName = (rand() % 100 < 20) ? "Enemy_Strong" : "Enemy_Normal";

			// プレファブから生成
			// ※"Enemy_Normal" は既存のプレファブ名に合わせてください
			PrefabManager::Instance().Spawn(registry, prefabName, pos, { 0,0,0 });

			// ログ（デバッグ用）
			// Logger::Log("Spawned Enemy at " + std::to_string(pos.x) + ", " + std::to_string(pos.z));
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemySpawnSystem, "EnemySpawnSystem")
