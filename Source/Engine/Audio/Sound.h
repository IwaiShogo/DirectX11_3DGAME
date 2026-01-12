/*****************************************************************//**
 * @file	Sound.h
 * @brief	音声データそのものを表すクラス
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

#ifndef ___SOUND_H___
#define ___SOUND_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{

	/**
	 * @class	Sound
	 * @brief	音声データのコンテナ
	 */
	class Sound
	{
	public:
		WAVEFORMATEX wfx = { 0 };		// フォーマット情報
		std::vector<BYTE> buffer;		// 波形データの実体
		XAUDIO2_BUFFER xBuffer = { 0 };	// XAudio2用のバッファ構造体

		std::string filepath;
		float duration = 0.0f;			// 再生時間（秒）
		// 1. CPUロード
		bool LoadCPU(const std::string& path);

		// 2. 初期化
		void Initialize();
	};

}	// namespace Arche

#endif // !___SOUND_H___