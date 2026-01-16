#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Enemy/BossAI.h" // 追加
#include <cmath>

namespace Arche
{
	class EnemyFactory
	{
	public:
		static Entity Spawn(Registry& reg, EnemyType type, XMFLOAT3 position)
		{
			Entity root = reg.create();
			reg.emplace<Transform>(root).position = position;

			// 本体ステータス
			auto& stats = reg.emplace<EnemyStats>(root);
			stats.type = type;
			stats.isInitialized = false;

			// 物理・当たり判定 (Rootは移動用なのでColliderは小さめか、子に任せる)
			auto& rb = reg.emplace<Rigidbody>(root);
			rb.useGravity = false;
			rb.drag = 5.0f;
			rb.mass = 1.0f;

			// 敵の種類に応じて構築
			switch (type)
			{
			case EnemyType::Zako_Cube:    BuildZakoCube(reg, root, stats, rb); break;
			case EnemyType::Zako_Speed:   BuildZakoSpeed(reg, root, stats, rb); break;
			case EnemyType::Zako_Armored: BuildZakoArmored(reg, root, stats, rb); break;

			case EnemyType::Boss_Tank:      BuildBossTank(reg, root, stats, rb); break;
			case EnemyType::Boss_Prism:     BuildBossPrism(reg, root, stats, rb); break;
			case EnemyType::Boss_Carrier:   BuildBossCarrier(reg, root, stats, rb); break;
			case EnemyType::Boss_Construct: BuildBossConstruct(reg, root, stats, rb); break;
			case EnemyType::Boss_Omega:     BuildBossOmega(reg, root, stats, rb); break;
			}

			return root;
		}

	private:
		// パーツ追加ヘルパー
		// hasCollider: trueなら当たり判定をつける
		// hasHealth: trueならHPを持たせる（部位破壊可能にする）
		static Entity AddPart(Registry& reg, Entity parent, GeoShape shape, XMFLOAT4 color,
			XMFLOAT3 offset, XMFLOAT3 scale, bool hasCollider = true, bool hasHealth = false)
		{
			Entity part = reg.create();
			reg.emplace<Transform>(part).position = offset;
			reg.get<Transform>(part).scale = scale;

			// Visual
			auto& geo = reg.emplace<GeometricDesign>(part);
			geo.shapeType = shape;
			geo.isWireframe = true; // ワイヤーフレームでサイバー感
			geo.color = color;

			// Collider (形状に合わせて設定)
			if (hasCollider) {
				Collider col;
				col.layer = Layer::Enemy;
				if (shape == GeoShape::Cube) col = Collider::CreateBox(scale.x, scale.y, scale.z, Layer::Enemy);
				else if (shape == GeoShape::Sphere) col = Collider::CreateSphere(scale.x, Layer::Enemy);
				else if (shape == GeoShape::Cylinder) col = Collider::CreateCylinder(scale.x, scale.y, Layer::Enemy);
				else col = Collider::CreateBox(scale.x, scale.y, scale.z, Layer::Enemy); // Fallback

				reg.emplace<Collider>(part, col);
			}

			// 部位破壊用HP
			if (hasHealth) {
				auto& pStats = reg.emplace<EnemyStats>(part);
				pStats.hp = 50.0f; // デフォルト部位HP
				pStats.maxHp = 50.0f;
				pStats.scoreValue = 100;
				// タグ付け（システムが検索しやすくする）
				reg.emplace<Tag>(part).tag = "EnemyPart";
			}

			// 親子付け
			if (!reg.has<Relationship>(parent)) reg.emplace<Relationship>(parent);
			if (!reg.has<Relationship>(part)) reg.emplace<Relationship>(part);

			reg.get<Relationship>(parent).children.push_back(part);
			reg.get<Relationship>(part).parent = parent;

			return part;
		}

		// =========================================================
		// 構築ロジック
		// =========================================================

		static void BuildZakoCube(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 30; stats.maxHp = 30; stats.speed = 3.0f;
			// Rootにも当たり判定をつける
			reg.emplace<Collider>(root, Collider::CreateBox(1.5f, 1.5f, 1.5f, Layer::Enemy));

			AddPart(reg, root, GeoShape::Cube, { 0,1,0,1 }, { 0,0,0 }, { 1,1,1 }, false);
			Entity ring = AddPart(reg, root, GeoShape::Torus, { 0,1,0.5f,1 }, { 0,0,0 }, { 1.5f, 1.5f, 0.2f }, false);
			reg.get<Transform>(ring).rotation = { 45, 0, 0 };
		}

		static void BuildZakoSpeed(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 15; stats.maxHp = 15; stats.speed = 8.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(1.0f, 1.0f, 2.0f, Layer::Enemy)); // 細長い

			AddPart(reg, root, GeoShape::Cylinder, { 0,1,1,1 }, { 0,0,0 }, { 0.5f, 0.5f, 2.0f }, false);
			AddPart(reg, root, GeoShape::Cube, { 1,0.5f,0,1 }, { 0,0,-1.0f }, { 0.3f, 0.3f, 0.5f }, false);
		}

		static void BuildZakoArmored(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 80; stats.maxHp = 80; stats.speed = 1.5f; rb.mass = 3.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(2.0f, 2.0f, 2.0f, Layer::Enemy));

