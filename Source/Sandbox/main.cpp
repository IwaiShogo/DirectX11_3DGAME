/*****************************************************************//**
 * @file	main.cpp
 * @brief	サンドボックス（ゲーム）のエントリーポイント
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#undef ARCHE_BUILD_DLL

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Base/Logger.h"
#include "Engine/Core/Core.h"

#include "Sandbox/GameLoader.h"

namespace Arche
{
	// エンジン側にアプリケーションの実体を渡す関数
	Application* CreateApplication()
	{
		return new Application("Arche Engine", Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT);
	}
}

extern "C" __declspec(dllexport) Arche::Application* CreateApplication()
{
	return Arche::CreateApplication();
}