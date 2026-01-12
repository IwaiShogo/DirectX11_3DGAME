/*****************************************************************//**
 * @file	GameViewPanel.h
 * @brief	ゲームビュー（実際のゲーム画面の表示）
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___GAME_VIEW_PANEL_H___
#define ___GAME_VIEW_PANEL_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Application.h"

namespace Arche {

	class GameViewPanel
	{
	public:
		void Draw()
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("Game", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);

			// 1. サイズチェック、必要ならリサイズ
			// ------------------------------------------------------------
			ImVec2 panelSize = ImGui::GetContentRegionAvail();
			float targetAspect = (float)Config::SCREEN_WIDTH / (float)Config::SCREEN_HEIGHT;

			// 描画する画像サイズ
			ImVec2 imageSize = panelSize;

			// パネルの方が横長なら、横幅を制限（左右に黒帯）
			if (panelSize.x / panelSize.y > targetAspect)
			{
				imageSize.x = panelSize.y * targetAspect;
			}
			// パネルの方が縦長なら、高さを制限（上下に黒帯）
			else
			{
				imageSize.y = panelSize.x / targetAspect;
			}

			// 中央揃えのためのオフセット計算
			ImVec2 cursorStart = ImGui::GetCursorPos();
			float offsetX = (panelSize.x - imageSize.x) * 0.5f;
			float offsetY = (panelSize.y - imageSize.y) * 0.5f;
			ImGui::SetCursorPos(ImVec2(cursorStart.x + offsetX, cursorStart.y + offsetY));

			// 2. リサイズ処理（計算したimageSizeに合わせる）
			// ------------------------------------------------------------
			static ImVec2 lastSize = { 0, 0 };
			if ((imageSize.x > 0 && imageSize.y > 0) && (imageSize.x != lastSize.x || imageSize.y != lastSize.y))
			{
				// 作り直す
				Application::Instance().ResizeGameRenderTarget(imageSize.x, imageSize.y);
				lastSize = imageSize;
			}

			// 3. 画像描画
			// ------------------------------------------------------------
			auto srv = Application::Instance().GetGameSRV();
			if (srv) {
				ImGui::Image((void*)srv, imageSize);
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}
	};

} // namespace Arche

#endif // _DEBUG

#endif // !___GAME_VIEW_PANEL_H___