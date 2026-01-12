/*****************************************************************//**
 * @file	Time.h
 * @brief	FPS制御
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/24	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___TIME_H___
#define ___TIME_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{

	class ARCHE_API Time
	{
	public:
		// 初期化
		static void Initialize();

		// 更新
		static void Update();

		// コマ送り用
		static void StepFrame();

		// 前のフレームからの経過時間（秒）
		static float DeltaTime();

		// ゲーム開始からの総経過時間（秒）
		static float TotalTime();

		// 目標フレームレートを設定
		static void SetFrameRate(int fps);

		// 待機
		static void WaitFrame();

		// 公開変数（これらも実体はcppに置く）
		static float timeScale;
		static bool isPaused;

	private:
		static LARGE_INTEGER s_cpuFreq;
		static LARGE_INTEGER s_lastTime;
		static LARGE_INTEGER s_startTime;
		static double s_deltaTime;
		static bool s_isStepNext;
		static double s_targetFrameTime;
	};

}	// namespace Arche

#endif // !___TIME_H___