/*****************************************************************//**
 * @file	Core.h
 * @brief	エンジンの基本定義
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___CORE_H___
#define ___CORE_H___

#pragma warning(disable: 4251)

// Windows環境のみ
#ifdef _WIN32
	// エンジン側でビルドしているときは「エクスポート」
	#ifdef ARCHE_BUILD_DLL
		#define ARCHE_API __declspec(dllexport)
	// ゲーム側から使う時は「インポート」
	#else
		#define ARCHE_API __declspec(dllimport)
	#endif // ARCHE_BUILD_DLL
#else
	#error Arche only supports Windows!
#endif // _WIN32

#endif // !___CORE_H___