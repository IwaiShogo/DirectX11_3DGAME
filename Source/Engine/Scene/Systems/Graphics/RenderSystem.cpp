/*****************************************************************//**
 * @file	RenderSystem.cpp
 * @brief	描画処理
 *********************************************************************/

 // ===== インクルード =====
#include "Engine/pch.h"
#define NOMINMAX
#include "Engine/Scene/Systems/Graphics/RenderSystem.h"
#include "Engine/Audio/AudioManager.h"

#include "Engine/Renderer/Renderers/PrimitiveRenderer.h"
#include "Engine/Renderer/Renderers/ModelRenderer.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Renderer/Renderers/BillboardRenderer.h"

namespace Arche
{
	RenderSystem::RenderSystem()
	{
		m_systemName = "Render System";
	}

	void RenderSystem::Render(Registry& registry, const Context& context)
	{
		if (!m_isInitialized)
		{
			m_gridRenderer.Initialize();
			m_skyboxRenderer.Initialize();
			m_isInitialized = true;
		}

		// ------------------------------------------------------------
		// 1. メインの描画
		// ------------------------------------------------------------

		// 1. カメラ探索
		XMMATRIX viewMatrix = XMMatrixIdentity();
		XMMATRIX projMatrix = XMMatrixIdentity();
		XMVECTOR eye = XMVectorZero();
		static XMFLOAT3 savedRotation = { 0, 0, 0 };
		bool cameraFound = false;

		if (context.renderCamera.useOverride)
		{
			viewMatrix = XMLoadFloat4x4(&context.renderCamera.viewMatrix);
			projMatrix = XMLoadFloat4x4(&context.renderCamera.projMatrix);
			eye = XMLoadFloat3(&context.renderCamera.position);
			cameraFound = true;
		}
		else
		{
			registry.view<Camera, Transform>().each([&](Entity e, Camera& cam, Transform& trans)
				{
					if (cameraFound) return;

					savedRotation = trans.rotation;

					eye = XMLoadFloat3(&trans.position);
					// 回転行列を作成 (Pitch: X軸回転, Yaw: Y軸回転)
					XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(trans.rotation.x, trans.rotation.y, 0.0f);
					// 前方ベクトル (0, 0, 1) を回転させる
					XMVECTOR lookDir = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotationMatrix);
					// 上方向ベクトル (0, 1, 0) を回転させる
					XMVECTOR upDir = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotationMatrix);

					// LookToLH: 位置、向き、上でビュー行列を作る
					viewMatrix = XMMatrixLookToLH(eye, lookDir, upDir);
					projMatrix = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);
					cameraFound = true;
				});
		}

		// スカイボックスのテクスチャロード管理
		static std::string currentSkyboxPath = "INITIAL_STATE";

		if (context.environment.skyboxTexturePath != currentSkyboxPath)
		{
			if (context.environment.skyboxTexturePath.empty())
			{
				m_skyboxRenderer.SetTexture(nullptr);
			}
			else
			{
				auto tex = ResourceManager::Instance().GetTexture(context.environment.skyboxTexturePath);
				if (tex) m_skyboxRenderer.SetTexture(tex);
			}
			currentSkyboxPath = context.environment.skyboxTexturePath;
		}

		if (!cameraFound) return;

		// スカイボックス描画
		m_skyboxRenderer.Render(viewMatrix, projMatrix, context.environment);

		// グリッドと軸の描画
		if (context.debugSettings.showGrid)
		{
			XMVECTOR camPosVec = XMMatrixInverse(nullptr, viewMatrix).r[3];
			XMFLOAT3 camPos;
			XMStoreFloat3(&camPos, camPosVec);

			m_gridRenderer.Render(viewMatrix, projMatrix, camPos, 1000.0f);
		}

		// メインシーン描画開始
		PrimitiveRenderer::Begin(viewMatrix, projMatrix);

		if (context.debugSettings.showAxis) PrimitiveRenderer::DrawAxis();

		// 3. コライダー描画 (ワイヤーフレームフラグを渡すように修正)
		if (context.debugSettings.showColliders)
		{
			// ※PrimitiveRenderer::Draw** の第5引数(wireframe)に設定を渡すことで制御します
			bool isWire = context.debugSettings.wireframeMode;

			registry.view<Transform, Collider>().each([&](Entity e, Transform& t, Collider& c)
				{
					XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
					if (registry.has<Tag>(e))
					{
						std::string name = registry.get<Tag>(e).tag.c_str();
						if (name == "Player") color = { 0.0f, 1.0f, 0.0f, 1.0f };
						if (name == "Enemy") color = { 1.0f, 0.0f, 0.0f, 1.0f };
					}

					// ワールド座標計算
					XMVECTOR scale, rot, pos;
					XMMatrixDecompose(&scale, &rot, &pos, t.GetWorldMatrix());

					XMFLOAT3 gPos, gScale;
					XMStoreFloat3(&gPos, pos);
					XMStoreFloat3(&gScale, scale);
					XMFLOAT4 gRot;
					XMStoreFloat4(&gRot, rot);

					// オフセット運用
					XMVECTOR offsetVec = XMLoadFloat3(&c.offset);
					XMVECTOR centerVec = XMVector3Transform(offsetVec, t.GetWorldMatrix());
					XMFLOAT3 center;
					XMStoreFloat3(&center, centerVec);

					// タイプ別描画 (修正: 最後の引数に isWire を追加)
					if (c.type == ColliderType::Box)
					{
						XMFLOAT3 size = { c.boxSize.x * gScale.x, c.boxSize.y * gScale.y, c.boxSize.z * gScale.z };
						PrimitiveRenderer::DrawBox(center, size, gRot, color, isWire);
					}
					else if (c.type == ColliderType::Sphere)
					{
						// スケールの最大値を半径に適用
						float maxScale = std::max({ gScale.x, gScale.y, gScale.z });
						PrimitiveRenderer::DrawSphere(center, c.radius * maxScale, color, isWire);
					}
					else if (c.type == ColliderType::Capsule) {
						// 半径: スケールのXZ最大値, 高さ: Yスケール
						float maxScaleXZ = std::max(gScale.x, gScale.z);
						PrimitiveRenderer::DrawCapsule(center, c.radius * maxScaleXZ, c.height * gScale.y, gRot, color, isWire);
					}
					else if (c.type == ColliderType::Cylinder) {
						float maxScaleXZ = std::max(gScale.x, gScale.z);
						PrimitiveRenderer::DrawCylinder(center, c.radius * maxScaleXZ, c.height * gScale.y, gRot, color, isWire);
					}
				});

			registry.view<Transform, PointLight>().each([&](Entity e, Transform& t, PointLight& l) {
				// 光源の中心
				XMFLOAT4 iconColor = { 1.0f, 1.0f, 0.3f, 1.0f };
				PrimitiveRenderer::DrawSphere(t.position, 0.2f, iconColor, false); // 中心は常にソリッド

				// 範囲
				XMFLOAT4 rangeColor = { 1.0f, 1.0f, 0.3f, 0.3f };
				PrimitiveRenderer::DrawSphere(t.position, l.range, rangeColor, true); // 範囲は常にワイヤー
				});
		}

		// -------------------------------------------------------
		// 音源の可視化 (Billboard版)
		// -------------------------------------------------------
		if (context.debugSettings.showSoundLocation) {

			BillboardRenderer::Begin(viewMatrix, projMatrix);

			auto iconTex = ResourceManager::Instance().GetTexture("star");
			if (!iconTex) iconTex = ResourceManager::Instance().GetTexture("star");

			if (iconTex) {
				for (const auto& evt : AudioManager::Instance().GetSoundEvents()) {
					BillboardRenderer::Draw(
						iconTex.get(),
						evt.position,
						1.0f, 1.0f,
						{ 1.0f, 0.5f, 0.5f, 1.0f }
					);
				}
			}
			PrimitiveRenderer::GetDeviceContext()->OMSetBlendState(nullptr, nullptr, 0xffffffff);
		}

		// ------------------------------------------------------------
		// 2. シーンギズモの描画
		// ------------------------------------------------------------
		if (context.debugSettings.showAxis)
		{
			UINT numViewports = 1;
			D3D11_VIEWPORT oldViewport;
			PrimitiveRenderer::GetDeviceContext()->RSGetViewports(&numViewports, &oldViewport);

			float gizmoSize = 100.0f;
			float padding = 20.0f;

			D3D11_VIEWPORT gizmoViewport = {};
			gizmoViewport.Width = gizmoSize;
			gizmoViewport.Height = gizmoSize;
			gizmoViewport.MinDepth = 0.0f;
			gizmoViewport.MaxDepth = 1.0f;
			gizmoViewport.TopLeftX = oldViewport.Width - gizmoSize - padding;
			gizmoViewport.TopLeftY = padding;

			PrimitiveRenderer::GetDeviceContext()->RSSetViewports(1, &gizmoViewport);

			XMMATRIX gizmoRotMatrix = XMMatrixRotationRollPitchYaw(savedRotation.x, savedRotation.y, 0.0f);
			XMVECTOR offset = XMVector3TransformCoord(XMVectorSet(0, 0, -5.0f, 0), gizmoRotMatrix);
			XMVECTOR gizmoEye = offset;
			XMVECTOR gizmoTarget = XMVectorSet(0, 0, 0, 0);
			XMVECTOR gizmoUp = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), gizmoRotMatrix);
			XMMATRIX gizmoView = XMMatrixLookAtLH(gizmoEye, gizmoTarget, gizmoUp);
			XMMATRIX gizmoProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1.0f, 0.1f, 100.0f);

			PrimitiveRenderer::Begin(gizmoView, gizmoProj);

			// ギズモは常にソリッドで描画
			PrimitiveRenderer::SetFillMode(false);

			float len = 1.5f;
			PrimitiveRenderer::DrawArrow(XMFLOAT3(0, 0, 0), XMFLOAT3(len, 0, 0), XMFLOAT4(1, 0, 0, 1));
			PrimitiveRenderer::DrawArrow(XMFLOAT3(0, 0, 0), XMFLOAT3(0, len, 0), XMFLOAT4(0, 1, 0, 1));
			PrimitiveRenderer::DrawArrow(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, len), XMFLOAT4(0, 0, 1, 1));

			PrimitiveRenderer::DrawBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.7f, 0.7f, 0.7f), XMFLOAT4(0, 0, 0, 0), XMFLOAT4(0.8f, 0.8f, 0.8f, 1), false);

			// ビューポートを元に戻す
			PrimitiveRenderer::GetDeviceContext()->RSSetViewports(1, &oldViewport);
		}
	}

}	// namespace Arche