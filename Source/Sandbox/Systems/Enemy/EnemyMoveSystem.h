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
		EnemyMoveSystem()
		{
			m_systemName = "EnemyMoveSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 1. プレイヤーの位置を探す
			XMVECTOR playerPos = XMVectorZero();
			bool playerFound = false;

			// Tagで検索
			auto tagView = registry.view<Tag, Transform>();
			for (auto e : tagView)
			{
				if (tagView.get<Tag>(e).tag == "Player")
				{
					playerPos = XMLoadFloat3(&tagView.get<Transform>(e).position);
					playerFound = true;
					break;
				}
			}

			// プレイヤーがいなければ何もしない（Game Over時など）
			if (!playerFound) return;

			// 2. 全ての敵を移動させる
			auto view = registry.view<EnemyStats, Transform, Rigidbody>();
			for (auto entity : view)
			{
				auto& stats = view.get<EnemyStats>(entity);
				auto& trans = view.get<Transform>(entity);
				auto& rb = view.get<Rigidbody>(entity);

				XMVECTOR enemyPos = XMLoadFloat3(&trans.position);

				// 方向ベクトル計算 (Player - Enemy)
				XMVECTOR dir = playerPos - enemyPos;

				// Y軸（高さ）の差は無視して水平移動させる
				dir = XMVectorSetY(dir, 0.0f);

				// 距離の二乗（近すぎたら止まるなどの判定用）
				float distSq = XMVectorGetX(XMVector3LengthSq(dir));

				// 0.5m以上離れていたら追いかける
				if (distSq > 0.25f)
				{
					dir = XMVector3Normalize(dir);

					// 移動 (Velocity更新)
					XMVECTOR velocity = dir * stats.moveSpeed;

					// Y軸の速度（重力）は維持する
					velocity = XMVectorSetY(velocity, rb.velocity.y);

					XMStoreFloat3(&rb.velocity, velocity);

					// 回転 (LookAt)
					// 進行方向を向く
					float targetAngle = XMConvertToDegrees(atan2f(rb.velocity.x, rb.velocity.z));

					// 現在角度からスムーズに補間しても良いが、今回は即時反映
					trans.rotation.y = targetAngle;
				}
				else
				{
					// 到着したら停止
					rb.velocity.x = 0.0f;
					rb.velocity.z = 0.0f;
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemyMoveSystem, "EnemyMoveSystem")
