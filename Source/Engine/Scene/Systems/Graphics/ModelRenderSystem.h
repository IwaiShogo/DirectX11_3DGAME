/*****************************************************************//**
 * @file	ModelRenderSystem.h
 * @brief	ModelRendererを使って描画を行うシステム
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___MODEL_RENDER_SYSTEM_H___
#define ___MODEL_RENDER_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Renderer/Renderers/ModelRenderer.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Renderer/Core/ShadowMap.h"
#include "Engine/Renderer/Renderers/ShadowRenderer.h"

namespace Arche
{

	class ModelRenderSystem : public ISystem
	{
	public:
		ModelRenderSystem()
		{
			m_systemName = "Model Render System";
		}

		void Render(Registry& registry, const Context& context) override
		{
			// ---------------------------------------------------------
			// 0. 初期化
			// ---------------------------------------------------------
			if (!m_isShadowInit)
			{
				// 解像度は高めに設定 (2048 or 4096)
				m_shadowMap.Initialize(Application::Instance().GetDevice(), 2048, 2048);
				m_isShadowInit = true;
			}

			// ---------------------------------------------------------
			// 1. カメラ探索
			// ---------------------------------------------------------
			XMMATRIX viewMatrix = XMMatrixIdentity();
			XMMATRIX projMatrix = XMMatrixIdentity();
			XMVECTOR eye = XMVectorZero();
			// 影を落とすライトの向き（仮固定）
			XMFLOAT3 lightDir = { 0.5f, -1.0f, 0.5f };
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

						eye = XMLoadFloat3(&trans.position);
						XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(trans.rotation.x, trans.rotation.y, 0.0f);
						XMVECTOR lookDir = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotationMatrix);
						XMVECTOR upDir = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotationMatrix);

						viewMatrix = XMMatrixLookToLH(eye, lookDir, upDir);
						projMatrix = XMMatrixPerspectiveFovLH(cam.fov, cam.aspect, cam.nearZ, cam.farZ);
						cameraFound = true;
					});
			}

			if (!cameraFound) return;

			// ---------------------------------------------------------
			// 2. ライト情報の収集 (Point Light)
			// ---------------------------------------------------------
			std::vector<ModelRenderer::PointLightData> pointLights;

			registry.view<Transform, PointLight>().each([&](Entity e, Transform& t, PointLight& l) {
				ModelRenderer::PointLightData data;
				data.position = t.position;
				data.range = l.range;
				data.color = l.color;
				data.intensity = l.intensity;

				pointLights.push_back(data);
				});

			ModelRenderer::SetSceneLights(
				context.environment.ambientColor,
				context.environment.ambientIntensity,
				pointLights
			);

			// ---------------------------------------------------------
			// 3. 影生成パス (Shadow Pass)
			// ---------------------------------------------------------
			ID3D11DeviceContext* devContext = Application::Instance().GetContext();

			// 現在のレンダリングターゲットとビューポートを保存
			ID3D11RenderTargetView* prevRTV = nullptr;
			ID3D11DepthStencilView* prevDSV = nullptr;
			devContext->OMGetRenderTargets(1, &prevRTV, &prevDSV);

			D3D11_VIEWPORT prevVP;
			UINT numVP = 1;
			devContext->RSGetViewports(&numVP, &prevVP);

			// A. ライト行列計算
			XMVECTOR vLightDir = XMLoadFloat3(&lightDir);
			XMVECTOR lightPos = XMVectorScale(XMVector3Normalize(vLightDir), -20.0f);
			XMVECTOR target = XMVectorSet(0, 0, 0, 0);
			XMVECTOR up = XMVectorSet(0, 1, 0, 0);

			XMMATRIX lightView = XMMatrixLookAtLH(lightPos, target, up);
			XMMATRIX lightProj = XMMatrixOrthographicLH(40.0f, 40.0f, 1.0f, 100.0f);

			// 影描画を始める前に、読み込みスロット(t1)から影マップを外す
			ID3D11ShaderResourceView* nullSRV = nullptr;
			devContext->PSSetShaderResources(1, 1, &nullSRV);

			// B. 影マップへ描画
			m_shadowMap.Begin(devContext);
			ShadowRenderer::Begin(lightView, lightProj);

			registry.view<MeshComponent, Transform>().each([&](Entity e, MeshComponent& m, Transform& t)
				{
					if (!m.pModel && !m.modelKey.empty()) m.pModel = ResourceManager::Instance().GetModel(m.modelKey);
					if (m.pModel)
					{
						XMMATRIX world = t.GetWorldMatrix();
						if (m.scaleOffset.x != 1.0f || m.scaleOffset.y != 1.0f || m.scaleOffset.z != 1.0f)
							world = XMMatrixScaling(m.scaleOffset.x, m.scaleOffset.y, m.scaleOffset.z) * world;
						ShadowRenderer::Draw(m.pModel, world);
					}
				});

			m_shadowMap.End(devContext);

			// ビューポート復元
			devContext->OMSetRenderTargets(1, &prevRTV, prevDSV);
			devContext->RSSetViewports(1, &prevVP);

			if (prevRTV) prevRTV->Release();
			if (prevDSV) prevDSV->Release();

			// ---------------------------------------------------------
			// 4. メイン描画パス (Main Pass)
			// ---------------------------------------------------------

			// 生成した影マップと行列をModelRendererに渡す [追加]
			ModelRenderer::SetShadowMap(m_shadowMap.GetSRV());
			ModelRenderer::SetLightMatrix(lightView, lightProj);

			// 描画開始
			ModelRenderer::Begin(viewMatrix, projMatrix, lightDir, { 1, 1, 1 });

			// MeshComponentとTransformを持つEntityを描画
			registry.view<MeshComponent, Transform>().each([&](Entity e, MeshComponent& m, Transform& t)
				{
					// ロード処理（ShadowPassでロード済みならキャッシュされているはず）
					if (m.modelKey != m.loadedKey)
					{
						m.pModel = nullptr;
						m.loadedKey = "";
					}
					if (!m.pModel && !m.modelKey.empty())
					{
						m.pModel = ResourceManager::Instance().GetModel(m.modelKey);
						if (m.pModel) m.loadedKey = m.modelKey;
					}

					if (m.pModel)
					{
						XMMATRIX world = t.GetWorldMatrix();
						if (m.scaleOffset.x != 1.0f || m.scaleOffset.y != 1.0f || m.scaleOffset.z != 1.0f)
						{
							world = XMMatrixScaling(m.scaleOffset.x, m.scaleOffset.y, m.scaleOffset.z) * world;
						}

						// 通常描画
						ModelRenderer::Draw(m.pModel, world);
					}
				});
		}

	private:
		ShadowMap m_shadowMap;
		bool m_isShadowInit = false;
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::ModelRenderSystem, "Model Render System")

#endif // !___MODEL_RENDER_SYSTEM_H___