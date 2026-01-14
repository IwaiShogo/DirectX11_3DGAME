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
			m_timer += Time::DeltaTime();

			int enemyCount = (int)registry.view<EnemyStats>().size();

			if (enemyCount < 10 && m_timer > 3.0f)
			{
				SpawnEnemy(registry);
				m_timer = 0.0f;
			}
		}

	private:
		float m_timer = 0.0f;

		void SpawnEnemy(Registry& registry)
		{
			XMFLOAT3 pos = GetRandomPosition();

			std::string prefabName = (rand() % 100 < 20) ? "Enemy_Strong" : "Enemy_Normal";

			Entity e = PrefabManager::Instance().Spawn(registry, prefabName, pos);

			if (e == NullEntity)
			{
				Logger::Log("Prefab not found: " + prefabName);
			}
		}

		XMFLOAT3 GetRandomPosition()
		{
			// プレイヤーの周囲
			float r = 10.0f + (rand() % 10);
			float angle = (rand() % 360) * 3.14159f / 180.0f;
			return { cosf(angle) * r, 0.5f, sinf(angle) * r };
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemySpawnSystem, "EnemySpawnSystem")
