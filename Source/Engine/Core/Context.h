/*****************************************************************//**
 * @file	Context.h
 * @brief	共有データをまとめる構造体
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/23	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___CONTEXT_H___
#define ___CONTEXT_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/SceneEnvironment.h"

namespace Arche
{
	// エディタの状態
	enum class EditorState
	{
		Edit,	// 編集モード（物理停止、ギズモ有効）
		Play,	// 実行モード（物理稼働）
		Pause	// 一時停止
	};

	// レンダリング用のカメラ情報（追加）
	struct RenderCamera
	{
		XMFLOAT4X4 viewMatrix;
		XMFLOAT4X4 projMatrix;
		XMFLOAT3 position;
		bool useOverride = false;	// true ならこのカメラ情報を使用する
	};

	//  デバッグ設定（全シーン共有）
	struct DebugSettings
	{
		// ビルド構成によってデフォルト値を切り替える定数
#ifdef _DEBUG
		static constexpr bool DefaultOn = true;	 // Debug時は True
#else
		static constexpr bool DefaultOn = false; // Release時は False
#endif

		// 各フラグを定数で初期化
		bool showFps = DefaultOn;
		bool showGrid = DefaultOn;
		bool showAxis = DefaultOn;
		bool showColliders = DefaultOn;
		bool showSoundLocation = DefaultOn;
		bool enableMousePicking = DefaultOn;
		bool useDebugCamera = DefaultOn;

		// 以下の設定はDebug時でも最初はOFFにしておくのが一般的（お好みで DefaultOn にしてもOK）
		bool wireframeMode = false;
		bool showDemoWindow = false;
		bool pauseGame = false;
	};

	// コンテキスト本体
	struct Context
	{
		DebugSettings debugSettings;
		RenderCamera renderCamera;

		EditorState editorState = EditorState::Edit;

		SceneEnvironment environment;
	};

}	// namespace Arche

#endif // !___CONTEXT_H___