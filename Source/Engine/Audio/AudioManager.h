/*****************************************************************//**
 * @file	AudioManager.h
 * @brief	オーディオマネージャー
 * 
 * @details	
 * XAudio2エンジンの初期化と「MasteringVoice（スピーカー）」、
 * 「SubmixVoice（BGM/SEごとのミキサー）」を管理します。
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

#ifndef ___AUDIO_MANAGER_H___
#define ___AUDIO_MANAGER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Audio/Sound.h"
#include "Engine/Core/Base/StringId.h"

namespace Arche
{

	struct SoundEvent
	{
		StringId key;
		XMFLOAT3 position;
		float time;	// 残り表示時間
	};

	/**
	 * @class	AudioManager
	 * @brief	オーディオマネージャー
	 */
	class AudioManager
	{
	public:
		static AudioManager& Instance()
		{
			static AudioManager instance;
			return instance;
		}

		void Initialize();
		void Update(); // 毎フレーム呼ぶ（再生終了したボイスの掃除など）
		void Finalize();

		// --- 再生機能 ---

		// SE再生 (Fire and Forget: 鳴らしっぱなし)
		void PlaySE(std::string& key, float volume = 1.0f, float pitch = 0.0f);
		void Play3DSE(std::string& key, const XMFLOAT3& emitterPos, const XMFLOAT3& listenerPos, float range, float volume);

		// BGM再生 (ループ再生、BGMは同時に1つだけ)
		void PlayBGM(std::string& key, float volume = 1.0f, bool loop = true);
		void StopBGM(float fadeOutSeconds = 0.0f);

		// --- 全体設定 ---
		void SetMasterVolume(float volume);
		void SetSEVolume(float volume);
		void SetBGMVolume(float volume);

		const std::vector<SoundEvent>& GetSoundEvents() const { return m_soundEvents; }

		void OnInspector();

	private:
		AudioManager() = default;
		~AudioManager() = default;

		ComPtr<IXAudio2> m_xAudio2;
		IXAudio2MasteringVoice* m_masterVoice = nullptr;

		// サブミックス（カテゴリごとの音量調整用）
		IXAudio2SubmixVoice* m_seSubmix = nullptr;
		IXAudio2SubmixVoice* m_bgmSubmix = nullptr;

		// 現在再生中のBGM
		IXAudio2SourceVoice* m_currentBgmVoice = nullptr;

		// 再生中のSEリスト (終わったら解放するため保持)
		struct VoiceData {
			IXAudio2SourceVoice* voice;
			bool isLoop;
		};
		std::vector<VoiceData> m_seVoices;

		// 音声管理用変数
		float m_masterVolume = 1.0f;
		float m_seVolume = 1.0f;
		float m_bgmVolume = 1.0f;

		std::vector<SoundEvent> m_soundEvents;	// 再生履歴
	};

}	// namespace Arche

#endif // !___AUDIO_MANAGER_H___