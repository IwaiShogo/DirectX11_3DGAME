/*****************************************************************//**
 * @file	Prefab.h
 * @brief	エンティティを作成する関数群
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

#ifndef ___PREFAB_H___
#define ___PREFAB_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	namespace Prefab
	{
		/**
		 * @brief	指定した位置に、ワンショット音源を再生する
		 */
		inline void CreateSoundEffect(World& world, const std::string& soundKey, const XMFLOAT3& position, float volume = 1.0f, float range = 20.0f)
		{
			// 音声データを取得して長さを調べる
			float duration = 1.0f;	// デフォルト
			auto sound = ResourceManager::Instance().GetSound(soundKey);
			if (sound)
			{
				duration = sound->duration;
				// 少し余裕を持たせる（0.1秒）
				duration += 0.1f;
			}

			world.create_entity()
				.add<Tag>("SE_OneShot")
				.add<Transform>(position)
				.add<AudioSource>(soundKey, volume, range, false, true)
				.add<Lifetime>(duration);
		}

	}	// namespace Prefabs

}	// namespace Arche

#endif // !___PREFAB_H___