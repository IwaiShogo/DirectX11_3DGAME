/*****************************************************************//**
 * @file	ContentBrowser.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___CONTENT_BROWSER_H___
#define ___CONTENT_BROWSER_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"
#include "Editor/Core/EditorPrefs.h"
#include "Editor/Tools/ThumbnailCache.h"
#undef MoveFile

namespace Arche
{
	class ContentBrowser
		: public EditorWindow
	{
	public:
		ContentBrowser()
			: m_rootDirectory(std::filesystem::current_path())
			, m_currentDirectory(std::filesystem::current_path())
		{
			m_windowName = "Content Browser";

			// 保存されたパスを復元
			std::string savedPath = EditorPrefs::Instance().contentBrowserPath;
			if (std::filesystem::exists(savedPath))
				m_currentDirectory = savedPath;
			else
				m_currentDirectory = "Resources";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			// --- 全体レイアウト（テーブル） ---
			// Resizable:境界線をドラッグ可能, BordersInnerV:境界線を表示
			if (ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
			{
				// 左側: ツリー（固定幅 250px）
				ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthFixed, 250.0f);
				// 右側: グリッド（残りすべて）
				ImGui::TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch);

				// --------------------------------------------------------
				// 1. 左カラム: フォルダツリー
				// --------------------------------------------------------
				ImGui::TableNextColumn();

				ImGui::BeginChild("TreeRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

				DrawDirectoryTree(m_rootDirectory);

				ImGui::EndChild();

				// --------------------------------------------------------
				// 2. 右カラム: コンテンツ一覧
				// --------------------------------------------------------
				ImGui::TableNextColumn();
				
				ImGui::BeginChild("GridRegion", ImVec2(0, 0), false);

				DrawContentGrid(world);

				ImGui::EndChild();

				ImGui::EndTable();
			}

			HandlePopups(world);
			EditorPrefs::Instance().contentBrowserPath = m_currentDirectory.string();

			ImGui::End();
		}

	private:
		enum class CreateMode { Component, System };
		CreateMode m_createMode = CreateMode::Component;

		// パス
		std::filesystem::path m_rootDirectory;
		std::filesystem::path m_currentDirectory;

		// スクリプト作成用
		bool m_showCreatePopup = false;

		// リネーム用
		bool m_isRenaming = false;
		std::filesystem::path m_renamingPath;	// リネーム中のファイルパス
		char m_renameBuf[128] = "";

		// 削除用
		bool m_showDeleteModal = false;
		std::filesystem::path m_deletePath;

		// サムネイル管理
		ThumbnailCache m_thumbnailCache;

		// エンティティをプレファブとして保存するヘルパー
		void CreatePrefabFromEntity(World& world, Entity entity)
		{
			auto& reg = world.getRegistry();
			if (!reg.valid(entity)) return;

			// ファイル名決定 (Entity名.json)
			std::string name = "NewPrefab";
			if (reg.has<Tag>(entity)) name = reg.get<Tag>(entity).name.c_str();
			if (name.empty()) name = "Entity";

			std::filesystem::path filepath = m_currentDirectory / (name + ".json");

			// 重複チェック (同名ファイルがある場合は連番をつけるなどの処理が理想ですが、今回はログを出してリターン)
			if (std::filesystem::exists(filepath))
			{
				Logger::LogError("File already exists: " + filepath.string());
				return;
			}

			// 保存実行
			std::string pathStr = filepath.string();
			std::replace(pathStr.begin(), pathStr.end(), '\\', '/'); // パス統一

			SceneSerializer::SavePrefab(reg, entity, pathStr);
			Logger::Log("Created Prefab: " + pathStr);

			// エンティティをプレファブインスタンス化 (リンク情報を付与)
			if (!reg.has<PrefabInstance>(entity))
			{
				reg.emplace<PrefabInstance>(entity, pathStr);
			}
			else
			{
				reg.get<PrefabInstance>(entity).prefabPath = pathStr;
			}
		}

		// ====================================================================================
		// 左側: フォルダツリー描画 (再帰)
		// ====================================================================================
		void DrawDirectoryTree(const std::filesystem::path& path)
		{
			// 表示除外チェック
			if (IsIgnored(path.filename().string())) return;

			// フラグ設定
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

			// 現在選択中のディレクトリならハイライト
			if (path == m_currentDirectory)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			// サブフォルダがあるかチェック（リーフノード判定）
			bool hasSubFolders = false;
			try {
				for (const auto& entry : std::filesystem::directory_iterator(path)) {
					if (entry.is_directory() && !IsIgnored(entry.path().filename().string())) {
						hasSubFolders = true;
						break;
					}
				}
			}
			catch (...) {}

			if (!hasSubFolders)
			{
				flags |= ImGuiTreeNodeFlags_Leaf;
			}

			// ルートフォルダは最初から開いておく
			if (path == m_rootDirectory)
			{
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			}

			// ノード描画
			// ファイル名を表示（ルートの場合は "Project" と表示しても良い）
			std::string label = path.filename().string();
			bool opened = ImGui::TreeNodeEx(path.string().c_str(), flags, "%s", label.c_str());

			// クリックで選択ディレクトリ変更
			if (ImGui::IsItemClicked())
			{
				m_currentDirectory = path;
			}

			// フォルダへのD&D（移動）対応
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					std::filesystem::path srcItemPath = (const char*)payload->Data;
					if (std::filesystem::absolute(srcItemPath) != std::filesystem::absolute(path))
					{
						MoveFile(srcItemPath, path / srcItemPath.filename());
					}
				}
				ImGui::EndDragDropTarget();
			}

			// 再帰的に子フォルダを描画
			if (opened)
			{
				if (hasSubFolders)
				{
					for (const auto& entry : std::filesystem::directory_iterator(path))
					{
						if (entry.is_directory())
						{
							DrawDirectoryTree(entry.path());
						}
					}
				}
				ImGui::TreePop();
			}
		}

		// ====================================================================================
		// 右側: グリッド表示 (元のDrawの中身を移動)
		// ====================================================================================
		void DrawContentGrid(World& world)
		{
			// ヘッダー（戻るボタンなど）
			if (m_currentDirectory != m_rootDirectory)
			{
				if (ImGui::Button("<-")) m_currentDirectory = m_currentDirectory.parent_path();
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						std::filesystem::path src = (const char*)payload->Data;
						MoveFile(src, m_currentDirectory.parent_path() / src.filename());
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::SameLine();
			}
			ImGui::Text("Current: %s", m_currentDirectory.filename().string().c_str());
			ImGui::Separator();

			// --- グリッド設定 ---
			float padding = 16.0f;
			float thumbnailSize = 64.0f;
			float cellSize = thumbnailSize + padding;

			// 現在のウィンドウ幅から、1行に表示できるカラム数を計算
			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1) columnCount = 1;

			int index = 0; // ループ回数カウンタ

			// ディレクトリ内を走査
			for (auto& directoryEntry : std::filesystem::directory_iterator(m_currentDirectory))
			{
				const auto& path = directoryEntry.path();
				std::string filename = path.filename().string();
				if (IsIgnored(filename)) continue;

				ImGui::PushID(filename.c_str());

				// --- レイアウト制御 ---
				// カラムの先頭でなければ横に並べる
				if (index % columnCount != 0)
				{
					ImGui::SameLine();
				}

				// アイコンとテキストをひとかたまりのグループにする
				ImGui::BeginGroup();

				// 1. アイコン取得
				ID3D11ShaderResourceView* icon = m_thumbnailCache.GetIcon(path);

				// 背景透明でボタンを描画
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton(filename.c_str(), (ImTextureID)icon, { thumbnailSize, thumbnailSize }, { 0,0 }, { 1,1 }, ImVec4(0, 0, 0, 0));
				ImGui::PopStyleColor();

				// 2. ファイル名表示
				// サムネイルの幅に合わせてテキストを折り返す
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);

				if (m_isRenaming && m_renamingPath == path)
				{
					ImGui::SetKeyboardFocusHere();
					ImGui::PushItemWidth(thumbnailSize);
					if (ImGui::InputText("##Rename", m_renameBuf, sizeof(m_renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
						RenameFile(path, m_currentDirectory / m_renameBuf);
						m_isRenaming = false;
					}
					if (!ImGui::IsItemActive() && (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
						m_isRenaming = false;
					}
					ImGui::PopItemWidth();
				}
				else
				{
					// テキストが長くてもアイコンの中央に来るように表示（簡易的）
					ImGui::TextWrapped("%s", filename.c_str());
				}
				ImGui::PopTextWrapPos();

				ImGui::EndGroup(); // グループ終了

				// --- インタラクション (グループ全体に対して判定) ---

				// 右クリックメニュー
				// グループ（アイコン+テキスト）の上で右クリックしたら開く
				if (ImGui::BeginPopupContextItem("ItemContextMenu"))
				{
					if (ImGui::MenuItem("Rename", "F2")) StartRenaming(path);
					if (ImGui::MenuItem("Delete", "Del")) { m_deletePath = path; m_showDeleteModal = true; }
					ImGui::EndPopup();
				}

				// ドラッグ&ドロップ
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					std::string itemPath = std::filesystem::relative(path, std::filesystem::current_path()).string();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);

					// ドラッグ中のプレビュー
					ImGui::Image((ImTextureID)icon, { 32, 32 });
					ImGui::SameLine();
					ImGui::Text("%s", filename.c_str());

					ImGui::EndDragDropSource();
				}

				// フォルダへのドロップ
				if (directoryEntry.is_directory())
				{
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
						{
							// ペイロードから元のパスを取得
							std::filesystem::path srcPath = (const char*)payload->Data;
							// 移動先 = ドロップしたフォルダパス / 元のファイル名
							std::filesystem::path dstPath = path / srcPath.filename();

							MoveFile(srcPath, dstPath);
						}
						ImGui::EndDragDropTarget();
					}
				}

				// ダブルクリック (グループ全体で判定)
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
					{
						m_currentDirectory /= path.filename();
					}
					else
					{
						if (path.extension() == ".json")
						{
							// プレファブかシーンか判定
							if (path.string().find("Scene") == std::string::npos)
							{
								Editor::Instance().OpenPrefab(path.string());
							}
							else
							{
								Editor::Instance().RequestOpenScene(path.string());
							}
						}
						else
						{
							OpenFile(world, path);
						}
					}
				}

				ImGui::PopID();

				index++;
			}

			ImGui::Dummy(ImGui::GetContentRegionAvail());

			// 余白部分へのドロップ
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID"))
				{
					Entity droppedEntity = *(const Entity*)payload->Data;
					CreatePrefabFromEntity(world, droppedEntity);
				}
				ImGui::EndDragDropTarget();
			}

			// 背景右クリック（作成メニュー）
			if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Folder")) std::filesystem::create_directory(m_currentDirectory / "NewFolder");
				ImGui::Separator();
				if (ImGui::MenuItem("Create Scene")) SceneSerializer::CreateEmptyScene((m_currentDirectory / "NewScene.json").string());
				if (ImGui::MenuItem("Create Component")) { m_createMode = CreateMode::Component; m_showCreatePopup = true; }
				if (ImGui::MenuItem("Create System")) { m_createMode = CreateMode::System; m_showCreatePopup = true; }
				if (ImGui::MenuItem("Open in Explorer")) ShellExecuteA(NULL, "open", m_currentDirectory.string().c_str(), NULL, NULL, SW_SHOW);
				ImGui::EndPopup();
			}
		}

		// ====================================================================================
		// ポップアップ制御 (バグ修正済み)
		// ====================================================================================
		void HandlePopups(World& world)
		{
			// スクリプト作成ポップアップ
			if (m_showCreatePopup)
			{
				ImGui::OpenPopup("Create Script");
				m_showCreatePopup = false;
			}

			if (ImGui::BeginPopupModal("Create Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				static char buf[64] = "NewScript";
				ImGui::InputText("Class Name", buf, sizeof(buf));

				if (ImGui::Button("Create", ImVec2(120, 0)))
				{
					if (m_createMode == CreateMode::Component)
					{
						CreateComponentFile(buf);
					}
					else if (m_createMode == CreateMode::System)
					{
						CreateSystemFile(buf);
					}
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			// 削除確認ポップアップ
			if (m_showDeleteModal) ImGui::OpenPopup("Delete?");
			if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Are you sure you want to delete this?\nThis action cannot be undone.");
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", m_deletePath.filename().string().c_str());
				ImGui::Separator();

				if (ImGui::Button("Delete", ImVec2(120, 0)))
				{
					try {
						// 1. ファイルシステムから削除
						if (std::filesystem::exists(m_deletePath))
						{
							std::filesystem::remove_all(m_deletePath);

							// 2. シーン内の該当プレファブを自動Unpack
							std::filesystem::path deletedAbs = std::filesystem::absolute(m_deletePath);

							Registry& reg = world.getRegistry();
							std::vector<Entity> toUnpack;

							// PrefabInstanceを持つ全エンティティを検索
							auto view = reg.view<PrefabInstance>();
							for (auto entity : view)
							{
								auto& pref = view.get<PrefabInstance>(entity);
								std::filesystem::path prefPath = std::filesystem::absolute(pref.prefabPath);

								// パスが一致したらリストに追加
								// (remove_allした後なのでexistsチェックではなくパスの一致を見る)
								if (prefPath == deletedAbs)
								{
									toUnpack.push_back(entity);
								}
							}

							// Unpack実行
							for (auto e : toUnpack)
							{
								reg.remove<PrefabInstance>(e);
							}
							if (!toUnpack.empty())
							{
								Logger::LogWarning("Auto-unpacked " + std::to_string(toUnpack.size()) + " entities due to file deletion.");
							}
						}
					}
					catch (const std::exception& e) {
						Logger::LogError("Failed to delete: " + std::string(e.what()));
					}
					m_showDeleteModal = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					m_showDeleteModal = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		// --------------------------------------------------------
		// ユーティリティ関数
		// --------------------------------------------------------
		// 表示除外フィルタ
		bool IsIgnored(const std::string& name)
		{
			static const std::set<std::string> ignored =
			{
				".git", ".vs", ".editorconfig", ".gitattributes", ".gitignore",
				"x64", "Debug", "Release", "Intermediate", "Library", "Temp",
				"imgui.ini", ".sln", ".vcxproj", ".filters", ".user"
			};
			return ignored.find(name) != ignored.end();
		}

		// ファイルタイプに応じたオープン処理
		void OpenFile(World& world, const std::filesystem::path& path)
		{
			std::string ext = path.extension().string();

			// コードやソリューションならOSに関連付けられたアプリ(VS等)で開く
			if (ext == ".h" || ext == ".cpp" || ext == ".sln" || ext == ".cs")
			{
				ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOW);
			}
			// シーンファイルならロード
			else if (ext == ".json")
			{
				std::string pathStr = path.string();
				if (pathStr.find("Scene") != std::string::npos || pathStr.find("scene") != std::string::npos)
				{
					SceneSerializer::LoadScene(world, pathStr);
				}
			}
		}

		// データコンポーネント（struct）の生成
		void CreateComponentFile(const std::string& componentName)
		{
			// ファイルパス: "CurrentDir/ComponentName.h"
			std::string hPath = (m_currentDirectory / (componentName + ".h")).string();

			std::ofstream hFile(hPath);
			if (hFile.is_open())
			{
				// 1. ヘッダーガードなど
				hFile << "#pragma once\n";
				hFile << "// 必須インクルード（マクロ使用のため）\n";
				hFile << "#include \"Engine/Scene/Components/Components.h\"\n\n";

				hFile << "namespace Arche {\n\n";

				// 2. 構造体定義
				hFile << "\t/**\n";
				hFile << "\t * @struct " << componentName << "\n";
				hFile << "\t * @brief  ユーザー定義データ\n";
				hFile << "\t */\n";
				hFile << "\tstruct " << componentName << "\n";
				hFile << "\t{\n";
				hFile << "\t\t// 変数はここに定義\n";
				hFile << "\t\tfloat value = 10.0f;\n";
				hFile << "\t};\n\n";

				// 3. エンジンへの登録マクロ (これが重要！)
				// これを書くことで、自動的にInspectorに表示され、JSON保存もされます
				hFile << "\tARCHE_COMPONENT(" << componentName << ",\n";
				hFile << "\t\tREFLECT_VAR(value) // ここに変数を追加していく\n";
				hFile << "\t)\n";

				hFile << "}\n";

				hFile.close();
				Logger::Log("Created Component: " + hPath);
			}
		}

		// スクリプトテンプレート生成
		void CreateSystemFile(const std::string& className)
		{
			std::string hPath = (m_currentDirectory / (className + ".h")).string();

			// ヘッダーファイル生成
			std::ofstream hFile(hPath);
			if (hFile.is_open())
			{
				hFile << "#pragma once\n\n";
				hFile << "// ===== インクルード =====\n";
				hFile << "#include \"Engine/Scene/Core/ECS/ECS.h\"\n";
				hFile << "#include \"Engine/Scene/Components/Components.h\"\n";
				hFile << "#include \"Engine/Core/Window/Input.h\"\n\n";

				hFile << "namespace Arche\n{\n";

				hFile << "\tclass " << className << " : public ISystem\n";
				hFile << "\t{\n";
				hFile << "\tpublic:\n";
				hFile << "\t\t" << className << "()\n";
				hFile << "\t\t{\n";
				hFile << "\t\t\tm_systemName = \"" << className << "\";\n";
				hFile << "\t\t}\n\n";

				hFile << "\t\tvoid Update(Registry& registry) override\n";
				hFile << "\t\t{\n";
				hFile << "\t\t\t// ここにロジックを記述\n";
				hFile << "\t\t\t// auto view = registry.view<Transform, ...>();\n";
				hFile << "\t\t}\n";
				hFile << "\t};\n";
				hFile << "}\t// namespace Arche\n\n";

				// 自動登録マクロ（これがないとSystemRegistryに登録されません）
				hFile << "#include \"Engine/Scene/Serializer/SystemRegistry.h\"\n";
				hFile << "ARCHE_REGISTER_SYSTEM(Arche::" << className << ", \"" << className << "\")\n";

				hFile.close();
				Logger::Log("Created System: " + hPath);
			}
		}

		// リネーム開始
		void StartRenaming(const std::filesystem::path& path)
		{
			m_isRenaming = true;
			m_renamingPath = path;
			strcpy_s(m_renameBuf, path.filename().string().c_str());
		}

		// リネーム実行
		void RenameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
		{
			try
			{
				if (std::filesystem::exists(newPath))
				{
					Logger::LogError("File already exists: " + newPath.string());
					return;
				}
				std::filesystem::rename(oldPath, newPath);
				Logger::Log("Renamed to: " + newPath.string());
			}
			catch (const std::exception& e)
			{
				Logger::LogError("Rename failed: " + std::string(e.what()));
			}
		}

		// 移動実行
		void MoveFile(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath)
		{
			try
			{
				std::filesystem::rename(srcPath, dstPath);
				Logger::Log("Moved: " + srcPath.string() + " -> " + dstPath.string());
			}
			catch (const std::exception& e)
			{
				Logger::LogError("Move failed: " + std::string(e.what()));
			}
		}
	};

}	// namespace Arche

#endif // _DEBUG

#endif // !___CONTENT_BROWSER_H___