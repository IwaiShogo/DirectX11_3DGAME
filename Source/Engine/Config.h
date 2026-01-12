/*****************************************************************//**
 * @file	Config.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 *********************************************************************/

#ifndef ___CONFIG_H___
#define ___CONFIG_H___

namespace Arche
{
	namespace Config
	{
		// 画面サイズ
		static const int SCREEN_WIDTH = 1280;
		static const int SCREEN_HEIGHT = 720;

		// フレームレート
		static const unsigned int FRAME_RATE = 60;	// 目標FPS（リフレッシュレートの分子）

		// タイトル
		static const char* WINDOW_TITLE = "Arche Engine (DirectX 11 + ECS)";

		// VSync
		static const bool VSYNC_ENABLED = false;		// 垂直同期（true: 60fps固定、false: 無制限）

	}	// namespace Config

}	// namespace Arche

#endif // !___CONFIG_H___