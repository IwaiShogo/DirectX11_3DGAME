#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Player/PlayerMoveData.h"
#include <cmath>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class CameraFollowSystem : public ISystem
	{
	public:
		CameraFollowSystem() { m_systemName = "CameraFollowSystem"; }

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			Entity player = NullEntity;
			XMVECTOR targetPos = XMVectorZero();
			XMVECTOR playerVel = XMVectorZero();

			// プレイヤー探索
			auto entities = registry.view<Tag, Transform>();
			for (auto e : entities) {
				if (entities.get<Tag>(e).tag == "Player") {
					player = e;
					targetPos = XMLoadFloat3(&entities.get<Transform>(e).position);
					if (registry.has<Rigidbody>(e)) {
						playerVel = XMLoadFloat3(&registry.get<Rigidbody>(e).velocity);
					}
					break;
				}
			}

			if (player == NullEntity) return;

			float speed = XMVectorGetX(XMVector3Length(playerVel * XMVectorSet(1, 0, 1, 0)));

			auto cameras = registry.view<Camera, Transform>();
			for (auto e : cameras)
			{
				if (!registry.has<AudioListener>(e)) {
					registry.emplace<AudioListener>(e);
				}

				auto& camTrans = cameras.get<Transform>(e);

				// 1. 位置計算
				XMVECTOR baseOffset = XMVectorSet(0.0f, 22.0f, -18.0f, 0.0f);
				float zoomFactor = std::clamp(speed / 50.0f, 0.0f, 0.5f);
				XMVECTOR dynamicOffset = baseOffset + XMVectorSet(0, 4.0f, -4.0f, 0) * zoomFactor;

				XMVECTOR desiredPos = targetPos + dynamicOffset;
				XMVECTOR currentPos = XMLoadFloat3(&camTrans.position);
				XMVECTOR smoothedPos = XMVectorLerp(currentPos, desiredPos, dt * 3.0f);
				XMStoreFloat3(&camTrans.position, smoothedPos);

				// 2. 回転計算
				XMVECTOR lookTarget = targetPos + playerVel * 0.1f;
				XMMATRIX viewMat = XMMatrixLookAtLH(smoothedPos, lookTarget, XMVectorSet(0, 1, 0, 0));
				XMMATRIX camWorld = XMMatrixInverse(nullptr, viewMat);

				XMFLOAT4X4 mat; XMStoreFloat4x4(&mat, camWorld);
				float pitch = asinf(-mat._32);
				float yaw = 0.0f;
				float roll = 0.0f;

				if (cosf(pitch) > 0.001f) {
					yaw = atan2f(mat._31, mat._33);
					roll = atan2f(mat._12, mat._22);
				}
				else {
					yaw = atan2f(-mat._13, mat._11);
				}
				camTrans.rotation = { pitch, yaw, roll };
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::CameraFollowSystem, "CameraFollowSystem")