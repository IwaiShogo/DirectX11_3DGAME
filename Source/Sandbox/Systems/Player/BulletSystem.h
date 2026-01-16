#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Player/PlayerTime.h" // 追加
#include "Sandbox/Systems/Visual/FloatingTextSystem.h"
#include "Sandbox/Core/GameSession.h"
#include <vector>
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class BulletSystem : public ISystem
	{
	public:
		BulletSystem() { m_systemName = "BulletSystem"; m_group = SystemGroup::PlayOnly; }

		void DestroyRecursive(Registry& reg, Entity e) {
			if (reg.has<Relationship>(e)) {
				for (auto child : reg.get<Relationship>(e).children)
					if (reg.valid(child)) DestroyRecursive(reg, child);
			}
			reg.destroy(e);
		}

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();
			auto view = reg.view<Bullet, Transform>();
			std::vector<Entity> deadBullets;
			std::vector<Entity> deadEnemies;

			// プレイヤー情報キャッシュ
			Entity player = NullEntity;
			for (auto p : reg.view<PlayerTime, Transform>()) { player = p; break; }

			for (auto e : view)
			{
				auto& b = view.get<Bullet>(e);
				auto& t = view.get<Transform>(e);

				// 移動
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
				XMVECTOR fwd = XMVector3TransformCoord({ 0, 0, 1 }, rot);
				XMFLOAT3 fwdDir; XMStoreFloat3(&fwdDir, fwd);
				t.position.x += fwdDir.x * b.speed * dt;
				t.position.y += fwdDir.y * b.speed * dt;
				t.position.z += fwdDir.z * b.speed * dt;

				// 当たり判定
				bool hit = false;

				// Case 1: プレイヤーの弾 -> 敵
				if (b.owner == EntityType::Player) {
					float hitR = (reg.has<AttackAttribute>(e) && reg.get<AttackAttribute>(e).isPenetrate) ? 5.0f : 2.0f;
					for (auto target : reg.view<EnemyStats, Transform>()) {
						bool dead = false;
						for (auto de : deadEnemies) if (de == target) dead = true;
						if (dead) continue;

						auto& ePos = reg.get<Transform>(target).position;
						float dx = t.position.x - ePos.x; float dy = t.position.y - ePos.y; float dz = t.position.z - ePos.z;
						if (dx * dx + dy * dy + dz * dz < hitR * hitR) {
							auto& stats = reg.get<EnemyStats>(target);
							stats.hp -= b.damage;
							FloatingTextSystem::Spawn(reg, ePos, (int)b.damage, { 1,1,1,1 }, 0.8f);
							if (stats.hp <= 0.0f) {
								float rate = 1.0f; if (reg.has<AttackAttribute>(e)) rate = reg.get<AttackAttribute>(e).rewardRate;
								float rwd = stats.killReward * rate;
								if (reg.valid(player)) {
									auto& pt = reg.get<PlayerTime>(player);
									pt.currentTime += rwd; if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
									FloatingTextSystem::Spawn(reg, reg.get<Transform>(player).position, (int)rwd, { 0,1,0,1 }, 1.2f);
								}
								GameSession::lastScore += stats.scoreValue;
								deadEnemies.push_back(target);
							}
							if (!reg.get<AttackAttribute>(e).isPenetrate) { hit = true; break; }
						}
					}
				}
				// Case 2: 敵の弾 -> プレイヤー
				else if (b.owner == EntityType::Enemy && reg.valid(player)) {
					auto& pPos = reg.get<Transform>(player).position;
					float dx = t.position.x - pPos.x; float dy = t.position.y - pPos.y; float dz = t.position.z - pPos.z;
					if (dx * dx + dy * dy + dz * dz < 1.0f) { // プレイヤー判定小さめ
						auto& pt = reg.get<PlayerTime>(player);
						pt.currentTime -= b.damage; // 時間ダメージ
						FloatingTextSystem::Spawn(reg, pPos, (int)b.damage, { 1,0,0,1 }, 1.0f); // 赤字
						hit = true;
					}
				}

				if (hit) deadBullets.push_back(e);

				b.lifeTime -= dt;
				if (b.lifeTime <= 0.0f) deadBullets.push_back(e);
			}

			for (auto e : deadBullets) if (reg.valid(e)) reg.destroy(e);
			for (auto e : deadEnemies) if (reg.valid(e)) DestroyRecursive(reg, e);
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::BulletSystem, "BulletSystem")