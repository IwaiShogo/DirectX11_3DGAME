/*****************************************************************//**
 * @file	EditorCamera.h
 * @brief	シーンビュー操作用デバッグカメラ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/
#ifndef ___EDITOR_CAMERA_H___
#define ___EDITOR_CAMERA_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Config.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{
	class EditorCamera
	{
	public:
		void Initialize(float fov, float aspect, float nearZ, float farZ)
		{
			m_fov = fov;
			m_aspect = aspect;
			m_nearZ = nearZ;
			m_farZ = farZ;
			UpdateProjection();
			UpdateView();
		}

		void Update()
		{
			// 右クリックしているときだけ操作可能
			if (Input::GetMouseRightButton())
			{
				float dt = Time::DeltaTime();

				// --- 回転 ---
				XMFLOAT2 mouseMove = { Input::GetMouseDeltaX(), Input::GetMouseDeltaY() };
				float rotSpeed = 0.005f;

				m_yaw += mouseMove.x * rotSpeed;
				m_pitch += mouseMove.y * rotSpeed;

				// --- 移動（WASD） ---
				float moveSpeed = 10.0f * dt;
				if (Input::GetKey(VK_SHIFT)) moveSpeed *= 2.0f;	// シフトダッシュ

				XMVECTOR pos = XMLoadFloat3(&m_position);
				XMVECTOR forward = XMLoadFloat3(&m_forward);
				XMVECTOR right = XMLoadFloat3(&m_right);
				XMVECTOR up = XMVectorSet(0, 1, 0, 0);

				if (Input::GetKey('W')) pos += forward * moveSpeed;
				if (Input::GetKey('S')) pos -= forward * moveSpeed;
				if (Input::GetKey('D')) pos += right * moveSpeed;
				if (Input::GetKey('A')) pos -= right * moveSpeed;
				if (Input::GetKey('E')) pos += up * moveSpeed;	   // 上昇
				if (Input::GetKey('Q')) pos -= up * moveSpeed;	   // 下降

				XMStoreFloat3(&m_position, pos);
			}

			// ホイールでズーム
			float wheel = Input::GetMouseWheel();
			if (wheel != 0.0f)
			{
				XMVECTOR pos = XMLoadFloat3(&m_position);
				XMVECTOR forward = XMLoadFloat3(&m_forward);
				pos += forward * wheel * 0.5f;
				XMStoreFloat3(&m_position, pos);
			}

			UpdateView();
		}

		void SetAspect(float aspect)
		{
			if (m_aspect != aspect)
			{
				m_aspect = aspect;
				UpdateProjection();
			}
		}

		void SetPosition(const XMFLOAT3& pos)
		{
			m_position = pos;
			UpdateView();
		}

		void SetRotation(float pitch, float yaw)
		{
			m_pitch = pitch;
			m_yaw = yaw;
			UpdateView();
		}

		void Focus(const XMFLOAT3& focusPoint, float distance = 5.0f)
		{
			// 現在の方向（Forward）を使って、ターゲットの手前に移動する
			XMVECTOR fwd = XMLoadFloat3(&m_forward);
			XMVECTOR target = XMLoadFloat3(&focusPoint);

			// 新しいカメラ位置 = ターゲット位置 - (前方ベクトル * 距離)
			XMVECTOR newPos = target - (fwd * distance);

			XMStoreFloat3(&m_position, newPos);
			UpdateView();
		}

		XMFLOAT3 GetPositionFloat3() const { return m_position; }
		float GetPitch() const { return m_pitch; }
		float GetYaw() const { return m_yaw; }

		XMMATRIX GetViewMatrix() const { return XMLoadFloat4x4(&m_viewMatrix); }
		XMMATRIX GetProjectionMatrix() const { return XMLoadFloat4x4(&m_projMatrix); }
		XMVECTOR GetPosition() const { return XMLoadFloat3(&m_position); }

	private:
		void UpdateView()
		{
			// 回転行列
			XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);

			// 各ベクトル更新
			XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotation);
			XMVECTOR right = XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), rotation);
			XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotation);

			XMStoreFloat3(&m_forward, forward);
			XMStoreFloat3(&m_right, right);
			XMStoreFloat3(&m_up, up);

			// ビュー行列作成
			XMVECTOR pos = XMLoadFloat3(&m_position);
			XMMATRIX view = XMMatrixLookToLH(pos, forward, up);
			XMStoreFloat4x4(&m_viewMatrix, view);
		}

		void UpdateProjection()
		{
			XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);
			XMStoreFloat4x4(&m_projMatrix, proj);
		}

	private:
		XMFLOAT3 m_position = { 0.0f, 2.0f, -5.0f };
		XMFLOAT3 m_forward = { 0.0f, 0.0f, 1.0f };
		XMFLOAT3 m_right = { 1.0f, 0.0f, 0.0f };
		XMFLOAT3 m_up = { 0.0f, 1.0f, 0.0f };

		float m_yaw = 0.0f;
		float m_pitch = 0.0f;

		float m_fov = XM_PIDIV4;
		float m_aspect = (float)Config::SCREEN_WIDTH / (float)Config::SCREEN_HEIGHT;
		float m_nearZ = 0.1f;
		float m_farZ = 1000.0f;

		XMFLOAT4X4 m_viewMatrix;
		XMFLOAT4X4 m_projMatrix;
	};
}

#endif // _DEBUG

#endif // !___EDITOR_CAMERA_H___