			AddPart(reg, root, GeoShape::Cube, { 0.5f,0,0.5f,1 }, { 0,0,0 }, { 1.5f, 1.5f, 1.5f }, false);
			// 盾（破壊可能パーツにする）
			Entity shield = AddPart(reg, root, GeoShape::Cube, { 0.8f,0,0.8f,1 }, { 0,0,1.2f }, { 2.2f, 2.2f, 0.2f }, true, true);
			reg.get<EnemyStats>(shield).hp = 100; // 盾はHP高い
		}

		// --- ボス ---

		static void BuildBossTank(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 500; stats.speed = 1.5f; stats.scoreValue = 2000; rb.mass = 10.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(3.0f, 2.0f, 3.0f, Layer::Enemy));

			// AI追加
			auto& ai = reg.emplace<BossAI>(root);
			ai.bossName = "Tank";

			// ボディ
			AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 0,0,0 }, { 3,1.5f,3 }, false);
			// 左右砲台 (破壊可能)
			Entity tL = AddPart(reg, root, GeoShape::Cylinder, { 1,0.5f,0,1 }, { -1.8f, 0.5f, 0 }, { 0.8f, 0.8f, 2.0f }, true, true);
			Entity tR = AddPart(reg, root, GeoShape::Cylinder, { 1,0.5f,0,1 }, { 1.8f, 0.5f, 0 }, { 0.8f, 0.8f, 2.0f }, true, true);
			// 主砲
			AddPart(reg, root, GeoShape::Cylinder, { 1,1,0,1 }, { 0,1.5f,1.0f }, { 0.4f, 0.4f, 3.0f }, false);
		}

		static void BuildBossPrism(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 400; stats.speed = 2.0f; stats.scoreValue = 2000;
			reg.emplace<Collider>(root, Collider::CreateBox(2.5f, 2.5f, 2.5f, Layer::Enemy));

			auto& ai = reg.emplace<BossAI>(root);
			ai.bossName = "Prism";

			// コア
			Entity core = AddPart(reg, root, GeoShape::Cube, { 1,0,1,1 }, { 0,1,0 }, { 2.5f, 2.5f, 2.5f }, false);
			reg.get<Transform>(core).rotation = { 45, 45, 0 };

			// ビット (破壊可能)
			for (int i = 0; i < 4; ++i) {
				float x = sinf(i * 1.57f) * 4.0f;
				float z = cosf(i * 1.57f) * 4.0f;
				Entity bit = AddPart(reg, root, GeoShape::Cube, { 1,0,1,0.5f }, { x, 0, z }, { 1,1,1 }, true, true);
				reg.get<Transform>(bit).rotation = { 45, 0, 45 };
			}
		}

		static void BuildBossCarrier(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 600; stats.speed = 1.0f; stats.scoreValue = 3000;
			reg.emplace<Collider>(root, Collider::CreateBox(6.0f, 1.0f, 8.0f, Layer::Enemy));

			auto& ai = reg.emplace<BossAI>(root);
			ai.bossName = "Carrier";

			AddPart(reg, root, GeoShape::Cube, { 0,0.5f,1,1 }, { 0,0,0 }, { 6, 0.5f, 8 }, false); // 甲板
			// ブリッジ (弱点)
			Entity bridge = AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 2, 1, -2 }, { 1.5f, 2.0f, 2.0f }, true, true);
			reg.get<EnemyStats>(bridge).hp = 200;
		}

		static void BuildBossConstruct(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 700; stats.speed = 2.5f; stats.scoreValue = 4000;
			reg.emplace<Collider>(root, Collider::CreateSphere(2.5f, Layer::Enemy));

			auto& ai = reg.emplace<BossAI>(root);
			ai.bossName = "Construct";

			AddPart(reg, root, GeoShape::Sphere, { 1,0.5f,0,1 }, { 0,0,0 }, { 2.5f, 2.5f, 2.5f }, false);
			// 触手 (破壊可能)
			for (int i = 0; i < 8; ++i) {
				float angle = i * 0.785f;
				float r = 3.0f;
				Entity arm = AddPart(reg, root, GeoShape::Cube, { 1,0.2f,0,1 },
					{ sinf(angle) * r, 0, cosf(angle) * r }, { 0.5f, 0.5f, 4.0f }, true, true);
				reg.get<Transform>(arm).rotation.y = XMConvertToDegrees(angle);
			}
		}

		static void BuildBossOmega(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb)
		{
			stats.hp = 3000; stats.speed = 0.5f; stats.scoreValue = 10000; rb.mass = 50.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(5.0f, 5.0f, 5.0f, Layer::Enemy));

			auto& ai = reg.emplace<BossAI>(root);
			ai.bossName = "Omega";

			// コア
			AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 0,0,0 }, { 4,4,4 }, false);
			Entity c2 = AddPart(reg, root, GeoShape::Cube, { 1,0,0,0.5f }, { 0,0,0 }, { 5,5,5 }, false);
			reg.get<Transform>(c2).rotation = { 45, 45, 45 };

			// リング
			Entity r1 = AddPart(reg, root, GeoShape::Torus, { 1,0,0,0.5f }, { 0,0,0 }, { 10,10,0.5f }, false);
			Entity r2 = AddPart(reg, root, GeoShape::Torus, { 1,0,0,0.5f }, { 0,0,0 }, { 12,12,0.5f }, false);
			reg.get<Transform>(r2).rotation = { 90,0,0 };

			// ガードビット (破壊可能シールド)
			for (int i = 0; i < 6; ++i) {
				float a = i * 1.047f;
				Entity shield = AddPart(reg, root, GeoShape::Sphere, { 0,1,1,1 }, { sinf(a) * 8, 0, cosf(a) * 8 }, { 1.5f,1.5f,1.5f }, true, true);
				reg.get<EnemyStats>(shield).hp = 300;
			}
		}
	};
}