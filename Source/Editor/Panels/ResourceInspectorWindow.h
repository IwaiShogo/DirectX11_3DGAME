/*****************************************************************//**
 * @file	ResourceInspectorWindow.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___RESOURCE_INSPECTOR_WINDOW_H___
#define ___RESOURCE_INSPECTOR_WINDOW_H___

// ===== インクルード =====
#include "Editor/Core/Editor.h"
#include "Engine/Resource/ResourceManager.h"

namespace Arche
{
	class ResourceInspectorWindow : public EditorWindow
	{
	public:
		ResourceInspectorWindow()
		{
			m_windowName = "Resource Monitor";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			if (ImGui::BeginTabBar("ResourceTabs"))
			{
				// --- Texture Tab ---
				if (ImGui::BeginTabItem("Textures"))
				{
					DrawTextureList();
					ImGui::EndTabItem();
				}

				// --- Model Tab ---
				if (ImGui::BeginTabItem("Models"))
				{
					DrawModelList();
					ImGui::EndTabItem();
				}

				// --- Sound Tab ---
				if (ImGui::BeginTabItem("Sounds"))
				{
					DrawSoundList();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::End();
		}

	private:
		void DrawTextureList()
		{
			auto& textures = ResourceManager::Instance().GetTextureMap();

			// 検索フィルタ
			static char filterBuf[64] = "";
			ImGui::InputTextWithHint("##Filter", "Filter...", filterBuf, 64);

			ImGui::Separator();

			// リスト表示
			if (ImGui::BeginTable("TexTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 50);
				ImGui::TableSetupColumn("Key Name");
				ImGui::TableSetupColumn("Info");
				ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80);
				ImGui::TableHeadersRow();

				for (auto& [key, tex] : textures)
				{
					// フィルタリング
					if (filterBuf[0] != '\0' && key.find(filterBuf) == std::string::npos) continue;

					ImGui::TableNextRow();

					// 1. Preview
					ImGui::TableSetColumnIndex(0);
					if (tex && tex->srv)
					{
						ImGui::Image((void*)tex->srv.Get(), ImVec2(40, 40));
						// ホバーで拡大表示
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Image((void*)tex->srv.Get(), ImVec2(200, 200));
							ImGui::EndTooltip();
						}
					}

					// 2. Key Name (実パスもツールチップで)
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s", key.c_str());
					if (ImGui::IsItemHovered() && tex) ImGui::SetTooltip("%s", tex->filepath.c_str());

					// 3. Info
					ImGui::TableSetColumnIndex(2);
					if (tex) ImGui::Text("%dx%d", tex->width, tex->height);

					// 4. Actions
					ImGui::TableSetColumnIndex(3);
					if (ImGui::Button(("Reload##" + key).c_str()))
					{
						ResourceManager::Instance().ReloadTexture(key);
					}
				}
				ImGui::EndTable();
			}
		}

		void DrawModelList()
		{
			auto& models = ResourceManager::Instance().GetModelMap();

			ImGui::BeginChild("ModelList");
			for (auto& [key, model] : models)
			{
				ImGui::Text("Model: %s", key.c_str());
				if (model)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(%d meshes)", (int)model->GetMeshes().size());
				}
			}
			ImGui::EndChild();
		}

		void DrawSoundList()
		{
			auto& sounds = ResourceManager::Instance().GetSoundMap();

			ImGui::BeginChild("SoundList");
			for (auto& [key, sound] : sounds)
			{
				ImGui::Text("Sound: %s", key.c_str());
				if (sound)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(%.2f sec)", sound->duration);

					ImGui::SameLine();
					if (ImGui::SmallButton(("Play##" + key).c_str()))
					{
						std::string tempKey = key;
						AudioManager::Instance().PlaySE(tempKey); // 再生テスト
					}
				}
			}
			ImGui::EndChild();
		}
	};

}	// namespace Arche

#endif // !___RESOURCE_INSPECTOR_WINDOW_H___