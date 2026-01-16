#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

namespace Arche
{
	class CameraFollowSystem : public ISystem
	{
	public:
		CameraFollowSystem()
		{
			m_systemName = "CameraFollowSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 1. プレイヤーの位置を探す
			XMVECTOR targetPos = XMVectorZero();
			bool playerFound = false;

			auto entities = registry.view<Tag, Transform>();
			for (auto e : entities)
			{
				// Tagコンポーネントの "tag" 変数で判定（"Player"と設定されているものを探す）
				if (entities.get<Tag>(e).tag == "Player")
				{
					targetPos = XMLoadFloat3(&entities.get<Transform>(e).position);
					playerFound = true;
					break;
				}
			}

			if (!playerFound) return;

			// 2. カメラを移動させる
			auto cameras = registry.view<Camera, Transform>();
			for (auto e : cameras)
			{
				auto& camTrans = cameras.get<Transform>(e);

				// 目標位置 = プレイヤー位置 + オフセット
				XMVECTOR offsetVec = XMLoadFloat3(&offset);
				XMVECTOR desiredPos = targetPos + offsetVec;
				XMVECTOR currentPos = XMLoadFloat3(&camTrans.position);

				// 線形補間 (Lerp) で滑らかに移動
				XMVECTOR smoothedPos = XMVectorLerp(currentPos, desiredPos, smoothSpeed * dt);

				XMStoreFloat3(&camTrans.position, smoothedPos);

				// カメラの注視点（LookAt）を設定
				// 常にプレイヤーを見るように回転させる
				XMVECTOR eye = smoothedPos;
				XMVECTOR focus = targetPos;
				XMVECTOR up = XMVectorSet(0, 1, 0, 0);

				// LookAt行列から回転（Quaternion/Euler）を計算するのは少し手間なので、
				// 今回は簡易的に「固定角度」にするか、計算するか。
				// 簡易実装: オフセットに基づいた固定角度を入れておく
				camTrans.rotation.x = 45.0f; // 下を向く
				camTrans.rotation.y = 0.0f;
				camTrans.rotation.z = 0.0f;
			}
		}

	private:
		XMFLOAT3 offset = { 0.0f, 21.0f, -18.0f };
		float smoothSpeed = 5.0f;	// 追従の滑らかさ
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::CameraFollowSystem, "CameraFollowSystem")
