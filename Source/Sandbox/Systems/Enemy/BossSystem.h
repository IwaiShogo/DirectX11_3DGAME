#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Time/Time.h"
#include "Sandbox/Components/Enemy/BossAI.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Engine/Resource/PrefabManager.h"
#include "Sandbox/Components/Player/Bullet.h" // 弾コンポーネント用

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
					pFound = true; break;
				}
			}
			if (!pFound) return;

			// 全ボス更新
			auto view = reg.view<BossAI, Transform, EnemyStats>();
			for (auto e : view)
			{
				auto& ai = view.get<BossAI>(e);
				auto& trans = view.get<Transform>(e);
				auto& stats = view.get<EnemyStats>(e);

				// 状態遷移管理
				ai.stateTimer -= dt;

				if (ai.stateTimer <= 0.0f)
				{
					if (ai.state == BossState::Idle)
					{
						// 攻撃選択ロジック
						float hpRatio = stats.hp / stats.maxHp;
						int rnd = rand() % 100;

						// HPが減ると必殺技頻度アップ
						if (rnd < (hpRatio < 0.5f ? 40 : 20)) ai.state = BossState::Attack_Ult;
						else if (rnd < 70) ai.state = BossState::Attack_B;
						else ai.state = BossState::Attack_A;

						ai.stateTimer = 5.0f; // 基本攻撃時間
						ai.phaseTimer = 0.0f;
						ai.patternStep = 0;   // ステップリセット
						ai.shotCount = 0;
					}
					else
					{
						ai.state = BossState::Idle;
						ai.stateTimer = ai.attackInterval;
					}
				}

				// 個別AI実行
				if (ai.bossName == "Tank") UpdateTank(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Prism") UpdatePrism(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Carrier") UpdateCarrier(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Construct") UpdateConstruct(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Omega") UpdateOmega(reg, e, ai, trans, pPos, dt);
				else if (ai.bossName == "Titan") UpdateTitan(reg, e, ai, trans, pPos, dt);
			}
		}

	private:
		// --------------------------------------------------------
		// 共通: 弾生成ヘルパー
		// --------------------------------------------------------
		void SpawnBullet(Registry& reg, XMFLOAT3 origin, float angleY, float angleX, float speed, float size, XMFLOAT4 color)
		{
			float radY = XMConvertToRadians(angleY);
			float radX = XMConvertToRadians(angleX);
			float cX = cosf(radX);
			XMVECTOR vDir = XMVectorSet(sinf(radY) * cX, sinf(radX), cosf(radY) * cX, 0);

			XMVECTOR vOrigin = XMLoadFloat3(&origin);
			// 発射位置は呼び出し側で指定された高さを尊重するため、ここでのYオフセットは削除しました

			float offsetDistance = 5.5f;
			XMVECTOR vSpawnPos = vOrigin + vDir * offsetDistance;

			XMFLOAT3 spawnPos;
			XMStoreFloat3(&spawnPos, vSpawnPos);

			Entity b = reg.create();
			reg.emplace<Transform>(b).position = spawnPos;

			float finalSize = size * 0.5f;
			reg.get<Transform>(b).scale = { finalSize, finalSize, finalSize };
			reg.get<Transform>(b).rotation = { -radX, radY, 0 };

			XMVECTOR vel = vDir * speed;

			auto& g = reg.emplace<GeometricDesign>(b);
			g.shapeType = GeoShape::Sphere;
			g.color = color;
			g.isWireframe = false;

			auto& attr = reg.emplace<AttackAttribute>(b);
			attr.damage = 1.0f;

			auto& bul = reg.emplace<Bullet>(b);
			bul.damage = 1.0f;
			bul.speed = speed;
			bul.lifeTime = 5.0f;
			bul.owner = EntityType::Enemy;

			auto& rb = reg.emplace<Rigidbody>(b);
			rb.useGravity = false;
			XMStoreFloat3(&rb.velocity, vel);

			reg.emplace<Collider>(b, Collider::CreateSphere(0.5f, Layer::Enemy));

			auto& t = reg.get<Transform>(b);
			XMMATRIX m = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) * XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) * XMMatrixTranslation(spawnPos.x, spawnPos.y, spawnPos.z);
			XMStoreFloat4x4(&t.worldMatrix, m);
		}

		// --------------------------------------------------------
		// Boss: Titan (The Last Boss)
		// --------------------------------------------------------
		void UpdateTitan(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt)
		{
			// ★修正: ターゲットを y=0.8f (プレイヤーの胴体中央) に固定
			XMVECTOR targetPos = XMVectorSet(XMVectorGetX(pPos), 0.8f, XMVectorGetZ(pPos), 0);

			XMVECTOR myPos = XMLoadFloat3(&t.position);
			XMVECTOR dirToP = targetPos - myPos;

			float distXZ = sqrtf(powf(XMVectorGetX(dirToP), 2) + powf(XMVectorGetZ(dirToP), 2));
			float pitch = XMConvertToDegrees(atan2f(XMVectorGetY(dirToP), distXZ));
			float yaw = XMConvertToDegrees(atan2f(XMVectorGetX(dirToP), XMVectorGetZ(dirToP)));

			if (ai.state != BossState::Attack_Ult) {
				float curAngle = t.rotation.y;
				float diff = yaw - curAngle;
				while (diff < -180) diff += 360; while (diff > 180) diff -= 360;
				t.rotation.y += diff * dt * 2.0f;
			}

			// Attack A: Missile Rain
			if (ai.state == BossState::Attack_A)
			{
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.1f) {
					ai.phaseTimer = 0.0f;
					XMVECTOR targetBodyPos = XMVectorSet(XMVectorGetX(pPos), 0.8f, XMVectorGetZ(pPos), 0);
					float offsetX = (float)(rand() % 20 - 10);
					float offsetZ = (float)(rand() % 20 - 10);
					XMVECTOR skyPosVec = targetBodyPos + XMVectorSet(offsetX, 20.0f, offsetZ, 0);
					XMFLOAT3 skyPos; XMStoreFloat3(&skyPos, skyPosVec);
					XMVECTOR dirToPlayer = XMVector3Normalize(targetBodyPos - skyPosVec);
					float skyYaw = XMConvertToDegrees(atan2f(XMVectorGetX(dirToPlayer), XMVectorGetZ(dirToPlayer)));
					float skyPitch = XMConvertToDegrees(atan2f(XMVectorGetY(dirToPlayer), sqrtf(powf(XMVectorGetX(dirToPlayer), 2) + powf(XMVectorGetZ(dirToPlayer), 2))));
					SpawnBullet(reg, skyPos, skyYaw, skyPitch, 20.0f, 2.5f, { 1, 0.5f, 0, 1 });
				}
			}
			// Attack B: Gatling Sweep
			else if (ai.state == BossState::Attack_B)
			{
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.05f) {
					ai.phaseTimer = 0.0f;
					XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(t.rotation.y));
					XMVECTOR rArm = XMVector3TransformCoord({ 4.5f, 0.0f, 1.0f }, rot);
					XMFLOAT3 pR; XMStoreFloat3(&pR, myPos + rArm);
					// ★修正: 発射位置(銃の位置)をプレイヤーの高さに合わせて固定
					pR.y = 0.8f;
					float sweep = sinf(Time::TotalTime() * 10.0f) * 10.0f;
					SpawnBullet(reg, pR, t.rotation.y + sweep, pitch, 25.0f, 2.0f, { 1, 1, 0, 1 });
				}
			}
			// Attack Ult: Titan Smash
			else if (ai.state == BossState::Attack_Ult)
			{
				ai.phaseTimer += dt;
				if (ai.patternStep == 1) {
					t.position.y += dt * 30.0f;
					t.position.x += XMVectorGetX(dirToP) * dt * 2.0f;
					t.position.z += XMVectorGetZ(dirToP) * dt * 2.0f;
				}
				else if (ai.patternStep == 2) {
					t.position.y -= dt * 60.0f;
					if (t.position.y <= 0.0f) {
						t.position.y = 0.0f; ai.patternStep++; ai.phaseTimer = 0.0f;
						for (int i = 0; i < 36; ++i) {
							XMFLOAT3 impactPos = t.position;
							// ★修正: 衝撃波の発生高度もプレイヤーに合わせる
							impactPos.y = 0.8f;
							SpawnBullet(reg, impactPos, i * 10.0f, 0, 15.0f, 4.0f, { 1, 0, 0, 1 });
						}
					}
				}
				// Step 3: 硬直
				else if (ai.patternStep == 3) {
					if (ai.phaseTimer > 1.5f) { ai.state = BossState::Idle; ai.stateTimer = 2.0f; } // 終了
				}
			}
		}

		// --------------------------------------------------------
		// Boss: Omega
		// --------------------------------------------------------
		void UpdateOmega(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt)
		{
			t.rotation.y += dt * 30.0f;
			// ★修正: 発射基準位置をプレイヤーの高さに合わせる
			XMFLOAT3 spawnOrigin = t.position;
			spawnOrigin.y = 0.8f;

			if (ai.state == BossState::Attack_A) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.08f) {
					ai.phaseTimer = 0.0f;
					for (int i = 0; i < 4; ++i) SpawnBullet(reg, spawnOrigin, t.rotation.y + i * 90.0f, 0, 12.0f, 2.0f, { 0, 1, 1, 1 });
				}
			}
			else if (ai.state == BossState::Attack_B) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.5f) {
					ai.phaseTimer = 0.0f;
					for (int i = 0; i < 6; ++i) {
						float angle = (rand() % 360) * (3.14f / 180.0f);
						XMFLOAT3 bPos; XMStoreFloat3(&bPos, pPos + XMVectorSet(cosf(angle) * 15.0f, 0.8f, sinf(angle) * 15.0f, 0));
						XMVECTOR vToP = XMVectorSet(XMVectorGetX(pPos), 0.8f, XMVectorGetZ(pPos), 0) - XMLoadFloat3(&bPos);
						SpawnBullet(reg, bPos, XMConvertToDegrees(atan2f(XMVectorGetX(vToP), XMVectorGetZ(vToP))), 0, 10.0f, 1.5f, { 1, 0, 1, 1 });
					}
				}
			}
			// --- Attack Ult: Doomsday (極太弾幕) ---
			else if (ai.state == BossState::Attack_Ult)
			{
				// 中央へ移動
				t.position.x = std::lerp(t.position.x, 0.0f, dt);
				t.position.z = std::lerp(t.position.z, 35.0f, dt);

				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.05f) {
					ai.phaseTimer = 0.0f;
					// 螺旋状に大量発射
					ai.shotCount += 10;
					for (int i = 0; i < 2; ++i) {
						float angle = (float)ai.shotCount + i * 180.0f;
						SpawnBullet(reg, t.position, angle, 0, 15.0f, 3.0f, { 0, 0, 1, 1 }); // 青
					}
				}
			}
		}

		// --------------------------------------------------------
		// Boss: Tank
		// --------------------------------------------------------
		void UpdateTank(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt) {
			XMVECTOR dir = pPos - XMLoadFloat3(&t.position);
			t.rotation.y = std::lerp(t.rotation.y, XMConvertToDegrees(atan2f(XMVectorGetX(dir), XMVectorGetZ(dir))), dt * 2.0f);
			if (ai.state != BossState::Idle) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.5f) {
					ai.phaseTimer = 0;
					XMFLOAT3 sPos = t.position;
					// ★修正: タンクの銃口高度をプレイヤーに合わせる
					sPos.y = 0.8f;
					SpawnBullet(reg, sPos, t.rotation.y, 0, 15.0f, 2.5f, { 1,0,0,1 });
				}
			}
		}
		// --------------------------------------------------------
