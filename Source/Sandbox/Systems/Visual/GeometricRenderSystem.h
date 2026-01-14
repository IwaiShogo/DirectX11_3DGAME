#pragma once

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Renderer/Renderers/PrimitiveRenderer.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class GeometricRenderSystem : public ISystem
	{
	public:
		GeometricRenderSystem()
		{
			m_systemName = "GeometricRenderSystem";
			m_group = SystemGroup::Always;
		}

		void Render(Registry& registry, const Context& context) override
		{
			// --------------------------------------------------------
			// 1. カメラ情報の取得 (描画にはView/Projection行列が必須)
			// --------------------------------------------------------
			XMMATRIX viewMatrix = XMMatrixIdentity();
			XMMATRIX projMatrix = XMMatrixIdentity();
			bool cameraFound = false;

			if (context.renderCamera.useOverride)
			{
				viewMatrix = XMLoadFloat4x4(&context.renderCamera.viewMatrix);
				projMatrix = XMLoadFloat4x4(&context.renderCamera.projMatrix);
				cameraFound = true;
			}
			else
			{
				// アクティブなカメラを探す
				auto view = registry.view<Camera, Transform>();
				for (auto e : view)
				{
					auto& cam = view.get<Camera>(e);
					auto& trans = view.get<Transform>(e);

					// ビュー行列
					XMVECTOR eye = XMLoadFloat3(&trans.position);
					XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(trans.rotation.x, trans.rotation.y, 0.0f);
					XMVECTOR lookDir = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotationMatrix);
					XMVECTOR upDir = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotationMatrix);
					viewMatrix = XMMatrixLookToLH(eye, lookDir, upDir);

					// プロジェクション行列
					projMatrix = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);

					cameraFound = true;
					break; // 1つ見つかればOK
				}
			}

			if (!cameraFound) return;

			PrimitiveRenderer::Begin(viewMatrix, projMatrix);

			// --------------------------------------------------------
			// 2. ジオメトリの描画
			// --------------------------------------------------------
			bool globalWireframe = context.debugSettings.wireframeMode;

			float dt = Time::DeltaTime();
			float unscaleDt = Time::DeltaTime();

			registry.view<GeometricDesign, Transform>().each([&](Entity e, GeometricDesign& geo, Transform& trans) {
				// 1. 演出ロジックの更新
				float timeStep = geo.ignoreTimeScale ? 0.016f : dt;
				geo.timer += timeStep;

				// A. 点滅演出
				bool flashOverride = false;
				if (geo.flashTimer > 0.0f)
				{
					geo.flashTimer -= timeStep;
					int interval = (int)(geo.flashTimer * 20.0f);
					if (interval % 2 == 0) flashOverride = true;
				}

				// B. パルスアニメーション
				float scaleMultiplier = 1.0f;
				if (geo.pulseSpeed > 0.0f)
				{
					float sine = sinf(geo.timer * geo.pulseSpeed);
					scaleMultiplier = 1.0f + (sine * 0.2f);
				}

				// 2. 描画の準備
				XMMATRIX world = trans.GetWorldMatrix();

				// サイズ補正
				XMMATRIX scaleMat = XMMatrixScaling(
					geo.size.x * scaleMultiplier,
					geo.size.y * scaleMultiplier,
					geo.size.z * scaleMultiplier
				);
				world = scaleMat * world;

				// ワイヤーフレーム判定
				bool finalWireframe = (geo.isWireframe ^ flashOverride) || globalWireframe;

				// 3. 形状毎の描画呼び出し
				switch (geo.shapeType)
				{
				case GeoShape::Cube:
					PrimitiveRenderer::DrawBox(world, geo.color, finalWireframe);
					break;
				case GeoShape::Sphere:
					PrimitiveRenderer::DrawSphere(world, geo.color, finalWireframe);
					break;
				case GeoShape::Cylinder:
					PrimitiveRenderer::DrawCylinder(world, geo.color, finalWireframe);
					break;
				case GeoShape::Capsule:
					PrimitiveRenderer::DrawCapsule(world, geo.color, finalWireframe);
					break;
				case GeoShape::Pyramid:
					PrimitiveRenderer::DrawPyramid(world, geo.color, finalWireframe);
					break;
				case GeoShape::Cone:
					PrimitiveRenderer::DrawCone(world, geo.color, finalWireframe);
					break;
				case GeoShape::Torus:
					PrimitiveRenderer::DrawTorus(world, geo.color, finalWireframe);
					break;
				case GeoShape::Diamond:
					PrimitiveRenderer::DrawDiamond(world, geo.color, finalWireframe);
					break;
				}
				});
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::GeometricRenderSystem, "GeometricRenderSystem")
