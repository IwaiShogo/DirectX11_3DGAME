#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Time/Time.h"
#include "Sandbox/Components/Enemy/BossAI.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Engine/Resource/PrefabManager.h" // 弾生成用

namespace Arche
{
	class BossSystem : public ISystem
	{
	public:
		BossSystem() { m_systemName = "BossSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();
			float time = Time::TotalTime();

			// プレイヤー位置取得
			XMVECTOR pPos = XMVectorZero();
			bool pFound = false;
			for (auto e : reg.view<Tag, Transform>()) {
				if (reg.get<Tag>(e).tag == "Player") {
					pPos = XMLoadFloat3(&reg.get<Transform>(e).position);
					pFound = true;
					break;
				}
			}
			if (!pFound) return;

			// 全ボス更新
			auto view = reg.view<BossAI, Transform, EnemyStats>();
			for (auto e : view)
			{
				auto& ai = view.get<BossAI>(e);
				auto& trans = view.get<Transform>(e);

				// 状態タイマー更新
				ai.stateTimer -= dt;

				// --- 状態遷移ロジック ---
				if (ai.stateTimer <= 0.0f)
				{
					if (ai.state == BossState::Idle)
					{
						// 攻撃へ遷移 (ランダムでパターン分岐)
						int pattern = rand() % 2;
						if (pattern == 0) ai.state = BossState::Attack_A;
						else ai.state = BossState::Attack_B;

						ai.stateTimer = 5.0f; // 攻撃時間
						ai.phaseTimer = 0.0f; // 攻撃内タイマーリセット
					}
					else
					{
						// 攻撃終了 -> 待機
						ai.state = BossState::Idle;
						ai.stateTimer = ai.attackInterval;
					}
				}

				// --- 個別AI ---
				if (ai.bossName == "Tank") UpdateTank(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Prism") UpdatePrism(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Carrier") UpdateCarrier(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Omega") UpdateOmega(reg, e, ai, trans, pPos, dt, time);
			}
		}

	private:
		// --------------------------------------------------------
		// Tank: 砲撃と突進
		// --------------------------------------------------------
		void UpdateTank(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt)
		{
			// 常に向き補正
			XMVECTOR myPos = XMLoadFloat3(&t.position);
			XMVECTOR dir = pPos - myPos;
			float targetAngle = XMConvertToDegrees(atan2f(dir.m128_f32[0], dir.m128_f32[2]));
			t.rotation.y = std::lerp(t.rotation.y, targetAngle, dt * 2.0f);

			if (ai.state == BossState::Attack_A)
			{
				// 連射
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.5f) {
					ai.phaseTimer = 0.0f;
					// 正面に弾発射
					SpawnBullet(reg, t.position, t.rotation.y, 10.0f);
				}
			}
			else if (ai.state == BossState::Attack_B)
			{
				// 突進
				t.position.x += dir.m128_f32[0] * dt * 0.8f; // ゆっくり
				t.position.z += dir.m128_f32[2] * dt * 0.8f;
			}
		}

		// --------------------------------------------------------
		// Omega: 全方位弾と回転レーザー(擬似)
		// --------------------------------------------------------
		void UpdateOmega(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt, float time)
		{
			// 常に回転
			t.rotation.y += dt * 10.0f;

			if (ai.state == BossState::Attack_A)
			{
				// 全方位弾幕
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.2f) {
					ai.phaseTimer = 0.0f;
					for (int i = 0; i < 8; ++i) {
						float a = i * 45.0f + t.rotation.y;
						SpawnBullet(reg, t.position, a, 8.0f);
					}
				}
			}
			else if (ai.state == BossState::Attack_B)
			{
				// 上下動しながらプレイヤー狙い撃ち
				t.position.y = sinf(time * 2.0f) * 2.0f;
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.1f) {
					ai.phaseTimer = 0.0f;
					// プレイヤー方向へ
					XMVECTOR dir = pPos - XMLoadFloat3(&t.position);
					float a = XMConvertToDegrees(atan2f(dir.m128_f32[0], dir.m128_f32[2]));
					SpawnBullet(reg, t.position, a, 12.0f);
				}
			}
		}

		// 他のボスも同様に実装...
		void UpdatePrism(Registry&, Entity, BossAI&, Transform&, XMVECTOR, float) {}
		void UpdateCarrier(Registry&, Entity, BossAI&, Transform&, XMVECTOR, float) {}

		// 簡易弾生成
		void SpawnBullet(Registry& reg, XMFLOAT3 origin, float angleDeg, float speed)
		{
			// 本来はBulletFactoryやPrefabManagerを使う
			Entity b = PrefabManager::Instance().Spawn(reg, "EnemyBullet", origin, { 0, angleDeg, 0 });
			if (reg.valid(b)) {
				// 速度設定
				if (reg.has<Rigidbody>(b)) {
					float rad = XMConvertToRadians(angleDeg);
					reg.get<Rigidbody>(b).velocity = { sinf(rad) * speed, 0, cosf(rad) * speed };
				}
			}
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::BossSystem, "BossSystem")
