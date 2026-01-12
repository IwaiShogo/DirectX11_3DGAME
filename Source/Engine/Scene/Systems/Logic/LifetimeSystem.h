/*****************************************************************//**
 * @file	LifetimeSystem.h
 * @brief	時間が来たらEntityを削除するシステム
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/26	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___LIFETIME_SYSTEM_H___
#define ___LIFETIME_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{

	class LifetimeSystem
		: public ISystem
	{
	public:
		LifetimeSystem()
		{
			m_systemName = "Lifetime System";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 削除リスト
			std::vector<Entity> toDestroy;

			registry.view<Lifetime>().each([&](Entity e, Lifetime& life)
				{
					life.time -= dt;
					if (life.time <= 0.0f)
					{
						toDestroy.push_back(e);
					}
				});

			// まとめて削除
			for (Entity e : toDestroy)
			{
				registry.destroy(e);
			}
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::LifetimeSystem, "Lifetime System")

#endif // !___LIFETIME_SYSTEM_H___