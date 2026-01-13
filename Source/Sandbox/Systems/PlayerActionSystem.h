#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/PlayerData.h"
#include "Sandbox/Components/GameProgression.h"

namespace Arche
{
	class PlayerActionSystem : public ISystem
	{
	public:
		PlayerActionSystem()
		{
			m_systemName = "PlayerActionSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// プレイヤー処理
			registry.view<PlayerController, Transform, Rigidbody>().each([&](Entity e, PlayerController& ctrl, Transform& trans, Rigidbody& rb) {
				// 1. 入力取得
				float h = Input::GetAxis(Axis::Horizontal);
				float v = Input::GetAxis(Axis::Vertical);
				XMVECTOR inputVec = XMVectorSet(h, 0, v, 0);

				// 入力がある場合のみ正規化 
				if (XMVector3LengthSq(inputVec).m128_f32[0] > 0.1f)
				{
					inputVec = XMVector3Normalize(inputVec);
				}

				// 2. 状態遷移ロジック
				switch (ctrl.state)
				{
				case PlayerState::Idle:
				case PlayerState::Run:
					HandleMovement(ctrl, trans, inputVec, dt);

					// ダッシュ斬り: Aボタン
					if (Input::GetButtonDown(Button::A))
					{
						ctrl.state = PlayerState::Dash;
						ctrl.actionTimer = 0.0f;
						// ダッシュ方向決定
						if (XMVector3LengthSq(inputVec).m128_f32[0] < 0.1f)
						{

						}
					}
					break;

				case PlayerState::Dash:
					UpdateDash(registry, e, ctrl, inputVec, dt);
					break;
				}
				});
		}

	private:
		// ドリフト衝撃波の計算ロジック
		void UpdateDash(Registry& reg, Entity entity, PlayerController& ctrl, XMVECTOR inputVec, float dt)
		{
			ctrl.actionTimer += dt;

			// ダッシュ終了判定
			if (ctrl.actionTimer >= ctrl.dashDuration)
			{
				ctrl.state = PlayerState::Idle;
				return;
			}

			// ドリフト判定の実装
			// 現在の進行方向
			XMVECTOR currentDir = XMLoadFloat3(&ctrl.moveDirection);

			// 入力ベクトルとの角度差を計算
			if (XMVector3LengthSq(inputVec).m128_f32[0] > 0.5f)
			{
				float dot = XMVector3Dot(currentDir, inputVec).m128_f32[0];

				if (dot < 0.5f && !ctrl.isDrifting)
				{
					TriggerDriftShockwave(reg, entity);
					ctrl.isDrifting = true;
				}
			}
		}

		void TriggerDriftShockwave(Registry& reg, Entity playerEntity)
		{
			Logger::Log("Drift Shockwave Triggerd!");

			//TimeManagerSystem::RequestHitStop(0.1f);
		}

		void HandleMovement(PlayerController& ctrl, Transform& t, XMVECTOR input, float dt)
		{
			// 通常移動ロジック
			if (XMVector3LengthSq(input).m128_f32[0] > 0.1f)
			{
				XMStoreFloat3(&ctrl.moveDirection, input);
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerActionSystem, "PlayerActionSystem")