// Boss: Prism (幾何学的なレーザー攻撃)
// --------------------------------------------------------
		void UpdatePrism(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt) {
			XMVECTOR myPos = XMLoadFloat3(&t.position);
			t.rotation.y += dt * 45.0f; // 常に回転

			// Attack A: Star Prism (星形に弾を配置)
			if (ai.state == BossState::Attack_A) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.3f) {
					ai.phaseTimer = 0.0f;
					float angleBase = (float)ai.shotCount * 20.0f;
					for (int i = 0; i < 5; ++i) {
						float angle = angleBase + (i * 72.0f);
						XMFLOAT3 origin = t.position;
						origin.y = 0.8f; // プレイヤーの高さ
						SpawnBullet(reg, origin, angle, 0, 15.0f, 1.5f, { 1, 0, 1, 1 });
					}
					ai.shotCount++;
				}
			}
			// Attack B: Prism Laser (プレイヤーを囲む6方向からの収束攻撃)
			else if (ai.state == BossState::Attack_B) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.8f) {
					ai.phaseTimer = 0.0f;
					for (int i = 0; i < 6; ++i) {
						float angle = (i * 60.0f);
						XMVECTOR spawnVec = pPos + XMVectorSet(sinf(XMConvertToRadians(angle)) * 12.0f, 0.8f, cosf(XMConvertToRadians(angle)) * 12.0f, 0);
						XMFLOAT3 spawnPos; XMStoreFloat3(&spawnPos, spawnVec);

						// プレイヤーへ向ける
						XMVECTOR dir = XMVector3Normalize(pPos - spawnVec);
						float yaw = XMConvertToDegrees(atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)));
						SpawnBullet(reg, spawnPos, yaw, 0, 20.0f, 2.0f, { 0, 1, 1, 1 });
					}
				}
			}
			// Attack Ult: Reflective Shell (全方位への反射弾幕)
			else if (ai.state == BossState::Attack_Ult) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.05f) {
					ai.phaseTimer = 0.0f;
					float angle = sinf(Time::TotalTime() * 5.0f) * 180.0f;
					XMFLOAT3 origin = t.position; origin.y = 0.8f;
					SpawnBullet(reg, origin, angle, 0, 12.0f, 2.5f, { 1, 1, 1, 1 });
					SpawnBullet(reg, origin, angle + 180.0f, 0, 12.0f, 2.5f, { 1, 1, 1, 1 });
				}
			}
		}
		// --------------------------------------------------------
