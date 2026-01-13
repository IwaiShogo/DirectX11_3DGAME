
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include <objbase.h>
#include "Engine/Core/Application.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"

// 関数ポインタの型定義
typedef Arche::Application* (*CreateAppFunc)();

// グローバル変数
HMODULE g_sandboxModule = nullptr;
Arche::Application* g_app = nullptr;
std::filesystem::file_time_type g_lastWriteTime;	// 最終更新日時
int g_reloadCount = 0;	// リロード回数カウンタ
std::filesystem::path g_currentDllPath;

// パス設定
const std::string DLL_NAME = "Sandbox.dll";
const std::string DLL_COPY_NAME = "Sandbox_Loaded.dll";
const std::string PDB_NAME = "Sandbox.pdb";
const std::string PDB_COPY_NAME = "Sandbox_Loaded.pdb";

// ヘルパー: 実行ファイル（EXE）のあるディレクトリパスを取得
// ============================================================
std::filesystem::path GetExeDirectory()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
}

// ゴミファイルのお掃除関数
void CleanUpOldDLLs()
{
	std::filesystem::path exeDir = GetExeDirectory();

	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(exeDir))
		{
			// ファイル名を取得
			std::string filename = entry.path().filename().string();

			// "Sandbox_Loaded_" で始まり、かつ ".dll" または ".pdb" で終わるファイルを探す
			if (filename.find("Sandbox_Loaded_") != std::string::npos)
			{
				if (filename.find(".dll") != std::string::npos || filename.find(".pdb") != std::string::npos)
				{
					// 削除試行 (使用中のファイル等でエラーが出ても無視して続行)
					try {
						std::filesystem::remove(entry.path());
						std::cout << "[Runner] Cleaned up: " << filename << std::endl;
					}
					catch (...) {}
				}
			}
		}
	}
	catch (...) {}
}

// DLLのロード処理
// ============================================================
bool LoadGameDLL()
{
	// 1. 安全のため、古いDLLがあれば解放
	if (g_sandboxModule)
	{
		FreeLibrary(g_sandboxModule);
		g_sandboxModule = nullptr;
	}

	// パスの構築
	std::filesystem::path exeDir = GetExeDirectory();
	std::filesystem::path sourceDll = exeDir / DLL_NAME;
	std::filesystem::path sourcePdb = exeDir / PDB_NAME;

	// コピー先のファイル名を毎回変える
	std::string uniqueSuffix = "_Loaded_" + std::to_string(g_reloadCount++);
	std::filesystem::path copyDll = exeDir / ("Sandbox" + uniqueSuffix + ".dll");
	std::filesystem::path copyPdb = exeDir / ("Sandbox" + uniqueSuffix + ".pdb");

	auto currentWriteTime = std::filesystem::last_write_time(sourceDll);

	// 2. DLLをコピー
	int retries = 0;
	while (retries < 50)
	{
		try {
			// DLLのコピー
			std::filesystem::copy_file(sourceDll, copyDll, std::filesystem::copy_options::overwrite_existing);

			// PDBのコピーは失敗しても無視する (エラーで止めない)
			try {
				if (std::filesystem::exists(sourcePdb)) {
					std::filesystem::copy_file(sourcePdb, copyPdb, std::filesystem::copy_options::overwrite_existing);
				}
			}
			catch (...) {
				std::cout << "[Runner] Warning: PDB copy failed (Ignored)." << std::endl;
			}

			// 成功したら時刻を記録して抜ける
			g_lastWriteTime = std::filesystem::last_write_time(sourceDll);
			break;
		}
		catch (std::exception& e) {
			// 失敗したらちょっと待つ
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			retries++;

			// ログを少し詳細に（デバッグ用）
			if (retries % 10 == 0) {
				std::cout << "[Runner] Waiting for file lock... (" << retries << "/50) " << e.what() << std::endl;
			}
		}
	}

	if (retries == 50) // 50回失敗したら諦める
	{
		std::cout << "[Runner] Failed to copy DLL after retries (File locked?)" << std::endl;
		return false;
	}

	// 3. ロード
	g_sandboxModule = LoadLibraryA(copyDll.string().c_str());
	if (!g_sandboxModule) return false;

	// 4. 関数を取り出す
	CreateAppFunc createApp = (CreateAppFunc)GetProcAddress(g_sandboxModule, "CreateApplication");
	if (!createApp)
	{
		std::cout << "[Runner] Could not find 'CreateApplication' function!" << std::endl;
		return false;
	}

	// 5. アプリ生成
	g_app = createApp();

	g_currentDllPath = copyDll;

	std::cout << "[Runner] Game Loaded Successfully! (" << copyDll.filename() << ")" << std::endl;

	if (g_reloadCount > 1)
	{
		std::cout << "[Runner] Reload #" << (g_reloadCount - 1) << " Completed." << std::endl;
	}

	return true;
}

