#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

#include "Sandbox/Components/Player/Bullet.h"

namespace Arche
{
	class BulletSystem : public ISystem
	{
	public:
		BulletSystem()
		{
			m_systemName = "BulletSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			auto view = registry.view<Bullet, Transform, Rigidbody>();

			for (auto entity : view)
			{
				auto& b = view.get<Bullet>(entity);
				auto& t = view.get<Transform>(entity);
				auto& rb = view.get<Rigidbody>(entity);

				// 移動（前進）
				XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(t.rotation.x),
					XMConvertToRadians(t.rotation.y),
					0.0f
				);

				XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotMat);

				// Rigidbodyの速度を更新
				XMVECTOR velocity = forward * b.speed;
				XMStoreFloat3(&rb.velocity, velocity);
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::BulletSystem, "BulletSystem")