// Boss: Carrier (爆撃と制圧)
// --------------------------------------------------------
		void UpdateCarrier(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt) {
			XMVECTOR myPos = XMLoadFloat3(&t.position);
			// プレイヤーの周囲を円を描くようにゆっくり移動
			t.position.x += sinf(Time::TotalTime() * 0.5f) * dt * 10.0f;
			t.position.z += cosf(Time::TotalTime() * 0.5f) * dt * 10.0f;

			// Attack A: Carpet Bombing (横一列の爆撃)
			if (ai.state == BossState::Attack_A) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.15f) {
					ai.phaseTimer = 0.0f;
					for (int i = -3; i <= 3; ++i) {
						XMFLOAT3 spawn = t.position;
						spawn.x += i * 3.0f;
						spawn.y = 15.0f; // 高所から
						SpawnBullet(reg, spawn, 0, -90, 20.0f, 3.0f, { 1, 0.5f, 0, 1 });
					}
				}
			}
			// Attack B: Homing Swarm (低速だがプレイヤーを執拗に追う弾幕)
			else if (ai.state == BossState::Attack_B) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.4f) {
					ai.phaseTimer = 0.0f;
					XMFLOAT3 origin = t.position; origin.y = 2.0f;
					XMVECTOR dir = XMVector3Normalize(pPos - XMLoadFloat3(&origin));
					float yaw = XMConvertToDegrees(atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)));

					for (int i = -1; i <= 1; ++i) {
						SpawnBullet(reg, origin, yaw + (i * 15.0f), 0, 8.0f, 2.0f, { 0.2f, 1, 0.2f, 1 });
					}
				}
			}
			// Attack Ult: Air Raid (全エリアへのランダム爆撃)
			else if (ai.state == BossState::Attack_Ult) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.05f) {
					ai.phaseTimer = 0.0f;
					XMFLOAT3 randomPos = { (float)(rand() % 60 - 30), 20.0f, (float)(rand() % 60 - 30) };
					SpawnBullet(reg, randomPos, 0, -90, 25.0f, 4.0f, { 1, 0, 0, 1 });
				}
			}
		}
		// --------------------------------------------------------
