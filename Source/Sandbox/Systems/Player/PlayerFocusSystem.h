#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class PlayerFocusSystem : public ISystem
	{
	public:
		PlayerFocusSystem()
		{
			m_systemName = "PlayerFocusSystem";
		}

		void Update(Registry& registry) override
		{
			Entity playerEntity = NullEntity;
			auto playerView = registry.view<PlayerController, Transform>();
			for (auto e : playerView) { playerEntity = e; break; }

			if (playerEntity == NullEntity) return;

			auto& ctrl = registry.get<PlayerController>(playerEntity);
			auto& pTrans = registry.get<Transform>(playerEntity);

			FindBestTarget(registry, playerEntity, ctrl, pTrans);
			UpdateReticleVisuals(registry, ctrl);
		}

	private:
		// 演出用エンティティ
		Entity m_ringMain = NullEntity; // メインの細いリング
		Entity m_ringSub = NullEntity; // 斜めに回るサブリング
		Entity m_core = NullEntity; // 中心のコア

		XMFLOAT3 m_currentPos = { 0, -100, 0 }; // 補間用位置
		float m_spinAngle = 0.0f;				// 回転アニメーション用

		void FindBestTarget(Registry& reg, Entity player, PlayerController& ctrl, Transform& pTrans)
		{
			// (ターゲット検索ロジックは前回と同じなので省略可ですが、念のため記載)
			XMVECTOR pPos = XMLoadFloat3(&pTrans.position);
			XMMATRIX rotM = XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(pTrans.rotation.x),
				XMConvertToRadians(pTrans.rotation.y),
				0.0f);
			XMVECTOR pForward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotM);

			Entity bestTarget = NullEntity;
			float closestDistSq = ctrl.focusRange * ctrl.focusRange;
			float minDot = cosf(XMConvertToRadians(ctrl.focusAngle));

			auto enemyView = reg.view<EnemyStats, Transform>();
			for (auto e : enemyView)
			{
				auto& eTrans = enemyView.get<Transform>(e);
				XMVECTOR ePos = XMLoadFloat3(&eTrans.position);
				XMVECTOR toEnemy = ePos - pPos;
				float distSq = XMVectorGetX(XMVector3LengthSq(toEnemy));

				if (distSq < closestDistSq)
				{
					XMVECTOR toEnemyDir = XMVector3Normalize(toEnemy);
					float dot = XMVectorGetX(XMVector3Dot(pForward, toEnemyDir));
					if (dot > minDot)
					{
						closestDistSq = distSq;
						bestTarget = e;
					}
				}
			}
			ctrl.focusTarget = bestTarget;
		}

		void UpdateReticleVisuals(Registry& reg, const PlayerController& ctrl)
		{
			// --------------------------------------------------------
			// 1. パーツ生成 (無ければ作る)
			// --------------------------------------------------------
			if (!reg.valid(m_ringMain))
			{
				m_ringMain = reg.create();
				reg.emplace<Transform>(m_ringMain);
				auto& geo = reg.emplace<GeometricDesign>(m_ringMain);

				// ★重要テクニック: 円柱(Cylinder)を使い、高さを潰すことで「極細リング」を作る
				geo.shapeType = GeoShape::Cylinder;
				geo.isWireframe = true;
				geo.color = { 0.0f, 1.0f, 1.0f, 0.8f }; // シアン
			}
			if (!reg.valid(m_ringSub))
			{
				m_ringSub = reg.create();
				reg.emplace<Transform>(m_ringSub);
				auto& geo = reg.emplace<GeometricDesign>(m_ringSub);

				// サブリングも円柱で極細に
				geo.shapeType = GeoShape::Cylinder;
				geo.isWireframe = true;
				geo.color = { 0.0f, 0.8f, 1.0f, 0.5f }; // 少し薄いシアン
			}
			if (!reg.valid(m_core))
			{
				m_core = reg.create();
				reg.emplace<Transform>(m_core);
				auto& geo = reg.emplace<GeometricDesign>(m_core);

				// 中心は八面体でデジタル感を出す
				geo.shapeType = GeoShape::Diamond;
				geo.isWireframe = true;
				geo.color = { 1.0f, 0.0f, 0.5f, 1.0f }; // マゼンタ
			}

			// --------------------------------------------------------
			// 2. ターゲット追従と状態管理
			// --------------------------------------------------------
			float dt = Time::DeltaTime();
			bool hasTarget = reg.valid(ctrl.focusTarget) && reg.has<Transform>(ctrl.focusTarget);

			XMVECTOR currentPosVec = XMLoadFloat3(&m_currentPos);
			XMVECTOR targetPosVec = currentPosVec;
			float scaleFactor = 0.0f;

			// ロックオン時の色: 赤(攻撃的) / 通常時: 青(探索中)
			XMFLOAT4 targetColorOuter = { 0.0f, 1.0f, 1.0f, 0.6f }; // 青
			XMFLOAT4 targetColorCore = { 0.0f, 1.0f, 1.0f, 0.4f };

			if (hasTarget)
			{
				// ターゲット位置へ
				auto& tTarget = reg.get<Transform>(ctrl.focusTarget);
				targetPosVec = XMLoadFloat3(&tTarget.position) + XMVectorSet(0, 1.0f, 0, 0); // 胸のあたり
				scaleFactor = 1.0f;

				// ロックオン中は色を赤っぽく、回転を速くする
				targetColorOuter = { 1.0f, 0.2f, 0.2f, 0.9f }; // 赤
				targetColorCore = { 1.0f, 0.0f, 0.0f, 1.0f };

				m_spinAngle += dt * 360.0f; // 高速回転
			}
			else
			{
				// ターゲット無し：ゆっくり回転、色は青、サイズ0へ
				scaleFactor = 0.0f;
				m_spinAngle += dt * 45.0f; // 低速回転
			}

			// 位置の補間 (吸い付くような動き)
			currentPosVec = XMVectorLerp(currentPosVec, targetPosVec, dt * 15.0f);
			XMStoreFloat3(&m_currentPos, currentPosVec);


			// --------------------------------------------------------
			// 3. 各パーツのアニメーション適用
			// --------------------------------------------------------

			// --- [A] メインリング (水平回転) ---
			if (reg.valid(m_ringMain))
			{
				auto& t = reg.get<Transform>(m_ringMain);
				auto& geo = reg.get<GeometricDesign>(m_ringMain);

				t.position = m_currentPos;

				// Lerpでサイズと色を変更
				float s = std::lerp(t.scale.x, 1.5f * scaleFactor, dt * 10.0f);

				// ★重要: Yスケールを極小(0.02)にすることで、円柱を「リング」に見せる
				t.scale = { s, 0.02f, s };

				// 回転: ターゲット方向を向きつつ、Z軸回転などで演出
				// ここではシンプルにY軸で回す + 少し傾ける
				t.rotation.x = 20.0f * sinf(Time::TotalTime()); // ゆらゆらさせる
				t.rotation.y = m_spinAngle;
				t.rotation.z = 0.0f;

				// 色の更新
				geo.color = LerpColor(geo.color, targetColorOuter, dt * 5.0f);
			}

			// --- [B] サブリング (縦回転・軌道エレベータ風) ---
			if (reg.valid(m_ringSub))
			{
				auto& t = reg.get<Transform>(m_ringSub);
				t.position = m_currentPos;

				// メインより少し大きく
				float s = std::lerp(t.scale.x, 1.8f * scaleFactor, dt * 10.0f);

				// こちらも極細に
				t.scale = { s, 0.02f, s };

				// 90度傾けて縦回転させる
				t.rotation.x = 90.0f;
				t.rotation.y = 0.0f;
				t.rotation.z = -m_spinAngle * 0.5f; // メインと逆回転
			}

			// --- [C] コア (中心で明滅) ---
			if (reg.valid(m_core))
			{
				auto& t = reg.get<Transform>(m_core);
				auto& geo = reg.get<GeometricDesign>(m_core);

				t.position = m_currentPos;

				// 呼吸するように脈打つ
				float pulse = 1.0f + 0.2f * sinf(Time::TotalTime() * 10.0f);
				float s = std::lerp(t.scale.x, 0.5f * scaleFactor * pulse, dt * 20.0f);
				t.scale = { s, s, s };

				// 複雑に回転
				t.rotation.x = m_spinAngle;
				t.rotation.y = m_spinAngle;

				geo.color = LerpColor(geo.color, targetColorCore, dt * 10.0f);
			}
		}

		// 色補間ヘルパー
		XMFLOAT4 LerpColor(const XMFLOAT4& a, const XMFLOAT4& b, float t)
		{
			return {
				std::lerp(a.x, b.x, t),
				std::lerp(a.y, b.y, t),
				std::lerp(a.z, b.z, t),
				std::lerp(a.w, b.w, t)
			};
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerFocusSystem, "PlayerFocusSystem")
