/*****************************************************************//**
 * @file	Application.cpp
 * @brief	アプリケーション実装（マルチビューレンダリング対応）
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Core/Base/Logger.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"

// Renderer（静的初期化用）
#include "Engine/Renderer/Renderers/PrimitiveRenderer.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Renderer/Renderers/ModelRenderer.h"
#include "Engine/Renderer/Renderers/BillboardRenderer.h"
#include "Engine/Renderer/Renderers/ShadowRenderer.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Engine/Core/Graphics/Graphics.h"

#include "Engine/EngineLoader.h"

#ifdef _DEBUG
#include "Editor/Core/Editor.h"
#include "Editor/Core/GameCommands.h"
#include "Editor/Core/InspectorGui.h"
#include "Editor/Core/EditorPrefs.h"
#include "Editor/Tools/ThumbnailGenerator.h"
#endif // _DEBUG

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Arche
{
	Application* Application::s_instance = nullptr;

	std::wstring ToWideString(const std::string& str)
	{
		if (str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
	}

	HMODULE GetCurrentModuleHandle()
	{
		HMODULE hModule = nullptr;
		GetModuleHandleEx(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCTSTR)GetCurrentModuleHandle, // この関数(Application::Run)のアドレスを使ってDLLを特定
			&hModule);
		return hModule;
	}

	Application::Application(const std::string& title, uint32_t width, uint32_t height)
		: m_title(title), m_width(width), m_height(height)
	{
		if (!s_instance) s_instance = this;

		m_windowClassName = "ArcheEngineWindowClass_" + std::to_string((unsigned long long)this);

		if (!InitializeWindow()) abort();
		if (!InitializeGraphics()) abort();

		// --- サブシステム初期化 ---
		// シーンマネージャー
		new SceneManager();
		SceneManager::Instance().GetWorld().getRegistry().SetParentLookup([&](Entity e) -> Entity
		{
			auto& reg = SceneManager::Instance().GetWorld().getRegistry();
			if (reg.has<Relationship>(e))
			{
				return reg.get<Relationship>(e).parent;
			}
			return NullEntity;
		});

		// 入力
		Input::Initialize();
		// リソースマネージャー
		ResourceManager::Instance().Initialize(m_device.Get());
		// オーディオマネージャー
		AudioManager::Instance().Initialize();
		// FPS制御
		Time::Initialize();
		Time::SetFrameRate(Config::FRAME_RATE);

		// レンダラー静的初期化
		PrimitiveRenderer::Initialize(m_device.Get(), m_context.Get());
		SpriteRenderer::Initialize(m_device.Get(), m_context.Get(), m_width, m_height);
		ModelRenderer::Initialize(m_device.Get(), m_context.Get());
		BillboardRenderer::Initialize(m_device.Get(), m_context.Get());
		ShadowRenderer::Initialize(m_device.Get(), m_context.Get());
		TextRenderer::Initialize(m_device.Get(), m_context.Get());
		Graphics::Initialize(m_device.Get(), m_context.Get(), m_swapChain.Get());

#ifdef _DEBUG
		ThumbnailGenerator::Initialize(m_device.Get(), m_context.Get());

		// --- ImGui & Editor 初期化 ---
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// ドッキング有効化
		ImGui::StyleColorsDark();

		const char* fontPath = "C:\\Windows\\Fonts\\meiryo.ttc";
		if (std::filesystem::exists(fontPath))
		{
			io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
		}
		else
		{
			io.Fonts->AddFontDefault();
		}

		// インスペクターのプレビュー用にゲーム内フォントをロード
		const auto& fontNames = FontManager::Instance().GetLoadedFontNames();
		for (const auto& name : fontNames)
		{
			// 名前からパスを取得 (UTF-8)
			std::string path = FontManager::Instance().GetFontPath(name);

			if (!path.empty())
			{
				// パスオブジェクトを作成
				// u8pathを経由することで、日本語パスも正しく認識させます
				auto fsPath = std::filesystem::u8path(path);

				// 1. まずファイルが存在するかチェック
				if (std::filesystem::exists(fsPath))
				{
					// 2. バイナリモードで開く (ateは外す)
					std::ifstream file(fsPath, std::ios::binary);

					if (file.is_open())
					{
						// 末尾に移動してサイズを取得
						file.seekg(0, std::ios::end);
						std::streamsize size = file.tellg();
						file.seekg(0, std::ios::beg); // 先頭に戻す

						if (size > 0)
						{
							unsigned char* fontData = new unsigned char[(size_t)size];
							if (file.read((char*)fontData, size))
							{
								// プレビュー用なので軽量化のため Default (英数字のみ) でロード
								ImFontConfig config;
								config.FontDataOwnedByAtlas = true;

								ImFont* f = io.Fonts->AddFontFromMemoryTTF(
									fontData,
									(int)size,
									18.0f,
									&config,
									io.Fonts->GetGlyphRangesDefault()
								);

								if (f)
								{
									Arche::g_InspectorFontMap[name] = f;
								}
								else
								{
									// ImGuiでのロード失敗時
									delete[] fontData;
								}
							}
							else
							{
								// 読み込み失敗
								delete[] fontData;
							}
						}
					}
				}
			}
		}

		ImGui_ImplWin32_Init(m_hwnd);
		ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());
		m_isImguiInitialized = true;

		// Editor初期化
		Editor::Instance().Initialize();

		// レンダーターゲット作成（初期サイズはウィンドウサイズ）
		m_sceneRT = std::make_unique<RenderTarget>();
		m_sceneRT->Create(m_device.Get(), m_width, m_height);

		m_gameRT = std::make_unique<RenderTarget>();
		m_gameRT->Create(m_device.Get(), m_width, m_height);
#endif // _DEBUG
	}

	Application::~Application()
	{
		Finalize();
	}

	void Application::Finalize()
	{
#ifdef _DEBUG
		Editor::Instance().Shutdown();
		ThumbnailGenerator::Shutdown();
		if (m_isImguiInitialized)
		{
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}
#endif // _DEBUG
		AudioManager::Instance().Finalize();

		//SystemRegistry::Instance().Clear();
		//ComponentRegistry::Instance().Clear();

		ResourceManager::Instance().Clear();

		// レンダラーの静的リソース開放
		SpriteRenderer::Shutdown();
		BillboardRenderer::Shutdown();
		PrimitiveRenderer::Shutdown();
		ModelRenderer::Shutdown();
		ShadowRenderer::Shutdown();
		TextRenderer::Shutdown();

		SceneManager* sm = &SceneManager::Instance();
		if (sm) delete sm;

		// ウィンドウ破棄
		if (m_hwnd)
		{
			DestroyWindow(m_hwnd);
			m_hwnd = nullptr;
		}

		// クラス登録解除
		std::wstring classNameW = ToWideString(m_windowClassName);
		UnregisterClassW(classNameW.c_str(), GetCurrentModuleHandle());

		s_instance = nullptr;
	}

	void Application::Run()
	{
		SceneManager::Instance().Initialize();

#ifdef _DEBUG
		Context& ctx = SceneManager::Instance().GetContext();
		GameCommands::RegisterAll(SceneManager::Instance().GetWorld(), ctx);
#endif // _DEBUG

		// シーンロードの分岐
		std::string startScene = "Resources/Game/Scenes/GameScene.json";	// デフォルト
		std::string tempPath = "temp_hotreload.json";
		std::string configPath = "game_config.json";

		// パターンA: ホットリロード復帰
		if (std::filesystem::exists(tempPath))
		{
			SceneManager::Instance().LoadSceneAsync(tempPath, new ImmediateTransition());
			std::filesystem::remove(tempPath);
			Logger::Log("HotReload: State Reset (Debug).");
		}
		// パターンB: リリースビルド設定（Confingあり）
		else if (std::filesystem::exists(configPath))
		{
			try
			{
				std::ifstream f(configPath);
				json config;
				f >> config;

				if (config.contains("StartScene"))
				{
					startScene = config["StartScene"].get<std::string>();
				}
			}
			catch (...)
			{
				Logger::LogError("Failed to load game_config.json");
			}

			SceneManager::Instance().LoadSceneAsync(startScene, new ImmediateTransition());
			Logger::Log("Release Build: Loaded " + startScene);
		}
		// パターンC: エディタデフォルト
		else
		{
#ifdef _DEBUG
			std::string lastScene = EditorPrefs::Instance().lastScenePath;
			if (!lastScene.empty() && std::filesystem::exists(lastScene))
			{
				startScene = lastScene;
			}
			SceneManager::Instance().LoadSceneAsync(startScene, new ImmediateTransition());
#endif // _DEBUG
		}

		// 残留メッセージ
		MSG cleanupMsg = {};
		while (PeekMessage(&cleanupMsg, nullptr, 0, 0, PM_REMOVE))
		{
			if (cleanupMsg.message == WM_QUIT) continue;
			TranslateMessage(&cleanupMsg);
			DispatchMessage(&cleanupMsg);
		}

		MSG msg = {};
		while (msg.message != WM_QUIT && !m_reloadRequested)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				// 1. 更新処理
				Update();

				// 2. 描画処理
				Render();

				// 3. フレームレート調整
				Time::WaitFrame();
			}
		}
	}

	void Application::Update()
	{
		// FPS制御
		Time::Update();
		// 入力
		Input::Update();
		// オーディオ
		AudioManager::Instance().Update();
		// リソース（非同期ロード）
		ResourceManager::Instance().Update();

#ifdef _DEBUG
		// ImGui開始
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		if (Editor::Instance().GetMode() == Editor::EditorMode::Prefab)
		{
			World* prefabWorld = Editor::Instance().GetActiveWorld();
			if (prefabWorld)
			{
				prefabWorld->Tick(EditorState::Edit);
			}
		}
		else
		{
			SceneManager::Instance().Update();
		}
#else
		SceneManager::Instance().Update();
#endif // _DEBUG
	}

	// ======================================================================
	// 描画パイプライン（SceneRT -> GameRT -> BackBuffer）
	// ======================================================================
	void Application::Render()
	{
		if (!m_renderTargetView) return;

#ifdef _DEBUG
		// ----------------------------------------------------
		// 1. Scene View Pass (エディタカメラ)
		// ----------------------------------------------------
		if (m_sceneRT)
		{
			// 背景色
			float clearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };	// デフォルト
			// プレファブモード: 青色
			if (Editor::Instance().GetMode() == Editor::EditorMode::Prefab)
			{
				clearColor[0] = 0.1f;
				clearColor[1] = 0.2f;
				clearColor[2] = 0.35f;
				clearColor[3] = 1.0f;
			}

			m_sceneRT->Clear(m_context.Get(), clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
			m_sceneRT->Bind(m_context.Get());

			// デバッグカメラ情報をContextにセット
			Context& ctx = SceneManager::Instance().GetContext();

			// エディタカメラを取得
			EditorCamera& cam = Editor::Instance().GetSceneViewPanel().GetCamera();

			// 行列を保存
			XMStoreFloat4x4(&ctx.renderCamera.viewMatrix, cam.GetViewMatrix());
			XMStoreFloat4x4(&ctx.renderCamera.projMatrix, cam.GetProjectionMatrix());
			XMStoreFloat3(&ctx.renderCamera.position, cam.GetPosition());
			ctx.renderCamera.useOverride = true;

			// 描画対象のワールドを切り替える
			if (Editor::Instance().GetMode() == Editor::EditorMode::Prefab)
			{
				World* activeWorld = Editor::Instance().GetActiveWorld();
				SceneManager::Instance().Render(activeWorld);
			}
			else
			{
				SceneManager::Instance().Render();
			}

			Graphics::SetCaptureSource(m_sceneRT->GetSRV());
		}

		// ----------------------------------------------------
		// 2. Game View Pass（ゲーム内カメラ）
		// ----------------------------------------------------
		if (m_gameRT)
		{
			m_gameRT->Clear(m_context.Get(), 0.0f, 0.0f, 0.0f, 1.0f);	// 黒背景
			m_gameRT->Bind(m_context.Get());

			// ゲームビュー用にContextのデバッグ設定を一時的にOFFにする
			Context& ctx = SceneManager::Instance().GetContext();
			auto backSettings = ctx.debugSettings;	// 設定をバッグアップ

			// ゲームビューではデバッグ表示を無効化
			ctx.debugSettings.showGrid = false;
			ctx.debugSettings.showAxis = false;
			ctx.debugSettings.showColliders = false;
			ctx.debugSettings.showSoundLocation = false;
			ctx.debugSettings.useDebugCamera = false;
			ctx.renderCamera.useOverride = false;

			SceneManager::Instance().Render();

			Graphics::SetCaptureSource(m_gameRT->GetSRV());

			// 設定を元に戻す
			ctx.debugSettings = backSettings;
		}

		// ----------------------------------------------------
		// 3. ImGui Pass (BackBuffer)
		// ----------------------------------------------------
		// レンダーターゲットをバックバッファに戻す
		m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

		// バックバッファクリア
		static float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
		m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		
		// エディタUI構築 & 描画
		Context& ctx = SceneManager::Instance().GetContext();
		Editor::Instance().Draw(SceneManager::Instance().GetWorld(), ctx);
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
#else
		// ====================================================
		// Release Mode (Game Only)
		// ====================================================
		m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

		float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_context->ClearRenderTargetView(m_renderTargetView.Get(), color);
		m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		Graphics::SetCaptureSource(nullptr);

		// ゲーム画面描画
		SceneManager::Instance().Render();
#endif // _DEBUG

		// フリップ
		m_swapChain->Present(Config::VSYNC_ENABLED ? 1 : 0, 0);
	}

#ifdef _DEBUG
	void Application::ResizeSceneRenderTarget(float width, float height)
	{
		if (m_sceneRT && (width > 0 && height > 0))
		{
			// サイズが変わった時だけ再生成などの処理を入れる
			// 現在のRenderTargetクラスの実装に合わせて調整してくだい
			m_sceneRT->Create(m_device.Get(), (uint32_t)width, (uint32_t)height);

			TextRenderer::ClearCache();
		}
	}

	void Application::ResizeGameRenderTarget(float width, float height)
	{
		if (m_gameRT && (width > 0 && height > 0))
		{
			m_gameRT->Create(m_device.Get(), (uint32_t)width, (uint32_t)height);

			TextRenderer::ClearCache();
		}
	}
#endif // _DEBUG

	void Application::SaveState()
	{
		SceneSerializer::SaveScene(SceneManager::Instance().GetWorld(), "temp_hotreload.json");
		Logger::Log("HotReload: State Saved.");
	}

	void Application::LoadState()
	{
		std::string tempPath = "temp_hotreload.json";
		std::ifstream f(tempPath);
		if (f.good())
		{
			f.close();
			// 現在のシーンをクリアしてからロード
			SceneSerializer::LoadScene(SceneManager::Instance().GetWorld(), tempPath);
			Logger::Log("HotReload: State Loaded.");
		}
	}

	// ======================================================================
	// ウィンドウ関連の実装
	// ======================================================================
	bool Application::InitializeWindow()
	{
		HINSTANCE hInstance = GetCurrentModuleHandle();

		WNDCLASSW wc = {};
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = hInstance;
		std::wstring classNameW = ToWideString(m_windowClassName);
		wc.lpszClassName = classNameW.c_str();
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		RegisterClassW(&wc);

		RECT rc = { 0, 0, (LONG)m_width, (LONG)m_height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		// ウィンドウ作成
		std::wstring titleW = ToWideString(m_title);
		m_hwnd = CreateWindowExW(
			0, classNameW.c_str(), titleW.c_str(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top,
			nullptr, nullptr, wc.hInstance, nullptr
		);

		if (!m_hwnd) return false;

		ShowWindow(m_hwnd, SW_SHOW);

		// 最前面
		SetForegroundWindow(m_hwnd);
		SetFocus(m_hwnd);

#ifdef _DEBUG
		// エディタモード時はD&Dを受け付ける
		DragAcceptFiles(m_hwnd, TRUE);
#endif // _DEBUG

		return true;
	}

	bool Application::InitializeGraphics()
	{
		DXGI_SWAP_CHAIN_DESC scd = {};
		scd.BufferCount = 1;
		scd.BufferDesc.Width = m_width;
		scd.BufferDesc.Height = m_height;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = m_hwnd;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.Windowed = TRUE;

		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL featureLevel;

		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
			featureLevels, 1, D3D11_SDK_VERSION, &scd,
			&m_swapChain, &m_device, &featureLevel, &m_context
		);

		if (FAILED(hr)) return false;

		// RTV作成
		ComPtr<ID3D11Texture2D> backBuffer;
		m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
		m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);

		// DSV作成
		D3D11_TEXTURE2D_DESC dsd = {};
		dsd.Width = m_width;
		dsd.Height = m_height;
		dsd.MipLevels = 1;
		dsd.ArraySize = 1;
		dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsd.SampleDesc.Count = 1;
		dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ComPtr<ID3D11Texture2D> depthBuffer;
		m_device->CreateTexture2D(&dsd, nullptr, &depthBuffer);
		m_device->CreateDepthStencilView(depthBuffer.Get(), nullptr, &m_depthStencilView);

		// ビューポート
		D3D11_VIEWPORT vp = {};
		vp.Width = (float)m_width;
		vp.Height = (float)m_height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &vp);

		return true;
	}

	void Application::Resize(uint32_t width, uint32_t height)
	{
		// サイズが0または変更がない場合は無視
		if (width == 0 || height == 0) return;
		if (m_width == width && m_height == height) return;

		m_width = width;
		m_height = height;

		// 1. 既存の描画ターゲットへの参照を解放
		m_context->OMSetRenderTargets(0, nullptr, nullptr);
		m_renderTargetView.Reset();
		m_depthStencilView.Reset();
		m_context->ClearState();

		// 2. スワップチェーンのバッファサイズ変更
		HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		if (FAILED(hr))
		{
			Logger::LogError("Failed to resize swap chain buffers.");
			return;
		}

		// 3. RTV（レンダーターゲットビュー）の再作成
		ComPtr<ID3D11Texture2D> backBuffer;
		hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
		if (FAILED(hr)) return;

		hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
		if (FAILED(hr)) return;

		// 4. DSV（深度ステンシルビュー）の再作成
		D3D11_TEXTURE2D_DESC dsd = {};
		dsd.Width = width;
		dsd.Height = height;
		dsd.MipLevels = 1;
		dsd.ArraySize = 1;
		dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsd.SampleDesc.Count = 1;
		dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ComPtr<ID3D11Texture2D> depthBuffer;
		m_device->CreateTexture2D(&dsd, nullptr, &depthBuffer);
		m_device->CreateDepthStencilView(depthBuffer.Get(), nullptr, &m_depthStencilView);

		// 5. ビューポートの更新
		D3D11_VIEWPORT vp = {};
		vp.Width = (float)width;
		vp.Height = (float)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &vp);
	}

	LRESULT CALLBACK Application::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
#ifdef _DEBUG
		// ImGui用ハンドラ
		if (s_instance && s_instance->m_isImguiInitialized)
		{
			extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
			if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
		}
#endif // _DEBUG

		switch (msg)
		{
		case WM_SIZE:
			if (s_instance && wParam != SIZE_MINIMIZED)
			{
				// LOWORD, HIWORDマクロで新しい幅と高さを取得
				uint32_t width = (uint32_t)LOWORD(lParam);
				uint32_t height = (uint32_t)HIWORD(lParam);

				// リサイズ実行
				s_instance->Resize(width, height);
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_MOUSEWHEEL:
		{
			float wheelDelta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
			Input::UpdateMouseWheel(wheelDelta);
			return 0;
		}

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE) PostQuitMessage(0);
			return 0;

#ifdef _DEBUG
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;

			// 現在のContentBrowserのパスを取得（未設定ならResources）
			std::string destDir = EditorPrefs::Instance().contentBrowserPath;
			if (destDir.empty()) destDir = "Resources";

			// ドロップされたファイル数を取得
			UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);
			char filePath[MAX_PATH];

			int successCount = 0;

			for (UINT i = 0; i < fileCount; ++i)
			{
				if (DragQueryFileA(hDrop, i, filePath, MAX_PATH))
				{
					try
					{
						std::filesystem::path srcPath = filePath;
						// 移動先パス = 現在のディレクトリ + ファイル名
						std::filesystem::path dstPath = std::filesystem::path(destDir) / srcPath.filename();

						// フォルダの場合は除外（今回はファイルのみ対応）
						if (!std::filesystem::is_directory(srcPath))
						{
							// コピー実行 (同名ファイルは上書き)
							std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::overwrite_existing);
							successCount++;
						}
					}
					catch (const std::filesystem::filesystem_error& e)
					{
						Logger::LogError("File Import Error: " + std::string(e.what()));
					}
				}
			}

			if (successCount > 0)
			{
				Logger::Log("Imported " + std::to_string(successCount) + " files to " + destDir);
			}

			DragFinish(hDrop);
			return 0;
		}
#endif // _DEBUG

		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

}	// namespace Arche