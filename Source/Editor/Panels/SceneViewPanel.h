/*****************************************************************//**
 * @file	SceneViewPanel.h
 * @brief	シーンビューの表示とマウスピック処理
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SCENE_VIEW_PANEL_H___
#define ___SCENE_VIEW_PANEL_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Editor/Tools/EditorCamera.h"
#include "Engine/Core/Context.h"

namespace Arche
{
	// 前方宣言
	class World;

	class SceneViewPanel
	{
	public:
		// コンストラクタ：カメラ初期化
		SceneViewPanel();
		~SceneViewPanel();

		void Draw(World& world, std::vector<Entity>& selection);
		void FocusEntity(Entity entity, World& world);

		EditorCamera& GetCamera() { return m_camera; }

	private:
		EditorCamera m_camera;
	};
}

#endif // _DEBUG

#endif // !___SCENE_VIEW_PANEL_H___