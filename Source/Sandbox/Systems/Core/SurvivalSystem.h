#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	// リングパーツ管理用
	struct PlayerRingContext
	{
		Entity mainRing = NullEntity;
		Entity subRing = NullEntity;
		Entity ticks[4] = { NullEntity }; // 回転するビット
	};

	class SurvivalSystem : public ISystem
	{
	public:
		SurvivalSystem()
		{
			m_systemName = "SurvivalSystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			auto view = registry.view<PlayerTime, Transform>();
			for (auto entity : view)
			{
				auto& timeData = view.get<PlayerTime>(entity);
				auto& trans = view.get<Transform>(entity);

				if (timeData.isDead) continue;

				// 時間減少
				timeData.currentTime -= dt;

				// スロー演出
				if (timeData.currentTime <= 1.0f && timeData.currentTime > 0.0f)
					Time::timeScale = 0.5f;
				else
					Time::timeScale = 1.0f;

				// ゲームオーバー
				if (timeData.currentTime <= 0.0f) {
					timeData.currentTime = 0.0f;
					timeData.isDead = true;
					Time::timeScale = 0.0f;
					Logger::Log("TIME UP!");
				}

				// 無敵点滅
				if (timeData.invincibilityTimer > 0.0f) {
					timeData.invincibilityTimer -= dt;
					if (registry.has<GeometricDesign>(entity))
						registry.get<GeometricDesign>(entity).isWireframe = (int)(timeData.invincibilityTimer * 20.0f) % 2 == 0;
				}
				else {
					if (registry.has<GeometricDesign>(entity))
						registry.get<GeometricDesign>(entity).isWireframe = false;
				}

				// ★Request 4: スタイリッシュなリング更新
				UpdateRingVisual(registry, entity, timeData);
			}
		}

	private:
		std::map<Entity, PlayerRingContext> m_rings;

		void UpdateRingVisual(Registry& reg, Entity player, const PlayerTime& timeData)
		{
			if (m_rings.find(player) == m_rings.end())
			{
				PlayerRingContext ctx;

				// Helper
				auto CreateRing = [&](Entity& e, float scale) {
					e = reg.create();
					reg.emplace<Transform>(e);
					auto& g = reg.emplace<GeometricDesign>(e);
					g.shapeType = GeoShape::Cylinder; // フラットシリンダー
					g.isWireframe = true;
					g.color = { 0, 1, 1, 1 };
					};

				CreateRing(ctx.mainRing, 2.0f);
				CreateRing(ctx.subRing, 1.5f);

				// 4つのビット
				for (int i = 0; i < 4; i++) {
					ctx.ticks[i] = reg.create();
					reg.emplace<Transform>(ctx.ticks[i]);
					auto& g = reg.emplace<GeometricDesign>(ctx.ticks[i]);
					g.shapeType = GeoShape::Cube;
					g.isWireframe = false; // ソリッド
				}

				m_rings[player] = ctx;
			}

			PlayerRingContext& ctx = m_rings[player];
			auto& pTrans = reg.get<Transform>(player);
			float ratio = timeData.currentTime / timeData.maxTime;
			if (ratio < 0) ratio = 0;

			// 色の決定 (UIと同じロジック)
			XMFLOAT4 color;
			if (ratio > 0.5f) color = { 1.0f - (ratio - 0.5f) * 2.0f, 1.0f, (ratio - 0.5f) * 2.0f, 1.0f };
			else color = { 1.0f, ratio * 2.0f, 0.0f, 1.0f };

			float baseY = pTrans.position.y - 0.9f; // 足元

			// Main Ring (縮小していく)
			if (reg.valid(ctx.mainRing)) {
				auto& t = reg.get<Transform>(ctx.mainRing);
				auto& g = reg.get<GeometricDesign>(ctx.mainRing);
				float s = 3.0f * ratio;
				if (s < 0.5f) s = 0.5f; // 最小サイズ確保

				t.position = { pTrans.position.x, baseY, pTrans.position.z };
				t.scale = { s, 0.02f, s }; // 極薄
				t.rotation.y += Time::DeltaTime() * 30.0f; // ゆっくり回転
				g.color = color;
			}

			// Sub Ring (逆回転・一定サイズ)
			if (reg.valid(ctx.subRing)) {
				auto& t = reg.get<Transform>(ctx.subRing);
				auto& g = reg.get<GeometricDesign>(ctx.subRing);

				t.position = { pTrans.position.x, baseY, pTrans.position.z };
				t.scale = { 2.5f, 0.02f, 2.5f }; // 外枠として機能
				t.rotation.y -= Time::DeltaTime() * 60.0f; // 速く逆回転

				// アルファ値を下げて薄くする
				XMFLOAT4 subCol = color;
				subCol.w = 0.3f;
				g.color = subCol;
			}

			// Ticks (周囲を回るビット)
			float radius = 3.5f * ratio;
			if (radius < 0.8f) radius = 0.8f;

			for (int i = 0; i < 4; i++) {
				if (reg.valid(ctx.ticks[i])) {
					auto& t = reg.get<Transform>(ctx.ticks[i]);
					auto& g = reg.get<GeometricDesign>(ctx.ticks[i]);

					float angle = Time::TotalTime() * 2.0f + (i * XM_PI / 2.0f);
					float x = cosf(angle) * radius;
					float z = sinf(angle) * radius;

					t.position = { pTrans.position.x + x, baseY, pTrans.position.z + z };
					t.scale = { 0.1f, 0.1f, 0.1f };
					t.rotation.y = XMConvertToDegrees(-angle);
					g.color = color;
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::SurvivalSystem, "SurvivalSystem")
