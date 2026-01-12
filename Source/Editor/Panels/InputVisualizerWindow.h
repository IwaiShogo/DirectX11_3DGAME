/*****************************************************************//**
 * @file	InputVisualizerWindow.h
 * @brief	入力デバッグ専用のウィンドウ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___INPUT_VISUALIZER_WINDOW_H___
#define ___INPUT_VISUALIZER_WINDOW_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"

namespace Arche
{
	class InputVisualizerWindow : public EditorWindow
	{
	public:
		InputVisualizerWindow()
		{
			m_windowName = "Input Visualizer";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			bool connected = Input::IsControllerConnected();
			if (!connected)
			{
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Controller Disconnected");
				ImGui::End();
				return;
			}

			ImGui::TextColored(ImVec4(0, 1, 0, 1), "Controller Connected");
			ImGui::Separator();

			// スティック描画ロジック（既存のコードを流用）
			float lx = Input::GetAxis(Axis::Horizontal);
			float ly = Input::GetAxis(Axis::Vertical);
			float rx = Input::GetAxis(Axis::RightHorizontal);
			float ry = Input::GetAxis(Axis::RightVertical);

			DrawStick("Left", lx, ly);
			ImGui::SameLine(0, 20);
			DrawStick("Right", rx, ry);

			ImGui::Separator();
			ImGui::Text("Buttons:");

			// ボタンマトリックス表示
			DrawBtn("A", Button::A); ImGui::SameLine(); DrawBtn("B", Button::B); ImGui::SameLine();
			DrawBtn("X", Button::X); ImGui::SameLine(); DrawBtn("Y", Button::Y); ImGui::NewLine();
			DrawBtn("LB", Button::LShoulder); ImGui::SameLine();
			DrawBtn("RB", Button::RShoulder); ImGui::SameLine();
			DrawBtn("START", Button::Start);

			ImGui::End();
		}

	private:
		void DrawStick(const char* label, float x, float y)
		{
			ImGui::BeginGroup();
			ImGui::Text("%s", label);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();
			float sz = 60.0f; // 少し小さく
			dl->AddRect(p, ImVec2(p.x + sz, p.y + sz), IM_COL32_WHITE);
			float cx = p.x + sz * 0.5f, cy = p.y + sz * 0.5f;
			dl->AddLine(ImVec2(cx, p.y), ImVec2(cx, p.y + sz), IM_COL32(100, 100, 100, 100));
			dl->AddLine(ImVec2(p.x, cy), ImVec2(p.x + sz, cy), IM_COL32(100, 100, 100, 100));
			dl->AddCircleFilled(ImVec2(cx + x * sz * 0.5f, cy - y * sz * 0.5f), 4.0f, IM_COL32(255, 0, 0, 255));
			ImGui::Dummy(ImVec2(sz, sz));
			ImGui::Text("%.2f, %.2f", x, y);
			ImGui::EndGroup();
		}

		void DrawBtn(const char* label, Button btn)
		{
			bool on = Input::GetButton(btn);
			if (on) ImGui::TextColored(ImVec4(0, 1, 0, 1), "[%s]", label);
			else ImGui::TextDisabled("[%s]", label);
		}
	};

}	// namespace Arche

#endif // _DEBUG

#endif // !___INPUT_VISUALIZER_WINDOW_H___