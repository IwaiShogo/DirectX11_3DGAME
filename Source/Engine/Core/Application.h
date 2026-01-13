/*****************************************************************//**
 * @file	Application.h
 * @brief	アプリケーションの基盤クラス（SceneView / GameView 対応）
 * 
 * @details	
 * ウィンドウ生成、メインループ、エンジンの初期化・終了を管理します。
 * クライアントはこのクラスを継承してゲーム固有の設定を行います。
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___APPLICATION_H___
#define ___APPLICATION_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Config.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Core/Time/Time.h"

// Debug時のみレンダーターゲットを使用
#ifdef _DEBUG
#include "Engine/Renderer/Core/RenderTarget.h"
#endif // _DEBUG

// ===== 前方宣言 =====
class Input;

namespace Arche
{
	/**
	 * @class	Application
	 * @brief	エンジンの中核となるアプリケーションクラス
	 */
	class ARCHE_API Application
	{
	public:
		/**
		 * @brief	コンストラクタ
		 * @param	title	ウィンドウタイトル
		 * @param	width	画面幅
		 * @param	height	画面高さ
		 */
		Application(const std::string& title = Config::WINDOW_TITLE, uint32_t width = Config::SCREEN_WIDTH, uint32_t height = Config::SCREEN_HEIGHT);

		/**
		 * @brief	デストラクタ
		 */
		virtual ~Application();

		/**
		 * @brief	アプリケーションの実行（メインループ開始）
		 */
		void Run();

		static void RegisterEngineResources();

		// --- アクセサ ---

		/**
		 * @brief	ウィンドウハンドルの取得
		 */
		HWND GetWindowHandle() const { return m_hwnd; }

		/**
		 * @brief	シングルトンインスタンスの取得
		 */
		static Application& Instance() { return *s_instance; }

		/**
		 * @brief	デバイスの取得
		 * @return	ID3D11Device*
		 */
		ID3D11Device* GetDevice() const { return m_device.Get(); }

		/**
		 * @brief	コンテキストの取得
		 * @return ID3D11DeviceContext*
		 */
		ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

		// --- エディタ用機能（Debugのみ） ---
#ifdef _DEBUG
		/**
		 * @brief	シーンビュー（エディタ視点）のテクスチャ描画
		 * @return	ID3D11ShaderResourceView*
		 */
		ID3D11ShaderResourceView* GetSceneSRV() const { return m_sceneRT ? m_sceneRT->GetSRV() : nullptr; }

		/**
		 * @brief	ゲームビュー（プレイヤー視点）のテクスチャ描画
		 * @return	ID3D11ShaderResourceView*
		 */
		ID3D11ShaderResourceView* GetGameSRV() const { return m_gameRT ? m_gameRT->GetSRV() : nullptr; }

		/**
		 * @brief	シーンビュー（エディタ視点）RTのリサイズ
		 * @param	width	画面幅
		 * @param	height	画面高さ
		 */
		void ResizeSceneRenderTarget(float width, float height);

		/**
		 * @brief	ゲームビュー（プレイヤー視点）RTのリサイズ
		 * @param	width	画面幅
		 * @param	height	画面高さ
		 */
		void ResizeGameRenderTarget(float width, float height);
#endif // _DEBUG
		
		// リロードが必要かどうか確認するフラグ
		bool IsReloadRequested() const { return m_reloadRequested; }

		// 外部からリロードを要求する関数
		void RequestReload() { m_reloadRequested = true; }

		// ホットリロード用の状態保存・復元
		virtual void SaveState();
		virtual void LoadState();

	private:
		// 内部初期化
		bool InitializeWindow();
		bool InitializeGraphics();

		// DirectXリソースのリサイズ処理
		void Resize(uint32_t width, uint32_t height);

		// 更新処理 / 描画処理
		void Update();
		void Render();

		// 終了処理
		void Finalize();

		// ウィンドウプロシージャ（静的メンバ）
		static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		// シングルトンポインタ
		static Application* s_instance;

		// ウィンドウ設定
		std::string m_title;
		std::string m_windowClassName;
		uint32_t m_width;
		uint32_t m_height;
		HWND m_hwnd = nullptr;

		// DirectX 11 Resources
		ComPtr<ID3D11Device>			m_device;
		ComPtr<ID3D11DeviceContext>		m_context;
		ComPtr<IDXGISwapChain>			m_swapChain;
		ComPtr<ID3D11RenderTargetView>	m_renderTargetView;	// バックバッファ（Release用）
		ComPtr<ID3D11DepthStencilView>	m_depthStencilView;	// 深度バッファ

#ifdef _DEBUG
		// エディタ用レンダーターゲット
		std::unique_ptr<RenderTarget> m_sceneRT;	// Scene View用
		std::unique_ptr<RenderTarget> m_gameRT;		// Game View用

		bool m_isImguiInitialized = false;
#endif // _DEBUG

		bool m_reloadRequested = false;
	};

	/**
	 * @brief	クライアント定義関数
	 * @return	Application*
	 */
	Application* CreateApplication();

}// namespace Arche

#endif // !___APPLICATION_H___