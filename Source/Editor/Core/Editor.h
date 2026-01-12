/*****************************************************************//**
 * @file	Editor.h
 * @brief	デバッグGUI全体の管理者
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___EDITOR_H___
#define ___EDITOR_H___

#ifdef _DEBUG

// ===== インクルード ====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Context.h"

// パネル群
#include "Editor/Panels/SceneViewPanel.h"
#include "Editor/Panels/GameViewPanel.h"

namespace Arche
{
	// 前方宣言
	class HierarchyWindow;

	// 各ウィンドウの親クラス
	class EditorWindow
	{
	public:
		bool m_isOpen = true;
		std::string m_windowName = "Window";

		virtual ~EditorWindow() = default;
		virtual void Draw(World& world, std::vector<Entity>& selection, Context& ctx) = 0;
	};

	// エディタの管理者
	class Editor
	{
	public:
		static Editor& Instance()
		{
			static Editor instance;
			return instance;
		}

		void Initialize();
		void Shutdown();
		void Draw(World& world, Context& ctx);

		// 選択管理用メソッド
		void SetSelectedEntity(Entity e)
		{
			m_selection.clear();
			if (e != NullEntity) m_selection.push_back(e);
			m_selectedEntity = e;
		}

		// 複数選択用の操作
		void AddToSelection(Entity e)
		{
			if (std::find(m_selection.begin(), m_selection.end(), e) == m_selection.end())
			{
				m_selection.push_back(e);
				m_selectedEntity = e;
			}
		}

		void RemoveFromSelection(Entity e)
		{
			auto it = std::remove(m_selection.begin(), m_selection.end(), e);
			m_selection.erase(it, m_selection.end());
			if (m_selectedEntity == e) m_selectedEntity = NullEntity;
		}

		void ClearSelection()
		{
			m_selection.clear();
			m_selectedEntity = NullEntity;
		}

		// ゲッター
		Entity& GetSelectedEntity() { return m_selectedEntity; }
		std::vector<Entity>& GetSelection() { return m_selection; }

		SceneViewPanel& GetSceneViewPanel() { return m_sceneViewPanel; }

		enum class EditorMode { Scene, Prefab };

		// 現在のモード取得
		EditorMode GetMode() const { return m_editorMode; }

		// プレファブモードで編集中のワールドを取得
		World* GetActiveWorld() { return (m_editorMode == EditorMode::Scene) ? &SceneManager::Instance().GetWorld() : m_prefabWorld.get(); }

		// プレファブを開く
		void OpenPrefab(const std::string& path);

		// プレファブを保存してシーンに戻る
		void SavePrefabAndExit();

		// 変更を破棄してシーンに戻る
		void ExitPrefabMode();

		// 外部から呼ばれる「シーンを開く」「終了する」関数
		void RequestOpenScene(const std::string& path);
		void RequestCloseEngine();

		// ポップアップ描画用
		void DrawSavePopup();

	private:
		Editor() = default;
		~Editor() = default;

		// シーン遷移・終了の保留管理
		enum class PendingAction { None, LoadScene, CloseEngine };
		PendingAction m_pendingAction = PendingAction::None;
		std::string m_pendingScenePath = "";
		bool m_showSavePopup = false;	// ポップアップ表示フラグ

		Entity m_selectedEntity = NullEntity;	// プライマリ（最後に選んだもの）
		std::vector<Entity> m_selection;		// 選択中の全エンティティ

		// ウィンドウ間利用リスト
		std::vector<std::unique_ptr<EditorWindow>> m_windows;

		HierarchyWindow* m_hierarchyPanel = nullptr;

		SceneViewPanel m_sceneViewPanel;
		GameViewPanel m_gameViewPanel;

		EditorMode m_editorMode = EditorMode::Scene;
		std::unique_ptr<World> m_prefabWorld;			// プレファブ編集用の一時ワールド
		std::string m_currentPrefabPath;				// 編集中のパス
		Entity m_prefabRoot = NullEntity;

		// キャッシュ用変数
		SceneEnvironment m_cachedEnvironment;	// 環境設定のバックアップ
		SceneEnvironment m_prefabEnvironment;	// プレファブモード用の背景設定
		bool m_needSkyboxRestore = false;
	};
}	// namespace Arche

#endif // _DEBUG

#endif // !___EDITOR_H___