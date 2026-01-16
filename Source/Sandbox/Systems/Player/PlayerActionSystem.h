#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

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
			auto view = registry.view<PlayerController, Transform, Rigidbody>();
			for (auto entity : view)
			{
				auto& ctrl = view.get<PlayerController>(entity);
				auto& trans = view.get<Transform>(entity);
				auto& rb = view.get<Rigidbody>(entity);

				// 1. アクション開始判定 (Idle または Run のときのみ)
				if (ctrl.state == PlayerState::Idle || ctrl.state == PlayerState::Run)
				{
					// ダッシュ開始: Bボタン, Rボタン, 右クリックなど
					if (Input::GetButtonDown(Button::B) || Input::GetButtonDown(Button::RShoulder) || Input::GetMouseRightButton())
					{
						StartDash(ctrl, rb);
					}
					// 攻撃開始: Xボタン, 左クリック
					else if (Input::GetButtonDown(Button::X) || Input::GetMouseLeftButton())
					{
						ctrl.state = PlayerState::Attack;
						ctrl.actionTimer = 0.0f;
						
						// 弾の発射処理
						Shoot(registry, trans, ctrl);
					}
				}

				// 2. アクション中の更新処理
				switch (ctrl.state)
				{
				case PlayerState::Dash:
				case PlayerState::Drift:
					UpdateDash(registry, entity, ctrl, rb, trans, dt);
					break;

				case PlayerState::Attack:
					UpdateAttack(ctrl, dt);
					break;
				}
			}
		}

	private:
		void Shoot(Registry& reg, const Transform& playerTrans, const PlayerController& ctrl) // 引数にctrl追加
		{
			XMMATRIX world = playerTrans.GetWorldMatrix();
			XMVECTOR spawnPos = XMVector3TransformCoord(XMVectorSet(0, 0.0f, 1.0f, 1.0f), world);
			XMFLOAT3 pos;
			XMStoreFloat3(&pos, spawnPos);

			// 発射方向の決定
			XMVECTOR shootDir;
			XMMATRIX shootRot;

			// ターゲットがいる場合、そちらを向く
			if (reg.valid(ctrl.focusTarget) && reg.has<Transform>(ctrl.focusTarget))
			{
				XMVECTOR targetPos = XMLoadFloat3(&reg.get<Transform>(ctrl.focusTarget).position);
				// 偏差射撃(相手の移動先予測)は一旦無しで、相手の中心を狙う
				targetPos = XMVectorSetY(targetPos, pos.y); // 高さは水平補正（弾が重力落下しないなら）

				XMVECTOR dir = XMVector3Normalize(targetPos - XMLoadFloat3(&pos));

				// 方向ベクトルから回転行列（LookAtの逆）を作成
				// 簡易的に atan2 で Y軸回転のみ計算（水平射撃）
				float angle = atan2f(dir.m128_f32[0], dir.m128_f32[2]);
				shootRot = XMMatrixRotationY(angle);
			}
			else
			{
				// 通常：プレイヤーの向き
				shootRot = XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(playerTrans.rotation.x),
					XMConvertToRadians(playerTrans.rotation.y),
					0.0f);
			}

			// プレファブ生成
			XMFLOAT3 spawnRot = playerTrans.rotation;
			if (reg.valid(ctrl.focusTarget))
			{
				// ターゲット方向の角度を計算して代入
				XMVECTOR targetPos = XMLoadFloat3(&reg.get<Transform>(ctrl.focusTarget).position);
				XMVECTOR dir = targetPos - spawnPos;
				spawnRot.y = XMConvertToDegrees(atan2f(dir.m128_f32[0], dir.m128_f32[2]));
				spawnRot.x = 0.0f; // 水平発射
			}

			Entity b = PrefabManager::Instance().Spawn(reg, "Bullet_Normal", pos, spawnRot);

			if (b != NullEntity)
			{
				Logger::Log("Bang!");
			}
		}

		void StartDash(PlayerController& ctrl, Rigidbody& rb)
		{
			ctrl.state = PlayerState::Dash;
			ctrl.actionTimer = 0.0f;
			ctrl.isDrifting = false;

			// 現在の移動方向（入力がなければ前方）へ加速
			XMVECTOR dir = XMLoadFloat3(&ctrl.moveDirection);
			if (XMVector3LengthSq(dir).m128_f32[0] < 0.1f)
			{
				// 停止中ならZ+方向へ
				dir = XMVectorSet(0, 0, 1, 0);
				XMStoreFloat3(&ctrl.moveDirection, dir);
			}

			// 初速を与える
			XMVECTOR vel = dir * ctrl.dashSpeed;
			rb.velocity.x = vel.m128_f32[0];
			rb.velocity.z = vel.m128_f32[2];
		}

		void UpdateDash(Registry& reg, Entity entity, PlayerController& ctrl, Rigidbody& rb, Transform& trans, float dt)
		{
			ctrl.actionTimer += dt;

			// ダッシュ終了
			if (ctrl.actionTimer >= ctrl.dashDuration)
			{
				ctrl.state = PlayerState::Idle;
				rb.velocity = { 0, rb.velocity.y, 0 }; // 慣性を消して停止

				// 演出リセット
				if (reg.has<GeometricDesign>(entity))
				{
					auto& geo = reg.get<GeometricDesign>(entity);
					geo.isWireframe = false;
					geo.pulseSpeed = 0.0f;
				}
				return;
			}

			// --- ドリフト判定 ---
			// 現在の進行ベクトル(ダッシュ方向)
			XMVECTOR dashDir = XMLoadFloat3(&ctrl.moveDirection);

			// 現在の入力ベクトル
			float h = Input::GetAxis(Axis::Horizontal);
			float v = Input::GetAxis(Axis::Vertical);

			// カメラ補正 (MoveSystemと同様のロジックが必要だが、簡易的に入力そのままで判定)
			// ※厳密にはカメラ向きを考慮すべきですが、急激な入力変化検知なら生の値でも機能します
			XMVECTOR inputVec = XMVectorSet(h, 0, v, 0);

			if (XMVector3LengthSq(inputVec).m128_f32[0] > 0.5f)
			{
				inputVec = XMVector3Normalize(inputVec);

				// 内積: ダッシュ方向と入力方向のズレを見る
				// ここでは「カメラ考慮なし」の簡易判定なので、本来はMoveSystemと同じカメラ補正後のベクトルを使うのがベストです
				// とりあえず「何かしら強い入力」があったらドリフトとみなす簡易版：

				// ドリフト発動条件: ダッシュ後半かつ未発動
				if (ctrl.actionTimer > ctrl.dashDuration * 0.2f && !ctrl.isDrifting)
				{
					// 入力方向へ急旋回（ドリフト）
					TriggerDriftShockwave(reg, entity);
					ctrl.isDrifting = true;
					ctrl.state = PlayerState::Drift;

					// 進行方向を入力方向へ書き換え（鋭角ターン）
					// ※本来はカメラ補正後のベクトルを入れるべき
					XMStoreFloat3(&ctrl.moveDirection, inputVec);

					// 速度ベクトルも更新
					XMVECTOR newVel = inputVec * ctrl.dashSpeed;
					rb.velocity.x = newVel.m128_f32[0];
					rb.velocity.z = newVel.m128_f32[2];
				}
			}
		}

		void UpdateAttack(PlayerController& ctrl, float dt)
		{
			ctrl.actionTimer += dt;
			// 仮：0.5秒で攻撃終了
			if (ctrl.actionTimer > 0.5f)
			{
				ctrl.state = PlayerState::Idle;
			}
		}

		void TriggerDriftShockwave(Registry& reg, Entity playerEntity)
		{
			Logger::Log("Drift Shockwave!!!");

			// 演出: プレイヤーをワイヤーフレーム化して光らせる
			if (reg.has<GeometricDesign>(playerEntity))
			{
				auto& geo = reg.get<GeometricDesign>(playerEntity);
				geo.isWireframe = true;
				geo.flashTimer = 0.5f; // 点滅開始
			}

			// ここに衝撃波の当たり判定生成ロジックを追加
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerActionSystem, "PlayerActionSystem")
