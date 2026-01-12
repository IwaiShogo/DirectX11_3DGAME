/*****************************************************************//**
 * @file	Input.cpp
 * @brief	入力を管理するクラス
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

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{
	void Input::Initialize()
	{
		// 必要な初期化があれば記述
		ZeroMemory(&s_state, sizeof(XINPUT_STATE));
		ZeroMemory(&s_oldState, sizeof(XINPUT_STATE));
		s_buttonDuration.fill(0.0f);
		GetCursorPos(&s_prevMousePos);	//マウス位置の保存
	}

	void Input::Update()
	{
		// キーボード状態の更新
		memcpy(s_oldKeyState, s_keyState, sizeof(s_keyState));
		GetKeyboardState(s_keyState);

		// 1. 現在の状態を過去に保存
		s_oldState = s_state;

		// 2. 新しい状態を取得
		DWORD result = XInputGetState(0, &s_state);
		s_isConnected = (result == ERROR_SUCCESS);
		if (!s_isConnected)
		{
			ZeroMemory(&s_state, sizeof(XINPUT_STATE));
		}

		// 3. リピートタイマーの更新
		float dt = Time::DeltaTime();
		for (int i = 0; i < (int)Button::MaxCount; ++i)
		{
			if (GetButton((Button)i))
			{
				s_buttonDuration[i] += dt;
			}
			else
			{
				s_buttonDuration[i] = 0.0f;
			}
		}

		// マウス処理
		POINT currentPos;
		GetCursorPos(&currentPos);

		// 差分計算
		s_mouseDeltaX = static_cast<float>(currentPos.x - s_prevMousePos.x);
		s_mouseDeltaY = static_cast<float>(currentPos.y - s_prevMousePos.y);

		s_prevMousePos = currentPos;

		s_mouseWheelDelta = s_wheelAccumulator;
		s_wheelAccumulator = 0.0f;
	}

	float Input::GetAxis(Axis axis)
	{
		float value = 0.0f;

		// 1. キーボード入力
		if (axis == Axis::Horizontal)
		{
			if (GetKey('D') || GetKey(VK_RIGHT)) value += 1.0f;
			if (GetKey('A') || GetKey(VK_LEFT))  value -= 1.0f;
		}
		else if (axis == Axis::Vertical)
		{
			if (GetKey('W') || GetKey(VK_UP))    value += 1.0f;
			if (GetKey('S') || GetKey(VK_DOWN))  value -= 1.0f;
		}

		// 2. コントローラー入力
		if (s_isConnected)
		{
			float padValue = 0.0f;
			float deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

			// 左スティック
			if (axis == Axis::Horizontal)
			{
				padValue = (float)s_state.Gamepad.sThumbLX;
			}
			else if (axis == Axis::Vertical)
			{
				padValue = (float)s_state.Gamepad.sThumbLY;
			}

			// 右スティック
			if (axis == Axis::RightHorizontal)
			{
				padValue = (float)s_state.Gamepad.sThumbRX;
			}
			else if (axis == Axis::RightVertical)
			{
				padValue = (float)s_state.Gamepad.sThumbRY;
			}

			// デッドゾーン処理
			float deadzoneR = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
			// 左スティックと右スティックで分岐
			float currentDeadzone = (axis == Axis::RightHorizontal || axis == Axis::RightVertical) ? deadzoneR : XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
			if (std::abs(padValue) > currentDeadzone)
			{
				float normalized = padValue / 32767.0f;
				if (std::abs(normalized) > std::abs(value))
				{
					value = normalized;
				}
			}
		}

		// -1.0 ~ 1.0 にクランプ
		if (value > 1.0f) value = 1.0f;
		if (value < -1.0f) value = -1.0f;

		return value;
	}

	bool Input::GetKey(int keyCode)
	{
		return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
	}

	float Input::GetMouseDeltaX()
	{
		return s_mouseDeltaX;
	}

	float Input::GetMouseDeltaY()
	{
		return s_mouseDeltaY;
	}

	bool Input::GetMouseRightButton()
	{
		return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	}

	bool Input::GetMouseLeftButton()
	{
		return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	}

	float Input::GetMouseWheel()
	{
		return s_mouseWheelDelta;
	}

	void Input::UpdateMouseWheel(float delta)
	{
		s_wheelAccumulator += delta;
	}

	bool Input::GetKeyDown(int keyCode)
	{
		// 配列買い参照防止
		if (keyCode < 0 || keyCode >= 256) return false;

		// 最上位ビットが1なら押されている
		bool isDown = (s_keyState[keyCode] & 0x80) != 0;
		bool wasDown = (s_oldKeyState[keyCode] & 0x80) != 0;

		// 今回押されていて、前回押されていなければtrue
		return isDown && !wasDown;
	}

	// ----------------------------------------------------------------------
	// 各種判定の実装
	// ----------------------------------------------------------------------
	// 押されているか（Press）
	bool Input::GetButton(Button button)
	{
		// コントローラー
		if (s_isConnected)
		{
			if (s_state.Gamepad.wButtons & GetXInputButtonMask(button)) return true;
		}

		// キーボード
		int key = GetKeyboardKey(button);
		if (key != 0 && (GetAsyncKeyState(key) & 0x8000)) return true;

		return false;
	}

	// 押した瞬間（Trigger）
	bool Input::GetButtonDown(Button button)
	{
		bool isDown = false;

		// コントローラー: 今回ON 且つ 前回OFF
		if (s_isConnected)
		{
			int mask = GetXInputButtonMask(button);
			if ((s_state.Gamepad.wButtons & mask) && !(s_oldState.Gamepad.wButtons & mask))
			{
				isDown = true;
			}
		}

		// キーボード: GetButtonがtrue かつ 前フレームのInput::Update時点で押されていなかったか
		if (!isDown)
		{
			// キーボードのキーが押されているかチェック
			int key = GetKeyboardKey(button);
			if (key != 0 && (GetAsyncKeyState(key) & 0x8000))
			{
				if (s_buttonDuration[(int)button] == 0.0f) isDown = true;
			}
		}

		return isDown;
	}

	// 離した瞬間（Release）
	bool Input::GetButtonUp(Button button)
	{
		// コントローラー: 今回OFF かつ 前回ON
		if (s_isConnected)
		{
			int mask = GetXInputButtonMask(button);
			if (!(s_state.Gamepad.wButtons & mask) && (s_oldState.Gamepad.wButtons & mask))
			{
				return true;
			}
		}
		return false;
	}

	// リピート判定（Repeat）
	bool Input::GetButtonRepeat(Button button)
	{
		// 押した瞬間ならtrue
		if (GetButtonDown(button)) return true;

		// 押しっぱなし時間
		float t = s_buttonDuration[(int)button];

		// 開始時間を超えているか
		if (t > REPEAT_START_TIME)
		{
			// 現在のフレームで「区切り」をまたいだか判定
			float dt = Time::DeltaTime();
			float prevT = t - dt;

			// 剰余（mod）で判定する簡易ロジック
			float checkTime = t - REPEAT_START_TIME;
			float prevCheckTime = prevT - REPEAT_START_TIME;

			int count = (int)(checkTime / REPEAT_INTERVAL);
			int prevCount = (int)(prevCheckTime / REPEAT_INTERVAL);

			if (count > prevCount) return true;
		}

		return false;
	}

	// ----------------------------------------------------------------------
	// マッピングヘルパー
	// ----------------------------------------------------------------------
	int Input::GetXInputButtonMask(Button button)
	{
		switch (button)
		{
		case Button::A: return XINPUT_GAMEPAD_A;
		case Button::B: return XINPUT_GAMEPAD_B;
		case Button::X: return XINPUT_GAMEPAD_X;
		case Button::Y: return XINPUT_GAMEPAD_Y;
		case Button::Start: return XINPUT_GAMEPAD_START;
		case Button::Back: return XINPUT_GAMEPAD_BACK;
		case Button::LShoulder: return XINPUT_GAMEPAD_LEFT_SHOULDER;
		case Button::RShoulder: return XINPUT_GAMEPAD_RIGHT_SHOULDER;
		case Button::Up: return XINPUT_GAMEPAD_DPAD_UP;
		case Button::Down: return XINPUT_GAMEPAD_DPAD_DOWN;
		case Button::Left: return XINPUT_GAMEPAD_DPAD_LEFT;
		case Button::Right: return XINPUT_GAMEPAD_DPAD_RIGHT;
		default: return 0;
		}
	}

	int Input::GetKeyboardKey(Button button)
	{
		switch (button)
		{
		case Button::A: return VK_SPACE;
		case Button::B: return VK_BACK;
		case Button::X: return 'X';
		case Button::Y: return 'Y';
		case Button::Start: return VK_RETURN;
		case Button::Up:    return VK_UP;
		case Button::Down:  return VK_DOWN;
		case Button::Left:  return VK_LEFT;
		case Button::Right: return VK_RIGHT;
		default: return 0;
		}
	}

	bool Input::IsControllerConnected()
	{
		return s_isConnected;
	}

	float Input::ApplyDeadzone(float value, float deadzone)
	{
		if (std::abs(value) < deadzone) return 0.0f;
		return value;
	}

}	// namespace Arche