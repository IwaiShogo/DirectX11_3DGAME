/*****************************************************************//**
 * @file	LightingSettingsWindow.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___LIGHTING_SETTINGS_WINDOW_H___
#define ___LIGHTING_SETTINGS_WINDOW_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Engine/Scene/Systems/Graphics/RenderSystem.h" // RenderSystemにアクセスするため

namespace Arche
{
	class LightingSettingsWindow : public EditorWindow
	{
	public:
		LightingSettingsWindow()
		{
			m_windowName = "Lighting Settings";
			m_isOpen = false;
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			auto& env = ctx.environment;

			if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Skybox Material");

				// テクスチャパス表示ボタン（D&D用）
				std::string btnText = env.skyboxTexturePath.empty() ? "(None)" : env.skyboxTexturePath;
				ImGui::Button(btnText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30, 0));

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const char* droppedPath = (const char*)payload->Data; // 例: "Resources\Game\Skybox.dds" または "Skybox.dds"

						std::string pathStr = droppedPath;

						// バックスラッシュをスラッシュに統一
						std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

						// "Resources/Game/" が既に含まれているかチェック
						std::string rootPrefix = "Resources/Game/";

						std::string fullPath;
						if (pathStr.find(rootPrefix) == 0)
						{
							// 既にプレフィックスが付いている場合はそのまま使う
							fullPath = pathStr;
						}
						else
						{
							// 付いていない場合のみ結合する
							fullPath = rootPrefix + pathStr;
						}

						// 拡張子チェック (.dds)
						if (fullPath.find(".dds") != std::string::npos)
						{
							env.skyboxTexturePath = fullPath;
						}
					}
					ImGui::EndDragDropTarget();
				}

				// クリアボタン
				ImGui::SameLine();
				if (ImGui::Button("X"))
				{
					env.skyboxTexturePath = "";
				}

				// プロシージャルカラー設定
				if (env.skyboxTexturePath.empty())
				{
					ImGui::Separator();
					ImGui::Text("Procedural Sky Colors");
					ImGui::ColorEdit3("Top Color", (float*)&env.skyColorTop);
					ImGui::ColorEdit3("Horizon Color", (float*)&env.skyColorHorizon);
					ImGui::ColorEdit3("Bottom Color", (float*)&env.skyColorBottom);
				}
			}

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Environment Lighting", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::ColorEdit3("Ambient Color", (float*)&env.ambientColor);
				ImGui::DragFloat("Intensity", &env.ambientIntensity, 0.01f, 0.0f, 10.0f);
			}

			ImGui::End();
		}
	};
}

#endif // !___LIGHTING_SETTINGS_WINDOW_H___