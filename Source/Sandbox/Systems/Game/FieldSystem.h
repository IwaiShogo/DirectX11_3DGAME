#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Core/GameSession.h"
#include <vector>
#include <cmath>

namespace Arche
{
	struct FieldProperties {
		float radius = 20.0f;
		float floorHeight = -2.0f;
	};

	struct FieldTag {};

	struct FieldContext
	{
		std::vector<Entity> floorTiles;
		std::vector<Entity> cityBuildings;
		std::vector<Entity> barrierRings;
		Entity mainBarrier = NullEntity;
		float time = 0.0f;
	};

	class FieldSystem : public ISystem
	{
	public:
		FieldSystem() { m_systemName = "FieldSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static FieldContext ctx;

			bool init = false;
			for (auto e : reg.view<FieldTag>()) { init = true; break; }
			if (!init) {
				ctx = FieldContext();
				float r = (GameSession::selectedStageId == 5) ? 35.0f : 25.0f;

				Entity config = reg.create();
				reg.emplace<FieldTag>(config);
				auto& props = reg.emplace<FieldProperties>(config);
				props.radius = r;

				GenerateField(reg, ctx, r);
			}

			float dt = Time::DeltaTime();
			ctx.time += dt;

			// --- 演出更新 ---

			// 1. バリアの回転と明滅
			if (reg.valid(ctx.mainBarrier)) {
				auto& t = reg.get<Transform>(ctx.mainBarrier);
				t.rotation.y += dt * 0.2f;
				auto& g = reg.get<GeometricDesign>(ctx.mainBarrier);
				g.color.w = 0.1f + 0.05f * sinf(ctx.time);
			}

			// 2. 背景ビル群の上下運動 (EQのように)
			for (int i = 0; i < ctx.cityBuildings.size(); ++i) {
				if (reg.valid(ctx.cityBuildings[i])) {
					auto& t = reg.get<Transform>(ctx.cityBuildings[i]);
					// ランダムなリズムで動く
					float speed = 1.0f + (i % 5) * 0.5f;
					float h = 5.0f + 10.0f * sinf(ctx.time * speed + i);
					if (h < 1.0f) h = 1.0f;
					t.scale.y = h;
					t.position.y = -10.0f + h * 0.5f;

					// 色の変化
					auto& g = reg.get<GeometricDesign>(ctx.cityBuildings[i]);
					g.color.w = 0.1f + (h / 20.0f) * 0.4f;
				}
			}

			// 3. 床のウェーブ
			for (auto e : ctx.floorTiles) {
				if (reg.valid(e)) {
					auto& t = reg.get<Transform>(e);
					float dist = sqrt(t.position.x * t.position.x + t.position.z * t.position.z);
					float wave = sinf(dist * 0.3f - ctx.time * 4.0f);
					t.position.y = -2.0f + wave * 0.15f;
					reg.get<GeometricDesign>(e).color.w = 0.2f + (wave * 0.5f + 0.5f) * 0.3f;
				}
			}
		}

	private:
		void GenerateField(Registry& reg, FieldContext& ctx, float radius)
		{
			int stageId = GameSession::selectedStageId;
			if (stageId == 0) stageId = 1;

			// ステージ色設定
			XMFLOAT4 baseCol = { 0,1,1,1 };
			if (stageId == 2) baseCol = { 1,0.5f,0,1 };
			if (stageId == 4) baseCol = { 0.8f,0,1,1 };
			if (stageId == 5) baseCol = { 1,0,0,1 };

			// 床生成
			int count = (int)(radius / 1.5f);
			for (int x = -count; x <= count; ++x) {
				for (int z = -count; z <= count; ++z) {
					float dist = sqrtf((float)(x * x + z * z)) * 2.0f;
					if (dist > radius) continue;

					Entity e = reg.create();
					reg.emplace<Transform>(e).position = { x * 2.0f, -2.0f, z * 2.0f };
					reg.get<Transform>(e).scale = { 1.9f, 0.1f, 1.9f }; // 隙間なく

					auto& g = reg.emplace<GeometricDesign>(e);
					g.shapeType = GeoShape::Cube;
					g.isWireframe = true;
					g.color = { baseCol.x, baseCol.y, baseCol.z, 0.2f };

					ctx.floorTiles.push_back(e);
				}
			}

			// 背景ビル群（遠景）
			int buildings = 60;
			for (int i = 0; i < buildings; ++i) {
				float angle = (i / (float)buildings) * XM_2PI;
				float dist = radius + 10.0f + (rand() % 20);

				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { cosf(angle) * dist, -10.0f, sinf(angle) * dist };
				reg.get<Transform>(e).scale = { 2.0f, 10.0f, 2.0f };

				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube;
				g.isWireframe = true;
				g.color = { baseCol.x * 0.5f, baseCol.y * 0.5f, baseCol.z * 0.5f, 0.1f };

				ctx.cityBuildings.push_back(e);
			}

			// 透明な壁
			{
				ctx.mainBarrier = reg.create();
				reg.emplace<Transform>(ctx.mainBarrier).position = { 0, 5.0f, 0 };
				reg.get<Transform>(ctx.mainBarrier).scale = { radius * 2.0f, 20.0f, radius * 2.0f };
				auto& g = reg.emplace<GeometricDesign>(ctx.mainBarrier);
				g.shapeType = GeoShape::Cylinder;
				g.isWireframe = true;
				g.color = { baseCol.x, baseCol.y, baseCol.z, 0.1f };
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::FieldSystem, "FieldSystem")