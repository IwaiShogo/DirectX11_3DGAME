/*****************************************************************//**
 * @file	SystemMonitorWindow.h
 * @brief	システムの稼働状況とパフォーマンスを監視するウィンドウ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SYSTEM_MONITOR_WINDOW_H___
#define ___SYSTEM_MONITOR_WINDOW_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"

namespace Arche
{
	class SystemMonitorWindow : public EditorWindow
	{
	public:
		SystemMonitorWindow()
		{
			m_windowName = "System Monitor";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			// 上部: 統計情報
			float totalTime = 0.0f;
			for (const auto& sys : world.getSystems()) if (sys->m_isEnabled) totalTime += (float)sys->m_lastExecutionTime;

			ImGui::Text("Active Systems: %d", (int)world.getSystems().size());
			ImGui::SameLine();
			ImGui::Text("| Total Logic Time: %.3f ms", totalTime);

			ImGui::Separator();

			// 検索バー
			static char searchBuf[64] = "";
			ImGui::InputTextWithHint("##Search", "Search System...", searchBuf, sizeof(searchBuf));

			// 追加ボタン (右寄せ)
			ImGui::SameLine(ImGui::GetWindowWidth() - 110);
			if (ImGui::Button("Add System..."))
			{
				ImGui::OpenPopup("AddSystemPopup");
			}
			DrawAddSystemPopup(world);

			ImGui::Separator();

			// 削除予約変数の初期化
			m_systemToRemove.clear();

			// テーブル設定
			ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;
			if (ImGui::BeginTable("SystemsTable", 4, flags))
			{
				ImGui::TableSetupColumn("En", ImGuiTableColumnFlags_WidthFixed, 30.0f);
				ImGui::TableSetupColumn("System Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableHeadersRow();

				// --- カテゴリ分け描画 ---
				DrawSystemCategory(world, "Engine Systems", true, searchBuf, totalTime);
				DrawSystemCategory(world, "User Systems", false, searchBuf, totalTime);

				ImGui::EndTable();
			}

			if (!m_systemToRemove.empty())
			{
				world.removeSystem(m_systemToRemove);
				m_systemToRemove.clear();
			}

			// D&Dによるシステム追加
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					std::string path = (const char*)payload->Data;
					std::filesystem::path fpath(path);
					std::string name = fpath.stem().string();
					SystemRegistry::Instance().CreateSystem(world, name, SystemGroup::PlayOnly);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::End();
		}

	private:
		std::string m_systemToRemove;

		bool IsEngineSystem(const std::string& name)
		{
			static const std::vector<std::string> engineSys = {
				"Physics System", "Collision System", "UI System",
				"Lifetime System", "Hierarchy System", "Render System", "Model Render System",
				"Sprite Render System", "Billboard System", "Text Render System", "Audio System", "Button System",
				"Animation System"
			};
			for (const auto& s : engineSys) if (s == name) return true;
			return false;
		}

		void DrawSystemCategory(World& world, const char* categoryName, bool isEngine, const char* filter, float totalFrameTime)
		{
			// カテゴリヘッダー（常に開いておく）
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
			ImGui::Text("");
			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", categoryName);
			ImGui::TableSetColumnIndex(2); ImGui::Text("");
			ImGui::TableSetColumnIndex(3); ImGui::Text("");

			for (const auto& sys : world.getSystems())
			{
				// カテゴリ判定
				if (IsEngineSystem(sys->m_systemName) != isEngine) continue;

				// 検索フィルタ (Case-insensitive)
				if (filter[0] != '\0')
				{
					std::string n = sys->m_systemName;
					std::string f = filter;
					auto toLower = [](std::string& s) { std::transform(s.begin(), s.end(), s.begin(), ::tolower); };
					toLower(n); toLower(f);
					if (n.find(f) == std::string::npos) continue;
				}

				ImGui::PushID(sys.get());
				ImGui::TableNextRow();

				// 1. Checkbox
				ImGui::TableSetColumnIndex(0);
				float checkX = ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::GetFrameHeight()) * 0.5f;
				ImGui::SetCursorPosX(checkX);
				ImGui::Checkbox("##Enabled", &sys->m_isEnabled);

				// 2. Name
				ImGui::TableSetColumnIndex(1);
				if (!sys->m_isEnabled) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

				// インジケーター（重い処理にはアイコン）
				std::string label = sys->m_systemName;
				if (sys->m_lastExecutionTime > 2.0) label = "(!) " + label;
				else label = "    " + label;

				ImGuiSelectableFlags sFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
				ImGui::Selectable(label.c_str(), false, sFlags);

				if (!sys->m_isEnabled) ImGui::PopStyleColor();

				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("Remove System", nullptr, false, !isEngine)) // エンジンシステムは削除不可に
					{
						m_systemToRemove = sys->m_systemName;	// 削除予約
					}
					ImGui::EndPopup();
				}

				// 3. Group
				ImGui::TableSetColumnIndex(2);
				const char* gName = (sys->m_group == SystemGroup::Always) ? "Always" : (sys->m_group == SystemGroup::PlayOnly ? "Play" : "Edit");
				ImVec4 gCol = (sys->m_group == SystemGroup::Always) ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f) : ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
				ImGui::TextColored(gCol, gName);

				// 4. Time & Gauge
				ImGui::TableSetColumnIndex(3);
				float timeMs = (float)sys->m_lastExecutionTime;
				float ratio = (totalFrameTime > 0.0f) ? (timeMs / totalFrameTime) : 0.0f;

				// バーの色決定（負荷が高いと赤くなる）
				ImVec4 barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
				if (ratio > 0.5f) barColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
				else if (ratio > 0.2f) barColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);

				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
				char overlay[32];
				sprintf_s(overlay, "%.2f ms", timeMs);
				ImGui::ProgressBar(ratio, ImVec2(-1, 0), overlay);
				ImGui::PopStyleColor();

				ImGui::PopID();
			}
		}

		void DrawAddSystemPopup(World& world)
		{
			if (ImGui::BeginPopup("AddSystemPopup"))
			{
				static char sBuf[64] = "";
				if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
				ImGui::InputTextWithHint("##AddSearch", "Search...", sBuf, sizeof(sBuf));
				ImGui::Separator();

				for (auto& [name, creator] : SystemRegistry::Instance().GetCreators())
				{
					// 検索＆エンジンシステム除外（追加リストにはユーザーシステムだけ出すなど）
					// ここでは全て出しますが、必要に応じてフィルタリングしてください
					std::string n = name; std::string s = sBuf;
					std::transform(n.begin(), n.end(), n.begin(), ::tolower);
					std::transform(s.begin(), s.end(), s.begin(), ::tolower);
					if (s[0] != '\0' && n.find(s) == std::string::npos) continue;

					// 登録チェック
					bool exists = false;
					for (const auto& sys : world.getSystems()) if (sys->m_systemName == name) exists = true;

					if (!exists)
					{
						if (ImGui::MenuItem(name.c_str()))
						{
							SystemRegistry::Instance().CreateSystem(world, name, SystemGroup::PlayOnly);
							ImGui::CloseCurrentPopup();
						}
					}
					else
					{
						ImGui::TextDisabled("%s (Added)", name.c_str());
					}
				}
				ImGui::EndPopup();
			}
		}
	};

}	// namespace Arche

#endif // _DEBUG

#endif // !___SYSTEM_MONITOR_WINDOW_H___