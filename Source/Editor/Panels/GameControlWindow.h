/*****************************************************************//**
 * @file	GameControlWindow.h
 * @brief	再生制御やデバッグ表示など、ツールバー的な役割を持つウィンドウ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___GAME_CONTROL_WINDOW_H___
#define ___GAME_CONTROL_WINDOW_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"

namespace Arche
{
	class GameControlWindow : public EditorWindow
	{
	public:
		GameControlWindow()
		{
			m_windowName = "Game Control";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			// 1. FPSと時間
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::SameLine();
			ImGui::Text("Time: %.2fs", Time::TotalTime());

			ImGui::Separator();

			// 2. 再生制御（Play中のみ操作可能）
			ImGui::BeginDisabled(ctx.editorState != EditorState::Play);
			{
				// タイムスケールスライダー
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Speed", &Time::timeScale, 0.0f, 20.0f, "%.1fx");

				ImGui::SameLine();

				// Pause / Resume / Step
				if (Time::isPaused)
				{
					if (ImGui::Button("Resume")) Time::isPaused = false;
					ImGui::SameLine();
					if (ImGui::Button("Step >")) Time::StepFrame();
				}
				else
				{
					if (ImGui::Button("Pause ")) Time::isPaused = true;
				}
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			// 3. デバッグ表示設定
			if (ImGui::TreeNodeEx("Debug View Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Grid", &ctx.debugSettings.showGrid); ImGui::SameLine();
				ImGui::Checkbox("Axis", &ctx.debugSettings.showAxis);

				ImGui::Checkbox("Colliders", &ctx.debugSettings.showColliders);
				if (ctx.debugSettings.showColliders)
				{
					ImGui::SameLine();
					ImGui::Checkbox("Wireframe", &ctx.debugSettings.wireframeMode);
				}
				ImGui::TreePop();
			}

			ImGui::End();
		}
	};
}

#endif // _DEBUG

#endif // !___GAME_CONTROL_WINDOW_H___