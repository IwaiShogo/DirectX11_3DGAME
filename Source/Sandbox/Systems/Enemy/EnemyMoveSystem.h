#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

#include "Sandbox/Components/Enemy/EnemyStats.h"

namespace Arche
{
	class EnemyMoveSystem : public ISystem
	{
	public:
		EnemyMoveSystem() { m_systemName = "EnemyMoveSystem"; }

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();
			float time = Time::TotalTime();

			// 1. プレイヤー位置取得
			XMVECTOR playerPos = XMVectorZero();
			bool playerFound = false;
			auto tagView = registry.view<Tag, Transform>();
			for (auto e : tagView) {
				if (tagView.get<Tag>(e).tag == "Player") {
					playerPos = XMLoadFloat3(&tagView.get<Transform>(e).position);
					playerFound = true;
					break;
				}
			}
			if (!playerFound) return;

			// 2. 敵の移動ロジック
			auto view = registry.view<EnemyStats, Transform, Rigidbody>();
			for (auto entity : view)
			{
				auto& stats = view.get<EnemyStats>(entity);
				auto& trans = view.get<Transform>(entity);
				auto& rb = view.get<Rigidbody>(entity);

				XMVECTOR enemyPos = XMLoadFloat3(&trans.position);
				XMVECTOR toPlayer = playerPos - enemyPos;
				float distSq = XMVectorGetX(XMVector3LengthSq(toPlayer));
				float dist = sqrtf(distSq);

				XMVECTOR desiredVel = XMVectorZero();

				// タイプ別AI
				if (stats.type == EnemyType::Zako_Speed)
				{
					// 高速突撃: 偏差射撃のように未来位置を狙う（簡易的にそのまま突っ込む）
					desiredVel = XMVector3Normalize(toPlayer) * stats.speed;
				}
				else if (stats.type == EnemyType::Boss_Omega || stats.type == EnemyType::Boss_Prism)
				{
					// 固定砲台/要塞タイプ: ゆっくり近づくが、近づきすぎない
					float targetDist = 15.0f;
					if (dist > targetDist) {
						desiredVel = XMVector3Normalize(toPlayer) * (stats.speed * 0.5f);
					}
					else if (dist < targetDist - 2.0f) {
						// 近すぎたら少し下がる
						desiredVel = XMVector3Normalize(toPlayer) * -1.0f;
					}

					// ゆっくり回転 (威厳)
					trans.rotation.y += dt * 10.0f;
				}
				else if (stats.type == EnemyType::Boss_Carrier)
				{
					// 空母タイプ: プレイヤーの周りを旋回
					XMVECTOR dir = XMVector3Normalize(toPlayer);
					// 右ベクトル (Y軸回転)
					XMVECTOR orbitDir = XMVectorSet(dir.m128_f32[2], 0, -dir.m128_f32[0], 0);
					desiredVel = orbitDir * stats.speed + (dir * 0.5f); // 旋回しつつ少し寄る
				}
				else
				{
					// デフォルト (Zako_Cube, Boss_Tank等): 単純追跡
					if (dist > 0.5f) {
						desiredVel = XMVector3Normalize(toPlayer) * stats.speed;
					}
				}

				// Y軸（重力）維持
				desiredVel = XMVectorSetY(desiredVel, rb.velocity.y);

				// 加速適用 (慣性を持たせるためLerp)
				XMVECTOR currentVel = XMLoadFloat3(&rb.velocity);
				XMVECTOR newVel = XMVectorLerp(currentVel, desiredVel, dt * 5.0f);
				XMStoreFloat3(&rb.velocity, newVel);

				// 回転: 進行方向を向く (ボス以外)
				if (stats.type != EnemyType::Boss_Omega && stats.type != EnemyType::Boss_Prism)
				{
					if (fabs(rb.velocity.x) > 0.1f || fabs(rb.velocity.z) > 0.1f) {
						float targetAngle = XMConvertToDegrees(atan2f(rb.velocity.x, rb.velocity.z));
						// 角度補間
						float currentAngle = trans.rotation.y;
						float diff = targetAngle - currentAngle;
						// 180度またぎ処理省略 (簡易版)
						trans.rotation.y = std::lerp(currentAngle, targetAngle, dt * 10.0f);
					}
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemyMoveSystem, "EnemyMoveSystem")
