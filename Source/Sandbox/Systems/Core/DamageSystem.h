#pragma once
#undef PlaySound
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Physics/PhysicsEvents.h"
#include "Engine/Audio/AudioManager.h" // ★追加
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Systems/Visual/FloatingTextSystem.h" 

namespace Arche
{
	class DamageSystem : public ISystem
	{
	public:
		DamageSystem() { m_systemName = "DamageSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			const auto& events = Physics::EventManager::Instance().GetEvents();
			for (const auto& ev : events) {
				if (ev.state != Physics::CollisionState::Enter) continue;
				if (reg.valid(ev.self) && reg.valid(ev.other)) {
					HandleCollision(reg, ev.self, ev.other);
					HandleCollision(reg, ev.other, ev.self);
				}
			}
		}

	private:
		// ★サウンド再生ヘルパー
		void PlaySound(Registry& reg, std::string key, XMFLOAT3 pos, float vol = 1.0f) {
			XMFLOAT3 listenerPos = pos;
			for (auto e : reg.view<AudioListener, Transform>()) {
				listenerPos = reg.get<Transform>(e).position;
				break;
			}
			AudioManager::Instance().Play3DSE(key, pos, listenerPos, 50.0f, vol);
		}

		void HandleCollision(Registry& reg, Entity attacker, Entity defender)
		{
			if (!reg.has<AttackAttribute>(attacker)) return;
			if (!reg.has<EnemyStats>(defender)) return;

			auto& attr = reg.get<AttackAttribute>(attacker);
			auto& stats = reg.get<EnemyStats>(defender);
			auto& pos = reg.get<Transform>(defender).position;

			stats.hp -= attr.damage;
			FloatingTextSystem::Spawn(reg, pos, (int)attr.damage, { 1, 1, 1, 1 }, 0.8f);

			// ヒット音
			PlaySound(reg, "se_hit.wav", pos, 0.7f);

			if (!attr.isPenetrate) reg.destroy(attacker);

			if (stats.hp <= 0.0f) {
				float finalReward = stats.killReward * attr.rewardRate;
				if (finalReward > 0) {
					for (auto p : reg.view<PlayerTime, Transform>()) {
						FloatingTextSystem::Spawn(reg, reg.get<Transform>(p).position, (int)finalReward, { 0, 1, 0, 1 }, 1.5f);
					}
				}
				RecoverTime(reg, finalReward);
				SpawnExplosion(reg, pos);

				// 爆発音
				PlaySound(reg, "se_explosion.wav", pos, 1.0f);

				reg.destroy(defender);
			}
		}

		void RecoverTime(Registry& reg, float amount) {
			for (auto e : reg.view<PlayerTime>()) {
				auto& pt = reg.get<PlayerTime>(e);
				if (!pt.isDead) {
					pt.currentTime += amount;
					if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
				}
			}
		}

		void SpawnExplosion(Registry& reg, DirectX::XMFLOAT3 pos) {
			for (int i = 0; i < 5; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = {
					pos.x + (rand() % 100 / 100.0f - 0.5f), pos.y, pos.z + (rand() % 100 / 100.0f - 0.5f)
				};
				reg.get<Transform>(e).scale = { 0.8f, 0.8f, 0.8f };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube; g.color = { 0, 1, 1, 1 }; g.isWireframe = true;
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::DamageSystem, "DamageSystem")