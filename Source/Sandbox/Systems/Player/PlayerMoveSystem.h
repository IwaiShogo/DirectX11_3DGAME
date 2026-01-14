#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

#include "Sandbox/Components/Player/PlayerMoveData.h"
#include "Sandbox/Components/Player/PlayerController.h"

namespace Arche
{
	class PlayerMoveSystem : public ISystem
	{
	public:
		PlayerMoveSystem()
		{
			m_systemName = "PlayerMoveSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 1. カメラの基準方向を取得
			// (カメラが回転していても、操作は見た目通りにするため)
			XMVECTOR camForward = XMVectorSet(0, 0, 1, 0);
			XMVECTOR camRight = XMVectorSet(1, 0, 0, 0);

			auto cameras = registry.view<Camera, Transform>();
			for (auto e : cameras)
			{
				auto& t = cameras.get<Transform>(e);
				// カメラの回転行列
				XMMATRIX camRot = XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(t.rotation.x),
					XMConvertToRadians(t.rotation.y),
					0.0f);

				camForward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), camRot);
				camRight = XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), camRot);

				// 水平移動のみにしたいのでY成分を消して正規化
				camForward = XMVector3Normalize(XMVectorSetY(camForward, 0.0f));
				camRight = XMVector3Normalize(XMVectorSetY(camRight, 0.0f));
				break; // メインカメラ1つだけと想定
			}

			// 2. プレイヤーの制御
			auto view = registry.view<PlayerMoveData, PlayerController, Rigidbody, Transform>();
			for (auto entity : view)
			{
				auto& moveData = view.get<PlayerMoveData>(entity);
				auto& ctrl = view.get<PlayerController>(entity);
				auto& rb = view.get<Rigidbody>(entity);
				auto& trans = view.get<Transform>(entity);

				if (ctrl.state == PlayerState::Dash || ctrl.state == PlayerState::Drift)
				{
					continue;
				}

				// --- 入力取得 ---
				float h = Input::GetAxis(Axis::Horizontal);
				float v = Input::GetAxis(Axis::Vertical);

				// --- 移動ベクトル計算 ---
				XMVECTOR moveDir = (camForward * v) + (camRight * h);
				float inputLen = XMVector3LengthSq(moveDir).m128_f32[0];

				if (inputLen > 0.01f)
				{
					moveDir = XMVector3Normalize(moveDir);

					// 現在の進行方向を保存
					XMStoreFloat3(&ctrl.moveDirection, moveDir);

					// ダッシュ判定 (Shiftキー)
					bool isRunning = Input::GetKey(VK_SHIFT) || Input::GetButton(Button::LShoulder);
					float speed = isRunning ? moveData.runSpeed : moveData.walkSpeed;

					// 速度適用 (Y軸＝重力は維持するため、XとZだけ書き換える)
					XMVECTOR velocity = moveDir * speed;
					rb.velocity.x = velocity.m128_f32[0];
					rb.velocity.z = velocity.m128_f32[2];

					// --- 回転（進行方向を向く）---
					float targetAngle = XMConvertToDegrees(atan2f(rb.velocity.x, rb.velocity.z));

					// 現在の角度とターゲット角度の差分を計算してスムーズに補間
					float currentAngle = trans.rotation.y;
					float angleDiff = targetAngle - currentAngle;

					// -180~180に正規化
					while (angleDiff > 180.0f) angleDiff -= 360.0f;
					while (angleDiff < -180.0f) angleDiff += 360.0f;

					trans.rotation.y += angleDiff * moveData.rotationSpeed * dt;
					ctrl.state = PlayerState::Run;
				}
				else
				{
					// 入力なし：即座に停止（慣性を残したい場合はここを調整）
					rb.velocity.x = 0.0f;
					rb.velocity.z = 0.0f;
					ctrl.state = PlayerState::Idle;
				}

				// --- ジャンプ ---
				if (Input::GetButtonDown(Button::A) && rb.isGrounded)
				{
					rb.velocity.y = moveData.jumpPower;
					rb.isGrounded = false; // 接地フラグ解除（物理システム側で再判定されるまで）
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerMoveSystem, "PlayerMoveSystem")
