#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Player/PlayerMoveData.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Systems/Core/DamageSystem.h" // AttackAttribute
#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	struct ActionState {
		float cooldown = 0.0f;
		float shootCooldown = 0.0f;
		float dashTimer = 0.0f;

		// エフェクト管理
		struct Effect { Entity e; float life; float maxLife; };
		std::vector<Effect> effects;
	};

	class PlayerActionSystem : public ISystem
	{
	public:
		PlayerActionSystem() { m_systemName = "PlayerActionSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static ActionState state;
			float dt = Time::DeltaTime();
			if (state.cooldown > 0) state.cooldown -= dt;
			if (state.shootCooldown > 0) state.shootCooldown -= dt;

			// プレイヤー取得
			auto view = reg.view<PlayerController, Transform>();
			Entity player = NullEntity;
			for (auto e : view) { player = e; break; }
			if (player == NullEntity) return;

			auto& t = reg.get<Transform>(player);
			auto& ctrl = reg.get<PlayerController>(player);

			// --- 入力判定 (Xboxコントローラー & キーボードマウス) ---

			// ロックオン (LB / 右クリック)
			bool inputLock = Input::GetButton(Button::LShoulder) || Input::GetMouseRightButton();

			// 攻撃 (X / 左クリック)
			bool inputAttack = Input::GetButton(Button::X) || Input::GetMouseLeftButton();

			// ダッシュ斬り (ロック中に攻撃)
			bool inputDashCut = inputLock && (Input::GetButtonDown(Button::X) || Input::GetMouseLeftButton());

			// 回転斬り (B / Fキー)
			bool inputSpin = Input::GetButtonDown(Button::B) || Input::GetKeyDown('F');

			// 衝撃波 (Y / Shift+E)
			bool inputShock = Input::GetButtonDown(Button::Y) || (Input::GetKey(VK_SHIFT) && Input::GetKeyDown('E'));

			// --- アクション実行 ---

			// 1. ダッシュ斬り (最優先・高リスク・高リターン)
			if (inputDashCut && state.cooldown <= 0)
			{
				DoDashCut(reg, t, state);
				state.cooldown = 0.8f; // 硬直長め
			}
			// 2. 衝撃波 (範囲・中リターン)
			else if (inputShock && state.cooldown <= 0)
			{
				DoShockwave(reg, t.position, state);
				state.cooldown = 1.5f;
			}
			// 3. 回転斬り (周囲・標準リターン)
			else if (inputSpin && state.cooldown <= 0)
			{
				DoSpinAttack(reg, t.position, state);
				state.cooldown = 0.6f;
			}
			// 4. 銃撃 (遠距離・低リターン)
			// ロックオンしていない状態で攻撃ボタン
			else if (inputAttack && !inputLock && state.shootCooldown <= 0 && state.cooldown <= 0)
			{
				Shoot(reg, t, ctrl);
				state.shootCooldown = 0.12f; // 連射速度
			}

			// エフェクト更新
			UpdateEffects(reg, state, dt);
		}

	private:
		// --- アクション実装 ---

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

		// ダッシュ斬り：一気に近づいて切る。ハイリスク・ハイリターン
		void DoDashCut(Registry& reg, Transform& t, ActionState& state)
		{
			// 前方へ高速移動
			XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
			XMVECTOR fwd = XMVector3TransformCoord({ 0, 0, 1 }, rot);
			XMFLOAT3 fwdDir; XMStoreFloat3(&fwdDir, fwd);

			float dist = 10.0f;
			t.position.x += fwdDir.x * dist;
			t.position.z += fwdDir.z * dist;

			// 通過線上に攻撃判定
			// 中心座標、半径、ダメージ、回復倍率
			SpawnHitbox(reg, t.position, 5.0f, 50.0f, 3.0f); // ★回復量: 3倍 (一発逆転)

			// 斬撃エフェクト (残像)
			for (int i = 0; i < 5; ++i) {
				XMFLOAT3 p = {
					t.position.x - fwdDir.x * (i * 2.0f),
					t.position.y,
					t.position.z - fwdDir.z * (i * 2.0f)
				};
				SpawnEffect(reg, p, GeoShape::Pyramid, { 0, 1, 1, 0.5f }, 0.3f, state);
			}
		}

		// 回転斬り：周囲をなぎ払う
		void DoSpinAttack(Registry& reg, XMFLOAT3 pos, ActionState& state)
		{
			SpawnHitbox(reg, pos, 7.0f, 20.0f, 1.0f); // ★回復量: 1倍 (標準)

			// リングエフェクト
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = pos;
			reg.get<Transform>(e).scale = { 1, 0.1f, 1 };
			auto& g = reg.emplace<GeometricDesign>(e);
			g.shapeType = GeoShape::Torus;
			g.color = { 1, 0.5f, 0, 1 }; // オレンジ
			g.isWireframe = true;

			state.effects.push_back({ e, 0.4f, 0.4f });
		}

		// 衝撃波：広範囲を吹き飛ばす
		void DoShockwave(Registry& reg, XMFLOAT3 pos, ActionState& state)
		{
			SpawnHitbox(reg, pos, 12.0f, 30.0f, 1.5f); // ★回復量: 1.5倍 (まとめて倒すと美味しい)

			// 球体拡散エフェクト
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = pos;
			reg.get<Transform>(e).scale = { 1, 1, 1 };
			auto& g = reg.emplace<GeometricDesign>(e);
			g.shapeType = GeoShape::Sphere;
			g.color = { 1, 0, 1, 0.5f }; // 紫
			g.isWireframe = true;

			state.effects.push_back({ e, 0.5f, 0.5f });
		}

		// --- 補助関数 ---

		// 攻撃判定 (簡易的な範囲判定)
		void SpawnHitbox(Registry& reg, XMFLOAT3 center, float radius, float damage, float rewardRate)
		{
			// 範囲内の敵を検索してダメージを与える
			auto enemies = reg.view<EnemyStats, Transform>();
			for (auto e : enemies) {
				auto& et = reg.get<Transform>(e);
				float dx = et.position.x - center.x;
				float dz = et.position.z - center.z;

				if (dx * dx + dz * dz < radius * radius) {
					// ダメージ適用
					auto& stats = reg.get<EnemyStats>(e);
					stats.hp -= damage;

					// ヒットエフェクト
					SpawnHitParticle(reg, et.position);

					// 撃破処理
					if (stats.hp <= 0) {
						float reward = stats.killReward * rewardRate; // 倍率適用

						// 時間回復
						for (auto p : reg.view<PlayerTime>()) {
							auto& pt = reg.get<PlayerTime>(p);
							pt.currentTime += reward;
							if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
						}

						// スコア加算 (GameSessionへ)
						GameSession::lastScore += stats.scoreValue;

						// 敵削除
						reg.destroy(e);
					}
				}
			}
		}

		void SpawnEffect(Registry& reg, XMFLOAT3 pos, GeoShape shape, XMFLOAT4 col, float life, ActionState& state)
		{
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = pos;
			reg.get<Transform>(e).scale = { 1, 1, 1 };
			auto& g = reg.emplace<GeometricDesign>(e);
			g.shapeType = shape; g.color = col; g.isWireframe = true;
			state.effects.push_back({ e, life, life });
		}

		void SpawnHitParticle(Registry& reg, XMFLOAT3 pos) {
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = pos;
			reg.get<Transform>(e).scale = { 0.5f, 0.5f, 0.5f };
			auto& g = reg.emplace<GeometricDesign>(e);
			g.shapeType = GeoShape::Cube; g.color = { 1,0,0,1 }; g.isWireframe = true;
			// ※ゴミが残らないよう別途LifetimeSystemが必要だが、ここでは省略
		}

		void UpdateEffects(Registry& reg, ActionState& state, float dt)
		{
			for (auto it = state.effects.begin(); it != state.effects.end(); ) {
				it->life -= dt;
				if (it->life <= 0) {
					reg.destroy(it->e);
					it = state.effects.erase(it);
				}
				else {
					if (reg.valid(it->e)) {
						auto& t = reg.get<Transform>(it->e);
						float p = 1.0f - (it->life / it->maxLife);
						// 急速拡大
						float s = 1.0f + p * 20.0f;
						t.scale = { s, s, s };
						// フェードアウト
						reg.get<GeometricDesign>(it->e).color.w = it->life / it->maxLife;
					}
					++it;
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerActionSystem, "PlayerActionSystem")