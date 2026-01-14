/*****************************************************************//**
 * @file	HierarchyWindow.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___HIERARCHY_WINDOW_H___
#define ___HIERARCHY_WINDOW_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Editor/Core/EditorCommands.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"

namespace Arche
{

	class HierarchyWindow
		: public EditorWindow
	{
	public:
		HierarchyWindow()
		{
			m_windowName = "Hierarchy";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			// ------------------------------------------------------------
			// Hierarchy Window
			// ------------------------------------------------------------
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			if (Editor::Instance().GetMode() == Editor::EditorMode::Prefab)
			{
				// 保存ボタン
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
				if (ImGui::Button("Save Prefab"))
				{
					Editor::Instance().SavePrefabAndExit();
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();

				// 終了ボタン
				if (ImGui::Button("Exit"))
				{
					Editor::Instance().ExitPrefabMode();
				}
				ImGui::Separator();
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "Editing Prefab...");
				ImGui::Separator();

				World* prefabWorld = Editor::Instance().GetActiveWorld();
				Registry& prefabReg = prefabWorld->getRegistry();

				prefabReg.each([&](auto entityID) {
					Entity e = entityID;
					if (!prefabReg.has<Tag>(e)) return;
					bool isRoot = true;
					if (prefabReg.has<Relationship>(e))
					{
						if (prefabReg.get<Relationship>(e).parent != NullEntity) isRoot = false;
					}

					if (isRoot)
					{
						// selection を渡す
						DrawEntityNode(*prefabWorld, e, selection);
					}
					});
			}
			else
			{
				// 検索バー
				ImGui::InputTextWithHint("##HierSearch", "Search Entity...", m_searchFilter, sizeof(m_searchFilter));
				ImGui::Separator();

				// 自動展開ロジック (プライマリ選択が変更された場合)
				Entity primary = selection.empty() ? NullEntity : selection.back();
				if (primary != m_lastSelected)
				{
					m_nodesToOpen.clear();
					if (primary != NullEntity && world.getRegistry().valid(primary))
					{
						Entity current = primary;
						Registry& reg = world.getRegistry();
						while (reg.has<Relationship>(current))
						{
							Entity parent = reg.get<Relationship>(current).parent;
							if (parent == NullEntity) break;

							m_nodesToOpen.insert(parent);
							current = parent;
						}
					}
					m_lastSelected = primary;
				}

				// Deleteキーでの削除処理 (選択されている全エンティティ)
				if (ImGui::IsWindowFocused() && !selection.empty())
				{
					if (ImGui::IsKeyPressed(ImGuiKey_Delete))
					{
						// コピーを作成してから削除（イテレータ無効化防止）
						auto targets = selection;
						for (Entity e : targets)
						{
							CommandHistory::Execute(std::make_shared<DeleteEntityCommand>(world, e));
						}
						// 選択解除
						Editor::Instance().ClearSelection();
					}
				}

				// 複製（Ctrl + D） (選択されている全エンティティ)
				if (ImGui::IsWindowFocused())
				{
					if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
					{
						if (!selection.empty())
						{
							// 新しく選択されるエンティティリスト
							std::vector<Entity> newSelection;

							auto targets = selection; // コピー
							for (Entity e : targets)
							{
								Entity newEntity = SceneSerializer::DuplicateEntity(world, e);
								newSelection.push_back(newEntity);
							}

							// 複製されたものを選択状態にする
							Editor::Instance().ClearSelection();
							for (Entity e : newSelection) Editor::Instance().AddToSelection(e);
						}
					}
				}

				// ----------------------------------------------------------------
				// エンティティ一覧描画
				// ----------------------------------------------------------------

				bool isSearching = (m_searchFilter[0] != '\0');

				if (isSearching)
				{
					// === 検索モード ===
					// (省略: ContainsICなどはそのまま)
					auto ContainsIC = [](const std::string& str, const std::string& query) {
						auto it = std::search(
							str.begin(), str.end(),
							query.begin(), query.end(),
							[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
						);
						return (it != str.end());
						};

					world.getRegistry().each([&](Entity e) {
						if (!world.getRegistry().has<Tag>(e)) return;
						Tag& tag = world.getRegistry().get<Tag>(e);

						if (ContainsIC(tag.name.c_str(), m_searchFilter))
						{
							ImGui::PushID((int)e);

							// 選択判定
							bool isSelected = std::find(selection.begin(), selection.end(), e) != selection.end();

							ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
							if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

							if (!world.getRegistry().isActive(e)) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
							ImGui::TreeNodeEx((void*)(uint64_t)e, flags, "%s (ID:%d)", tag.name.c_str(), e);
							if (!world.getRegistry().isActive(e)) ImGui::PopStyleColor();

							// クリック処理 (検索時も複数選択対応)
							if (ImGui::IsItemClicked())
							{
								if (ImGui::GetIO().KeyCtrl)
								{
									if (isSelected) Editor::Instance().RemoveFromSelection(e);
									else Editor::Instance().AddToSelection(e);
								}
								else
								{
									Editor::Instance().SetSelectedEntity(e);
								}
							}

							if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								Editor::Instance().GetSceneViewPanel().FocusEntity(e, world);
							}

							ImGui::PopID();
						}
						});
				}
				else
				{
					// === 通常モード（ツリー表示） ===
					world.getRegistry().each([&](Entity e) {
						if (!world.getRegistry().has<Tag>(e)) return;
						bool isRoot = true;
						if (world.getRegistry().has<Relationship>(e)) {
							if (world.getRegistry().get<Relationship>(e).parent != NullEntity) isRoot = false;
						}
						if (isRoot) {
							// 修正: selection を渡す
							DrawEntityNode(world, e, selection);
						}
						});
				}

				// ウィンドウの余白部分へのドロップ
				if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("Hierarchy"))) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
						Entity payloadEntity = *(const Entity*)payload->Data;
						SetParent(world, payloadEntity, NullEntity);
					}
					ImGui::EndDragDropTarget();
				}

				// 余白クリックで選択解除
				if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
				{
					Editor::Instance().ClearSelection();
				}

				// 右クリック: 新規作成
				if (ImGui::BeginPopupContextWindow(nullptr, ImGuiMouseButton_Right | ImGuiPopupFlags_NoOpenOverItems))
				{
					if (ImGui::MenuItem("Create Empty"))
					{
						Entity e = world.create_entity()
							.add<Tag>()
							.add<Transform>().id();

						if (world.getRegistry().has<Tag>(e))
						{
							auto& tag = world.getRegistry().get<Tag>(e);
							tag.componentOrder.push_back("Transform");
						}

						Editor::Instance().SetSelectedEntity(e);
					}
					ImGui::EndPopup();
				}

				ImGui::Dummy(ImGui::GetContentRegionAvail());

				// D&D受信 (外部ファイルなど)
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						// 1. パスの取得と補完
						const char* droppedPath = (const char*)payload->Data;
						std::string relativePath = droppedPath;
						std::string rootPath = "Resources\\Game\\";

						std::string fullPath;
						// パスが既に "Resources/Game/" で始まっているかチェック
						if (relativePath.find(rootPath) == 0)
						{
							fullPath = relativePath;
						}
						else
						{
							fullPath = rootPath + relativePath;
						}

						// バックスラッシュをスラッシュに統一
						std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

						// 2. 拡張子の取得
						std::filesystem::path fpath = fullPath;
						std::string ext = fpath.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

						// --- 拡張子による分岐 ---

						// パターンA: プレファブ (.json)
						if (ext == ".json")
						{
							Entity prefabRoot = SceneSerializer::LoadPrefab(world, fullPath);
							if (prefabRoot != NullEntity)
							{
								// 修正: 複数選択対応のセッターを使用
								Editor::Instance().SetSelectedEntity(prefabRoot);
							}
						}
						// パターンB: 通常のアセット
						else
						{
							// 空のエンティティを新規作成
							Entity newEntity = world.create_entity().id();

							// 基本コンポーネント追加
							std::string entityName = fpath.stem().string();
							world.getRegistry().emplace<Tag>(newEntity, entityName);
							world.getRegistry().emplace<Transform>(newEntity);

							// アセットに応じたコンポーネント追加 (コメントアウトされていたロジックを復元・有効化する場合はここ)
							/*
							if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
							{
								StringId key(fullPath);
								ResourceManager::Instance().RegisterResource(key, fullPath, ResourceManager::ResourceType::Model);
								world.getRegistry().emplace<MeshComponent>(newEntity, key);
							}
							else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
							{
								StringId key(fullPath);
								ResourceManager::Instance().RegisterResource(key, fullPath, ResourceManager::ResourceType::Texture);
								world.getRegistry().emplace<SpriteComponent>(newEntity, key);
							}
							else if (ext == ".wav" || ext == ".mp3")
							{
								StringId key(fullPath);
								ResourceManager::Instance().RegisterResource(key, fullPath, ResourceManager::ResourceType::Sound);
								world.getRegistry().emplace<AudioSource>(newEntity, key);
							}
							*/

							// 複数選択対応のセッターを使用
							Editor::Instance().SetSelectedEntity(newEntity);
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
			ImGui::End();
		}

	private:
		// エンティティとその子供を再帰的に削除する
		void DeleteEntityRecursively(World& world, Entity entity)
		{
			Registry& reg = world.getRegistry();
			if (!reg.valid(entity)) return;

			// 1. 親から自分を外す
			if (reg.has<Relationship>(entity))
			{
				Entity parent = reg.get<Relationship>(entity).parent;
				if (parent != NullEntity && reg.has<Relationship>(parent))
				{
					auto& siblings = reg.get<Relationship>(parent).children;
					siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
				}
			}

			// 2. 子供を再帰的に削除するヘルパー
			auto recursiveDestroy = [&](auto&& self, Entity e) -> void
			{
				if(reg.has<Relationship>(e))
				{
					// コピーしてから回す
					std::vector<Entity> children = reg.get<Relationship>(e).children;
					for (Entity child : children)
					{
						self(self, child);
					}
				}

				// 削除実行
				reg.destroy(e);
			};

			recursiveDestroy(recursiveDestroy, entity);
		}

		void SetParent(World& world, Entity child, Entity parent)
		{
			// 自分自身を親にはできない
			if (child == parent) return;

			// 1. 現在の親から離脱
			if (world.getRegistry().has<Relationship>(child))
			{
				Entity oldParent = world.getRegistry().get<Relationship>(child).parent;
				if (oldParent != NullEntity && world.getRegistry().has<Relationship>(oldParent))
				{
					auto& children = world.getRegistry().get<Relationship>(oldParent).children;
					// 子リストから自分を削除
					children.erase(std::remove(children.begin(), children.end(), child), children.end());
				}
			}

			// 2. 新しい親に所属
			// 子側の設定
			if (!world.getRegistry().has<Relationship>(child)) world.getRegistry().emplace<Relationship>(child);
			world.getRegistry().get<Relationship>(child).parent = parent;

			// 親側の設定
			if (parent != NullEntity)
			{
				if (!world.getRegistry().has<Relationship>(parent)) world.getRegistry().emplace<Relationship>(parent);
				world.getRegistry().get<Relationship>(parent).children.push_back(child);
			}
		}

		void DrawEntityNode(World& world, Entity e, std::vector<Entity>& selection)
		{
			Registry& reg = world.getRegistry();
			if (!reg.valid(e)) return;

			Tag& tag = reg.get<Tag>(e);

			ImGui::PushID((int)e);

			bool isActive = reg.isActiveSelf(e);
			if (ImGui::Checkbox("##Active", &isActive))
			{
				reg.setActive(e, isActive);
			}
			ImGui::SameLine();

			// 選択判定
			bool isSelected = std::find(selection.begin(), selection.end(), e) != selection.end();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

			// 子がいるかチェック
			bool hasChildren = false;
			if (world.getRegistry().has<Relationship>(e)) {
				if (!world.getRegistry().get<Relationship>(e).children.empty()) hasChildren = true;
			}
			if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf; // 子がなければリーフ

			if (m_nodesToOpen.count(e))
			{
				ImGui::SetNextItemOpen(true);
			}

			// --- ノード描画 ---
			bool isEffectivelyActive = reg.isActive(e);
			bool isPrefab = reg.has<PrefabInstance>(e); // プレファブかどうかチェック
			bool colorPushed = false;

			if (!isEffectivelyActive)
			{
				// 非アクティブならグレー
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
				colorPushed = true;
			}
			else if (isPrefab)
			{
				// プレファブなら水色
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.65f, 1.0f, 1.0f));
				colorPushed = true;
			}

			// ツリーノード描画
			bool opened = ImGui::TreeNodeEx((void*)(uint64_t)e, flags, "%s (ID:%d)", tag.name.c_str(), e);

			// 色を戻す
			if (colorPushed) ImGui::PopStyleColor();

			// クリック処理
			if (ImGui::IsItemClicked())
			{
				if (ImGui::GetIO().KeyCtrl)
				{
					if (isSelected) Editor::Instance().RemoveFromSelection(e);
					else Editor::Instance().AddToSelection(e);
				}
				else
				{
					if (!isSelected) Editor::Instance().SetSelectedEntity(e);
				}
			}

			// ダブルクリック
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				Editor::Instance().GetSceneViewPanel().FocusEntity(e, world);
			}

			// --- ドラッグ＆ドロップ処理 ---

			// 1. ドラッグ元 (Source)
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("ENTITY_ID", &e, sizeof(Entity));
				ImGui::Text("Move %s", tag.name.c_str());
				ImGui::EndDragDropSource();
			}

			// 2. ドロップ先 (Target)
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
				{
					Entity payloadEntity = *(const Entity*)payload->Data;

					if (payloadEntity != e)
					{
						// マウス位置による判定
						float mouseY = ImGui::GetMousePos().y;
						float nodeMinY = ImGui::GetItemRectMin().y;
						float nodeMaxY = ImGui::GetItemRectMax().y;
						float height = nodeMaxY - nodeMinY;
						float relativeY = (mouseY - nodeMinY) / height;

						// ゾーン閾値
						float topThreshold = 0.25f;
						float bottomThreshold = 0.75f;

						// 親情報を取得（並び替え用）
						Entity parent = NullEntity;
						int myIndex = 0;
						if (reg.has<Relationship>(e))
						{
							parent = reg.get<Relationship>(e).parent;
						}

						// 親がいる場合のみインデックスを計算
						if (parent != NullEntity)
						{
							auto& siblings = reg.get<Relationship>(parent).children;
							auto it = std::find(siblings.begin(), siblings.end(), e);
							if (it != siblings.end()) myIndex = (int)std::distance(siblings.begin(), it);
						}

						// 描画リスト
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						ImU32 guideColor = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
						float guideThickness = 2.0f;

						// --- 判定ロジック ---

						// A) 上端ゾーン: 「直前に挿入」 (親がいる場合のみ)
						if (relativeY < topThreshold && parent != NullEntity)
						{
							drawList->AddLine({ ImGui::GetItemRectMin().x, nodeMinY }, { ImGui::GetItemRectMax().x, nodeMinY }, guideColor, guideThickness);
							if (payload->IsDelivery())
							{
								CommandHistory::Execute(std::make_shared<MoveEntityCommand>(world, payloadEntity, parent, myIndex));
							}
						}
						// B) 下端ゾーン: 「直後に挿入」 (親がいる場合のみ)
						else if (relativeY > bottomThreshold && parent != NullEntity)
						{
							drawList->AddLine({ ImGui::GetItemRectMin().x, nodeMaxY }, { ImGui::GetItemRectMax().x, nodeMaxY }, guideColor, guideThickness);
							if (payload->IsDelivery())
							{
								CommandHistory::Execute(std::make_shared<MoveEntityCommand>(world, payloadEntity, parent, myIndex + 1));
							}
						}
						// C) 中央ゾーン: 「子要素にする」 (既存動作)
						else
						{
							// 枠線表示（ImGui標準のハイライトと同じような矩形）
							drawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), guideColor, 0.0f, 0, guideThickness);

							if (payload->IsDelivery())
							{
								CommandHistory::Execute(std::make_shared<ReparentEntityCommand>(world, payloadEntity, e));
							}
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			// 右クリックメニュー
			if (ImGui::BeginPopupContextItem())
			{
				// Duplicate
				if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
				{
					Entity newEntity = SceneSerializer::DuplicateEntity(world, e);
					Editor::Instance().SetSelectedEntity(newEntity);

					ImGui::EndPopup();

					if (opened) ImGui::TreePop();
					ImGui::PopID();
					return;
				}

				// エンティティ削除
				if (ImGui::MenuItem("Delete Entity"))
				{
					CommandHistory::Execute(std::make_shared<DeleteEntityCommand>(world, e));
					Editor::Instance().RemoveFromSelection(e);	// 選択中なら解除
					ImGui::EndPopup();
					if (opened) ImGui::TreePop();	// ツリーが開いていた場合の整合性

					ImGui::PopID();
					return;
				}

				ImGui::Separator();

				// プレファブ関連メニュー
				if (reg.has<PrefabInstance>(e))
				{
					// Unpack (解除)
					if (ImGui::MenuItem("Unpack Prefab"))
					{
						// 単純にコンポーネントを外すだけ
						reg.remove<PrefabInstance>(e);
						ImGui::EndPopup();
						if (opened) ImGui::TreePop();

						ImGui::PopID();
						return;
					}

					// Revert (復元)
					if (ImGui::MenuItem("Revert to Prefab"))
					{
						SceneSerializer::RevertPrefab(world, e);
						ImGui::EndPopup();
						if (opened) ImGui::TreePop();

						ImGui::PopID();
						return;
					}

					ImGui::Separator();
				}

				// プレファブ保存
				if (ImGui::MenuItem("Save as Prefab"))
				{
					// 保存先ディレクトリ確保
					std::filesystem::create_directories("Resources/Game/Prefabs");

					// ファイル名決定（エンティティ名.json）
					std::string filename = "Entity";
					if (reg.has<Tag>(e))
					{
						filename = reg.get<Tag>(e).name.c_str();
					}

					std::string path = "Resources/Game/Prefabs/" + filename + ".json";

					// 保存実行
					SceneSerializer::SavePrefab(reg, e, path);

					if (!reg.has<PrefabInstance>(e))
					{
						reg.emplace<PrefabInstance>(e, path);
					}
					else
					{
						reg.get<PrefabInstance>(e).prefabPath = path;
					}
				}

				ImGui::EndPopup();
			}

			// --- 子要素の再帰描画 ---
			if (opened)
			{
				if (hasChildren)
				{
					auto children = reg.get<Relationship>(e).children;
					for (Entity child : children)
					{
						DrawEntityNode(world, child, selection);
					}
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

	private:
		// 自動展開するノードIDを一時保存するセット
		std::set<Entity> m_nodesToOpen;
		Entity m_lastSelected = NullEntity;

		// 検索用バッファ
		char m_searchFilter[64] = "";
	};

}	// namespace Arche

#endif // _DEBUG

#endif // !___HIERARCHY_WINDOW_H___