// DLLのアンロード処理
// ============================================================
void UnloadGameDLL()
{
	if (g_app)
	{
		g_app->SaveState();

		delete g_app;	// アプリの終了処理
		g_app = nullptr;
	}

	if (g_sandboxModule)
	{
		FreeLibrary(g_sandboxModule);	// DLLのロック解除
		g_sandboxModule = nullptr;
	}
	std::cout << "[Runner] Game Unloaded." << std::endl;
}

// ファイル監視チェック
// ============================================================
bool CheckForDllUpdate()
{
	try
	{
		std::filesystem::path dllPath = GetExeDirectory() / DLL_NAME;
		auto currentWriteTime = std::filesystem::last_write_time(dllPath);

		// 時刻が進んでいたら「更新あり」とみなす
		if (currentWriteTime > g_lastWriteTime)
		{
			return true;
		}
	}
	catch (...)
	{
		// ビルド中はファイルにアクセスできないことがあるので無視
	}
	return false;
}

// メインループ
// ============================================================
int main()
{
	// メモリリークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	HWND consoleWindow = GetConsoleWindow();
	MoveWindow(consoleWindow, 100, 100, 600, 300, TRUE);

	CleanUpOldDLLs();

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		std::cerr << "Failed to initialize COM library." << std::endl;
		return -1;
	}

	while (true)
	{
		// 1. DLLのロード（コピー＆ロード）
		// 失敗したらループを抜けて終了
		if (!LoadGameDLL()) {
			break;
		}

		// 2. 監視スレッドの起動（変更検知用）
		std::atomic<bool> stopWatcher = false;
		std::thread watcher([&]() {
			while (!stopWatcher) {
				// F5キー または 自動検知
				if (CheckForDllUpdate() || (GetAsyncKeyState(VK_F5) & 0x8000)) {
					if (g_app) g_app->RequestReload(); // ゲームループを終了させる
					// 連続検知防止
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			});

		// 3. アプリケーション実行
		// RequestReload() が呼ばれると Run() が終了して戻ってくる
		if (g_app) {
			g_app->Run();
		}

		// 4. 監視スレッドの終了待機
		stopWatcher = true;
		if (watcher.joinable()) {
			watcher.join();
		}

		// 5. アプリケーションの破棄
		// DLLを解放する前に、その中のクラスである g_app を削除する必要があります
		bool isReloading = false;
		if (g_app)
		{
			isReloading = g_app->IsReloadRequested();
			delete g_app;
			g_app = nullptr;
		}

		if (isReloading)
		{
			Arche::ComponentRegistry::Destroy();
			Arche::SystemRegistry::Destroy();

			Arche::Application::RegisterEngineResources();
		}

		if (!isReloading) break;

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		// 6. DLLの解放
		// これを行わないと、次の LoadGameDLL で新しいファイルをロードできません
		if (g_sandboxModule) {
			if (!FreeLibrary(g_sandboxModule))
			{
				std::cout << "[Runner] Warning: FreeLibrary failed." << std::endl;
			}
			g_sandboxModule = nullptr;

			// 解放した直後に、そのファイルを削除する
			try {
				if (std::filesystem::exists(g_currentDllPath)) {
					std::filesystem::remove(g_currentDllPath);
				}
				// PDBも削除（Visual Studioが掴んでいて消せないことがあるのでtry-catch必須）
				std::filesystem::path pdbPath = g_currentDllPath;
				pdbPath.replace_extension(".pdb");
				if (std::filesystem::exists(pdbPath)) {
					std::filesystem::remove(pdbPath);
				}
			}
			catch (...) {
				// デバッガがPDBを掴んでいる場合などは削除に失敗するが、動作に支障はないので無視
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		// ループの先頭に戻り、新しいDLLをロードする
	}
	
	CoUninitialize();
	UnloadGameDLL();
	return 0;
}