#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	class PlayerFocusSystem : public ISystem
	{
	public:
		PlayerFocusSystem() { m_systemName = "PlayerFocusSystem"; }

		void Update(Registry& reg) override
		{
			// プレイヤー検索
			Entity player = NullEntity;
			for (auto e : reg.view<PlayerController, Transform>()) { player = e; break; }
			if (player == NullEntity) return;

			auto& ctrl = reg.get<PlayerController>(player);
			auto& pTrans = reg.get<Transform>(player);
			XMVECTOR pPos = XMLoadFloat3(&pTrans.position);

			// 最も近い敵を探す
			Entity bestTarget = NullEntity;
			float minDistanceSq = 30.0f * 30.0f; // 射程距離の2乗

			for (auto e : reg.view<EnemyStats, Transform>())
			{
				// 画面内の敵のみ対象にするなどのロジックも可
				XMVECTOR ePos = XMLoadFloat3(&reg.get<Transform>(e).position);
				float distSq = XMVectorGetX(XMVector3LengthSq(ePos - pPos));

				if (distSq < minDistanceSq) {
					minDistanceSq = distSq;
					bestTarget = e;
				}
			}

			// ★コントローラーにターゲットをセット (これでActionSystemが参照できる)
			ctrl.focusTarget = bestTarget;
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerFocusSystem, "PlayerFocusSystem")