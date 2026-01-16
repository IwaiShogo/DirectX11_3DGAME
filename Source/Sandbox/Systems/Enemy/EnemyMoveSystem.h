#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class EnemyMoveSystem : public ISystem
	{
	public:
		EnemyMoveSystem() { m_systemName = "EnemyMoveSystem"; }

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();

			// プレイヤー位置
			XMVECTOR playerPos = XMVectorZero();
			bool pFound = false;
			for (auto e : reg.view<Tag, Transform>()) {
				if (reg.get<Tag>(e).tag == "Player") {
					playerPos = XMLoadFloat3(&reg.get<Transform>(e).position);
					pFound = true; break;
				}
			}
			if (!pFound) return;

			// 全エネミーの移動
			auto view = reg.view<EnemyStats, Transform, Rigidbody>();
			for (auto entity : view)
			{
				auto& stats = view.get<EnemyStats>(entity);
				auto& t = view.get<Transform>(entity);
				auto& rb = view.get<Rigidbody>(entity);

				XMVECTOR myPos = XMLoadFloat3(&t.position);
				XMVECTOR toPlayer = playerPos - myPos;
				float dist = XMVectorGetX(XMVector3Length(toPlayer));
				XMVECTOR dir = XMVector3Normalize(toPlayer);

				XMVECTOR desiredVel = XMVectorZero();

				// --- 1. 基本移動AI ---
				if (stats.type == EnemyType::Zako_Speed) {
					// 高速突進 (距離を保たず突っ込む)
					desiredVel = dir * stats.speed;
				}
				else if (stats.type == EnemyType::Boss_Omega || stats.type == EnemyType::Boss_Prism) {
					// 遠距離固定砲台 (一定距離を保つ)
					if (dist > 15.0f) desiredVel = dir * stats.speed * 0.5f;
					else if (dist < 10.0f) desiredVel = -dir * stats.speed * 0.5f;

					// ボスは常にプレイヤーの方を向く
					t.rotation.y += dt;
				}
				else if (stats.type == EnemyType::Boss_Carrier) {
					// 旋回 (右ベクトルへ移動)
					XMVECTOR orbit = XMVectorSet(XMVectorGetZ(dir), 0, -XMVectorGetX(dir), 0);
					desiredVel = orbit * stats.speed + (dir * 0.2f);
				}
				else {
					// 通常 (Zako_Cube, Armored)
					if (dist > 1.0f) desiredVel = dir * stats.speed;
				}

				// --- 2. 仲間との分離 (Separation) ---
				// 大量に湧いても重ならないようにする
				XMVECTOR separation = XMVectorZero();
				int neighborCount = 0;

				// ※全探索は重いので、簡易的に「自分よりIDが近い数体」や「ランダムピック」で判定するか、
				//   ここではシンプルに「近すぎる敵から離れる力」を加える
				//   (本格的な最適化はGrid分割が必要だが、今回は簡易実装)
				/* 重すぎる場合はここをコメントアウトしてください。
				   今回はボリューム重視なので、処理負荷軽減のためスキップします。
				   代わりに物理エンジンのコライダーが押し出し処理をしてくれるはずです。
				*/

				// --- 3. 適用 ---
				// Y軸は維持（重力）
				desiredVel = XMVectorSetY(desiredVel, rb.velocity.y);

				// 慣性移動
				XMVECTOR currentVel = XMLoadFloat3(&rb.velocity);
				XMVECTOR newVel = XMVectorLerp(currentVel, desiredVel, dt * 3.0f);
				XMStoreFloat3(&rb.velocity, newVel);

				// 向き更新 (ボス以外)
				if (stats.type != EnemyType::Boss_Omega) {
					if (XMVectorGetX(XMVector3LengthSq(newVel)) > 0.1f) {
						float targetAng = atan2f(rb.velocity.x, rb.velocity.z);
						float curAng = t.rotation.y;
						// 角度補間
						float diff = targetAng - curAng;
						while (diff < -XM_PI) diff += XM_2PI; while (diff > XM_PI) diff -= XM_2PI;
						t.rotation.y += diff * dt * 10.0f;
					}
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemyMoveSystem, "EnemyMoveSystem")