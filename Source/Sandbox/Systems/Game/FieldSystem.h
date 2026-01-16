#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Core/GameSession.h"
#include <vector>
#include <cmath>

namespace Arche
{
	// フィールドの物理・範囲データ
	struct FieldProperties {
		float radius = 20.0f;
		float floorHeight = -2.0f;
		float ceilingHeight = 10.0f;
	};

	struct FieldTag {};

	struct FieldContext
	{
		std::vector<Entity> floorTiles;
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

			// 初期化
			bool init = false;
			for (auto e : reg.view<FieldTag>()) { init = true; break; }
			if (!init) {
				ctx = FieldContext();
				// ステージごとの広さ定義
				float r = 25.0f;
				int s = GameSession::selectedStageId;
				if (s == 3) r = 15.0f; // Highwayは狭く
				if (s == 5) r = 30.0f; // Bossは広く

				// プロパティ保存（プレイヤーが参照する）
				Entity config = reg.create();
				reg.emplace<FieldTag>(config);
				auto& props = reg.emplace<FieldProperties>(config);
				props.radius = r;

				GenerateField(reg, ctx, r);
			}

			float dt = Time::DeltaTime();
			ctx.time += dt;

			// --- フィールド演出 ---
			// 1. バリアの明滅（境界線）
			if (reg.valid(ctx.mainBarrier)) {
				auto& g = reg.get<GeometricDesign>(ctx.mainBarrier);
				// プレイヤーが端に近づくと濃くする演出などを入れたいが、簡易的に呼吸
				g.color.w = 0.1f + 0.05f * sinf(ctx.time * 2.0f);

				// 回転
				auto& t = reg.get<Transform>(ctx.mainBarrier);
				t.rotation.y += dt * 0.1f;
			}

			// 2. 床の波打ち
			for (auto e : ctx.floorTiles) {
				if (reg.valid(e)) {
					auto& t = reg.get<Transform>(e);
					auto& g = reg.get<GeometricDesign>(e);

					float dist = sqrt(t.position.x * t.position.x + t.position.z * t.position.z);
					float wave = sinf(dist * 0.3f - ctx.time * 3.0f);

					// 高さ変動
					t.position.y = -2.0f + wave * 0.2f;
					// 色変化
					g.color.w = 0.3f + (wave * 0.5f + 0.5f) * 0.2f;
				}
			}
		}

	private:
		void GenerateField(Registry& reg, FieldContext& ctx, float radius)
		{
			int stageId = GameSession::selectedStageId;
			if (stageId == 0) stageId = 1;

			// --- テーマカラー ---
			XMFLOAT4 tileCol = { 0,1,1,1 };
			GeoShape tileShape = GeoShape::Cube;
			if (stageId == 2) { tileCol = { 1,0.5f,0,1 }; tileShape = GeoShape::Pyramid; }
			if (stageId == 4) { tileCol = { 0.8f,0,1,1 }; tileShape = GeoShape::Diamond; }
			if (stageId == 5) { tileCol = { 1,0,0,1 }; tileShape = GeoShape::Torus; }

			// --- 床生成 (グリッド状に配置) ---
			int count = (int)(radius / 1.5f);
			for (int x = -count; x <= count; ++x) {
				for (int z = -count; z <= count; ++z) {
					// 円形に切り抜く
					float dist = sqrtf((float)(x * x + z * z)) * 2.0f;
					if (dist > radius) continue;

					Entity e = reg.create();
					reg.emplace<Transform>(e).position = { x * 2.0f, -2.0f, z * 2.0f };
					reg.get<Transform>(e).scale = { 1.8f, 0.1f, 1.8f };

					auto& g = reg.emplace<GeometricDesign>(e);
					g.shapeType = tileShape;
					g.isWireframe = true;
					g.color = { tileCol.x, tileCol.y, tileCol.z, 0.2f };

					ctx.floorTiles.push_back(e);
				}
			}

			// --- 透明な壁 (バリア) ---
			{
				ctx.mainBarrier = reg.create();
				reg.emplace<Transform>(ctx.mainBarrier).position = { 0, 5.0f, 0 };
				// 半径に合わせてスケール調整 (Cylinderは直径1なのでradius*2)
				reg.get<Transform>(ctx.mainBarrier).scale = { radius * 2.0f, 20.0f, radius * 2.0f };

				auto& g = reg.emplace<GeometricDesign>(ctx.mainBarrier);
				g.shapeType = GeoShape::Cylinder;
				g.isWireframe = true; // ワイヤーフレームでデジタルの壁感を出す
				g.color = { tileCol.x, tileCol.y, tileCol.z, 0.1f };
			}

			// --- 装飾リング (上下に配置) ---
			for (int i = 0; i < 3; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { 0, -2.0f + i * 5.0f, 0 };
				reg.get<Transform>(e).scale = { radius * 2.0f + i, 0.5f, radius * 2.0f + i };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Torus;
				g.isWireframe = true;
				g.color = { 1,1,1, 0.1f };
				ctx.barrierRings.push_back(e);
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::FieldSystem, "FieldSystem")