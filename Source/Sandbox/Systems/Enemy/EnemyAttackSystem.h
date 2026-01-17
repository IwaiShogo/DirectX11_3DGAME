#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Engine/Audio/AudioManager.h"
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class EnemyAttackSystem : public ISystem
	{
	public:
		EnemyAttackSystem() { m_systemName = "EnemyAttackSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();

			XMFLOAT3 pPos = { 0,0,0 };
			bool pFound = false;
			for (auto e : reg.view<PlayerTime, Transform>()) { pPos = reg.get<Transform>(e).position; pFound = true; break; }
			if (!pFound) return;

			auto view = reg.view<EnemyStats, Transform>();
			for (auto e : view)
			{
				auto& stats = view.get<EnemyStats>(e);
				auto& t = view.get<Transform>(e);

				if (stats.type == EnemyType::Boss_Tank || stats.type == EnemyType::Boss_Prism ||
					stats.type == EnemyType::Boss_Carrier || stats.type == EnemyType::Boss_Construct ||
					stats.type == EnemyType::Boss_Omega || stats.type == EnemyType::Boss_Titan)
				{
					continue;
				}

				stats.attackTimer -= dt;
				float distSq = pow(pPos.x - t.position.x, 2) + pow(pPos.z - t.position.z, 2);
				float range = 40.0f;
				if (stats.type == EnemyType::Zako_Cube) range = 0.0f;

				if (distSq < range * range && stats.attackTimer <= 0.0f)
				{
					SpawnBullet(reg, t.position, pPos, stats.type);
					stats.attackTimer = 3.0f + (rand() % 100) / 50.0f;
				}
			}
		}

	private:
		void SpawnBullet(Registry& reg, XMFLOAT3 start, XMFLOAT3 target, EnemyType type)
		{
			Entity b = reg.create();

			XMVECTOR vStart = XMLoadFloat3(&start);
			vStart += XMVectorSet(0, 1.0f, 0, 0); // 少し低めから発射

			XMVECTOR vTarget = XMLoadFloat3(&target);
			vTarget += XMVectorSet(0, 0.5f, 0, 0);

			XMVECTOR vDir = XMVector3Normalize(vTarget - vStart);
			XMFLOAT3 dir; XMStoreFloat3(&dir, vDir);

			// ★修正: オフセット距離を短縮 (4.0f -> 1.5f)
			// 雑魚敵のコライダー半径(約1.0)より少し外に出れば十分です
			float offsetDistance = 1.5f;
			XMVECTOR vSpawnPos = vStart + vDir * offsetDistance;
			XMFLOAT3 spawnPos; XMStoreFloat3(&spawnPos, vSpawnPos);

			reg.emplace<Transform>(b).position = spawnPos;

			// ★修正: 弾のサイズを適切な大きさに縮小 (3.0 -> 1.0)
			reg.get<Transform>(b).scale = { 1.0f, 1.0f, 1.0f };

			float pitch = -asinf(dir.y);
			float yaw = atan2f(dir.x, dir.z);
			reg.get<Transform>(b).rotation = { pitch, yaw, 0 };

			auto& g = reg.emplace<GeometricDesign>(b);
			g.shapeType = GeoShape::Sphere;
			g.color = { 1, 0.2f, 0.2f, 1 };
			g.isWireframe = false;

			auto& attr = reg.emplace<AttackAttribute>(b);
			attr.damage = 1.0f;
			attr.isPenetrate = false;

			auto& bul = reg.emplace<Bullet>(b);
			bul.damage = 1.0f;
			bul.speed = 18.0f;
			bul.lifeTime = 5.0f;
			bul.owner = EntityType::Enemy;

			// ★修正: 当たり判定の半径をサイズに合わせて調整
			reg.emplace<Collider>(b, Collider::CreateSphere(0.5f, Layer::Enemy));

			AudioManager::Instance().Play3DSE("se_shot", spawnPos, target, 30.0f, 0.4f);

			auto& t = reg.get<Transform>(b);
			XMMATRIX worldMat = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
				XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) * XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
			XMStoreFloat4x4(&t.worldMatrix, worldMat);
		}
	};
}