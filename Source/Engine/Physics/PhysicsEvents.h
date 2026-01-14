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

		class ARCHE_API EventManager
		{
		public:
			// インスタンス
			static EventManager& Instance();

			// イベント通知
			void AddEvent(Entity self, Entity other, CollisionState state, const XMFLOAT3& normal)
			{
				m_events.push_back({ self, other, state, normal });
			}

			// イベント全クリア
			void Clear() { m_events.clear(); }

			// イベントリスト取得
			const std::vector<CollisionEvent>& GetEvents() const { return m_events; }

		private:
			std::vector<CollisionEvent> m_events;

			EventManager() = default;
			~EventManager() = default;
		};

	}	// namespace Physics

}	// namespace Arche

#endif // !___PHYSICS_EVENTS_H___