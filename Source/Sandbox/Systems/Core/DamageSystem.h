#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Physics/PhysicsEvents.h"
#include "Engine/Resource/PrefabManager.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class DamageSystem : public ISystem
	{
	public:
		DamageSystem() { m_systemName = "DamageSystem"; }

		void Update(Registry& registry) override
		{
			const auto& events = Physics::EventManager::Instance().GetEvents();
			for (const auto& ev : events)
			{
				if (ev.state != Physics::CollisionState::Enter) continue;
				Entity e1 = ev.self; Entity e2 = ev.other;
				if (registry.valid(e1) && registry.valid(e2)) {
					HandleCollision(registry, e1, e2);
					HandleCollision(registry, e2, e1);
				}
			}
		}

	private:
		void HandleCollision(Registry& reg, Entity bulletEntity, Entity targetEntity)
		{
			// 弾 vs 敵(または敵パーツ)
			if (!reg.has<Bullet>(bulletEntity)) return;

			// ターゲットがColliderを持っているか確認
			if (!reg.has<Collider>(targetEntity)) return;

			// EnemyStatsを探す
			// 1. 自分自身が持っているか？
			Entity hitEnemy = targetEntity;
			bool isPart = false;

			if (!reg.has<EnemyStats>(hitEnemy)) {
				// 2. 親が持っているか？ (パーツだがStatsがない場合、本体に判定を移譲)
				if (reg.has<Relationship>(hitEnemy)) {
					Entity parent = reg.get<Relationship>(hitEnemy).parent;
					if (reg.valid(parent) && reg.has<EnemyStats>(parent)) {
						hitEnemy = parent;
					}
				}
			}

			if (!reg.valid(hitEnemy) || !reg.has<EnemyStats>(hitEnemy)) return;

			// ダメージ計算
			auto& bullet = reg.get<Bullet>(bulletEntity);
			auto& stats = reg.get<EnemyStats>(hitEnemy);

			stats.hp -= bullet.damage;

			// ヒット演出
			if (reg.has<GeometricDesign>(targetEntity)) {
				reg.get<GeometricDesign>(targetEntity).flashTimer = 0.1f;
			}

			// 弾削除
			reg.destroy(bulletEntity);

			// 撃破判定
			if (stats.hp <= 0.0f)
			{
				// 親がいる場合（＝部位破壊されたパーツ）
				if (reg.has<Relationship>(hitEnemy) && reg.valid(reg.get<Relationship>(hitEnemy).parent))
				{
					Entity parent = reg.get<Relationship>(hitEnemy).parent;
					if (reg.has<EnemyStats>(parent)) {
						// 本体にも大ダメージ！
						reg.get<EnemyStats>(parent).hp -= bullet.damage * 5.0f;
						Logger::Log("Part Destroyed! Critical Damage to Boss!");
					}
					// パーツ消滅
					reg.destroy(hitEnemy);
				}
				else
				{
					// 本体撃破
					KillEnemy(reg, hitEnemy);
				}
			}
		}

		void KillEnemy(Registry& reg, Entity enemyEntity)
		{
			float reward = 0.0f;
			if (reg.has<EnemyStats>(enemyEntity)) reward = reg.get<EnemyStats>(enemyEntity).killReward;

			// プレイヤー時間回復
			auto view = reg.view<PlayerTime>();
			for (auto e : view) {
				auto& pt = view.get<PlayerTime>(e);
				if (!pt.isDead) {
					pt.currentTime += reward;
					if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
				}
			}

			// 全ての子要素も道連れに消す
			DestroyRecursive(reg, enemyEntity);
		}

		void DestroyRecursive(Registry& reg, Entity e)
		{
			if (reg.has<Relationship>(e)) {
				auto& children = reg.get<Relationship>(e).children;
				for (auto c : children) if (reg.valid(c)) DestroyRecursive(reg, c);
			}
			reg.destroy(e);
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::DamageSystem, "DamageSystem")
