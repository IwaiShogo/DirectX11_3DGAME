/*****************************************************************//**
 * @file	BuildSettingsWindow.h
 * @brief	ビルド設定ウィンドウ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___BUILD_SETTINGS_WINDOW_H___
#define ___BUILD_SETTINGS_WINDOW_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Editor/Core/EditorPrefs.h"

namespace Arche
{
	class BuildSettingsWindow : public EditorWindow
	{
	public:
		BuildSettingsWindow()
		{
			m_isOpen = false;
			m_windowName = "Build Settings";
			strcpy_s(m_gameName, "MyGame");
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			ImGui::Text("Game Configuration");
			ImGui::Separator();

			// 1. ゲーム名 (Exe名)
			ImGui::InputText("Game Name", m_gameName, sizeof(m_gameName));

			// 2. スタートシーン
			ImGui::Text("Start Scene:");
			ImGui::SameLine();

			// 入力ボックス (読み取り専用)
			ImGui::PushItemWidth(-1);
			ImGui::InputText("##StartScene", m_startScenePath, sizeof(m_startScenePath), ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();

			// ドラッグ＆ドロップでシーン設定
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					std::string path = (const char*)payload->Data;
					if (path.find(".json") != std::string::npos)
					{
						strcpy_s(m_startScenePath, path.c_str());
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::TextDisabled("(Drag & Drop Scene file here)");

			ImGui::Dummy(ImVec2(0, 20));
			ImGui::Separator();

			// 3. ビルド実行
			if (ImGui::Button("Build & Export", ImVec2(-1, 50)))
			{
				PerformBuild();
			}

			// ステータス表示
			if (!m_statusMessage.empty())
			{
				ImGui::TextColored(m_statusColor, "%s", m_statusMessage.c_str());
			}

			ImGui::End();
		}

	private:
		char m_gameName[64] = "MyGame";
		char m_startScenePath[256] = "Resources/Game/Scenes/GameScene.json";

		std::string m_statusMessage;
		ImVec4 m_statusColor = { 1,1,1,1 };

		// ビルド実行処理
		void PerformBuild()
		{
			namespace fs = std::filesystem;

			// A. 出力先フォルダの決定（今回はプロジェクトルート直下の "Builds/[GameName]" 固定とします）
			// ※本来はファイルダイアログを開くのがベスト
			fs::path projectRoot = fs::current_path();
			fs::path buildDir = projectRoot / "Builds" / m_gameName;

			try
			{
				// フォルダ作成 (既にあったら中身を消すか、上書きするか。今回は上書き)
				if (!fs::exists(buildDir)) fs::create_directories(buildDir);

				// B. バイナリのコピー
				// Releaseビルドのバイナリを探す (x64/Release が一般的)
				fs::path binSrc = projectRoot / "x64" / "Release";

				// もしReleaseがなければ現在の実行ディレクトリを使う（開発ビルドでのエクスポート）
				if (!fs::exists(binSrc / "ArcheRunner.exe"))
				{
					binSrc = fs::current_path(); // 実行中のexeがある場所
					Logger::LogWarning("Release binary not found. Using current binaries.");
				}

				// コピーするバイナリリスト
				std::vector<std::pair<fs::path, std::string>> filesToCopy = {
					{ binSrc / "ArcheRunner.exe", std::string(m_gameName) + ".exe" },
					{ binSrc / "ArcheEngine.dll", "ArcheEngine.dll" },
					{ binSrc / "Sandbox.dll", "Sandbox.dll" },
					{ projectRoot / "Library/Assimp/dll/assimp-vc143-mt.dll", "assimp-vc143-mt.dll" }
				};

				// 他のDLLも必要なら追加 (ImGuiなど)
				if (fs::exists(binSrc / "ImGui.dll")) filesToCopy.push_back({ binSrc / "ImGui.dll", "ImGui.dll" });

				for (const auto& item : filesToCopy)
				{
					fs::path src = item.first;
					fs::path dst = buildDir / item.second;

					if (fs::exists(src))
					{
						fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
					}
					else
					{
						Logger::LogError("Missing file: " + src.string());
					}
				}

				// C. リソースコピー
				fs::path resSrc = projectRoot / "Resources";
				fs::path resDst = buildDir / "Resources";
				if (fs::exists(resDst)) fs::remove_all(resDst);
				fs::copy(resSrc, resDst, fs::copy_options::recursive);

				// D. 設定ファイル生成
				json config;
				config["GameName"] = m_gameName;
				config["StartScene"] = m_startScenePath;
				config["Version"] = "1.0.0";

				std::ofstream out(buildDir / "game_config.json");
				out << config.dump(4);
				out.close();

				m_statusMessage = "Build Successful!";
				m_statusColor = { 0.2f, 1.0f, 0.2f, 1.0f };
				Logger::Log("Build Exported to: " + buildDir.string());

				// フォルダを開く
				ShellExecuteA(NULL, "open", buildDir.string().c_str(), NULL, NULL, SW_SHOW);
			}
			catch (const std::exception& e)
			{
				m_statusMessage = "Build Failed: " + std::string(e.what());
				m_statusColor = { 1.0f, 0.2f, 0.2f, 1.0f };
				Logger::LogError(m_statusMessage);
			}
		}
	};
}

#endif // !___BUILD_SETTINGS_WINDOW_H___