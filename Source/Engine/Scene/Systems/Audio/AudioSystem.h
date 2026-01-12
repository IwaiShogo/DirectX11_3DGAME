/*****************************************************************//**
 * @file	AudioSystem.h
 * @brief	音声再生
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

#ifndef ___AUDIO_SYSTEM_H___
#define ___AUDIO_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Resource/ResourceManager.h"

namespace Arche
{

	class AudioSystem
		: public ISystem
	{
	public:
		AudioSystem()
		{
			m_systemName = "Audio System";
		}

		void Update(Registry& registry) override
		{
			// 1. リスナーを探す
			XMFLOAT3 listenerPos = { 0, 0, 0 };
			bool listenerFound = false;

			registry.view<AudioListener, Transform>().each([&](Entity e, AudioListener& l, Transform& t)
				{
					if (!listenerFound)
					{
						// 最初の1人だけ採用
						listenerPos = t.position;
						listenerFound = true;
					}
				});

			// 聞く人がいない場合
			if (!listenerFound) return;

			// 2. 音源の更新
			registry.view<AudioSource, Transform>().each([&](Entity e, AudioSource& source, Transform& t)
				{
					// --- 再生制御ロジック ---
					if (source.playOnAwake && !source.isPlaying)
					{
						AudioManager::Instance().Play3DSE(source.soundKey, t.position, listenerPos, source.range, source.volume);
						source.isPlaying = true;	// 再生済みフラグとして使う
					}
				});
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::AudioSystem, "Audio System")

#endif // !___AUDIO_SYSTEM_H___