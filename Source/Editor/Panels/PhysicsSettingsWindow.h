/*****************************************************************//**
 * @file	PhysicsSettingsWindow.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___PROJECT_SETTINGS_WINDOW_H___
#define ___PROJECT_SETTINGS_WINDOW_H___

// ===== インクルード =====
#include "Editor/Core/Editor.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	class PhysicsSettingsWindow
		: public EditorWindow
	{
	public:
		PhysicsSettingsWindow()
		{
			m_isOpen = false;
			m_windowName = "Physics Settings";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			// --------------------------------------------------------
			// 1. レイヤー定義 (名前と色の編集)
			// --------------------------------------------------------
			if (ImGui::CollapsingHeader("Layer Definitions", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TextDisabled("Modify layer names. Clear text to remove.");
				ImGui::Separator();

				// 2列で表示 (Index | Name | Color)
				if (ImGui::BeginTable("LayerDefTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
				{
					ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 50.0f);
					ImGui::TableHeadersRow();

					for (int i = 0; i < PhysicsConfig::MAX_LAYERS; i++)
					{
						ImGui::TableNextRow();

						// ID
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%2d", i);

						// Name Input
						ImGui::TableSetColumnIndex(1);
						ImGui::PushID(i);

						// バッファを用意して入力させる
						std::string currentName = PhysicsConfig::GetLayerName(i);
						char buf[64];
						strcpy_s(buf, currentName.c_str());

						// 最初の6つ（Built-in）は変更不可にするか、警告付きにするのが通例ですが、今回は自由度重視で編集可能に
						// ただし空にしてもシステム的にDefault等は残るため注意が必要
						if (ImGui::InputText("##Name", buf, sizeof(buf)))
						{
							PhysicsConfig::SetLayerName(i, std::string(buf));
						}
						ImGui::PopID();

						// Color Picker
						ImGui::TableSetColumnIndex(2);
						ImGui::PushID(i + 1000);
						XMFLOAT4 col = PhysicsConfig::GetLayerColor(i);
						float colArr[4] = { col.x, col.y, col.z, col.w };
						if (ImGui::ColorEdit4("##Color", colArr, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
						{
							PhysicsConfig::SetLayerColor(i, { colArr[0], colArr[1], colArr[2], colArr[3] });
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
			}

			ImGui::Separator();

			// --------------------------------------------------------
			// 2. 衝突マトリックス (有効なレイヤーのみ表示)
			// --------------------------------------------------------
			if (ImGui::CollapsingHeader("Physics Collision Matrix", ImGuiTreeNodeFlags_DefaultOpen))
			{
				// 名前が設定されているレイヤーのリストを作成
				std::vector<int> activeIndices;
				for (int i = 0; i < PhysicsConfig::MAX_LAYERS; i++)
				{
					if (!PhysicsConfig::GetLayerName(i).empty()) activeIndices.push_back(i);
				}

				int count = (int)activeIndices.size();

				if (count > 0 && ImGui::BeginTable("LayerCollisionMatrix", count + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
				{
					// --- ヘッダー行 ---
					ImGui::TableSetupColumn("Layer");
					for (int idx : activeIndices)
					{
						// 縦書きは見にくいのでIDまたは短縮名を表示するか、マウスホバーでツールチップ
						ImGui::TableSetupColumn(PhysicsConfig::GetLayerName(idx).c_str());
					}
					ImGui::TableHeadersRow();

					// --- マトリックス ---
					for (int i = 0; i < count; i++)
					{
						int rowIdx = activeIndices[i];
						Layer rowLayer = PhysicsConfig::IndexToLayer(rowIdx);

						ImGui::TableNextRow();

						// 行見出し
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", PhysicsConfig::GetLayerName(rowIdx).c_str());

						// 列チェックボックス
						for (int j = 0; j < count; j++)
						{
							int colIdx = activeIndices[j];
							Layer colLayer = PhysicsConfig::IndexToLayer(colIdx);

							ImGui::TableSetColumnIndex(j + 1);

							// 対角線より上だけ表示
							if (j < i) continue;

							ImGui::PushID(rowIdx * 100 + colIdx);

							Layer maskA = PhysicsConfig::GetMask(rowLayer);
							bool check = (maskA & colLayer) == colLayer;

							// 中央寄せ
							float indent = (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight()) * 0.5f;
							if (indent > 0.0f) ImGui::Indent(indent);

							if (ImGui::Checkbox("", &check))
							{
								if (check) PhysicsConfig::Configure(rowLayer).collidesWith(colLayer);
								else	   PhysicsConfig::Configure(rowLayer).ignore(colLayer);
							}

							if (indent > 0.0f) ImGui::Unindent(indent);

							ImGui::PopID();
						}
					}
					ImGui::EndTable();
				}
				else
				{
					ImGui::TextDisabled("No layers defined. Add names above.");
				}
			}

			ImGui::End();
		}
	};
}

#endif // !___PROJECT_SETTINGS_WINDOW_H___