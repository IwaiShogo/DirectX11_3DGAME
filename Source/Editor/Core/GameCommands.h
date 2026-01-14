/*****************************************************************//**
 * @file	GameCommands.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/12/26	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___GAME_COMMANDS_H___
#define ___GAME_COMMANDS_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Core/Base/Logger.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Resource/Prefab.h"

namespace Arche
{

	namespace GameCommands
	{
		void RegisterAll(World& world, Context& ctx)
		{
			// =================================================================
			// コンソール操作系
			// =================================================================

			// cls: ログ消去
			Logger::RegisterCommand("cls", [](auto args) {
				Logger::Clear();
				});

			// =================================================================
			// アプリケーション制御系
			// =================================================================

			// reload_script: スクリプト（DLL）のリロード要求
			Logger::RegisterCommand("reload_script", [](auto args) {
				Logger::Log("Requesting Hot Reload...");
				// 現在の状態を保存
				Application::Instance().SaveState();
				// ループを抜けて再起動（main側でハンドリングされている前提）
				Application::Instance().RequestReload();
				});

			// quit: ゲーム終了
			Logger::RegisterCommand("quit", [](auto args) {
				PostQuitMessage(0);
				});

			// fps [value]: FPS制限変更
			Logger::RegisterCommand("fps", [](auto args) {
				if (args.empty()) return;
				int fps = std::stoi(args[0]);
				Time::SetFrameRate(fps);
				Logger::Log("FPS limit set to " + std::to_string(fps));
				});

			// =================================================================
			// シーン操作系
			// =================================================================

			// load_scene [Name]: シーン遷移
			Logger::RegisterCommand("load_scene", [](auto args) {
				if (args.empty()) { Logger::LogWarning("Usage: load_scene [SceneName]"); return; }

				std::string path = args[0];

				// 拡張子がなければ補完
				if (path.find(".json") == std::string::npos) path += ".json";

				// パスが含まれていなければ標準フォルダを探す
				if (path.find("/") == std::string::npos && path.find("\\") == std::string::npos)
				{
					std::string tryPath = "Resources/Game/Scenes/" + path;
					if (std::filesystem::exists(tryPath)) path = tryPath;
				}

				if (std::filesystem::exists(path)) {
					Logger::Log("Loading Scene: " + path);
					SceneManager::Instance().LoadScene(path, new ImmediateTransition());
				}
				else {
					Logger::LogError("Scene file not found: " + path);
				}
				});

			// =================================================================
			// デバッグ表示・エンティティ操作系
			// =================================================================

			// debug [grid/axis/col] [0/1]
			Logger::RegisterCommand("debug", [&](auto args) {
				if (args.size() < 2) { Logger::LogWarning("Usage: debug [grid/axis/col] [0/1]"); return; }
				bool enable = (args[1] == "1" || args[1] == "on");

				if (args[0] == "grid") ctx.debugSettings.showGrid = enable;
				else if (args[0] == "axis") ctx.debugSettings.showAxis = enable;
				else if (args[0] == "col") ctx.debugSettings.showColliders = enable;
				Logger::Log("Debug setting updated.");
				});

			// wireframe [on/off]
			Logger::RegisterCommand("wireframe", [&](auto args) {
				if (args.empty()) ctx.debugSettings.wireframeMode = !ctx.debugSettings.wireframeMode;
				else ctx.debugSettings.wireframeMode = (args[0] == "on" || args[0] == "1");
				Logger::Log("Wireframe: " + std::string(ctx.debugSettings.wireframeMode ? "ON" : "OFF"));
				});

			// list: エンティティ一覧
			Logger::RegisterCommand("list", [&world](auto args) {
				Logger::Log("--- Entity List ---");
				world.getRegistry().view<Tag>().each([&](Entity e, Tag& tag) {
					std::string msg = "ID:" + std::to_string((uint32_t)e) + " [" + tag.name.c_str() + "]";
					Logger::Log(msg);
					});
				Logger::Log("-------------------");
				});

			// tp [x] [y] [z]: プレイヤー移動
			Logger::RegisterCommand("tp", [&world](std::vector<std::string> args) {
				if (args.size() < 3) { Logger::LogWarning("Usage: tp [x] [y] [z]"); return; }
				float x = std::stof(args[0]); float y = std::stof(args[1]); float z = std::stof(args[2]);

				bool found = false;
				world.getRegistry().view<Tag, Transform>().each([&](Entity e, Tag& tag, Transform& t) {
					if (tag.tag == "Player") {
						t.position = { x, y, z };
						if (world.getRegistry().has<Rigidbody>(e)) {
							world.getRegistry().get<Rigidbody>(e).velocity = { 0, 0, 0 };
						}
						found = true;
					}
					});
				if (found) Logger::Log("Teleported Player.");
				else Logger::LogWarning("Player not found.");
				});

			// kill [id/all]
			Logger::RegisterCommand("kill", [&world](auto args) {
				if (args.empty()) return;
				if (args[0] == "all") {
					world.clearEntities(); // IDリセットはしないが全削除
					Logger::Log("Killed all entities.");
				}
				else {
					Entity id = (Entity)std::stoi(args[0]);
					if (world.getRegistry().valid(id)) {
						world.getRegistry().destroy(id);
						Logger::Log("Killed Entity ID: " + args[0]);
					}
					else Logger::LogWarning("Entity not found.");
				}
				});

			// spawn [enemy/sound]
			Logger::RegisterCommand("spawn", [&world](auto args) {
				if (args.empty()) return;
				XMFLOAT3 pos = { 0, 5, 0 };
				world.getRegistry().view<Tag, Transform>().each([&](auto, Tag& t, Transform& tr) {
					if (t.tag == "Player") pos = tr.position;
					});
				pos.y += 3.0f;

				if (args[0] == "enemy") {
					world.create_entity().add<Tag>("Enemy").add<Transform>(pos).add<Collider>();
					Logger::Log("Spawned Enemy");
				}
				else if (args[0] == "sound") {
					// Prefab::CreateSoundEffect(world, "jump", pos); // 必要なら実装
					Logger::Log("Spawned Sound (Mock)");
				}
				});
		}

	}	// namespace GameCommands

}	// namespace Arche

#endif // _DEBUG

#endif // !___GAME_COMMANDS_H___