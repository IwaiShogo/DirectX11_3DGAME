#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Physics/PhysicsEvents.h"

#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class DamageSystem : public ISystem
	{
	public:
		DamageSystem()
		{
			m_systemName = "DamageSystem";
		}

		void Update(Registry& registry) override
		{
			// 発生した全衝突イベントを取得
			const auto& events = Physics::EventManager::Instance().GetEvents();

			for (const auto& ev : events)
			{
				// 「当たった瞬間 (Enter)」だけ処理する
				if (ev.state != Physics::CollisionState::Enter) continue;

				Entity e1 = ev.self;
				Entity e2 = ev.other;

				// どちらが弾で、どちらが敵かを確認して処理関数へ
				if (registry.valid(e1) && registry.valid(e2))
				{
					HandleCollision(registry, e1, e2);
					HandleCollision(registry, e2, e1); // 逆パターンもチェック
				}
			}
		}

	private:
		void HandleCollision(Registry& reg, Entity bulletEntity, Entity targetEntity)
		{
			// 1. bulletEntity は弾か？
			if (!reg.has<Bullet>(bulletEntity)) return;
			auto& bullet = reg.get<Bullet>(bulletEntity);

			// 2. targetEntity は敵か？
			if (!reg.has<EnemyStats>(targetEntity)) return;
			auto& enemy = reg.get<EnemyStats>(targetEntity);

			// 3. オーナーチェック（プレイヤーの弾がプレイヤーに当たらないように）
			// 今回は EnemyStats がついていれば敵確定なので、そのまま処理します

			// --- ダメージ処理 ---

			// HPを減らす
			enemy.hp -= bullet.damage;
			Logger::Log("Hit! Enemy HP: " + std::to_string(enemy.hp));

			// ヒット演出 (敵を赤く光らせる)
			if (reg.has<GeometricDesign>(targetEntity))
			{
				auto& geo = reg.get<GeometricDesign>(targetEntity);
				geo.flashTimer = 0.2f; // 一瞬光る
			}

			// 弾を削除 (貫通させたい場合はここを削除条件付きにする)
			reg.destroy(bulletEntity);

			// 敵の死亡判定
			if (enemy.hp <= 0.0f)
			{
				KillEnemy(reg, targetEntity);
			}
		}

		void KillEnemy(Registry& reg, Entity enemyEntity)
		{
			Logger::Log("Enemy Destroyed!");

			// 将来的にはパーティクルなどを出したい場所

			// 敵を削除
			reg.destroy(enemyEntity);
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::DamageSystem, "DamageSystem")
