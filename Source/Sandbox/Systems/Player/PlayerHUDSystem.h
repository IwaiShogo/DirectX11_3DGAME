#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"

#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	struct PlayerHUDContext
	{
		Entity bgPanel = NullEntity;
		Entity mainText = NullEntity;
		Entity barBg = NullEntity;
		Entity barFill = NullEntity;
		Entity decoTop = NullEntity;
		Entity decoBtm = NullEntity;

		float currentDisplayTime = 0.0f;

		// 慣性制御用
		XMFLOAT3 currentPos = { 0, 0, 0 };
		bool isFirstFrame = true;
	};

	class PlayerHUDSystem : public ISystem
	{
	public:
		PlayerHUDSystem()
		{
			m_systemName = "PlayerHUDSystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			Entity player = NullEntity;
			for (auto e : registry.view<PlayerTime>()) { player = e; break; }

			Entity camera = NullEntity;
			for (auto e : registry.view<Camera, Transform>()) { camera = e; break; }

			if (player == NullEntity || camera == NullEntity) return;

			if (!registry.valid(m_hud.bgPanel)) CreateHUD(registry);

			UpdateHUD(registry, player, camera, dt);
		}

	private:
		PlayerHUDContext m_hud;
		float m_shakeTimer = 0.0f;
		float m_lastTime = 0.0f;

		void CreateHUD(Registry& reg)
		{
			// ★修正: 全体的にサイズを小さく定義します

			// [BG Panel]
			m_hud.bgPanel = reg.create();
			reg.emplace<Transform>(m_hud.bgPanel);
			auto& geoBg = reg.emplace<GeometricDesign>(m_hud.bgPanel);
			geoBg.shapeType = GeoShape::Cube;
			geoBg.color = { 0.0f, 0.05f, 0.1f, 0.15f };

			// [Decorations]
			auto MakeDeco = [&](Entity& e) {
				e = reg.create();
				reg.emplace<Transform>(e);
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube;
				g.color = { 0.0f, 1.0f, 1.0f, 0.4f };
				};
			MakeDeco(m_hud.decoTop);
			MakeDeco(m_hud.decoBtm);

			// [Bars]
			m_hud.barBg = reg.create();
			reg.emplace<Transform>(m_hud.barBg);
			reg.emplace<GeometricDesign>(m_hud.barBg).shapeType = GeoShape::Cube;
			reg.get<GeometricDesign>(m_hud.barBg).color = { 0, 0, 0, 0.4f };

			m_hud.barFill = reg.create();
			reg.emplace<Transform>(m_hud.barFill);
			reg.emplace<GeometricDesign>(m_hud.barFill).shapeType = GeoShape::Cube;

			// [Text]
			m_hud.mainText = reg.create();
			reg.emplace<Transform>(m_hud.mainText);
			auto& txt = reg.emplace<TextComponent>(m_hud.mainText);
			txt.fontKey = "Consolas";
			txt.isBold = true;
			// ★修正: フォントサイズを小さく (32 -> 12)
			// カメラに近いと大きく見えるため、これくらいで十分です
			txt.fontSize = 8.0f;
			txt.centerAlign = true;
		}

		void UpdateHUD(Registry& reg, Entity player, Entity camera, float dt)
		{
			auto& pTime = reg.get<PlayerTime>(player);
			auto& camTrans = reg.get<Transform>(camera);

			// 1. ダメージシェイク
			if (pTime.currentTime < m_lastTime - 0.1f) m_shakeTimer = 0.4f;
			m_lastTime = pTime.currentTime;
			if (m_shakeTimer > 0.0f) m_shakeTimer -= dt;

			// 2. 基準位置の計算
			XMMATRIX camRotM = XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(camTrans.rotation.x),
				XMConvertToRadians(camTrans.rotation.y),
				0.0f);

			XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), camRotM);
			XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), camRotM);
			XMVECTOR right = XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), camRotM);
			XMVECTOR camPos = XMLoadFloat3(&camTrans.position);

			// ★修正: カメラから 0.8m 前、0.12m 下に配置
			// これならプレイヤー(3m先)の手前に確実に表示されます
			XMVECTOR targetBasePos = camPos + (forward * 2.0f) + (up * 0.01f);

			// 3. 慣性処理
			if (m_hud.isFirstFrame) {
				XMStoreFloat3(&m_hud.currentPos, targetBasePos);
				m_hud.isFirstFrame = false;
			}
			XMVECTOR currentBasePos = XMLoadFloat3(&m_hud.currentPos);
			currentBasePos = XMVectorLerp(currentBasePos, targetBasePos, dt * 25.0f); // 追従速度アップ
			XMStoreFloat3(&m_hud.currentPos, currentBasePos);

			// シェイク
			if (m_shakeTimer > 0.0f) {
				float amp = 0.02f; // 揺れ幅も小さく
				currentBasePos += (right * ((rand() % 100 / 50.0f - 1.0f) * amp)) + (up * ((rand() % 100 / 50.0f - 1.0f) * amp));
			}

			XMFLOAT3 baseRot = camTrans.rotation;
			baseRot.z = 0.0f;

			// Helper
			auto SetTrans = [&](Entity e, float ox, float oy, float oz, float sx, float sy, float sz) {
				if (!reg.valid(e)) return;
				auto& t = reg.get<Transform>(e);
				XMVECTOR p = currentBasePos + (right * ox) + (up * oy) + (forward * oz);
				XMStoreFloat3(&t.position, p);
				t.rotation = baseRot;
				t.scale = { sx, sy, sz };
				};

			// ★修正: 各パーツのサイズとオフセットを小型化

			// [Bg Panel] サイズ: 0.56 x 0.2
			SetTrans(m_hud.bgPanel, 0, 0, 0.02f, 0.56f, 0.2f, 0.005f);

			// 赤く明滅
			auto& gBg = reg.get<GeometricDesign>(m_hud.bgPanel);
			if (m_shakeTimer > 0.0f) gBg.color = { 0.5f, 0.0f, 0.0f, 0.3f };
			else gBg.color = { 0.0f, 0.05f, 0.1f, 0.15f };

			// [Text]
			m_hud.currentDisplayTime = pTime.currentTime;
			if (m_hud.currentDisplayTime < 0) m_hud.currentDisplayTime = 0;
			char buffer[32];
			sprintf_s(buffer, "TIME %05.2f", m_hud.currentDisplayTime);

			auto& txt = reg.get<TextComponent>(m_hud.mainText);
			txt.text = buffer;

			float r = m_hud.currentDisplayTime / pTime.maxTime;
			if (m_shakeTimer > 0.0f || r < 0.2f) txt.color = { 1, 0, 0, 1 };
			else if (r < 0.5f) txt.color = { 1, 1, 0, 1 };
			else txt.color = { 0, 1, 1, 1 };

			// テキスト位置
			SetTrans(m_hud.mainText, 0, 0.03f, -0.02f, 1, 1, 1);

			// [Decorations]
			float decoW = 0.6f; float decoY = 0.11f;
			SetTrans(m_hud.decoTop, 0, decoY, 0, decoW, 0.008f, 0.005f);
			SetTrans(m_hud.decoBtm, 0, -decoY, 0, decoW, 0.008f, 0.005f);

			XMFLOAT4 mainCol = txt.color;
			mainCol.w = 0.5f;
			reg.get<GeometricDesign>(m_hud.decoTop).color = mainCol;
			reg.get<GeometricDesign>(m_hud.decoBtm).color = mainCol;

			// [Bar]
			float barMaxW = 0.48f;
			if (r > 1) r = 1; if (r < 0) r = 0;

			SetTrans(m_hud.barBg, 0, -0.06f, 0, barMaxW, 0.015f, 0.005f);

			float w = barMaxW * r;
			if (w < 0.001f) w = 0.001f;
			float xOff = -(barMaxW - w) * 0.5f;

			SetTrans(m_hud.barFill, xOff, -0.06f, -0.005f, w, 0.015f, 0.005f);

			mainCol.w = 0.8f;
			reg.get<GeometricDesign>(m_hud.barFill).color = mainCol;
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerHUDSystem, "PlayerHUDSystem")
