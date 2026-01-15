#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Physics/PhysicsEvents.h"
#include "Engine/Resource/PrefabManager.h"

#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class DamageSystem : public ISystem
	{
	public:
		DamageSystem()
		{
			m_systemName = "DamageSystem";
		}

		void Update(Registry& registry) override
		{
			// 発生した全衝突イベントを取得
			const auto& events = Physics::EventManager::Instance().GetEvents();

			for (const auto& ev : events)
			{
				// 「当たった瞬間 (Enter)」だけ処理する
				if (ev.state != Physics::CollisionState::Enter) continue;

				Entity e1 = ev.self;
				Entity e2 = ev.other;

				if (registry.valid(e1) && registry.valid(e2))
				{
					// パターンA: 弾 vs 敵
					HandleBulletCollision(registry, e1, e2);
					HandleBulletCollision(registry, e2, e1);

					// パターンB: 敵 vs プレイヤー
					HandlePlayerCollision(registry, e1, e2);
					HandlePlayerCollision(registry, e2, e1);
				}
			}
		}

	private:
		// ------------------------------------------------------------
		// 弾が敵に当たった時の処理
		// ------------------------------------------------------------
		void HandleBulletCollision(Registry& reg, Entity bulletEntity, Entity targetEntity)
		{
			if (!reg.has<Bullet>(bulletEntity)) return;
			if (!reg.has<EnemyStats>(targetEntity)) return;

			auto& bullet = reg.get<Bullet>(bulletEntity);
			auto& enemy = reg.get<EnemyStats>(targetEntity);

			// ダメージ処理
			enemy.hp -= bullet.damage;

			// 敵のヒット演出（一瞬光らせる）
			if (reg.has<GeometricDesign>(targetEntity))
			{
				reg.get<GeometricDesign>(targetEntity).flashTimer = 0.1f;
			}

			// 弾を削除
			reg.destroy(bulletEntity);

			// 死亡判定
			if (enemy.hp <= 0.0f)
			{
				KillEnemy(reg, targetEntity);
			}
		}

		// ------------------------------------------------------------
		// 敵がプレイヤーに衝突した時の処理（時間減少）
		// ------------------------------------------------------------
		void HandlePlayerCollision(Registry& reg, Entity enemyEntity, Entity playerEntity)
		{
			if (!reg.has<EnemyStats>(enemyEntity)) return;
			if (!reg.has<PlayerTime>(playerEntity)) return;

			auto& enemy = reg.get<EnemyStats>(enemyEntity);
			auto& pTime = reg.get<PlayerTime>(playerEntity);

			// 無敵時間中なら無視
			if (pTime.invincibilityTimer > 0.0f) return;
			if (pTime.isDead) return;

			// 時間減少（敵ごとのペナルティ値を適用）
			// ※ EnemyStatsに collisionPenalty がない場合は固定値(例: 5.0f)を使ってください
			float penalty = enemy.collisionPenalty;

			pTime.currentTime -= penalty;
			pTime.invincibilityTimer = 1.0f; // 1秒無敵

			Logger::Log("Time Lost! -" + std::to_string((int)penalty));

			// プレイヤーのヒット演出（ワイヤーフレーム化で点滅開始）
			if (reg.has<GeometricDesign>(playerEntity))
			{
				reg.get<GeometricDesign>(playerEntity).isWireframe = true;
			}

			// ポップアップ演出 (赤色で "-5")
			XMFLOAT3 pos = reg.get<Transform>(playerEntity).position;
			SpawnFloatingText(reg, pos, "-" + std::to_string((int)penalty), { 1.0f, 0.2f, 0.2f, 1.0f });
		}

		// ------------------------------------------------------------
		// 敵撃破時の処理（時間回復）
		// ------------------------------------------------------------
		void KillEnemy(Registry& reg, Entity enemyEntity)
		{
			// 報酬量を取得
			float reward = 0.0f;
			if (reg.has<EnemyStats>(enemyEntity))
			{
				reward = reg.get<EnemyStats>(enemyEntity).killReward;
			}

			// プレイヤーを探して時間を回復させる
			reg.view<PlayerTime>().each([&](Entity p, PlayerTime& pt) {
				if (!pt.isDead)
				{
					pt.currentTime += reward;
					if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
				}
				});

			// ポップアップ演出 (敵の位置に水色で "+3")
			XMFLOAT3 pos = reg.get<Transform>(enemyEntity).position;
			SpawnFloatingText(reg, pos, "+" + std::to_string((int)reward), { 0.0f, 1.0f, 1.0f, 1.0f });

			Logger::Log("Enemy Destroyed! Time +" + std::to_string((int)reward));

			// 敵を削除
			reg.destroy(enemyEntity);
		}

		// ------------------------------------------------------------
		// テキストポップアップ生成
		// ------------------------------------------------------------
		void SpawnFloatingText(Registry& reg, XMFLOAT3 pos, const std::string& textStr, const XMFLOAT4& color)
		{
			// 1. プレファブから生成
			// ※プレファブ名 "DamagePopup" は保存したファイル名に合わせてください
			Entity e = PrefabManager::Instance().Spawn(reg, "DamagePopup", pos, { 0,0,0 });

			if (reg.valid(e))
			{
				// 2. テキストの内容と色を上書き
				if (reg.has<TextComponent>(e))
				{
					auto& txt = reg.get<TextComponent>(e);
					txt.text = textStr;
					txt.color = color;
				}

				// 3. 位置の微調整 (ランダム性を持たせて重なりを防止)
				if (reg.has<Transform>(e))
				{
					auto& t = reg.get<Transform>(e);

					// 乱数生成 (-0.5 ~ +0.5 の範囲でずらす)
					float randX = ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
					float randY = ((float)rand() / RAND_MAX) * 0.5f; // Yは少し上に
					float randZ = ((float)rand() / RAND_MAX - 0.5f) * 1.0f;

					t.position.x += randX;
					t.position.y += 1.5f + randY; // 頭上に出す
					t.position.z += randZ;
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::DamageSystem, "DamageSystem")
