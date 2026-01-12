/*****************************************************************//**
 * @file	Editor.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#ifdef _DEBUG
#include "Editor/Core/Editor.h"
#include "Editor/Core/CommandHistory.h"
#include "Editor/Tools/GizmoSystem.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"
#include "Engine/Scene/Serializer/ComponentSerializer.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"
#include "Engine/Scene/Core/SceneManager.h"

// パネル群
#include "Editor/Panels/HierarchyWindow.h"
#include "Editor/Panels/InspectorWindow.h"
#include "Editor/Panels/ContentBrowser.h"
#include "Editor/Panels/PhysicsSettingsWindow.h"
#include "Editor/Panels/ResourceInspectorWindow.h"
#include "Editor/Panels/SystemMonitorWindow.h"
#include "Editor/Panels/GameControlWindow.h"
#include "Editor/Panels/InputVisualizerWindow.h"
#include "Editor/Panels/BuildSettingsWindow.h"
#include "Editor/Panels/AnimatorGraphWindow.h"
#include "Editor/Panels/LightingSettingsWindow.h"

namespace Arche
{
	void Editor::Initialize()
	{
		// 1. パネルの生成
		m_windows.clear();

		auto hierarchy = std::make_unique<HierarchyWindow>();
		m_hierarchyPanel = hierarchy.get();
		m_windows.push_back(std::move(hierarchy));
		m_windows.push_back(std::make_unique<InspectorWindow>());
		m_windows.push_back(std::make_unique<ContentBrowser>());
		m_windows.push_back(std::make_unique<PhysicsSettingsWindow>());
		m_windows.push_back(std::make_unique<ResourceInspectorWindow>());
		m_windows.push_back(std::make_unique<SystemMonitorWindow>());
		m_windows.push_back(std::make_unique<GameControlWindow>());
		m_windows.push_back(std::make_unique<InputVisualizerWindow>());
		m_windows.push_back(std::make_unique<BuildSettingsWindow>());
		m_windows.push_back(std::make_unique<AnimatorGraphWindow>());
		m_windows.push_back(std::make_unique<LightingSettingsWindow>());

		// 2. ImGuiの設定
		ImGuiIO& io = ImGui::GetIO();

		// マルチビューポート
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// キーボード操作有効
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// ウィンドウドッキング
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// 別ウィンドウ化を有効

		// 3. スタイル設定
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// 4. フォント読み込み
		static const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesJapanese();
		ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr, glyphRanges);
		if (font == nullptr)
		{
			io.Fonts->AddFontDefault();
		}
	}

	void Editor::Shutdown()
	{
		if (m_editorMode == EditorMode::Prefab) ExitPrefabMode();
		m_windows.clear();
		m_prefabWorld.reset();
		Logger::Log("Editor Shutdown Completed.");
	}

	// ------------------------------------------------------------
	// 共通: Playモード停止処理 (ボタン・ショートカット共通化用)
	// ------------------------------------------------------------
	void StopPlayMode(World& world, Context& ctx, const SceneEnvironment& cachedEnv)
	{
		ctx.editorState = EditorState::Edit;

		// 1. ワールドクリア
		world.clearSystems();
		world.clearEntities();

		// 2. シーン復元 (キャッシュから)
		SceneSerializer::LoadScene(world, "Resources/Engine/Cache/SceneCache.json");

		// 3. 環境設定の完全復元 (重要)
		// システム生成「前」に正しいパスに戻しておくことで、RenderSystemの初期化時にロードさせる
		SceneManager::Instance().GetContext().environment = cachedEnv;
		SceneManager::Instance().GetContext().environment.skyboxTexturePath = "";

		// 4. システムの復旧
		if (world.getSystems().empty())
		{
			auto& reg = SystemRegistry::Instance();
			reg.CreateSystem(world, "Physics System", SystemGroup::PlayOnly);
			reg.CreateSystem(world, "Collision System", SystemGroup::PlayOnly);
			reg.CreateSystem(world, "UI System", SystemGroup::Always);
			reg.CreateSystem(world, "Lifetime System", SystemGroup::PlayOnly);
			reg.CreateSystem(world, "Hierarchy System", SystemGroup::Always);
			reg.CreateSystem(world, "Animation System", SystemGroup::PlayOnly);
			reg.CreateSystem(world, "Render System", SystemGroup::Always);
			reg.CreateSystem(world, "Model Render System", SystemGroup::Always);
			reg.CreateSystem(world, "Sprite Render System", SystemGroup::Always);
			reg.CreateSystem(world, "Billboard System", SystemGroup::Always);
			reg.CreateSystem(world, "Text Render System", SystemGroup::Always);
			reg.CreateSystem(world, "Audio System", SystemGroup::Always);
			reg.CreateSystem(world, "Button System", SystemGroup::Always);
		}
	}

	void Editor::Draw(World& world, Context& ctx)
	{
		if (m_needSkyboxRestore)
		{
			SceneManager::Instance().GetContext().environment.skyboxTexturePath = m_cachedEnvironment.skyboxTexturePath;
			m_needSkyboxRestore = false;
		}

		bool ctrl = Input::GetKey(VK_CONTROL);

		// Undo / Redo
		if (ctrl && Input::GetKeyDown('Z')) CommandHistory::Undo();
		if (ctrl && Input::GetKeyDown('Y')) CommandHistory::Redo();

		// シーン保存 (Editモードのみ)
		if (ctrl && Input::GetKeyDown('S'))
		{
			if (ctx.editorState == EditorState::Edit)
			{
				std::string path = SceneManager::Instance().GetCurrentScenePath();
				if (path.empty()) path = "Resources/Game/Scenes/Untitled.json";

				SceneSerializer::SaveScene(world, path);
				SceneManager::Instance().SetDirty(false);
				EditorPrefs::Instance().lastScenePath = path;
				EditorPrefs::Instance().Save();
			}
		}

		// Play / Edit 切り替え (ショートカット: Ctrl + P)
		if (ctrl && Input::GetKeyDown('P'))
		{
			if (ctx.editorState == EditorState::Edit)
			{
				// Play開始
				SceneSerializer::SaveScene(world, "Resources/Engine/Cache/SceneCache.json");
				m_cachedEnvironment = SceneManager::Instance().GetContext().environment;
				ctx.editorState = EditorState::Play;
			}
			else
			{
				// Play停止 (共通処理呼び出し)
				StopPlayMode(world, ctx, m_cachedEnvironment);
				m_needSkyboxRestore = true;
			}
		}

		// --- ImGui ウィンドウ設定 ---
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::Begin("MainDockSpace", nullptr, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

		// ツールバー
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save Scene", "Ctrl + S"))
				{
					std::string path = SceneManager::Instance().GetCurrentScenePath();
					if (path.empty()) path = "Resources/Game/Scenes/Untitled.json";
					SceneSerializer::SaveScene(world, path);
					SceneManager::Instance().SetDirty(false);
					EditorPrefs::Instance().lastScenePath = path;
					EditorPrefs::Instance().Save();
				}
				if (ImGui::MenuItem("Exit")) RequestCloseEngine();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window"))
			{
				for (auto& window : m_windows)
				{
					if (ImGui::MenuItem(window->m_windowName.c_str(), nullptr, window->m_isOpen)) window->m_isOpen = !window->m_isOpen;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Reset Layout")) std::filesystem::remove("imgui.ini");
				ImGui::EndMenu();
			}

			// Play / Stop ボタン
			if (ctx.editorState == EditorState::Edit)
			{
				if (ImGui::Button("Play"))
				{
					SceneSerializer::SaveScene(world, "Resources/Engine/Cache/SceneCache.json");
					m_cachedEnvironment = SceneManager::Instance().GetContext().environment;
					ctx.editorState = EditorState::Play;
				}
			}
			else
			{
				if (ImGui::Button("Stop"))
				{
					// Play停止 (共通処理呼び出し)
					StopPlayMode(world, ctx, m_cachedEnvironment);
					m_needSkyboxRestore = true;
				}
			}

			// シーン名表示
			std::string currentScene = SceneManager::Instance().GetCurrentScenePath();
			if (currentScene.empty()) currentScene = "Untitled";
			else currentScene = std::filesystem::path(currentScene).filename().string();
			if (SceneManager::Instance().IsDirty()) currentScene += "*";

			float textWidth = ImGui::CalcTextSize(currentScene.c_str()).x;
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", currentScene.c_str());

			ImGui::EndMainMenuBar();
		}

		DrawSavePopup();
		ImGuizmo::BeginFrame();

		World* activeWorld = (m_editorMode == EditorMode::Prefab && m_prefabWorld) ? m_prefabWorld.get() : &world;

		for (auto& window : m_windows) window->Draw(*activeWorld, m_selection, ctx);
		m_sceneViewPanel.Draw(*activeWorld, m_selection);
		m_gameViewPanel.Draw();

		AudioManager::Instance().OnInspector();
		Logger::Draw("Debug Logger");

		ImGui::End();
	}

	void Editor::OpenPrefab(const std::string& path)
	{
		// 0. 環境設定のバックアップ
		m_cachedEnvironment = SceneManager::Instance().GetContext().environment;

		// プレファブ編集用背景設定
		auto& env = SceneManager::Instance().GetContext().environment;
		env.skyboxTexturePath = "";

		// 単色青 (グラデーションさせないため全て同じ色にする)
		XMFLOAT4 prefabBgColor = { 0.1f, 0.2f, 0.35f, 1.0f };
		env.skyColorTop = prefabBgColor;
		env.skyColorHorizon = prefabBgColor;
		env.skyColorBottom = prefabBgColor;
		// 環境光は少し明るめに
		env.ambientColor = { 0.5f, 0.6f, 0.7f };

		// 1. プレファブモードへ移行
		m_editorMode = EditorMode::Prefab;
		m_currentPrefabPath = path;
		CommandHistory::Clear();

		// 2. 編集用の一時ワールドを作成
		m_prefabWorld = std::make_unique<World>();

		auto& sysReg = SystemRegistry::Instance();
		sysReg.CreateSystem(*m_prefabWorld, "Render System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "Model Render System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "Sprite Render System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "Billboard System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "Text Render System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "UI System", SystemGroup::Always);
		sysReg.CreateSystem(*m_prefabWorld, "Hierarchy System", SystemGroup::Always);

		// 3. プレファブロード
		Entity root = SceneSerializer::LoadPrefab(*m_prefabWorld, path);
		m_prefabRoot = root;

		if (m_prefabRoot != NullEntity && m_prefabWorld->getRegistry().has<PrefabInstance>(m_prefabRoot))
		{
			m_prefabWorld->getRegistry().remove<PrefabInstance>(m_prefabRoot);
		}

		if (root != NullEntity) m_sceneViewPanel.FocusEntity(root, *m_prefabWorld);
		SetSelectedEntity(root);

		Logger::Log("Opened Prefab Mode: " + path);
	}

	void Editor::SavePrefabAndExit()
	{
		if (m_editorMode != EditorMode::Prefab || !m_prefabWorld) return;

		Entity root = m_prefabRoot;
		if (root != NullEntity && m_prefabWorld->getRegistry().valid(root))
		{
			SceneSerializer::SavePrefab(m_prefabWorld->getRegistry(), root, m_currentPrefabPath);
			Logger::Log("Prefab Saved: " + m_currentPrefabPath);
			SceneSerializer::ReloadPrefabInstances(SceneManager::Instance().GetWorld(), m_currentPrefabPath);
		}
		else
		{
			Logger::LogError("Failed to save prefab: Root entity not found.");
		}
		ExitPrefabMode();
	}

	void Editor::ExitPrefabMode()
	{
		CommandHistory::Clear();
		// 環境設定の復元
		SceneManager::Instance().GetContext().environment = m_cachedEnvironment;

		m_editorMode = EditorMode::Scene;
		m_prefabWorld.reset();
		m_currentPrefabPath = "";
		m_prefabRoot = NullEntity;
		SetSelectedEntity(NullEntity);
	}

	void Editor::RequestOpenScene(const std::string& path)
	{
		if (SceneManager::Instance().IsDirty())
		{
			m_pendingAction = PendingAction::LoadScene;
			m_pendingScenePath = path;
			m_showSavePopup = true;
		}
		else
		{
			SceneManager::Instance().LoadSceneAsync(path, new ImmediateTransition());
		}
	}

	void Editor::RequestCloseEngine()
	{
		if (SceneManager::Instance().IsDirty())
		{
			m_pendingAction = PendingAction::CloseEngine;
			m_showSavePopup = true;
		}
		else
		{
			PostQuitMessage(0);
		}
	}

	void Editor::DrawSavePopup()
	{
		if (m_showSavePopup)
		{
			ImGui::OpenPopup("Save Changes?");
			m_showSavePopup = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Save Changes?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Current scene has unsaved changes.\nDo you want to save before continuing?");
			ImGui::Separator();

			if (ImGui::Button("Save", ImVec2(120, 0)))
			{
				std::string currentPath = SceneManager::Instance().GetCurrentScenePath();
				if (currentPath.empty()) currentPath = "Resources/Game/Scenes/Untitled.json";

				SceneSerializer::SaveScene(SceneManager::Instance().GetWorld(), currentPath);
				SceneManager::Instance().SetDirty(false);
				Logger::Log("Saved: " + currentPath);

				if (m_pendingAction == PendingAction::LoadScene) SceneManager::Instance().LoadSceneAsync(m_pendingScenePath);
				else if (m_pendingAction == PendingAction::CloseEngine) PostQuitMessage(0);

				m_pendingAction = PendingAction::None;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();

			if (ImGui::Button("Don't Save", ImVec2(120, 0)))
			{
				if (m_pendingAction == PendingAction::LoadScene) SceneManager::Instance().LoadSceneAsync(m_pendingScenePath);
				else if (m_pendingAction == PendingAction::CloseEngine) PostQuitMessage(0);

				m_pendingAction = PendingAction::None;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_pendingAction = PendingAction::None;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

}	// namespace Arche

#endif // _DEBUG