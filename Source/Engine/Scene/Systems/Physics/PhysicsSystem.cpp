/*****************************************************************//**
 * @file	PhysicsSystem.cpp
 * @brief	物理挙動システムの実装
 * 
 * @details	
 * 物理挙動を計算し、エンティティの動きを管理します。
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/15	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/12/16	最終更新日
 * 			作業内容：	- 物理システムの基本的なフレームワークを追加
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Systems/Physics/PhysicsSystem.h"

namespace Arche
{

	// ===== 定数・マクロ定義 =====
	static const float GRAVITY = 9.81f;
	static const int SOLVER_ITERATIONS = 8;

	// ============================================================
	// Update: 積分（Semi-Implicit Euler）
	// ============================================================

	void PhysicsSystem::Update(Registry& registry)
	{
		// デルタタイムの制限（フレームレート低下時の付き抜け防止）
		float dt = std::min(Time::DeltaTime(), 0.05f);

		// 接地フラグのリセット
		auto viewRB = registry.view<Rigidbody>();
		for (auto entity : viewRB)
		{
			viewRB.get<Rigidbody>(entity).isGrounded = false;
		}

		registry.view<Transform, Rigidbody>().each([&](Entity e, Transform& t, Rigidbody& rb)
			{
				// Staticは何もしない
				if (rb.type == BodyType::Static) return;

				// KinematicとDynamicの共通処理
				if (rb.type == BodyType::Dynamic)
				{
					// 1. 重力
					if (rb.useGravity)
					{
						rb.velocity.y -= GRAVITY * dt;
					}

					// 2. 空気抵抗
					float dump = 1.0f - (rb.drag * dt);
					if (dump < 0.0f) dump = 0.0f;
					rb.velocity.x *= dump;
					rb.velocity.z *= dump;
					// rb.velocity.y *= dump;
				}

				// 3. 位置更新（Euler積分）
				t.position.x += rb.velocity.x * dt;
				t.position.y += rb.velocity.y * dt;
				t.position.z += rb.velocity.z * dt;

				// デバッグ用：床抜け防止リセット
				if (t.position.y < -50.0f)
				{
					t.position = { 0, 10, 0 };
					rb.velocity = { 0, 0, 0 };
				}
			});
	}

	// ============================================================
	// Solve: 衝突解決
	// ============================================================
	void PhysicsSystem::Solve(Registry& registry, const std::vector<Physics::Contact>& contacts)
	{
		for (const auto& contact : contacts)
		{
			if (!registry.has<Rigidbody>(contact.a) || !registry.has<Rigidbody>(contact.b)) continue;
			if (!registry.has<Transform>(contact.a) || !registry.has<Transform>(contact.b)) continue;

			auto& rbA = registry.get<Rigidbody>(contact.a);
			auto& rbB = registry.get<Rigidbody>(contact.b);
			auto& tA = registry.get<Transform>(contact.a);
			auto& tB = registry.get<Transform>(contact.b);

			bool fixedA = (rbA.type == BodyType::Static || rbA.type == BodyType::Kinematic);
			bool fixedB = (rbB.type == BodyType::Static || rbB.type == BodyType::Kinematic);

			// 両方固定なら何もしない
			if (fixedA && fixedB) continue;

			using namespace DirectX;
			XMVECTOR n = XMLoadFloat3(&contact.normal); // A -> B の法線
			float depth = contact.depth;

			// ---------------------------------------------------------
			// 1. 両方動く場合 (Dynamic vs Dynamic)
			// ---------------------------------------------------------
			if (!fixedA && !fixedB) {
				// 位置補正 (既存)
				float totalMass = rbA.mass + rbB.mass;
				float ratioA = rbB.mass / totalMass;
				float ratioB = rbA.mass / totalMass;

				XMVECTOR posA = XMLoadFloat3(&tA.position);
				XMVECTOR posB = XMLoadFloat3(&tB.position);
				posA -= n * (depth * ratioA);
				posB += n * (depth * ratioB);
				XMStoreFloat3(&tA.position, posA);
				XMStoreFloat3(&tB.position, posB);

				if (n.m128_f32[1] < -0.7f)
				{
					rbA.isGrounded = true;
					if (rbA.velocity.y < 0.0f) rbA.velocity.y = 0.0f;
				}

				// Case B: BがAの上に乗っている (法線が上向き)
				// n (A->B) が上を向いている = AがBの下にある
				if (n.m128_f32[1] > 0.7f)
				{
					rbB.isGrounded = true;
					if (rbB.velocity.y < 0.0f) rbB.velocity.y = 0.0f;
				}

				// 速度補正
				// お互いの相対速度を計算し、法線方向の成分を打ち消す
				XMVECTOR velA = XMLoadFloat3(&rbA.velocity);
				XMVECTOR velB = XMLoadFloat3(&rbB.velocity);
				XMVECTOR relativeVel = velB - velA;
				float velDot = XMVectorGetX(XMVector3Dot(relativeVel, n));

				// 接近している場合のみ (離れようとしている時は何もしない)
				if (velDot < 0.0f) {
					// 衝撃係数(e) = 0 (跳ね返らない) と仮定
					// 法線方向の相対速度成分を分配して加算
					XMVECTOR impulse = n * velDot;
					velA += impulse * ratioA;
					velB -= impulse * ratioB;

					XMStoreFloat3(&rbA.velocity, velA);
					XMStoreFloat3(&rbB.velocity, velB);
				}
			}
			// ---------------------------------------------------------
			// 2. Aだけ動く場合 (Player vs Wall)
			// ---------------------------------------------------------
			else if (!fixedA && fixedB) {
				// 位置補正 (既存)
				XMVECTOR posA = XMLoadFloat3(&tA.position);
				posA -= n * depth;
				XMStoreFloat3(&tA.position, posA);

				if (n.m128_f32[1] < -0.7f)
				{
					rbA.isGrounded = true;

					if (rbA.velocity.y < 0.0f)
					{
						rbA.velocity.y = 0.0f;
					}
				}

				// 速度補正 (これが重要！)
				XMVECTOR velA = XMLoadFloat3(&rbA.velocity);
				// 法線方向の速度成分を計算
				// AはBに対して -n 方向に押し出されるので、velocity と -n の内積を見るべきだが、
				// ここでは「壁に向かう成分」を消すと考えればよい。
				// 法線 n は A->B なので、壁の向き。velocity と n の内積がプラスなら壁に向かっている。
				float velDot = XMVectorGetX(XMVector3Dot(velA, n));

				// 壁に向かって進んでいる場合のみ補正
				if (velDot > 0.0f) {
					// 法線方向の成分を引くことで、壁に沿って滑る動きになる
					velA -= n * velDot;
					XMStoreFloat3(&rbA.velocity, velA);
				}
			}
			// ---------------------------------------------------------
			// 3. Bだけ動く場合 (Wall vs Enemy)
			// ---------------------------------------------------------
			else if (fixedA && !fixedB) {
				// 位置補正 (既存)
				XMVECTOR posB = XMLoadFloat3(&tB.position);
				posB += n * depth;
				XMStoreFloat3(&tB.position, posB);

				if (n.m128_f32[1] > 0.7f)
				{
					rbB.isGrounded = true;

					if (rbB.velocity.y < 0.0f)
					{
						rbB.velocity.y = 0.0f;
					}
				}

				// 速度補正
				XMVECTOR velB = XMLoadFloat3(&rbB.velocity);
				// Bは n 方向に押し出される。
				// n は A->B なので、velocity と n の内積がマイナスならA(壁)に向かっている。
				float velDot = XMVectorGetX(XMVector3Dot(velB, n));

				if (velDot < 0.0f) {
					velB -= n * velDot;
					XMStoreFloat3(&rbB.velocity, velB);
				}
			}
		}
	}

}	// namespace Arche