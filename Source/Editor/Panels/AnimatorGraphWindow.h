/*****************************************************************//**
 * @file	AnimatorGraphWindow.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___ANIMATOR_GRAPH_WINDOW_H___
#define ___ANIMATOR_GRAPH_WINDOW_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Engine/Scene/Animation/AnimatorController.h"
#include "Engine/Scene/Serializer/AnimatorControllerSerializer.h"

namespace Arche
{
	class AnimatorGraphWindow : public EditorWindow
	{
	public:
		AnimatorGraphWindow()
		{
			m_windowName = "Animator";
			ImNodes::CreateContext();
			ImNodes::GetStyle().GridSpacing = 24.0f;
			ImNodes::GetStyle().PinCircleRadius = 4.0f;
			ImNodes::GetStyle().PinLineThickness = 2.0f;

			ImNodesIO& io = ImNodes::GetIO();
			io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

			RefreshMotionList();
			RefreshControllerList();
		}

		~AnimatorGraphWindow() override
		{
			ImNodes::DestroyContext();
		}

		void SetContext(std::shared_ptr<AnimatorController> controller)
		{
			m_controller = controller;
			m_initialized = false;
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin(m_windowName.c_str(), &m_isOpen);
			ImGui::PopStyleVar();

			ImGui::Dummy(ImVec2(0.0f, 4.0f));

			// --- ツールバー (Controller Selection) ---
			ImGui::SetCursorPosX(8.0f);

			ImGui::PushItemWidth(250);
			std::string comboPreview = m_currentPath.empty() ? "(Select Controller)" : m_currentPath;

			// パスが長い場合はファイル名だけ表示するなど工夫も可能ですが、一旦パス表示
			if (ImGui::BeginCombo("##ControllerSelect", comboPreview.c_str(), ImGuiComboFlags_HeightLarge))
			{
				// 検索フィルター
				static char ctrlSearch[64] = "";
				ImGui::InputTextWithHint("##CtrlSearch", "Search...", ctrlSearch, 64);

				bool hasSearch = ctrlSearch[0] != '\0';

				for (const auto& path : m_controllerFiles)
				{
					// 検索ヒット確認
					if (hasSearch) {
						// 簡易的な部分一致検索 (大文字小文字区別なしにするとなお良し)
						std::string p = path;
						std::string s = ctrlSearch;
						// ToLower変換などは省略
						if (p.find(s) == std::string::npos) continue;
					}

					bool isSelected = (m_currentPath == path);
					if (ImGui::Selectable(path.c_str(), isSelected))
					{
						m_currentPath = path;
						// 選択したら即ロード
						LoadController(m_currentPath);
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::SameLine();
			// Reloadボタン (ファイル内容を再読み込み)
			if (ImGui::Button("Reload"))
			{
				LoadController(m_currentPath);
			}

			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				if (m_controller)
				{
					if (AnimatorControllerSerializer::Serialize(m_currentPath, m_controller))
						Logger::Log("Saved: " + m_currentPath);
					else
						Logger::LogError("Failed to save: " + m_currentPath);
				}
			}

			ImGui::SameLine();
			// 新規作成ボタン (モーダルを開く)
			if (ImGui::Button("New"))
			{
				ImGui::OpenPopup("CreateNewControllerModal");
			}

			ImGui::SameLine();
			if (ImGui::Button("Reset Layout")) { m_initialized = false; }

			ImGui::SameLine();
			// リスト更新ボタン
			if (ImGui::Button("Refresh Files")) {
				RefreshMotionList();
				RefreshControllerList();
			}

			ImGui::Dummy(ImVec2(0.0f, 4.0f));
			ImGui::Separator();

			// --- Create New Modal ---
			DrawCreateNewModal();

			if (!m_controller)
			{
				ImGui::SetCursorPosX(8.0f);
				ImGui::TextDisabled("No Animator Controller loaded.");
				ImGui::End();
				return;
			}

			// --- 3ペイン レイアウト ---
			static float sidebarWidth = 300.0f;
			float mainHeight = ImGui::GetContentRegionAvail().y;

			ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, mainHeight), true);
			{
				DrawSidebar();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("GraphRegion", ImVec2(0, mainHeight), false);
			{
				DrawGraph();
			}
			ImGui::EndChild();

			if (m_openContextMenu) {
				ImGui::OpenPopup("AnimatorContextMenu");
				m_openContextMenu = false;
			}
			DrawPopup();

			ImGui::End();
		}

	private:
		std::shared_ptr<AnimatorController> m_controller = nullptr;
		std::string m_currentPath = ""; // 初期値は空にして選択させる
		bool m_initialized = false;

		std::vector<std::string> m_motionFiles;
		std::vector<std::string> m_controllerFiles;

		int m_selectedLinkNodeIdx = -1;
		int m_selectedLinkTransIdx = -1;
		bool m_openContextMenu = false;

		static constexpr int ID_OFFSET_NODE = 10000;
		static constexpr int ID_OFFSET_PIN = 20000;
		static constexpr int ID_OFFSET_LINK = 30000;

		int GetNodeId(int index) { return index + ID_OFFSET_NODE; }
		int GetNodeIndex(int nodeId) { return nodeId - ID_OFFSET_NODE; }
		int GetInputId(int index) { return (index << 16) + 1 + ID_OFFSET_PIN; }
		int GetOutputId(int index) { return (index << 16) + 2 + ID_OFFSET_PIN; }
		int GetNodeIndexFromAttribute(int attrId) { return (attrId - ID_OFFSET_PIN) >> 16; }
		int GetLinkId(int nodeIndex, int transIndex) { return (nodeIndex << 16) + transIndex + ID_OFFSET_LINK; }
		int GetNodeIndexFromLink(int linkId) { return (linkId - ID_OFFSET_LINK) >> 16; }
		int GetTransIndexFromLink(int linkId) { return (linkId - ID_OFFSET_LINK) & 0xFFFF; }

		int FindStateIndex(const StringId& name)
		{
			for (int i = 0; i < m_controller->states.size(); ++i)
				if (m_controller->states[i].name == name) return i;
			return -1;
		}

		// コントローラー読み込みヘルパー
		void LoadController(const std::string& path)
		{
			auto loaded = AnimatorControllerSerializer::Deserialize(path);
			if (loaded)
			{
				m_controller = loaded;
				m_initialized = false;
				Logger::Log("Loaded: " + path);
			}
			else
			{
				Logger::LogError("Failed to load: " + path);
			}
		}

		// コントローラー一覧更新
		void RefreshControllerList()
		{
			m_controllerFiles.clear();
			std::string path = "Resources/Game/Animations"; // 検索対象フォルダ
			if (!std::filesystem::exists(path)) return;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".json")
				{
					// 相対パスを保存
					std::string relative = std::filesystem::relative(entry.path()).string();
					std::replace(relative.begin(), relative.end(), '\\', '/'); // Windowsパス対策
					m_controllerFiles.push_back(relative);
				}
			}
		}

		void RefreshMotionList()
		{
			m_motionFiles.clear();
			m_motionFiles.push_back(""); // None

			std::filesystem::path basePath = "Resources/Game/Models";
			if (!std::filesystem::exists(basePath)) return;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath))
			{
				if (entry.is_regular_file())
				{
					if (entry.path().extension() == ".fbx")
					{
						std::filesystem::path relativePath = std::filesystem::relative(entry.path(), basePath);
						std::string key = relativePath.string();
						std::replace(key.begin(), key.end(), '\\', '/'); // Windowsパス対策
						m_motionFiles.push_back(key);
					}
				}
			}
		}

		// 新規作成モーダル描画
		void DrawCreateNewModal()
		{
			if (ImGui::BeginPopupModal("CreateNewControllerModal", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				static char nameBuf[64] = "NewController";
				ImGui::InputText("Name", nameBuf, 64);
				ImGui::Text("Path: Resources/Game/Animations/%s.json", nameBuf);

				ImGui::Separator();

				if (ImGui::Button("Create", ImVec2(120, 0)))
				{
					std::string filename = nameBuf;
					if (!filename.empty())
					{
						std::string fullPath = "Resources/Game/Animations/" + filename + ".json";

						// 初期データ作成
						m_controller = std::make_shared<AnimatorController>();
						m_controller->entryState = "Entry";
						AnimatorState state;
						state.name = "Entry";
						state.position[0] = 100; state.position[1] = 100;
						m_controller->states.push_back(state);

						// 保存
						if (AnimatorControllerSerializer::Serialize(fullPath, m_controller))
						{
							m_currentPath = fullPath;
							RefreshControllerList(); // リスト更新
							m_initialized = false;
							Logger::Log("Created new controller: " + fullPath);
							ImGui::CloseCurrentPopup();
						}
						else
						{
							Logger::LogError("Failed to create file.");
						}
					}
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}
		}

		void DrawSidebar()
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

			// A. パラメータエリア
			ImGui::BeginChild("ParameterArea", ImVec2(0, 200), true);
			{
				ImGui::TextDisabled("PARAMETERS");
				ImGui::SameLine();
				if (ImGui::Button("+", ImVec2(24, 24)))
					ImGui::OpenPopup("AddParamPopup");

				if (ImGui::BeginPopup("AddParamPopup"))
				{
					const char* types[] = { "Float", "Int", "Bool", "Trigger" };
					for (int i = 0; i < 4; i++)
					{
						if (ImGui::MenuItem(types[i]))
						{
							AnimatorParameter param;
							param.name = "New " + std::string(types[i]);
							param.type = (AnimatorParameterType)i;
							m_controller->parameters.push_back(param);
						}
					}
					ImGui::EndPopup();
				}

				ImGui::Separator();

				for (auto it = m_controller->parameters.begin(); it != m_controller->parameters.end(); )
				{
					ImGui::PushID(&(*it));
					if (ImGui::Button("X")) {
						it = m_controller->parameters.erase(it);
						ImGui::PopID();
						continue;
					}
					ImGui::SameLine();

					char buf[64];
					strcpy_s(buf, it->name.c_str());
					ImGui::SetNextItemWidth(90);
					if (ImGui::InputText("##Name", buf, sizeof(buf))) it->name = buf;
					ImGui::SameLine();

					ImGui::SetNextItemWidth(60);
					switch (it->type)
					{
					case AnimatorParameterType::Float:	 ImGui::DragFloat("##Val", &it->defaultFloat, 0.1f); break;
					case AnimatorParameterType::Int:	 ImGui::DragInt("##Val", &it->defaultInt); break;
					case AnimatorParameterType::Bool:	 ImGui::Checkbox("##Val", &it->defaultBool); break;
					case AnimatorParameterType::Trigger: ImGui::Text("(Trig)"); break;
					}
					ImGui::PopID();
					++it;
				}
			}
			ImGui::EndChild();

			// B. インスペクタエリア
			ImGui::BeginChild("InspectorArea", ImVec2(0, 0), true);
			{
				ImGui::TextDisabled("TRANSITION INSPECTOR");
				ImGui::Separator();

				if (m_selectedLinkNodeIdx >= 0 && m_selectedLinkNodeIdx < m_controller->states.size())
				{
					auto& state = m_controller->states[m_selectedLinkNodeIdx];
					if (m_selectedLinkTransIdx >= 0 && m_selectedLinkTransIdx < state.transitions.size())
					{
						auto& trans = state.transitions[m_selectedLinkTransIdx];

						ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s -> %s", state.name.c_str(), trans.targetState.c_str());
						ImGui::Spacing();

						ImGui::Checkbox("Has Exit Time", &trans.hasExitTime);
						if (trans.hasExitTime) ImGui::DragFloat("Exit Time", &trans.exitTime, 0.01f, 0.0f, 1.0f);
						ImGui::DragFloat("Duration", &trans.duration, 0.01f, 0.0f, 5.0f);

						ImGui::Separator();
						ImGui::Text("Conditions");
						ImGui::SameLine();
						if (ImGui::Button("+##Cond"))
						{
							if (!m_controller->parameters.empty()) {
								AnimatorCondition cond;
								cond.parameter = m_controller->parameters[0].name;
								cond.mode = AnimatorConditionMode::Greater;
								cond.threshold = 0;
								trans.conditions.push_back(cond);
							}
						}

						for (auto it = trans.conditions.begin(); it != trans.conditions.end(); )
						{
							ImGui::PushID(&(*it));

							if (ImGui::BeginCombo("##Param", it->parameter.c_str()))
							{
								for (const auto& p : m_controller->parameters)
								{
									if (ImGui::Selectable(p.name.c_str(), p.name == it->parameter))
										it->parameter = p.name;
								}
								ImGui::EndCombo();
							}

							const char* modes[] = { "If", "IfNot", "Greater", "Less", "Equals", "NotEqual" };
							int modeIdx = (int)it->mode;
							ImGui::SetNextItemWidth(90);
							if (ImGui::Combo("##Mode", &modeIdx, modes, IM_ARRAYSIZE(modes)))
								it->mode = (AnimatorConditionMode)modeIdx;

							ImGui::SameLine();
							ImGui::SetNextItemWidth(60);
							ImGui::DragFloat("##Thres", &it->threshold, 0.1f);

							ImGui::SameLine();
							if (ImGui::Button("x")) {
								it = trans.conditions.erase(it);
								ImGui::PopID();
								continue;
							}
							ImGui::PopID();
							++it;
						}
					}
				}
				else
				{
					ImGui::TextWrapped("Select a transition arrow to edit settings.");
				}
			}
			ImGui::EndChild();

			ImGui::PopStyleVar();
		}

		void DrawGraph()
		{
			ImNodes::BeginNodeEditor();

			for (int i = 0; i < m_controller->states.size(); ++i)
			{
				auto& state = m_controller->states[i];

				ImGui::PushID(i);

				int nodeId = GetNodeId(i);
				int inputId = GetInputId(i);
				int outputId = GetOutputId(i);

				ImNodes::BeginNode(nodeId);
				if (!m_initialized) ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(state.position[0], state.position[1]));

				if (state.name == m_controller->entryState)
				{
					ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0, 180, 0, 255));
					ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(0, 220, 0, 255));
					ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(0, 255, 0, 255));
				}

				ImNodes::BeginNodeTitleBar();
				const char* title = state.name.c_str();
				if (!title || title[0] == '\0') title = "State";
				ImGui::Text("%s", title);
				ImNodes::EndNodeTitleBar();

				if (state.name == m_controller->entryState) ImNodes::PopColorStyle();

				ImNodes::BeginInputAttribute(inputId);
				ImGui::Dummy(ImVec2(16, 16));
				ImNodes::EndInputAttribute();

				// --- コンテンツ ---
				ImGui::PushItemWidth(120);

				// ステート名 (安全なコピーを使用)
				char nameBuf[128] = ""; // 初期化
				if (state.name.c_str()) {
					strncpy_s(nameBuf, state.name.c_str(), sizeof(nameBuf) - 1);
				}
				if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
				{
					state.name = nameBuf;
				}

				// モーション名プルダウン
				if (ImGui::BeginCombo("Motion", state.motionName.c_str()))
				{
					for (int k = 0; k < m_motionFiles.size(); k++)
					{
						ImGui::PushID(k);

						const auto& motion = m_motionFiles[k];
						bool isSelected = (state.motionName == motion);

						if (ImGui::Selectable(motion.c_str(), isSelected))
						{
							state.motionName = motion;
						}

						if (isSelected) ImGui::SetItemDefaultFocus();

						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

				ImGui::Checkbox("Loop", &state.loop);
				ImGui::SetNextItemWidth(60);
				ImGui::DragFloat("Speed", &state.speed, 0.1f);

				ImNodes::BeginOutputAttribute(outputId);
				ImGui::Dummy(ImVec2(16, 16));
				ImNodes::EndOutputAttribute();

				ImNodes::EndNode();

				ImGui::PopID();

				ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
				state.position[0] = pos.x; state.position[1] = pos.y;
			}
			m_initialized = true;

			// リンク描画
			for (int i = 0; i < m_controller->states.size(); ++i)
			{
				auto& state = m_controller->states[i];
				for (int t = 0; t < state.transitions.size(); ++t)
				{
					auto& trans = state.transitions[t];
					int targetIdx = FindStateIndex(trans.targetState);
					if (targetIdx != -1)
						ImNodes::Link(GetLinkId(i, t), GetOutputId(i), GetInputId(targetIdx));
				}
			}

			ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
			ImNodes::EndNodeEditor();

			HandleInteractions();
		}

		void HandleInteractions()
		{
			// 1. リンク作成
			int startNode, endNode, startAttr, endAttr;
			if (ImNodes::IsLinkCreated(&startNode, &startAttr, &endNode, &endAttr))
			{
				int srcIdx = GetNodeIndexFromAttribute(startAttr);
				int dstIdx = GetNodeIndexFromAttribute(endAttr);
				bool isStartInput = ((startAttr - ID_OFFSET_PIN) & 0xFFFF) == 1;
				bool isEndOutput = ((endAttr - ID_OFFSET_PIN) & 0xFFFF) == 2;
				if (isStartInput && isEndOutput) std::swap(srcIdx, dstIdx);

				if (srcIdx >= 0 && srcIdx < m_controller->states.size() && dstIdx >= 0 && dstIdx < m_controller->states.size())
				{
					auto& srcState = m_controller->states[srcIdx];
					auto& dstState = m_controller->states[dstIdx];
					bool exists = false;
					for (const auto& t : srcState.transitions) if (t.targetState == dstState.name) exists = true;
					if (!exists) {
						AnimatorTransition newTrans;
						newTrans.targetState = dstState.name;
						srcState.transitions.push_back(newTrans);
					}
				}
			}

			// 2. 選択情報の更新
			if (ImNodes::NumSelectedLinks() > 0)
			{
				std::vector<int> selectedLinks(ImNodes::NumSelectedLinks());
				ImNodes::GetSelectedLinks(selectedLinks.data());
				int linkId = selectedLinks[0];
				m_selectedLinkNodeIdx = GetNodeIndexFromLink(linkId);
				m_selectedLinkTransIdx = GetTransIndexFromLink(linkId);
			}

			// 3. 削除処理
			if (ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				if (ImNodes::NumSelectedLinks() > 0)
				{
					std::vector<int> selectedLinks(ImNodes::NumSelectedLinks());
					ImNodes::GetSelectedLinks(selectedLinks.data());
					std::vector<std::pair<int, int>> targets;
					for (int id : selectedLinks) targets.push_back({ GetNodeIndexFromLink(id), GetTransIndexFromLink(id) });
					std::sort(targets.begin(), targets.end(), [](auto& a, auto& b) {
						if (a.first != b.first) return a.first > b.first;
						return a.second > b.second;
						});
					for (auto& t : targets) {
						if (t.first < m_controller->states.size() && t.second < m_controller->states[t.first].transitions.size())
							m_controller->states[t.first].transitions.erase(m_controller->states[t.first].transitions.begin() + t.second);
					}
					ImNodes::ClearLinkSelection();
					m_selectedLinkNodeIdx = -1;
				}
				if (ImNodes::NumSelectedNodes() > 0)
				{
					std::vector<int> selectedNodes(ImNodes::NumSelectedNodes());
					ImNodes::GetSelectedNodes(selectedNodes.data());
					std::vector<int> nodeIndices;
					for (int id : selectedNodes) nodeIndices.push_back(GetNodeIndex(id));
					std::sort(nodeIndices.rbegin(), nodeIndices.rend());
					for (int idx : nodeIndices) {
						if (idx < m_controller->states.size()) {
							StringId delName = m_controller->states[idx].name;
							m_controller->states.erase(m_controller->states.begin() + idx);
							for (auto& s : m_controller->states) {
								auto& ts = s.transitions;
								ts.erase(std::remove_if(ts.begin(), ts.end(), [&](const auto& t) { return t.targetState == delName; }), ts.end());
							}
						}
					}
					ImNodes::ClearNodeSelection();
					m_initialized = false;
				}
			}

			// 4. 右クリックメニュー
			int hoveredNode, hoveredLink;
			bool isEditorHovered = ImNodes::IsEditorHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
			if (ImGui::IsMouseClicked(1) && isEditorHovered) {
				if (!ImNodes::IsNodeHovered(&hoveredNode) && !ImNodes::IsLinkHovered(&hoveredLink))
					m_openContextMenu = true;
			}
		}

		void DrawPopup()
		{
			if (ImGui::BeginPopup("AnimatorContextMenu"))
			{
				if (ImGui::MenuItem("Create State"))
				{
					std::string baseName = "New State";
					std::string name = baseName;
					int count = 0;
					while (FindStateIndex(name) != -1) name = baseName + " " + std::to_string(++count);

					AnimatorState newState;
					newState.name = name;
					m_controller->states.push_back(newState);
					ImNodes::SetNodeScreenSpacePos(GetNodeId((int)m_controller->states.size() - 1), ImGui::GetMousePos());
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
	};
}

#endif