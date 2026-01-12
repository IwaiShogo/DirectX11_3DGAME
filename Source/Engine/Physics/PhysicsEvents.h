/*****************************************************************//**
 * @file	PhysicsEvents.h
 * @brief	衝突イベントデータ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/15	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/
#ifndef ___PHYSICS_EVENTS_H___
#define ___PHYSICS_EVENTS_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"

namespace Arche
{

	namespace Physics
	{
		enum class CollisionState
		{
			Enter,	// 当たった瞬間
			Stay,	// 当たり続けている
			Exit,	// 離れたとき
		};

		struct CollisionEvent
		{
			Entity self;
			Entity other;
			CollisionState state;
			DirectX::XMFLOAT3 normal;	// 衝突法線
		};

		// フレームごとの全イベントを保持するコンテナ
		// （これをRegistryのコンテキストやシングルトンとして扱う）
		struct EventQueue
		{
			std::vector<CollisionEvent> events;

			void Clear() { events.clear(); }

			void Add(Entity a, Entity b, CollisionState state, const DirectX::XMFLOAT3& n)
			{
				events.push_back({ a, b, state, n });
				// 逆方向のイベントも必要なら登録
				DirectX::XMFLOAT3 invNormal = { -n.x, -n.y, -n.z };
				events.push_back({ b, a, state, invNormal });
			}

			static inline EventQueue& Instance()
			{
				static EventQueue instance;
				return instance;
			}
		};

	}	// namespace Physics

}	// namespace Arche

#endif // !___PHYSICS_EVENTS_H___