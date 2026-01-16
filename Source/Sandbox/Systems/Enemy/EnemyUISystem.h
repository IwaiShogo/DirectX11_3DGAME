#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	struct EnemyUIContext
	{
		Entity root = NullEntity;
		Entity frameL = NullEntity;
		Entity frameR = NullEntity;
		Entity bg = NullEntity;
		Entity delay = NullEntity;
		Entity fill = NullEntity;

		float delayedHp = 0.0f;
		float lastHp = 0.0f;
		float flashTimer = 0.0f;
	};

	class EnemyUISystem : public ISystem
	{
	public:
		EnemyUISystem()
		{
			m_systemName = "EnemyUISystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// 1. 掃除
			for (auto it = m_contexts.begin(); it != m_contexts.end(); )
			{
				if (!registry.valid(it->first))
				{
					DestroyUI(registry, it->second);
					it = m_contexts.erase(it);
				}
				else ++it;
			}

			// 2. カメラ位置取得
			XMVECTOR camPos = XMVectorZero();
			bool cameraFound = false;
			auto camView = registry.view<Camera, Transform>();
			for (auto c : camView) {
				camPos = XMLoadFloat3(&camView.get<Transform>(c).position);
				cameraFound = true;
				break;
			}
			if (!cameraFound) return;

			// 3. 更新
			auto view = registry.view<EnemyStats, Transform>();
			for (auto entity : view)
			{
				auto& stats = view.get<EnemyStats>(entity);
				auto& trans = view.get<Transform>(entity);

				if (!stats.isInitialized) {
					stats.maxHp = stats.hp;
					stats.isInitialized = true;
				}

				if (m_contexts.find(entity) == m_contexts.end()) {
					CreateUI(registry, entity, stats.hp);
				}

				UpdateUI(registry, entity, m_contexts[entity], stats, trans, camPos, dt);
			}
		}

	private:
		std::map<Entity, EnemyUIContext> m_contexts;

		void CreateUI(Registry& reg, Entity enemy, float hp)
		{
			EnemyUIContext ctx;
			ctx.delayedHp = hp;
			ctx.lastHp = hp;

			ctx.root = reg.create();
			reg.emplace<Transform>(ctx.root);

			auto Add = [&](Entity c, Entity p) {
				if (!reg.has<Relationship>(p)) reg.emplace<Relationship>(p);
				if (!reg.has<Relationship>(c)) reg.emplace<Relationship>(c);
				reg.get<Relationship>(p).children.push_back(c);
				reg.get<Relationship>(c).parent = p;
				};

			// 各パーツ生成
			auto MakePart = [&](Entity& e, GeoShape shape, XMFLOAT4 col, bool wire) {
				e = reg.create();
				reg.emplace<Transform>(e);
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = shape;
				g.color = col;
				g.isWireframe = wire;
				Add(e, ctx.root);
				};

			MakePart(ctx.frameL, GeoShape::Cube, { 0, 1, 1, 1 }, true);
			MakePart(ctx.frameR, GeoShape::Cube, { 0, 1, 1, 1 }, true);
			MakePart(ctx.bg, GeoShape::Cube, { 0, 0, 0, 0.5f }, false);
			MakePart(ctx.delay, GeoShape::Cube, { 1, 1, 1, 0.8f }, false);
			MakePart(ctx.fill, GeoShape::Cube, { 0, 1, 1, 1 }, false);

			m_contexts[enemy] = ctx;
		}

		void DestroyUI(Registry& reg, EnemyUIContext& ctx)
		{
			if (reg.valid(ctx.root)) reg.destroy(ctx.root);
			if (reg.valid(ctx.frameL)) reg.destroy(ctx.frameL);
			if (reg.valid(ctx.frameR)) reg.destroy(ctx.frameR);
			if (reg.valid(ctx.bg)) reg.destroy(ctx.bg);
			if (reg.valid(ctx.delay)) reg.destroy(ctx.delay);
			if (reg.valid(ctx.fill)) reg.destroy(ctx.fill);
		}

		void UpdateUI(Registry& reg, Entity enemy, EnemyUIContext& ctx, const EnemyStats& stats, const Transform& eTrans, XMVECTOR camPos, float dt)
		{
			// ダメージ検知
			if (stats.hp < ctx.lastHp) ctx.flashTimer = 0.15f;
			ctx.lastHp = stats.hp;
			if (ctx.flashTimer > 0.0f) ctx.flashTimer -= dt;

			// 位置更新（頭上）
			auto& tRoot = reg.get<Transform>(ctx.root);
			XMVECTOR enemyPos = XMLoadFloat3(&eTrans.position);
			XMVECTOR barPos = enemyPos + XMVectorSet(0, 2.2f, 0, 0);
			XMStoreFloat3(&tRoot.position, barPos);

			// ビルボード回転
			XMVECTOR toCam = camPos - barPos;
			float angle = atan2f(toCam.m128_f32[0], toCam.m128_f32[2]);
			tRoot.rotation = { 0.0f, XMConvertToDegrees(angle), 0.0f };

			// パラメータ
			float maxW = 1.5f;
			float h = 0.15f;
			float d = 0.02f;
			float hpRatio = std::clamp(stats.hp / stats.maxHp, 0.0f, 1.0f);

			// Delay Lerp
			ctx.delayedHp = std::lerp(ctx.delayedHp, stats.hp, dt * 5.0f);
			float delayRatio = std::clamp(ctx.delayedHp / stats.maxHp, 0.0f, 1.0f);
			if (delayRatio < hpRatio) delayRatio = hpRatio;

			auto SetRect = [&](Entity e, float w, float z, float xOff = 0) {
				if (!reg.valid(e)) return;
				auto& t = reg.get<Transform>(e);
				t.scale = { w, h, d };
				t.position = { xOff, 0.0f, z };
				};
			auto SetLeft = [&](Entity e, float ratio, float z) {
				if (!reg.valid(e)) return;
				auto& t = reg.get<Transform>(e);
				float w = maxW * ratio;
				if (w < 0.01f) w = 0.01f;
				t.scale = { w, h, d };
				t.position = { -(maxW - w) * 0.5f, 0.0f, z };
				};

			// パーツ更新
			SetRect(ctx.frameL, 0.05f, 0.0f, -maxW * 0.5f - 0.05f);
			SetRect(ctx.frameR, 0.05f, 0.0f, maxW * 0.5f + 0.05f);
			if (reg.valid(ctx.frameL)) reg.get<Transform>(ctx.frameL).scale.y = h + 0.1f;
			if (reg.valid(ctx.frameR)) reg.get<Transform>(ctx.frameR).scale.y = h + 0.1f;

			SetRect(ctx.bg, maxW, 0.05f);
			SetLeft(ctx.delay, delayRatio, -0.05f);
			SetLeft(ctx.fill, hpRatio, -0.1f);

			// ★色変化ロジック (Request 1)
			if (reg.valid(ctx.fill))
			{
				auto& geo = reg.get<GeometricDesign>(ctx.fill);
				if (ctx.flashTimer > 0.0f)
				{
					geo.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Flash White
				}
				else
				{
					// シアン(0,1,1) -> 黄(1,1,0) -> 赤(1,0,0)
					if (hpRatio > 0.5f)
					{
						// 50%~100%: Cyan -> Yellow (G:1固定, R:0->1, B:1->0)
						float t = (hpRatio - 0.5f) * 2.0f; // 0.0 ~ 1.0
						geo.color = { 1.0f - t, 1.0f, t, 1.0f };
					}
					else
					{
						// 0%~50%: Yellow -> Red (G:1->0, R:1固定, B:0固定)
						float t = hpRatio * 2.0f; // 0.0 ~ 1.0
						geo.color = { 1.0f, t, 0.0f, 1.0f };
					}
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemyUISystem, "EnemyUISystem")
