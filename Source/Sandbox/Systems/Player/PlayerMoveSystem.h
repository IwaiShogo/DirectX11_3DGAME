#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Player/PlayerMoveData.h"
#include "Sandbox/Systems/Game/FieldSystem.h"
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class PlayerMoveSystem : public ISystem
	{
	public:
		PlayerMoveSystem() { m_systemName = "PlayerMoveSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();

			// フィールド情報取得
			float fieldRadius = 20.0f;
			float floorY = -2.0f;
			for (auto e : reg.view<FieldProperties>()) {
				auto& p = reg.get<FieldProperties>(e);
				fieldRadius = p.radius;
				floorY = p.floorHeight;
				break;
			}

			auto view = reg.view<PlayerController, Transform, Rigidbody>();
			for (auto e : view)
			{
				auto& t = reg.get<Transform>(e);
				auto& move = reg.get<Rigidbody>(e);

				// --- 入力受付 (キーボード & コントローラー) ---
				float inputX = 0.0f;
				float inputZ = 0.0f;

				// キーボード
				if (Input::GetKey('W')) inputZ += 1.0f;
				if (Input::GetKey('S')) inputZ -= 1.0f;
				if (Input::GetKey('A')) inputX -= 1.0f;
				if (Input::GetKey('D')) inputX += 1.0f;

				// コントローラー (左スティック)
				float stickX = Input::GetAxis(Axis::Horizontal);
				float stickY = Input::GetAxis(Axis::Vertical);
				if (fabs(stickX) > 0.1f) inputX = stickX;
				if (fabs(stickY) > 0.1f) inputZ = stickY;

				// ベクトル正規化
				float len = sqrt(inputX * inputX + inputZ * inputZ);
				if (len > 1.0f) { inputX /= len; inputZ /= len; } // 1.0を超えないように（スティック対策）

				// --- 移動計算 ---
				float accel = 100.0f;
				float drag = 10.0f;
				// ダッシュ (Shift or ボタンB)
				if (Input::GetKey(VK_SHIFT) || Input::GetButton(Button::B)) accel *= 1.8f;

				// 速度加算
				move.velocity.x += inputX * accel * dt;
				move.velocity.z += inputZ * accel * dt;

				// 摩擦（減衰）
				move.velocity.x -= move.velocity.x * drag * dt;
				move.velocity.z -= move.velocity.z * drag * dt;

				// 座標更新
				t.position.x += move.velocity.x * dt;
				t.position.z += move.velocity.z * dt;

				// --- 向き更新 ---
				if (len > 0.1f) {
					float targetAngle = atan2f(inputX, inputZ);
					float curAngle = t.rotation.y;
					float diff = targetAngle - curAngle;
					// 角度補間
					while (diff < -XM_PI) diff += XM_2PI;
					while (diff > XM_PI) diff -= XM_2PI;
					t.rotation.y += diff * dt * 15.0f; // 素早く向く
				}

				// --- 重力 ---
				// ジャンプ (Space or ボタンA)
				if (t.position.y <= floorY + 0.1f && (Input::GetKeyDown(VK_SPACE) || Input::GetButtonDown(Button::A))) {
					move.velocity.y = 15.0f;
				}
				move.velocity.y -= 40.0f * dt;
				t.position.y += move.velocity.y * dt;

				// --- 衝突判定: 床 ---
				if (t.position.y < floorY + 2.5f) {
					t.position.y = floorY + 2.5f;
					move.velocity.y = 0;
				}

				// --- 衝突判定: 壁 ---
				float distSq = t.position.x * t.position.x + t.position.z * t.position.z;
				float limitR = fieldRadius - 1.0f;
				if (distSq > limitR * limitR) {
					float dist = sqrt(distSq);
					float pushX = -t.position.x / dist;
					float pushZ = -t.position.z / dist;
					t.position.x = -pushX * limitR;
					t.position.z = -pushZ * limitR;

					// 壁ずり
					float dot = move.velocity.x * pushX + move.velocity.z * pushZ;
					if (dot < 0) {
						move.velocity.x -= pushX * dot;
						move.velocity.z -= pushZ * dot;
					}
				}
			}

			for (auto e : reg.view<EnemyStats, Transform>()) {
				auto& t = reg.get<Transform>(e);
				auto type = reg.get<EnemyStats>(e).type;

				float enemyFloor = floorY;
				// 浮遊する敵は少し高く
				if (type == EnemyType::Boss_Omega || type == EnemyType::Boss_Carrier) enemyFloor = 5.0f;

				if (t.position.y < enemyFloor) t.position.y = enemyFloor;
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerMoveSystem, "PlayerMoveSystem")