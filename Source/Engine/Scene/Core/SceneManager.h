/*****************************************************************//**
 * @file	SceneManager.h
 * @brief	現在のシーン（World）を保持し、管理を行うクラス
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SCENE_MANAGER_H___
#define ___SCENE_MANAGER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneTransition.h"

namespace Arche
{
	struct AsyncOperation
	{
		bool isDone = false;
		float progress = 0.0f;
	};

	/**
	 * @class	SceneManager
	 * @brief	Worldの実体を持ち、更新と描画を行う
	 */
	class ARCHE_API SceneManager
	{
	public:
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		SceneManager();
		~SceneManager();

		static SceneManager& Instance();

		void Initialize();
		void Update();
		void Render();
		void Render(World* targetWorld);

		// --- シーンロード API ---

		// 1. 同期ロード（基本）
		void LoadScene(const std::string& filepath, ISceneTransition* transition);

		// 2. 非同期ロード（推奨）
		std::shared_ptr<AsyncOperation> LoadSceneAsync(const std::string& filepath, ISceneTransition* transition = nullptr);

		// シーン管理用追加
		const std::string& GetCurrentScenePath() const { return m_currentScenePath; }

		// 変更があるかどうか
		bool IsDirty() const { return m_isDirty; }
		void SetDirty(bool dirty) { m_isDirty = dirty; }

		// ------------------------------------------------------------

		void SetContext(const Context& context) { m_context = context; }
		Context& GetContext() { return m_context; }
		World& GetWorld() { return m_world; }

	private:
		// 内部処理
		void PerformLoad(const std::string& path);

	private:
		static SceneManager* s_instance;
		World m_world;
		Context m_context;
		std::string m_currentScenePath = "";

		// 未保存の変更があるか
		bool m_isDirty = false;

		// 遷移管理
		std::unique_ptr<ISceneTransition> m_transition = nullptr;
		std::string m_nextScenePath = "";

		// 非同期ロード管理
		bool m_isAsyncLoading = false;
		std::shared_ptr<AsyncOperation> m_currentAsyncOp;
	};

}	// namespace Arche

#endif // !___SCENE_MANAGER_H___