/*****************************************************************//**
 * @file	Input.h
 * @brief	キーボード、コントローラー操作
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/24	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___INPUT_H___
#define ___INPUT_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	/**
	 * @enum	Axis
	 * @brief	軸の種類
	 */
	enum class Axis
	{
		Horizontal,	// 横方向（AD、矢印左右、左スティックX）
		Vertical,	// 縦方向（WS、矢印上下、左スティックY）
		RightHorizontal,	// 右スティック
		RightVertical,		// 右スティック
	};

	/**
	 * @enum	Button
	 * @brief	ボタンの種類
	 */
	enum class Button
	{
		A, B, X, Y,
		Start, Back,
		LShoulder, RShoulder,
		Up, Down, Left, Right,
		MaxCount	// ボタンの総数確認用
	};

	class ARCHE_API Input
	{
	public:
		// 初期化
		static void Initialize();

		// 毎フレーム呼ぶ
		static void Update();

		// --- 取得関数 ---
		// 軸の値を取得（-1.0f ~ 1.0f）
		// キーボードとコントローラーの値を合成して返します
		static float GetAxis(Axis axis);

		// Press: 押されている間ずっと true
		static bool GetButton(Button button);
		// Trigger: 押した瞬間だけ true
		static bool GetButtonDown(Button button);
		// Release: 離した瞬間だけ true
		static bool GetButtonUp(Button button);
		// Repeat: 押しっぱなしで一定間隔ごとに true
		static bool GetButtonRepeat(Button button);

		// コントローラーが接続されているか
		static bool IsControllerConnected();

		static bool GetKey(int keyCode);

		// マウスの移動量を取得
		static float GetMouseDeltaX();
		static float GetMouseDeltaY();
		// マウスの右ボタン
		static bool GetMouseRightButton();
		// マウスの左ボタン
		static bool GetMouseLeftButton();
		// マウスホイールのスクロール量取得
		static float GetMouseWheel();
		// マウスホイールの更新
		static void UpdateMouseWheel(float delta);

		static bool GetKeyDown(int keyCode);	// キーボード用のDown判定ヘルパー

	private:
		// ボタンのマッピング用ヘルパー
		static int GetXInputButtonMask(Button button);
		static int GetKeyboardKey(Button button);

		// デッドゾーン処理用
		static float ApplyDeadzone(float value, float deadzone);

	private:
		// XInputの状態
		inline static XINPUT_STATE s_state = {};	// 現在のフレーム
		inline static XINPUT_STATE s_oldState = {};	// 1フレーム前
		inline static bool s_isConnected = false;

		// リピート制御用
		// 各ボタンが何秒押され続けているか
		inline static std::array<float, (size_t)Button::MaxCount> s_buttonDuration = { 0 };

		// リピート設定（秒）
		static constexpr float REPEAT_START_TIME = 0.5f;	// 連打開始までの時間
		static constexpr float REPEAT_INTERVAL = 0.1f;		// 連打間隔

		// マウス用
		inline static POINT s_prevMousePos = {};
		inline static float s_mouseDeltaX = 0.0f;
		inline static float s_mouseDeltaY = 0.0f;
		inline static float s_mouseWheelDelta = 0.0f;
		inline static float s_wheelAccumulator = 0.0f;	// 蓄積用

		// キーボード状態保存用
		inline static BYTE s_keyState[256] = {};	// 現在
		inline static BYTE s_oldKeyState[256] = {};	// 1フレーム
	};

}	// namespace Arche

#endif // !___INPUT_H___