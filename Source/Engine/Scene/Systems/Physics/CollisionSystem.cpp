/*****************************************************************//**
 * @file	CollisionSystem.cpp
 * @brief	衝突検出
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/24	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Systems/Physics/CollisionSystem.h"
#include "Engine/Physics/PhysicsEvents.h"
#include "Engine/Physics/SpatialHash.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{
	using namespace Physics;

	// ペア管理用
	using EntityPair = std::pair<Entity, Entity>;

	// 以前の接触状態を保持するスタティック変数
	static std::map<EntityPair, bool> g_prevContacts;
	static SpatialHash g_spatialHash;	// 空間ハッシュインスタンス

	Observer CollisionSystem::m_observer;
	bool CollisionSystem::m_isInitialized = false;

	// =================================================================
	// 数学・幾何ヘルパー関数
	// =================================================================

	// ベクトルの長さの二乗
	float LengthSq(FXMVECTOR v) {
		return XMVectorGetX(XMVector3LengthSq(v));
	}

	// 点Pの、線分AB上での最近接点を求める
	XMVECTOR ClosestPointOnSegment(XMVECTOR P, XMVECTOR A, XMVECTOR B)
	{
		XMVECTOR AP = P - A;
		XMVECTOR AB = B - A;
		float ab2 = LengthSq(AB);

		if (ab2 < 1e-6f) return A;

		float t = XMVectorGetX(XMVector3Dot(AP, AB)) / ab2;
		t = std::max(0.0f, std::min(1.0f, t));
		return A + AB * t;
	}

	// 2つの線分間の最短距離の二乗と、それぞれの線分上の最近接点(c1, c2)を求める
	float SegmentSegmentDistanceSq(XMVECTOR p1, XMVECTOR q1, XMVECTOR p2, XMVECTOR q2,
		XMVECTOR& outC1, XMVECTOR& outC2)
	{
		XMVECTOR d1 = q1 - p1;
		XMVECTOR d2 = q2 - p2;
		XMVECTOR r = p1 - p2;
		float a = LengthSq(d1);
		float e = LengthSq(d2);
		float f = XMVectorGetX(XMVector3Dot(d2, r));

		if (a <= 1e-6f && e <= 1e-6f) {
			outC1 = p1; outC2 = p2;
			return LengthSq(r);
		}

		float s, t;
		if (a <= 1e-6f) {
			s = 0.0f;
			t = std::max(0.0f, std::min(1.0f, f / e));
		}
		else {
			float c = XMVectorGetX(XMVector3Dot(d1, r));
			if (e <= 1e-6f) {
				t = 0.0f;
				s = std::max(0.0f, std::min(1.0f, -c / a));
			}
			else {
				float b = XMVectorGetX(XMVector3Dot(d1, d2));
				float denom = a * e - b * b;
				if (denom != 0.0f) {
					s = std::max(0.0f, std::min(1.0f, (b * f - c * e) / denom));
				}
				else {
					s = 0.0f;
				}
				t = (b * s + f) / e;
				if (t < 0.0f) {
					t = 0.0f;
					s = std::max(0.0f, std::min(1.0f, -c / a));
				}
				else if (t > 1.0f) {
					t = 1.0f;
					s = std::max(0.0f, std::min(1.0f, (b - c) / a));
				}
			}
		}

		outC1 = p1 + d1 * s;
		outC2 = p2 + d2 * t;
		XMVECTOR diff = outC1 - outC2;
		return LengthSq(diff);
	}

	// 点PからOBBへの最近接点を求める
	XMVECTOR ClosestPointOnOBB(XMVECTOR P, const OBB& box) {
		XMVECTOR d = P - XMLoadFloat3(&box.center);
		XMVECTOR q = XMLoadFloat3(&box.center);

		for (int i = 0; i < 3; ++i) {
			XMVECTOR axis = XMLoadFloat3(&box.axes[i]);
			float dist = XMVectorGetX(XMVector3Dot(d, axis));
			float extent = (&box.extents.x)[i];

			if (dist > extent) dist = extent;
			if (dist < -extent) dist = -extent;

			q += axis * dist;
		}
		return q;
	}

	// 点Pの、円柱表面上での最近接点を求める (修正版)
	// cylP: 中心, cylAxis: 軸(正規化), h: 高さ, r: 半径
	XMVECTOR ClosestPointOnCylinder(XMVECTOR P, XMVECTOR cylP, XMVECTOR cylAxis, float h, float r) {
		XMVECTOR d = P - cylP;
		// 軸方向の距離 (中心から)
		float distY = XMVectorGetX(XMVector3Dot(d, cylAxis));

		// 1. 半径方向のベクトルを計算
		// Pを軸上に投影した点
		XMVECTOR onAxis = cylP + cylAxis * distY;
		XMVECTOR radial = P - onAxis;
		float radLenSq = LengthSq(radial);

		// 軸上(中心)にいる場合の例外処理
		if (radLenSq < 1e-6f) {
			// 適当な半径方向 (X軸かZ軸)
			XMVECTOR arb = XMVectorSet(1, 0, 0, 0);
			if (std::abs(XMVectorGetX(XMVector3Dot(cylAxis, arb))) > 0.9f) arb = XMVectorSet(0, 0, 1, 0);
			radial = XMVector3Normalize(XMVector3Cross(cylAxis, arb)) * r;
			radLenSq = r * r;
		}

		float radLen = std::sqrt(radLenSq);
		// 表面（側面）への方向
		XMVECTOR radialDir = radial / radLen;

		// --- 領域判定 ---
		float halfH = h * 0.5f;

		// A. 完全に内部にいる場合
		if (distY >= -halfH && distY <= halfH && radLen <= r) {
			// 「側面」と「蓋（上下）」のどちらに近いか？
			float distToSide = r - radLen;
			float distToTop = halfH - distY;
			float distToBottom = distY - (-halfH);

			// 最も近い面へ射影
			if (distToSide < distToTop && distToSide < distToBottom) {
				// 側面へ
				return onAxis + radialDir * r;
			}
			else if (distToTop < distToBottom) {
				// 上面へ
				return cylP + cylAxis * halfH + radial;
			}
			else {
				// 下面へ
				return cylP + cylAxis * (-halfH) + radial;
			}
		}

		// B. 外部にいる場合（クランプ処理）

		// まず高さをクランプ
		float clampedY = distY;
		if (clampedY > halfH) clampedY = halfH;
		if (clampedY < -halfH) clampedY = -halfH;

		// 半径方向をチェック
		// 高さ範囲内なら、必ず側面にクランプ。
		// 高さ範囲外(蓋の上)なら、円盤内に収める。

		if (std::abs(distY) <= halfH) {
			// 胴体の横 -> 側面上へ
			return (cylP + cylAxis * distY) + radialDir * r;
		}
		else {
			// 蓋の上 -> 円盤領域へクランプ
			// radialLen が r を超えていれば r に抑える
			float capR = radLen;
			if (capR > r) capR = r;
			return (cylP + cylAxis * clampedY) + radialDir * capR;
		}
	}

	// =================================================================
	// Raycast
	// =================================================================

	// レイ vs 球
	bool IntersectRaySphere(XMVECTOR origin, XMVECTOR dir, XMVECTOR center, float radius, float& t)
	{
		XMVECTOR m = origin - center;
		float b = XMVectorGetX(XMVector3Dot(m, dir));
		float c = XMVectorGetX(XMVector3Dot(m, m)) - radius * radius;

		// レイの始点が球の外にあり、かつ球から遠ざかっている場合
		if (c > 0.0f && b > 0.0f) return false;

		float discr = b * b - c;
		if (discr < 0.0f) return false;

		t = -b - sqrt(discr);
		if (t < 0.0f) t = 0.0f; // 始点が中にある場合は0
		return true;
	}

	// レイ vs OBB (回転した箱)
	bool IntersectRayOBB(XMVECTOR origin, XMVECTOR dir, const OBB& obb, float& t)
	{
		XMVECTOR boxCenter = XMLoadFloat3(&obb.center);
		XMVECTOR delta = boxCenter - origin; // レイの始点から箱の中心へのベクトル

		// OBBの各軸
		XMVECTOR axes[3] = {
			XMLoadFloat3(&obb.axes[0]),
			XMLoadFloat3(&obb.axes[1]),
			XMLoadFloat3(&obb.axes[2])
		};
		float extents[3] = { obb.extents.x, obb.extents.y, obb.extents.z };

		float tMin = 0.0f;
		float tMax = 100000.0f;

		for (int i = 0; i < 3; ++i)
		{
			// レイの方向と、箱の中心への方向を、OBBの各軸に投影
			// e = (BoxCenter - RayOrigin) . Axis
			// f = RayDir . Axis
			float e = XMVectorGetX(XMVector3Dot(axes[i], delta));
			float f = XMVectorGetX(XMVector3Dot(axes[i], dir));

			// レイが軸と平行でない場合
			if (std::abs(f) > 1e-6f)
			{
				float t1 = (e + extents[i]) / f;
				float t2 = (e - extents[i]) / f;

				if (t1 > t2) std::swap(t1, t2);

				tMin = std::max(tMin, t1);
				tMax = std::min(tMax, t2);

				if (tMin > tMax) return false;
				if (tMax < 0.0f) return false;
			}
			else
			{
				// 平行な場合、レイがスラブの外にあれば交差しない
				if (-e - extents[i] > 0.0f || -e + extents[i] < 0.0f) return false;
			}
		}

		t = tMin;
		return true;
	}

	// レイ vs カプセル
	bool IntersectRayCapsule(XMVECTOR origin, XMVECTOR dir, const Capsule& cap, float& t)
	{
		XMVECTOR pa = XMLoadFloat3(&cap.start);
		XMVECTOR pb = XMLoadFloat3(&cap.end);
		float r = cap.radius;

		XMVECTOR ba = pb - pa;
		XMVECTOR oa = origin - pa;

		float baba = XMVectorGetX(XMVector3Dot(ba, ba));
		float bard = XMVectorGetX(XMVector3Dot(ba, dir));
		float baoa = XMVectorGetX(XMVector3Dot(ba, oa));
		float rdoa = XMVectorGetX(XMVector3Dot(dir, oa));
		float oaoa = XMVectorGetX(XMVector3Dot(oa, oa));

		float a = baba - bard * bard;
		float b = baba * rdoa - baoa * bard;
		float c = baba * oaoa - baoa * baoa - r * r * baba;
		float h = b * b - a * c;

		if (h >= 0.0f)
		{
			float t_hit = (-b - sqrt(h)) / a;
			float y = baoa + t_hit * bard;

			// 胴体部分 (円柱) との交差
			if (y > 0.0f && y < baba)
			{
				t = t_hit;
				return true;
			}

			// キャップ (半球) との交差
			XMVECTOR oc = (y <= 0.0f) ? oa : origin - pb;
			b = XMVectorGetX(XMVector3Dot(dir, oc));
			c = XMVectorGetX(XMVector3Dot(oc, oc)) - r * r;
			h = b * b - c;
			if (h > 0.0f)
			{
				t = -b - sqrt(h);
				return true;
			}
		}
		return false;
	}

	// レイ vs 円柱
	bool IntersectRayCylinder(XMVECTOR origin, XMVECTOR dir, const Cylinder& cyl, float& t)
	{
		XMVECTOR center = XMLoadFloat3(&cyl.center);
		XMVECTOR axis = XMLoadFloat3(&cyl.axis);
		float height = cyl.height;
		float radius = cyl.radius;

		// レイの始点と方向を、円柱基準のベクトルに変換して計算を簡略化する手もあるが、
		// ここではワールド空間のまま幾何学的に解く。

		// 1. 無限円柱との交差判定
		// 円柱表面の方程式: |(P - C) - dot(P - C, A) * A|^2 = r^2
		// P(t) = O + tD を代入して t の2次方程式 At^2 + Bt + C = 0 を解く

		XMVECTOR oc = origin - center;

		// レイ方向 D のうち、軸 A に垂直な成分
		XMVECTOR dProj = dir - XMVector3Dot(dir, axis) * axis;
		// 始点ベクトル OC のうち、軸 A に垂直な成分
		XMVECTOR ocProj = oc - XMVector3Dot(oc, axis) * axis;

		float a = XMVectorGetX(XMVector3LengthSq(dProj));
		float b = 2.0f * XMVectorGetX(XMVector3Dot(dProj, ocProj));
		float c = XMVectorGetX(XMVector3LengthSq(ocProj)) - radius * radius;

		float t1 = -1.0f, t2 = -1.0f;
		bool hitCylinder = false;

		if (std::abs(a) > 1e-6f) {
			float discr = b * b - 4.0f * a * c;
			if (discr >= 0.0f) {
				float root = std::sqrt(discr);
				t1 = (-b - root) / (2.0f * a);
				t2 = (-b + root) / (2.0f * a);
				hitCylinder = true;
			}
		}

		float halfH = height * 0.5f;
		float tClose = FLT_MAX;
		bool hit = false;

		// 側面ヒットの確認 (高さ範囲内か？)
		auto CheckHeight = [&](float tVal) {
			if (tVal < 0.0f) return;
			XMVECTOR p = origin + dir * tVal;
			float h = XMVectorGetX(XMVector3Dot(p - center, axis));
			if (std::abs(h) <= halfH) {
				if (tVal < tClose) {
					tClose = tVal;
					hit = true;
				}
			}
			};

		if (hitCylinder) {
			CheckHeight(t1);
			CheckHeight(t2);
		}

		// 2. 蓋（円盤）との交差判定
		// 平面 (P - (C +/- H/2 * A)) . A = 0
		// t = dot((C +/- H/2 * A) - O, A) / dot(D, A)

		float dDotA = XMVectorGetX(XMVector3Dot(dir, axis));
		if (std::abs(dDotA) > 1e-6f) {
			float ocDotA = XMVectorGetX(XMVector3Dot(oc, axis));

			// 上面 (C + H/2 * A)
			float tTop = (halfH - ocDotA) / dDotA;
			if (tTop > 0.0f) {
				XMVECTOR p = origin + dir * tTop;
				// 半径チェック: |P - (C + H/2 * A)|^2 <= r^2
				// 軸上の点は center + halfH * axis
				XMVECTOR pOnAxis = center + axis * halfH;
				if (LengthSq(p - pOnAxis) <= radius * radius) {
					if (tTop < tClose) {
						tClose = tTop;
						hit = true;
					}
				}
			}

			// 下面 (C - H/2 * A)
			float tBottom = (-halfH - ocDotA) / dDotA;
			if (tBottom > 0.0f) {
				XMVECTOR p = origin + dir * tBottom;
				XMVECTOR pOnAxis = center - axis * halfH;
				if (LengthSq(p - pOnAxis) <= radius * radius) {
					if (tBottom < tClose) {
						tClose = tBottom;
						hit = true;
					}
				}
			}
		}

		if (hit) {
			t = tClose;
			return true;
		}
		return false;
	}

	Entity CollisionSystem::Raycast(Registry& registry, const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, float& outDist)
	{
		Entity closestEntity = NullEntity;
		float closestDist = FLT_MAX;

		XMVECTOR originV = XMLoadFloat3(&rayOrigin);
		XMVECTOR dirV = XMLoadFloat3(&rayDir);

		registry.view<Transform, Collider>().each([&](Entity e, Transform& t, Collider& c)
			{
				// ワールド行列の分解
				XMVECTOR scale, rotQuat, pos;
				XMMatrixDecompose(&scale, &rotQuat, &pos, t.GetWorldMatrix());
				XMFLOAT3 gScale; XMStoreFloat3(&gScale, scale);
				XMMATRIX rotMat = XMMatrixRotationQuaternion(rotQuat);

				// 中心座標の計算
				XMVECTOR offsetVec = XMLoadFloat3(&c.offset);
				XMVECTOR centerVec = XMVector3Transform(offsetVec, t.GetWorldMatrix());
				XMFLOAT3 center; XMStoreFloat3(&center, centerVec);

				bool hit = false;
				float dist = 0.0f;

				// タイプごとの分岐
				if (c.type == ColliderType::Box)
				{
					OBB obb;
					obb.center = center;
					obb.extents = {
						c.boxSize.x * gScale.x * 0.5f,
						c.boxSize.y * gScale.y * 0.5f,
						c.boxSize.z * gScale.z * 0.5f
					};
					XMFLOAT4X4 rotM; XMStoreFloat4x4(&rotM, rotMat);
					obb.axes[0] = { rotM._11, rotM._12, rotM._13 };
					obb.axes[1] = { rotM._21, rotM._22, rotM._23 };
					obb.axes[2] = { rotM._31, rotM._32, rotM._33 };

					hit = IntersectRayOBB(originV, dirV, obb, dist);
				}
				else if (c.type == ColliderType::Sphere)
				{
					float maxS = std::max({ gScale.x, gScale.y, gScale.z });
					float r = c.radius * maxS;
					hit = IntersectRaySphere(originV, dirV, centerVec, r, dist);
				}
				else if (c.type == ColliderType::Capsule)
				{
					XMVECTOR axisY = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMat);
					float h = c.height * gScale.y;
					float r = c.radius * std::max(gScale.x, gScale.z);
					float halfLen = std::max(0.0f, h * 0.5f - r);

					Capsule cap;
					XMStoreFloat3(&cap.start, centerVec - axisY * halfLen);
					XMStoreFloat3(&cap.end, centerVec + axisY * halfLen);
					cap.radius = r;

					hit = IntersectRayCapsule(originV, dirV, cap, dist);
				}
				else if (c.type == ColliderType::Cylinder)
				{
					Cylinder cyl;
					cyl.center = center;
					XMVECTOR axisY = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMat);
					XMStoreFloat3(&cyl.axis, axisY);
					cyl.height = c.height * gScale.y;
					cyl.radius = c.radius * std::max(gScale.x, gScale.z);

					hit = IntersectRayCylinder(originV, dirV, cyl, dist);
				}

				// 最近接の更新
				if (hit)
				{
					if (dist < closestDist && dist >= 0.0f)
					{
						closestDist = dist;
						closestEntity = e;
					}
				}
			});

		outDist = closestDist;
		return closestEntity;
	}

	// =================================================================
	// 判定関数群
	// =================================================================

	// Sphere vs Sphere [cite: 2]
	bool CollisionSystem::CheckSphereSphere(const Sphere& a, const Sphere& b, Contact& outContact) {
		XMVECTOR centerA = XMLoadFloat3(&a.center);
		XMVECTOR centerB = XMLoadFloat3(&b.center);

		// 距離チェック
		XMVECTOR distVec = centerB - centerA; // A -> B
		float distSq = LengthSq(distVec);
		float rSum = a.radius + b.radius;

		if (distSq >= rSum * rSum) return false;

		// 衝突情報の作成
		float dist = std::sqrt(distSq);
		XMVECTOR normal;

		if (dist < 1e-4f) {
			normal = XMVectorSet(0, 1, 0, 0); // 完全に重なったら上へ
			dist = 0.0f;
		}
		else {
			normal = distVec / dist;
		}

		XMStoreFloat3(&outContact.normal, normal);
		outContact.depth = rSum - dist;
		return true;
	}

	// Sphere vs OBB [cite: 2]
	bool CollisionSystem::CheckSphereOBB(const Sphere& s, const OBB& b, Contact& outContact) {
		XMVECTOR sphereCenter = XMLoadFloat3(&s.center);
		XMVECTOR obbCenter = XMLoadFloat3(&b.center);
		XMVECTOR delta = sphereCenter - obbCenter;

		// OBBの回転行列（軸）を取得
		XMVECTOR axes[3] = {
			XMLoadFloat3(&b.axes[0]),
			XMLoadFloat3(&b.axes[1]),
			XMLoadFloat3(&b.axes[2])
		};
		float extents[3] = { b.extents.x, b.extents.y, b.extents.z };

		// 球の中心をOBBのローカル座標系に変換
		// localPos: OBB中心を原点とした、球中心の座標
		XMVECTOR localPos = XMVectorSet(
			XMVectorGetX(XMVector3Dot(delta, axes[0])),
			XMVectorGetX(XMVector3Dot(delta, axes[1])),
			XMVectorGetX(XMVector3Dot(delta, axes[2])),
			0.0f
		);

		// ローカル空間での最近接点を見つける
		XMVECTOR localClosest = localPos;
		bool isInside = true;

		// 各軸ごとにクランプ処理
		float localP[3] = { XMVectorGetX(localPos), XMVectorGetY(localPos), XMVectorGetZ(localPos) };
		float closest[3];

		for (int i = 0; i < 3; ++i) {
			if (localP[i] > extents[i]) {
				closest[i] = extents[i];
				isInside = false;
			}
			else if (localP[i] < -extents[i]) {
				closest[i] = -extents[i];
				isInside = false;
			}
			else {
				closest[i] = localP[i];
			}
		}

		localClosest = XMVectorSet(closest[0], closest[1], closest[2], 0.0f);

		// 判定分岐
		if (!isInside) {
			// --- 球の中心がOBBの【外部】にある場合 ---
			// ローカル空間での距離計算
			XMVECTOR distVecLocal = localPos - localClosest; // Sphere -> Closest
			float distSq = LengthSq(distVecLocal);

			if (distSq > s.radius * s.radius) return false;

			float dist = std::sqrt(distSq);

			// 法線をワールド空間に戻す
			// distVecLocal はローカルでの「Closest -> Sphere」ではなく「Sphere(内部) -> Closest」に向かうべきか？
			// distVecLocal = localPos(外) - localClosest(表面)。これは「表面 -> 外」へのベクトル。
			// A(Sphere) -> B(OBB) の法線が欲しいので、これは「Sphere -> OBB」つまり内向きであるべき。
			// なので normalized(-distVecLocal) が A->B 法線。

			XMVECTOR normalLocal;
			if (dist < 1e-6f) {
				normalLocal = XMVectorSet(0, 1, 0, 0); // 例外回避
			}
			else {
				normalLocal = -distVecLocal / dist;
			}

			// ローカル法線をワールドへ
			XMVECTOR normal = normalLocal.m128_f32[0] * axes[0] +
				normalLocal.m128_f32[1] * axes[1] +
				normalLocal.m128_f32[2] * axes[2];

			XMStoreFloat3(&outContact.normal, normal);
			outContact.depth = s.radius - dist;
		}
		else {
			// --- 球の中心がOBBの【内部】にある場合 ---
			// 最も近い面を探して、そこへ押し出す
			float minDrag = FLT_MAX;
			int minAxis = -1;
			float sign = 1.0f;

			for (int i = 0; i < 3; ++i) {
				// 面までの距離: extents - |localP|
				float distToFace = extents[i] - std::abs(localP[i]);
				if (distToFace < minDrag) {
					minDrag = distToFace;
					minAxis = i;
					sign = (localP[i] < 0.0f) ? -1.0f : 1.0f;
				}
			}

			// 法線（A->B）：球をOBBの外に出す方向の「逆」。
			// 球を出すには「最も近い面の方へ」動かす。
			// つまり移動方向は「中心 -> 面」。
			// A->B法線はその逆なので「面 -> 中心（内向き）」。
			// ローカル軸 minAxis の sign 方向が「外向き」なので、法線は -sign * axis。

			XMVECTOR normal = axes[minAxis] * (-sign);

			XMStoreFloat3(&outContact.normal, normal);
			// 内部にいる場合、深さは「半径 + 表面までの距離(minDrag)」
			outContact.depth = s.radius + minDrag;
		}

		return true;
	}

	// Sphere vs Capsule (厳密)
	bool CollisionSystem::CheckSphereCapsule(const Sphere& s, const Capsule& c, Contact& outContact) {
		XMVECTOR sphP = XMLoadFloat3(&s.center);
		XMVECTOR capA = XMLoadFloat3(&c.start);
		XMVECTOR capB = XMLoadFloat3(&c.end);

		XMVECTOR closest = ClosestPointOnSegment(sphP, capA, capB);
		XMVECTOR distVec = closest - sphP; // 球 -> カプセル軸
		float distSq = LengthSq(distVec);
		float rSum = s.radius + c.radius;

		if (distSq >= rSum * rSum) return false;

		float dist = std::sqrt(distSq);
		XMVECTOR normal;
		if (dist < 1e-4f) {
			normal = XMVectorSet(0, 1, 0, 0);
			dist = 0.0f;
		}
		else {
			normal = distVec / dist;
		}

		XMStoreFloat3(&outContact.normal, normal);
		outContact.depth = rSum - dist;
		return true;
	}

	// OBB vs OBB (SAT: 分離軸定理) [cite: 2]
	bool CollisionSystem::CheckOBBOBB(const OBB& a, const OBB& b, Contact& outContact) {
		XMVECTOR centerA = XMLoadFloat3(&a.center);
		XMVECTOR centerB = XMLoadFloat3(&b.center);
		XMVECTOR translation = centerB - centerA;

		XMVECTOR axesA[3] = { XMLoadFloat3(&a.axes[0]), XMLoadFloat3(&a.axes[1]), XMLoadFloat3(&a.axes[2]) };
		XMVECTOR axesB[3] = { XMLoadFloat3(&b.axes[0]), XMLoadFloat3(&b.axes[1]), XMLoadFloat3(&b.axes[2]) };
		float extA[3] = { a.extents.x, a.extents.y, a.extents.z };
		float extB[3] = { b.extents.x, b.extents.y, b.extents.z };

		float minOverlap = FLT_MAX;
		XMVECTOR mtvAxis = XMVectorSet(0, 1, 0, 0); // 最小移動ベクトル

		// 15軸すべてで分離判定
		auto TestAxis = [&](XMVECTOR axis) -> bool {
			if (LengthSq(axis) < 1e-6f) return true; // 平行などで軸が潰れた場合
			axis = XMVector3Normalize(axis);

			float rA = extA[0] * std::abs(XMVectorGetX(XMVector3Dot(axesA[0], axis))) +
				extA[1] * std::abs(XMVectorGetX(XMVector3Dot(axesA[1], axis))) +
				extA[2] * std::abs(XMVectorGetX(XMVector3Dot(axesA[2], axis)));

			float rB = extB[0] * std::abs(XMVectorGetX(XMVector3Dot(axesB[0], axis))) +
				extB[1] * std::abs(XMVectorGetX(XMVector3Dot(axesB[1], axis))) +
				extB[2] * std::abs(XMVectorGetX(XMVector3Dot(axesB[2], axis)));

			float dist = std::abs(XMVectorGetX(XMVector3Dot(translation, axis)));
			float overlap = (rA + rB) - dist;

			if (overlap < 0) return false; // 分離している

			// 最小の押し出し量を記録
			if (overlap < minOverlap) {
				minOverlap = overlap;
				mtvAxis = axis;
				// 軸の向きを A -> B に揃える
				if (XMVectorGetX(XMVector3Dot(translation, axis)) < 0) {
					mtvAxis = -axis;
				}
			}
			return true;
			};

		// 1. Aの面法線 (3)
		for (int i = 0; i < 3; ++i) if (!TestAxis(axesA[i])) return false;
		// 2. Bの面法線 (3)
		for (int i = 0; i < 3; ++i) if (!TestAxis(axesB[i])) return false;
		// 3. エッジの外積 (9)
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				if (!TestAxis(XMVector3Cross(axesA[i], axesB[j]))) return false;
			}
		}

		// 衝突確定
		XMStoreFloat3(&outContact.normal, mtvAxis);
		outContact.depth = minOverlap;
		return true;
	}

	// OBB vs Capsule (SATベース)
	bool CollisionSystem::CheckOBBCapsule(const OBB& box, const Capsule& cap, Contact& outContact) {
		// カプセルを「半径付き線分」としてSATで判定
		XMVECTOR centerA = XMLoadFloat3(&box.center);
		// カプセル中心
		XMVECTOR capStart = XMLoadFloat3(&cap.start);
		XMVECTOR capEnd = XMLoadFloat3(&cap.end);
		XMVECTOR capAxis = capEnd - capStart;
		XMVECTOR centerB = (capStart + capEnd) * 0.5f;
		XMVECTOR translation = centerB - centerA;

		XMVECTOR axesA[3] = { XMLoadFloat3(&box.axes[0]), XMLoadFloat3(&box.axes[1]), XMLoadFloat3(&box.axes[2]) };
		float extA[3] = { box.extents.x, box.extents.y, box.extents.z };

		float minOverlap = FLT_MAX;
		XMVECTOR mtvAxis = XMVectorSet(0, 1, 0, 0);

		auto TestAxis = [&](XMVECTOR axis) -> bool {
			if (LengthSq(axis) < 1e-6f) return true;
			axis = XMVector3Normalize(axis);

			// Boxの投影
			float rA = extA[0] * std::abs(XMVectorGetX(XMVector3Dot(axesA[0], axis))) +
				extA[1] * std::abs(XMVectorGetX(XMVector3Dot(axesA[1], axis))) +
				extA[2] * std::abs(XMVectorGetX(XMVector3Dot(axesA[2], axis)));

			// Capsuleの投影: (線分長/2 * cosθ) + 半径
			float rB = (XMVectorGetX(XMVector3Length(capAxis)) * 0.5f) * std::abs(XMVectorGetX(XMVector3Dot(XMVector3Normalize(capAxis), axis))) +
				cap.radius;

			float dist = std::abs(XMVectorGetX(XMVector3Dot(translation, axis)));
			float overlap = (rA + rB) - dist;

			if (overlap < 0) return false;
			if (overlap < minOverlap) {
				minOverlap = overlap;
				mtvAxis = axis;
				if (XMVectorGetX(XMVector3Dot(translation, axis)) < 0) mtvAxis = -axis;
			}
			return true;
			};

		// Boxの面法線 (3)
		for (int i = 0; i < 3; ++i) if (!TestAxis(axesA[i])) return false;

		// Capsuleの軸との外積 (3)
		for (int i = 0; i < 3; ++i) if (!TestAxis(XMVector3Cross(axesA[i], capAxis))) return false;

		// 線分端点とBoxの最近接点方向 (Cross積だけではエッジvsエッジしか見れないため)
		// 簡易的に「球 vs OBB」のロジックを混ぜて補完する
		// （今回はSAT軸のみで判定し、最近接点ベースの軸は省略）

		// Box(A) -> Capsule(B)
		XMStoreFloat3(&outContact.normal, mtvAxis);
		outContact.depth = minOverlap;
		return true;
	}

	bool CollisionSystem::CheckOBBCylinder(const OBB& box, const Cylinder& cyl, Contact& outContact)
	{
		XMVECTOR boxCenter = XMLoadFloat3(&box.center);
		XMVECTOR boxAxes[3] = { XMLoadFloat3(&box.axes[0]), XMLoadFloat3(&box.axes[1]), XMLoadFloat3(&box.axes[2]) };
		float boxExtents[3] = { box.extents.x, box.extents.y, box.extents.z };

		XMVECTOR cylCenter = XMLoadFloat3(&cyl.center);
		XMVECTOR cylAxis = XMLoadFloat3(&cyl.axis);
		XMVECTOR translation = cylCenter - boxCenter;

		float minOverlap = FLT_MAX;
		XMVECTOR mtvAxis = XMVectorSet(0, 1, 0, 0);

		auto TestAxis = [&](XMVECTOR axis) -> bool {
			if (LengthSq(axis) < 1e-6f) return true;
			axis = XMVector3Normalize(axis);

			// Box Projection
			float rBox = boxExtents[0] * std::abs(XMVectorGetX(XMVector3Dot(boxAxes[0], axis))) +
				boxExtents[1] * std::abs(XMVectorGetX(XMVector3Dot(boxAxes[1], axis))) +
				boxExtents[2] * std::abs(XMVectorGetX(XMVector3Dot(boxAxes[2], axis)));

			// Cylinder Projection
			// 円柱の射影半径 = (高さ/2 * |cosθ|) + (半径 * |sinθ|)
			// |cosθ| = |dot(axis, cylAxis)|
			// |sinθ| = |cross(axis, cylAxis)| の長さ
			float rCylH = (cyl.height * 0.5f) * std::abs(XMVectorGetX(XMVector3Dot(cylAxis, axis)));
			float rCylR = cyl.radius * std::sqrt(XMVectorGetX(XMVector3LengthSq(XMVector3Cross(axis, cylAxis))));
			float rCyl = rCylH + rCylR;

			float dist = std::abs(XMVectorGetX(XMVector3Dot(translation, axis)));
			float overlap = (rBox + rCyl) - dist;

			if (overlap < 0) return false;
			if (overlap < minOverlap) {
				minOverlap = overlap;
				mtvAxis = axis;
				if (XMVectorGetX(XMVector3Dot(translation, axis)) < 0) mtvAxis = -axis;
			}
			return true;
			};

		// Box Axes
		for (int i = 0; i < 3; ++i) if (!TestAxis(boxAxes[i])) return false;

		// Cylinder Axis
		if (!TestAxis(cylAxis)) return false;

		// Cross Products (Box axes x Cylinder axis)
		for (int i = 0; i < 3; ++i) if (!TestAxis(XMVector3Cross(boxAxes[i], cylAxis))) return false;

		XMStoreFloat3(&outContact.normal, mtvAxis);
		outContact.depth = minOverlap;
		return true;
	}

	// Capsule vs Capsule
	bool CollisionSystem::CheckCapsuleCapsule(const Capsule& a, const Capsule& b, Contact& outContact) {
		XMVECTOR a1 = XMLoadFloat3(&a.start);
		XMVECTOR a2 = XMLoadFloat3(&a.end);
		XMVECTOR b1 = XMLoadFloat3(&b.start);
		XMVECTOR b2 = XMLoadFloat3(&b.end);

		XMVECTOR c1, c2; // 線分上の最近接点
		float distSq = SegmentSegmentDistanceSq(a1, a2, b1, b2, c1, c2);
		float rSum = a.radius + b.radius;

		if (distSq >= rSum * rSum) return false;

		float dist = std::sqrt(distSq);
		XMVECTOR normal;

		if (dist < 1e-4f) {
			normal = XMVectorSet(0, 1, 0, 0);
			dist = 0.0f;
		}
		else {
			normal = (c2 - c1) / dist; // A -> B
		}

		XMStoreFloat3(&outContact.normal, normal);
		outContact.depth = rSum - dist;
		return true;
	}

	// Sphere vs Cylinder (修正：外部法線の反転)
	bool CollisionSystem::CheckSphereCylinder(const Sphere& s, const Cylinder& c, Contact& outContact) {
		XMVECTOR sCenter = XMLoadFloat3(&s.center);
		XMVECTOR cCenter = XMLoadFloat3(&c.center);
		XMVECTOR cAxis = XMLoadFloat3(&c.axis);

		XMVECTOR d = sCenter - cCenter;
		float distY = XMVectorGetX(XMVector3Dot(d, cAxis));

		XMVECTOR onAxis = cCenter + cAxis * distY;
		XMVECTOR radialVec = sCenter - onAxis;
		float distRadSq = LengthSq(radialVec);
		float distRad = std::sqrt(distRadSq);

		float halfH = c.height * 0.5f;
		float r = c.radius;

		// 1. 領域判定
		if (std::abs(distY) > halfH + s.radius) return false;
		if (distRad > r + s.radius) return false;

		// 2. 内部判定
		bool insideY = (std::abs(distY) < halfH);
		bool insideR = (distRad < r);
		bool isCenterInside = insideY && insideR;

		if (isCenterInside) {
			// --- 内部 (前回修正済み・OK) ---
			float depthSide = r - distRad;
			float depthTop = halfH - distY;
			float depthBottom = distY - (-halfH);

			float minDepth = depthSide;
			// A->B法線なので「内向き」。radialVec(外向き)の逆
			XMVECTOR normal = (distRad > 1e-5f) ? (-radialVec / distRad) : XMVectorSet(-1, 0, 0, 0);

			if (depthTop < minDepth) {
				minDepth = depthTop;
				normal = -cAxis; // 下へ
			}
			if (depthBottom < minDepth) {
				minDepth = depthBottom;
				normal = cAxis; // 上へ
			}

			XMStoreFloat3(&outContact.normal, normal);
			outContact.depth = s.radius + minDepth;
			return true;
		}
		else {
			// --- 外部 (【修正箇所】ここが逆でした) ---
			XMVECTOR closest;

			if (insideY) {
				closest = onAxis + (radialVec / distRad) * r;
			}
			else if (insideR) {
				float sign = (distY > 0) ? 1.0f : -1.0f;
				closest = cCenter + cAxis * (halfH * sign) + radialVec;
			}
			else {
				float sign = (distY > 0) ? 1.0f : -1.0f;
				XMVECTOR capCenter = cCenter + cAxis * (halfH * sign);
				closest = capCenter + (radialVec / distRad) * r;
			}

			// 距離チェック
			XMVECTOR distVec = closest - sCenter; // 【修正】 A->B (Sphere -> Surface) に変更
			float distSq = LengthSq(distVec);

			if (distSq > s.radius * s.radius) return false;

			float dist = std::sqrt(distSq);

			XMVECTOR normal;
			if (dist < 1e-5f) {
				// 密着時: A->B なので 内向き
				normal = (distRad > 1e-5f) ? (-radialVec / distRad) : -cAxis;
			}
			else {
				normal = distVec / dist; // これで A->B (内向き) になる
			}

			XMStoreFloat3(&outContact.normal, normal);
			outContact.depth = s.radius - dist;
			return true;
		}
	}

	// Capsule vs Cylinder (すり抜け防止強化)
	bool CollisionSystem::CheckCapsuleCylinder(const Capsule& cap, const Cylinder& cyl, Contact& outContact) {
		// 最も深い衝突情報を記録するための変数
		float maxDepth = -FLT_MAX;
		XMVECTOR bestNormal = XMVectorZero();
		bool hit = false;

		// 判定用の一時球体
		Sphere s;
		s.radius = cap.radius;
		Contact temp; // 法線・深度取得用

		// 1. 始点 (Start) の判定
		s.center = cap.start;
		if (CheckSphereCylinder(s, cyl, temp)) {
			if (temp.depth > maxDepth) {
				maxDepth = temp.depth;
				bestNormal = XMLoadFloat3(&temp.normal);
				hit = true;
			}
		}

		// 2. 終点 (End) の判定
		s.center = cap.end;
		if (CheckSphereCylinder(s, cyl, temp)) {
			if (!hit || temp.depth > maxDepth) {
				maxDepth = temp.depth;
				bestNormal = XMLoadFloat3(&temp.normal);
				hit = true;
			}
		}

		// 3. 軸上の最近接点 (Closest Point on Axis) の判定
		XMVECTOR capA = XMLoadFloat3(&cap.start);
		XMVECTOR capB = XMLoadFloat3(&cap.end);
		XMVECTOR cylPos = XMLoadFloat3(&cyl.center);
		XMVECTOR cylAxis = XMLoadFloat3(&cyl.axis);
		float cylH = cyl.height;

		XMVECTOR cylP1 = cylPos - cylAxis * (cylH * 0.5f);
		XMVECTOR cylP2 = cylPos + cylAxis * (cylH * 0.5f);

		XMVECTOR cCap, cCyl;
		// カプセル軸と円柱軸の最短距離にある点(cCap)を求める
		SegmentSegmentDistanceSq(capA, capB, cylP1, cylP2, cCap, cCyl);

		XMStoreFloat3(&s.center, cCap);
		if (CheckSphereCylinder(s, cyl, temp)) {
			if (!hit || temp.depth > maxDepth) {
				maxDepth = temp.depth;
				bestNormal = XMLoadFloat3(&temp.normal);
				hit = true;
			}
		}

		if (hit) {
			// 【重要】outContactのa, b（エンティティ情報）を消さないように、
			// 法線と深度だけを更新する
			XMStoreFloat3(&outContact.normal, bestNormal);
			outContact.depth = maxDepth;
			return true;
		}
		return false;
	}

	// Cylinder vs Cylinder (Gap解消・振動防止)
	bool CollisionSystem::CheckCylinderCylinder(const Cylinder& a, const Cylinder& b, Contact& outContact) {
		XMVECTOR posA = XMLoadFloat3(&a.center);
		XMVECTOR axisA = XMLoadFloat3(&a.axis);
		XMVECTOR posB = XMLoadFloat3(&b.center);
		XMVECTOR axisB = XMLoadFloat3(&b.axis);

		// 1. 側面チェック (カプセル近似)
		XMVECTOR pa = posA - axisA * (a.height * 0.5f);
		XMVECTOR qa = posA + axisA * (a.height * 0.5f);
		XMVECTOR pb = posB - axisB * (b.height * 0.5f);
		XMVECTOR qb = posB + axisB * (b.height * 0.5f);

		XMVECTOR c1, c2;
		float distSq = SegmentSegmentDistanceSq(pa, qa, pb, qb, c1, c2);
		float rSum = a.radius + b.radius;

		bool sideHit = (distSq < rSum * rSum);
		float sideDepth = 0.0f;
		XMVECTOR sideNormal = XMVectorZero();

		if (sideHit) {
			float dist = std::sqrt(distSq);
			sideDepth = rSum - dist;
			if (dist < 1e-5f) {
				XMVECTOR arb = XMVector3Cross(axisA, axisB);
				if (LengthSq(arb) < 1e-4f) arb = XMVectorSet(1, 0, 0, 0);
				sideNormal = XMVector3Normalize(arb);
			}
			else {
				sideNormal = (c2 - c1) / dist; // A->B
			}
		}

		// 2. SATチェック (縦方向)
		float minSatOverlap = FLT_MAX;
		XMVECTOR satNormal = XMVectorZero();
		bool satHit = true;

		auto TestAxis = [&](XMVECTOR axis) -> bool {
			if (LengthSq(axis) < 1e-6f) return true;
			axis = XMVector3Normalize(axis);

			auto Proj = [](XMVECTOR ax, XMVECTOR cP, XMVECTOR cAx, float h, float r) {
				float p = XMVectorGetX(XMVector3Dot(cP, ax));
				float c = std::abs(XMVectorGetX(XMVector3Dot(cAx, ax)));
				float s = XMVectorGetX(XMVector3Length(XMVector3Cross(cAx, ax)));
				float e = (h * 0.5f) * c + r * s;
				return std::make_pair(p - e, p + e);
				};
			auto rA = Proj(axis, posA, axisA, a.height, a.radius);
			auto rB = Proj(axis, posB, axisB, b.height, b.radius);
			float d1 = rA.second - rB.first;
			float d2 = rB.second - rA.first;
			if (d1 < 0 || d2 < 0) return false;

			float o = std::min(d1, d2);
			if (o < minSatOverlap) {
				minSatOverlap = o;
				satNormal = axis;
				// A->B に揃える
				if (XMVectorGetX(XMVector3Dot(posB - posA, axis)) < 0) satNormal = -satNormal;
			}
			return true;
			};

		if (!TestAxis(axisA)) satHit = false;
		if (satHit && !TestAxis(axisB)) satHit = false;

		// 【重要】Gap対策: 側面(Radial) と 縦(SAT) の両方が重なっていないと衝突ではない
		// 以前は (!sideHit && !satHit) だったので、片方でもTrueならヒットになっていた
		// これにより「浮いているのにカプセル近似がヒットする」現象を防ぐ
		if (!sideHit || !satHit) return false;

		// 決定ロジック
		if (minSatOverlap < sideDepth * 0.6f) {
			XMStoreFloat3(&outContact.normal, satNormal);
			outContact.depth = minSatOverlap;
		}
		else {
			XMStoreFloat3(&outContact.normal, sideNormal);
			outContact.depth = sideDepth;
		}

		return true;
	}

	// =================================================================
	// メイン更新ループ
	// =================================================================

	void CollisionSystem::Initialize(Registry& registry)
	{
		if (m_isInitialized) return;

		// 1. Observerの設定
		m_observer.connect(registry)
			.update<Transform>()
			.update<Collider>()
			.group<Collider>()
			.where<Transform, Collider>();

		// 2. 初期化
		registry.view<Transform, Collider>().each([&](Entity e, Transform& t, Collider& c) {
			if (!registry.has<WorldCollider>(e)) {
				registry.emplace<WorldCollider>(e);
			}
			auto& wc = registry.get<WorldCollider>(e);
			wc.isDirty = true;
			UpdateWorldCollider(registry, e, t, c, wc);
		});

		m_isInitialized = true;
	}

	// キャッシュ（WorldCollider）の更新処理
	void CollisionSystem::UpdateWorldCollider(Registry& registry, Entity e, const Transform& t, const Collider& c, WorldCollider& wc) 
	{
		// ワールド行列の分解 (Scale, Rotation, Position)
		XMVECTOR scale, rotQuat, pos;
		XMMatrixDecompose(&scale, &rotQuat, &pos, t.GetWorldMatrix());
		XMFLOAT3 gScale; XMStoreFloat3(&gScale, scale);
		XMMATRIX rotMat = XMMatrixRotationQuaternion(rotQuat);

		// オフセット込みの中心座標
		XMVECTOR offsetVec = XMLoadFloat3(&c.offset);
		offsetVec = XMVectorSetW(offsetVec, 1.0f);
		XMVECTOR centerVec = XMVector3TransformCoord(offsetVec, t.GetWorldMatrix());

		// AABB計算用の最小/最大
		XMVECTOR vMin, vMax;

		// 形状毎の計算
		if(c.type == ColliderType::Box)
		{
			XMStoreFloat3(&wc.center, centerVec);
			wc.extents = {
				c.boxSize.x * gScale.x * 0.5f,
				c.boxSize.y * gScale.y * 0.5f,
				c.boxSize.z * gScale.z * 0.5f
			};
			XMFLOAT4X4 rotM; XMStoreFloat4x4(&rotM, rotMat);
			wc.axes[0] = { rotM._11, rotM._12, rotM._13 };
			wc.axes[1] = { rotM._21, rotM._22, rotM._23 };
			wc.axes[2] = { rotM._31, rotM._32, rotM._33 };

			// AABB計算
			XMVECTOR center = centerVec;
			XMVECTOR ext = XMLoadFloat3(&wc.extents);
			XMVECTOR axisX = XMLoadFloat3(&wc.axes[0]);
			XMVECTOR axisY = XMLoadFloat3(&wc.axes[1]);
			XMVECTOR axisZ = XMLoadFloat3(&wc.axes[2]);

			XMVECTOR newExt =
				XMVectorAbs(axisX) * XMVectorGetX(ext) +
				XMVectorAbs(axisY) * XMVectorGetY(ext) +
				XMVectorAbs(axisZ) * XMVectorGetZ(ext);

			vMin = center - newExt;
			vMax = center + newExt;
		}
		else if (c.type == ColliderType::Sphere)
		{
			XMStoreFloat3(&wc.center, centerVec);
			wc.radius = c.radius * std::max({ gScale.x, gScale.y, gScale.z });

			XMVECTOR r = XMVectorReplicate(wc.radius);

			vMin = centerVec - r;
			vMax = centerVec + r;
		}
		else if(c.type == ColliderType::Capsule)
		{
			XMVECTOR axisY = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMat);
			float h = c.height * gScale.y;
			float r = c.radius * std::max(gScale.x, gScale.z);
			float segLen = std::max(0.0f, h * 0.5f - r);

			XMVECTOR p1 = centerVec - axisY * segLen;
			XMVECTOR p2 = centerVec + axisY * segLen;
			XMStoreFloat3(&wc.start, p1);
			XMStoreFloat3(&wc.end, p2);
			wc.radius = r;

			// AABB計算
			XMVECTOR capMin = XMVectorMin(p1, p2) - XMVectorReplicate(r);
			XMVECTOR capMax = XMVectorMax(p1, p2) + XMVectorReplicate(r);
			vMin = capMin;
			vMax = capMax;
		}
		else if(c.type == ColliderType::Cylinder)
		{
			XMStoreFloat3(&wc.center, centerVec);
			XMVECTOR axisY = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMat);
			XMStoreFloat3(&wc.axis, axisY);
			wc.height = c.height * gScale.y;
			wc.radius = c.radius * std::max(gScale.x, gScale.z);

			// AABB計算
			// 円柱の頂点群から計算するのが正確だが、ここでは「球」として近似したAABBで代用
			// あるいは中心軸線分 + 半径で計算
			float halfH = wc.height * 0.5f;
			float rad = wc.radius;
			// 軸に垂直な平面での広がりを考慮
			XMVECTOR ex = XMVectorAbs(axisY) * halfH;
			// 半径分を全方向に追加
			XMVECTOR radiusExt = XMVectorSet(rad, rad, rad, 0) * XMVectorSplatOne();
		
			vMin = centerVec - ex - radiusExt;
			vMax = centerVec + ex + radiusExt;
		}

		// ----------------------------------------------------------------------
		// Swept AABB
		// 高速移動ですり抜けないよう、1フレーム前の位置も含めたAABBにする
		// ----------------------------------------------------------------------
		if (registry.has<Rigidbody>(e))
		{
			const auto& rb = registry.get<Rigidbody>(e);
			if (rb.type == BodyType::Dynamic)
			{
				XMVECTOR vel = XMLoadFloat3(&rb.velocity);
				float dt = Time::DeltaTime();

				// 1フレーム前の位置（現在の位置 - velocity * dt）
				XMVECTOR prevPosOffset = -(vel * dt);

				XMVECTOR prevMin = vMin + prevPosOffset;
				XMVECTOR prevMax = vMax + prevPosOffset;

				vMin = XMVectorMin(vMin, prevMin);
				vMax = XMVectorMax(vMax, prevMax);
			}
		}

		XMStoreFloat3(&wc.aabb.min, vMin);
		XMStoreFloat3(&wc.aabb.max, vMax);
		wc.isDirty = false;
	}

	void CollisionSystem::Update(Registry& registry)
	{
		if (!m_isInitialized) Initialize(registry);

		// 1. イベントマネージャーの初期化
		auto& eventMgr = EventManager::Instance();
		eventMgr.Clear();

		// 2. 空間ハッシュのリセット
		g_spatialHash.Clear();

		// 3. Observerによる差分更新（動いたものだけ計算し直す）
		m_observer.each([&](Entity e) {
			if(registry.has<Transform>(e) && registry.has<Collider>(e)) {
				// 自動追加
				if(!registry.has<WorldCollider>(e)) {
					registry.emplace<WorldCollider>(e);
				}

				auto& t = registry.get<Transform>(e);
				auto& c = registry.get<Collider>(e);
				auto& wc = registry.get<WorldCollider>(e);

				UpdateWorldCollider(registry, e, t, c, wc);
			}
		});

		// クリア
		m_observer.clear();

		// 4. 全エンティティを空間ハッシュに登録
		registry.view<WorldCollider>().each([&](Entity e, WorldCollider& wc)
		{
			g_spatialHash.Register(e, wc.aabb.min, wc.aabb.max);
		});

		// 5. 衝突判定（Spatial Hash + Narrow Phase）
		std::vector<Contact> contactsForSolver;
		std::map<EntityPair, Contact> currentContactsMap;

		registry.view<Transform, Collider, WorldCollider>().each([&](Entity eA, Transform& tA, Collider& cA, WorldCollider& wcA)
		{
			// 周辺エンティティのみ取得（高速化）
			auto candidates = g_spatialHash.Query(wcA.aabb.min, wcA.aabb.max);

			for (Entity eB : candidates)
			{
				if (eA >= eB) continue;	// 重複チェック（A-B と B-A は同じ）
				if (!registry.valid(eB)) continue;

				auto& cB = registry.get<Collider>(eB);

				// レイヤーマスク判定
				if (!(cA.mask & cB.layer) || !(cB.mask & cA.layer)) continue;

				// Static同士は判定しない
				bool isStaticA = (!registry.has<Rigidbody>(eA) || registry.get<Rigidbody>(eA).type == BodyType::Static);
				bool isStaticB = (!registry.has<Rigidbody>(eB) || registry.get<Rigidbody>(eB).type == BodyType::Static);
				if (isStaticA && isStaticB) continue;

				auto& wcB = registry.get<WorldCollider>(eB);

				// Broad Phase (AABB)
				if (wcA.aabb.max.x < wcB.aabb.min.x || wcA.aabb.min.x > wcB.aabb.max.x ||
					wcA.aabb.max.y < wcB.aabb.min.y || wcA.aabb.min.y > wcB.aabb.max.y ||
					wcA.aabb.max.z < wcB.aabb.min.z || wcA.aabb.min.z > wcB.aabb.max.z)
				{
					continue; // 重なっていない
				}

				// Narrow Phase
				Contact contact;
				contact.a = eA;
				contact.b = eB;
				bool hit = false;
				auto& tB = registry.get<Transform>(eB);

				// Sphere vs ...
				if (cA.type == ColliderType::Sphere)
				{
					Sphere sA = { wcA.center, wcA.radius };

					if (cB.type == ColliderType::Sphere)
					{
						Sphere sB = { wcB.center, wcB.radius };
						hit = CheckSphereSphere(sA, sB, contact);
					}
					else if (cB.type == ColliderType::Box)
					{
						OBB oB = { wcB.center, wcB.extents, wcB.axes[0], wcB.axes[1], wcB.axes[2] };
						hit = CheckSphereOBB(sA, oB, contact);
					}
					else if (cB.type == ColliderType::Capsule)
					{
						Capsule cpB = { wcB.start, wcB.end, wcB.radius };
						hit = CheckSphereCapsule(sA, cpB, contact);
					}
					else if (cB.type == ColliderType::Cylinder)
					{
						Cylinder cyB = { wcB.center, wcB.axis, wcB.height, wcB.radius };
						hit = CheckSphereCylinder(sA, cyB, contact);
					}
				}
				// Box vs ...
				else if (cA.type == ColliderType::Box)
				{
					OBB oA = { wcA.center, wcA.extents, wcA.axes[0], wcA.axes[1], wcA.axes[2] };

					if (cB.type == ColliderType::Box)
					{
						OBB oB = { wcB.center, wcB.extents, wcB.axes[0], wcB.axes[1], wcB.axes[2] };
						hit = CheckOBBOBB(oA, oB, contact);
					}
					else if (cB.type == ColliderType::Sphere)
					{
						Sphere sB = { wcB.center, wcB.radius };
						hit = CheckSphereOBB(sB, oA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
					else if (cB.type == ColliderType::Capsule)
					{
						Capsule cpB = { wcB.start, wcB.end, wcB.radius };
						hit = CheckOBBCapsule(oA, cpB, contact);
					}
					else if (cB.type == ColliderType::Cylinder)
					{
						Cylinder cyB = { wcB.center, wcB.axis, wcB.height, wcB.radius };
						hit = CheckOBBCylinder(oA, cyB, contact);
					}
				}
				// Capsule vs ...
				else if (cA.type == ColliderType::Capsule)
				{
					Capsule cpA = { wcA.start, wcA.end, wcA.radius };

					if (cB.type == ColliderType::Capsule)
					{
						Capsule cpB = { wcB.start, wcB.end, wcB.radius };
						hit = CheckCapsuleCapsule(cpA, cpB, contact);
					}
					else if (cB.type == ColliderType::Sphere)
					{
						Sphere sB = { wcB.center, wcB.radius };
						hit = CheckSphereCapsule(sB, cpA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
					else if (cB.type == ColliderType::Box)
					{
						OBB oB = { wcB.center, wcB.extents, wcB.axes[0], wcB.axes[1], wcB.axes[2] };
						hit = CheckOBBCapsule(oB, cpA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
					else if (cB.type == ColliderType::Cylinder)
					{
						Cylinder cyB = { wcB.center, wcB.axis, wcB.height, wcB.radius };
						hit = CheckCapsuleCylinder(cpA, cyB, contact);
					}
				}
				// Cylinder vs ...
				else if (cA.type == ColliderType::Cylinder)
				{
					Cylinder cyA = { wcA.center, wcA.axis, wcA.height, wcA.radius };

					if (cB.type == ColliderType::Cylinder)
					{
						Cylinder cyB = { wcB.center, wcB.axis, wcB.height, wcB.radius };
						hit = CheckCylinderCylinder(cyA, cyB, contact);
					}
					else if (cB.type == ColliderType::Sphere)
					{
						Sphere sB = { wcB.center, wcB.radius };
						hit = CheckSphereCylinder(sB, cyA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
					else if (cB.type == ColliderType::Capsule)
					{
						Capsule cpB = { wcB.start, wcB.end, wcB.radius };
						hit = CheckCapsuleCylinder(cpB, cyA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
					else if (cB.type == ColliderType::Box)
					{
						OBB oB = { wcB.center, wcB.extents, wcB.axes[0], wcB.axes[1], wcB.axes[2] };
						hit = CheckOBBCylinder(oB, cyA, contact);
						if (hit) contact.normal = { -contact.normal.x, -contact.normal.y, -contact.normal.z }; // 反転
					}
				}

				if (hit)
				{
					// TriggerならSolverには送らないが、イベントには残す
					if (!cA.isTrigger && !cB.isTrigger)
					{
						contactsForSolver.push_back(contact);
					}

					// イベント検知用に保存
					EntityPair pair = (eA < eB) ? EntityPair(eA, eB) : EntityPair(eB, eA);
					currentContactsMap[pair] = contact;

					// Normal向き補正（保存時A->Bにする）
					if (eA != pair.first)
					{
						// eB(pair.first) -> eA(pair.second) の向きとして保存されている場合、反転
						// contact.normal は A->B なのでそのままでOK?
						// ここはイベントで使う時に注意が必要
					}
				}
			}
		});

		// 6. イベント発行（Enter / Stay / Exit）
		// Exit: 前回あって今回ない
		for (auto& prev : g_prevContacts)
		{
			if (currentContactsMap.find(prev.first) == currentContactsMap.end())
			{
				eventMgr.AddEvent(prev.first.first, prev.first.second, CollisionState::Exit, { 0,0,0 });
			}
		}
		// Enter / Stay
		for (auto& curr : currentContactsMap)
		{
			CollisionState state = CollisionState::Stay;

			// 前回無かったらEnter
			if (g_prevContacts.find(curr.first) == g_prevContacts.end())
			{
				state = CollisionState::Enter;
			}

			eventMgr.AddEvent(curr.first.first, curr.first.second, state, curr.second.normal);
		}

		// 履歴更新
		g_prevContacts.clear();
		for (auto& curr : currentContactsMap)
		{
			g_prevContacts[curr.first] = true;
		}

		// 7. 物理応答
		PhysicsSystem::Solve(registry, contactsForSolver);
	}

	void CollisionSystem::Reset()
	{
		g_prevContacts.clear();
		g_spatialHash.Clear();

		// Observerリセット
		m_observer.clear();
		m_isInitialized = false;
	}

}	// namespace Arche