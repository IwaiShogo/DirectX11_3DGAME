/*****************************************************************//**
 * @file	EditorPrefs.h
 * @brief	エディタの状態を保存する
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___EDITOR_PREFS_H___
#define ___EDITOR_PREFS_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	class EditorPrefs
	{
	public:
		// シングルトン
		static EditorPrefs& Instance() { static EditorPrefs s; return s; }

		// --- 保存するデータ ---
		std::string lastScenePath = "";
		std::string contentBrowserPath = "Resources";

		struct CameraData {
			float x = 0.0f, y = 0.0f, z = -10.0f;
			float pitch = 0.0f, yaw = 0.0f;
		} camera;

		// --- 読み込み ---
		void Load()
		{
			if (!std::filesystem::exists("EditorConfig.json")) return;

			std::ifstream i("EditorConfig.json");
			if (!i.is_open()) return;

			json j;
			i >> j;

			if (j.contains("LastScene")) lastScenePath = j["LastScene"];
			if (j.contains("ContentBrowserPath")) contentBrowserPath = j["ContentBrowserPath"];

			if (j.contains("Camera")) {
				auto& c = j["Camera"];
				camera.x = c.value("x", 0.0f);
				camera.y = c.value("y", 0.0f);
				camera.z = c.value("z", -10.0f);
				camera.pitch = c.value("pitch", 0.0f);
				camera.yaw = c.value("yaw", 0.0f);
			}
		}

		// --- 保存 ---
		void Save()
		{
			json j;
			j["LastScene"] = lastScenePath;
			j["ContentBrowserPath"] = contentBrowserPath;

			j["Camera"]["x"] = camera.x;
			j["Camera"]["y"] = camera.y;
			j["Camera"]["z"] = camera.z;
			j["Camera"]["pitch"] = camera.pitch;
			j["Camera"]["yaw"] = camera.yaw;

			std::ofstream o("EditorConfig.json");
			o << j.dump(4);
		}

	private:
		EditorPrefs() { Load(); }
		~EditorPrefs() { Save(); }
	};
}

#endif // !___EDITOR_PREFS_H___