// Boss: Construct (物理障壁と連撃)
// --------------------------------------------------------
		void UpdateConstruct(Registry& reg, Entity e, BossAI& ai, Transform& t, XMVECTOR pPos, float dt) {
			XMVECTOR myPos = XMLoadFloat3(&t.position);
			XMVECTOR dirToP = XMVector3Normalize(pPos - myPos);
			float yaw = XMConvertToDegrees(atan2f(XMVectorGetX(dirToP), XMVectorGetZ(dirToP)));
			t.rotation.y = std::lerp(t.rotation.y, yaw, dt * 3.0f);

			// Attack A: Shield Shot (横に並んだ弾の壁を放つ)
			if (ai.state == BossState::Attack_A) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.6f) {
					ai.phaseTimer = 0.0f;
					for (int i = -4; i <= 4; ++i) {
						XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(t.rotation.y));
						XMVECTOR offset = XMVector3TransformCoord({ i * 2.0f, 0.8f, 2.0f }, rot);
						XMFLOAT3 spawn; XMStoreFloat3(&spawn, myPos + offset);
						SpawnBullet(reg, spawn, t.rotation.y, 0, 18.0f, 2.0f, { 0.8f, 0.8f, 0.8f, 1 });
					}
				}
			}
			// Attack B: Piston Burst (前後交互に強力な高速弾)
			else if (ai.state == BossState::Attack_B) {
				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.1f) {
					ai.phaseTimer = 0.0f;
					ai.shotCount++;
					float offsetSide = (ai.shotCount % 2 == 0) ? 2.0f : -2.0f;
					XMMATRIX rot = XMMatrixRotationY(XMConvertToRadians(t.rotation.y));
					XMVECTOR offset = XMVector3TransformCoord({ offsetSide, 0.8f, 3.0f }, rot);
					XMFLOAT3 spawn; XMStoreFloat3(&spawn, myPos + offset);
					SpawnBullet(reg, spawn, t.rotation.y, 0, 35.0f, 2.5f, { 1, 0.8f, 0, 1 });
				}
			}
			// Attack Ult: Gravity Well (プレイヤーを引き寄せながらの全方位射出)
			else if (ai.state == BossState::Attack_Ult) {
				// プレイヤーを引き寄せる（疑似重力）
				XMVECTOR pullDir = XMVector3Normalize(myPos - pPos);
				for (auto player : reg.view<Tag, Transform>()) {
					if (reg.get<Tag>(player).tag == "Player") {
						auto& pt = reg.get<Transform>(player);
						pt.position.x += XMVectorGetX(pullDir) * dt * 3.0f;
						pt.position.z += XMVectorGetZ(pullDir) * dt * 3.0f;
					}
				}

				ai.phaseTimer += dt;
				if (ai.phaseTimer > 0.1f) {
					ai.phaseTimer = 0.0f;
					for (int i = 0; i < 12; ++i) {
						XMFLOAT3 origin = t.position; origin.y = 0.8f;
						SpawnBullet(reg, origin, i * 30.0f + (Time::TotalTime() * 50.0f), 0, 15.0f, 1.8f, { 1, 0.2f, 0, 1 });
					}
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::BossSystem, "BossSystem")