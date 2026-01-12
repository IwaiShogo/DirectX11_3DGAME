/*****************************************************************//**
 * @file	SceneEnvironment.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SCENE_ENVIRONMENT_H___
#define ___SCENE_ENVIRONMENT_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	struct SceneEnvironment
	{
		// スカイボックス設定
		std::string skyboxTexturePath = "";	// 空ならプロシージャル

		XMFLOAT4 skyColorTop = { 0.25f, 0.45f, 0.8f, 1.0f };	// 青空
		XMFLOAT4 skyColorHorizon = { 0.7f, 0.75f, 0.8f, 1.0f };	// 地平線
		XMFLOAT4 skyColorBottom = { 0.3f, 0.3f, 0.35f, 1.0f };	// 地面

		// 環境光設定
		float ambientIntensity = 1.0f;
		XMFLOAT3 ambientColor = { 1.0f, 1.0f, 1.0f };	// 白
	};

}	// namespace Arche

#endif // !___SCENE_ENVIRONMENT_H___