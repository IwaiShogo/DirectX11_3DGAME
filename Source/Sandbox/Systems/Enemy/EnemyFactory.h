#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Enemy/BossAI.h"
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

			auto& stats = reg.emplace<EnemyStats>(root);
			stats.type = type;
			stats.isInitialized = false;

			auto& rb = reg.emplace<Rigidbody>(root);
			rb.useGravity = false;
			rb.drag = 5.0f;
			rb.mass = 1.0f;

			switch (type)
			{
			case EnemyType::Zako_Cube:    BuildZakoCube(reg, root, stats, rb); break;
			case EnemyType::Zako_Speed:   BuildZakoSpeed(reg, root, stats, rb); break;
			case EnemyType::Zako_Armored: BuildZakoArmored(reg, root, stats, rb); break;
			case EnemyType::Zako_Sniper:  BuildZakoSniper(reg, root, stats, rb); break;
			case EnemyType::Zako_Kamikaze:BuildZakoKamikaze(reg, root, stats, rb); break;
			case EnemyType::Zako_Shield:  BuildZakoShield(reg, root, stats, rb); break;

			case EnemyType::Boss_Tank:      BuildBossTank(reg, root, stats, rb); break;
			case EnemyType::Boss_Prism:     BuildBossPrism(reg, root, stats, rb); break;
			case EnemyType::Boss_Carrier:   BuildBossCarrier(reg, root, stats, rb); break;
			case EnemyType::Boss_Construct: BuildBossConstruct(reg, root, stats, rb); break;
			case EnemyType::Boss_Omega:     BuildBossOmega(reg, root, stats, rb); break;
			case EnemyType::Boss_Titan:     BuildBossTitan(reg, root, stats, rb); break;
			}

			auto& t = reg.get<Transform>(root);
			XMMATRIX worldMat = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
				XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
				XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
			XMStoreFloat4x4(&t.worldMatrix, worldMat);

			return root;
		}

	private:
		static Entity AddPart(Registry& reg, Entity parent, GeoShape shape, XMFLOAT4 color,
			XMFLOAT3 offset, XMFLOAT3 scale, bool hasCollider = true, bool hasHealth = false)
		{
			Entity part = reg.create();
			reg.emplace<Transform>(part).position = offset;
			reg.get<Transform>(part).scale = scale;

			auto& geo = reg.emplace<GeometricDesign>(part);
			geo.shapeType = shape;
			geo.isWireframe = true;
			geo.color = color;

			if (hasCollider) {
				Collider col;
				col.layer = Layer::Enemy;
				if (shape == GeoShape::Cube) col = Collider::CreateBox(scale.x, scale.y, scale.z, Layer::Enemy);
				else if (shape == GeoShape::Sphere) col = Collider::CreateSphere(scale.x, Layer::Enemy);
				else if (shape == GeoShape::Cylinder) col = Collider::CreateCylinder(scale.x, scale.y, Layer::Enemy);
				else col = Collider::CreateBox(scale.x, scale.y, scale.z, Layer::Enemy);
				reg.emplace<Collider>(part, col);
			}

			if (hasHealth) {
				auto& pStats = reg.emplace<EnemyStats>(part);
				pStats.hp = 50.0f; pStats.maxHp = 50.0f; pStats.scoreValue = 100;
				reg.emplace<Tag>(part).tag = "EnemyPart";
			}

			if (!reg.has<Relationship>(parent)) reg.emplace<Relationship>(parent);
			if (!reg.has<Relationship>(part)) reg.emplace<Relationship>(part);
			reg.get<Relationship>(parent).children.push_back(part);
			reg.get<Relationship>(part).parent = parent;
			return part;
		}

		// --- 雑魚 (速度アップ) ---
		static void BuildZakoCube(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 30; stats.maxHp = 30;
			stats.speed = 6.0f; // 3.0 -> 6.0
			stats.scoreValue = 100;
			reg.emplace<Collider>(root, Collider::CreateBox(1.5f, 1.5f, 1.5f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Cube, { 0,1,0,1 }, { 0,0,0 }, { 1,1,1 }, false);
			Entity ring = AddPart(reg, root, GeoShape::Torus, { 0,1,0.5f,1 }, { 0,0,0 }, { 1.5f, 1.5f, 0.2f }, false);
			reg.get<Transform>(ring).rotation = { 45, 0, 0 };
		}
		static void BuildZakoSpeed(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 15; stats.maxHp = 15;
			stats.speed = 14.0f; // 8.0 -> 14.0
			stats.scoreValue = 200;
			reg.emplace<Collider>(root, Collider::CreateBox(1.0f, 1.0f, 2.0f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Cylinder, { 0,1,1,1 }, { 0,0,0 }, { 0.5f, 0.5f, 2.0f }, false);
			AddPart(reg, root, GeoShape::Cube, { 1,0.5f,0,1 }, { 0,0,-1.0f }, { 0.3f, 0.3f, 0.5f }, false);
		}
		static void BuildZakoArmored(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 80; stats.maxHp = 80;
			stats.speed = 3.5f; // 1.5 -> 3.5
			rb.mass = 3.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(2.0f, 2.0f, 2.0f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Cube, { 0.5f,0,0.5f,1 }, { 0,0,0 }, { 1.5f, 1.5f, 1.5f }, false);
			Entity shield = AddPart(reg, root, GeoShape::Cube, { 0.8f,0,0.8f,1 }, { 0,0,1.2f }, { 2.2f, 2.2f, 0.2f }, true, true);
			reg.get<EnemyStats>(shield).hp = 100;
		}
		static void BuildZakoSniper(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 20; stats.maxHp = 20;
			stats.speed = 5.0f; // 新規追加のため標準的
			reg.emplace<Collider>(root, Collider::CreateBox(1.0f, 2.0f, 1.0f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Diamond, { 1, 1, 0, 1 }, { 0, 0, 0 }, { 1, 2, 1 }, false);
			AddPart(reg, root, GeoShape::Cylinder, { 1, 0, 0, 1 }, { 0, 0.5f, 0.8f }, { 0.2f, 0.2f, 1.5f }, false);
		}
		static void BuildZakoKamikaze(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 10; stats.maxHp = 10;
			stats.speed = 18.0f; // プレイヤーより速く
			reg.emplace<Collider>(root, Collider::CreateSphere(1.0f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Sphere, { 1, 0, 0, 1 }, { 0, 0, 0 }, { 1.2f, 1.2f, 1.2f }, false);
			AddPart(reg, root, GeoShape::Pyramid, { 1, 0.5f, 0, 1 }, { 0, 0, -0.8f }, { 0.5f, 0.5f, 1.5f }, false);
		}
		static void BuildZakoShield(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 60; stats.maxHp = 60;
			stats.speed = 4.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(1.5f, 1.5f, 1.5f, Layer::Enemy));
			AddPart(reg, root, GeoShape::Cube, { 0.5f, 0.5f, 0.5f, 1 }, { 0, 0, 0 }, { 1.5f, 1.5f, 1.5f }, false);
			Entity s = AddPart(reg, root, GeoShape::Cube, { 0, 0.5f, 1, 0.5f }, { 0, 0, 1.5f }, { 3.0f, 3.0f, 0.2f }, true, true);
			reg.get<EnemyStats>(s).hp = 9999;
		}

		// --- ボス (速度アップ) ---
		static void BuildBossTank(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 500;
			stats.speed = 3.5f; // 1.5 -> 3.5
			stats.scoreValue = 2000; rb.mass = 10.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(3.0f, 2.0f, 3.0f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Tank";
			AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 0,0,0 }, { 3,1.5f,3 }, false);
			Entity tL = AddPart(reg, root, GeoShape::Cylinder, { 1,0.5f,0,1 }, { -1.8f, 0.5f, 0 }, { 0.8f, 0.8f, 2.0f }, true, true);
			Entity tR = AddPart(reg, root, GeoShape::Cylinder, { 1,0.5f,0,1 }, { 1.8f, 0.5f, 0 }, { 0.8f, 0.8f, 2.0f }, true, true);
			AddPart(reg, root, GeoShape::Cylinder, { 1,1,0,1 }, { 0,1.5f,1.0f }, { 0.4f, 0.4f, 3.0f }, false);
		}
		static void BuildBossPrism(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 400;
			stats.speed = 4.5f; // 2.0 -> 4.5
			stats.scoreValue = 2000;
			reg.emplace<Collider>(root, Collider::CreateBox(2.5f, 2.5f, 2.5f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Prism";
			Entity core = AddPart(reg, root, GeoShape::Cube, { 1,0,1,1 }, { 0,1,0 }, { 2.5f, 2.5f, 2.5f }, false);
			reg.get<Transform>(core).rotation = { 45, 45, 0 };
			for (int i = 0; i < 4; ++i) {
				float x = sinf(i * 1.57f) * 4.0f; float z = cosf(i * 1.57f) * 4.0f;
				Entity bit = AddPart(reg, root, GeoShape::Cube, { 1,0,1,0.5f }, { x, 0, z }, { 1,1,1 }, true, true);
				reg.get<Transform>(bit).rotation = { 45, 0, 45 };
			}
		}
		static void BuildBossCarrier(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 600;
			stats.speed = 2.5f; // 1.0 -> 2.5
			stats.scoreValue = 3000;
			reg.emplace<Collider>(root, Collider::CreateBox(6.0f, 1.0f, 8.0f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Carrier";
			AddPart(reg, root, GeoShape::Cube, { 0,0.5f,1,1 }, { 0,0,0 }, { 6, 0.5f, 8 }, false);
			Entity bridge = AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 2, 1, -2 }, { 1.5f, 2.0f, 2.0f }, true, true);
			reg.get<EnemyStats>(bridge).hp = 200;
		}
		static void BuildBossConstruct(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 700;
			stats.speed = 5.0f; // 2.5 -> 5.0
			stats.scoreValue = 4000;
			reg.emplace<Collider>(root, Collider::CreateSphere(2.5f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Construct";
			AddPart(reg, root, GeoShape::Sphere, { 1,0.5f,0,1 }, { 0,0,0 }, { 2.5f, 2.5f, 2.5f }, false);
			for (int i = 0; i < 8; ++i) {
				float angle = i * 0.785f; float r = 3.0f;
				Entity arm = AddPart(reg, root, GeoShape::Cube, { 1,0.2f,0,1 }, { sinf(angle) * r, 0, cosf(angle) * r }, { 0.5f, 0.5f, 4.0f }, true, true);
				reg.get<Transform>(arm).rotation.y = XMConvertToDegrees(angle);
			}
		}
		static void BuildBossOmega(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 3000;
			stats.speed = 2.0f; // 0.5 -> 2.0
			stats.scoreValue = 10000; rb.mass = 50.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(5.0f, 5.0f, 5.0f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Omega";
			AddPart(reg, root, GeoShape::Cube, { 1,0,0,1 }, { 0,0,0 }, { 4,4,4 }, false);
			Entity c2 = AddPart(reg, root, GeoShape::Cube, { 1,0,0,0.5f }, { 0,0,0 }, { 5,5,5 }, false);
			reg.get<Transform>(c2).rotation = { 45, 45, 45 };
			Entity r1 = AddPart(reg, root, GeoShape::Torus, { 1,0,0,0.5f }, { 0,0,0 }, { 10,10,0.5f }, false);
			Entity r2 = AddPart(reg, root, GeoShape::Torus, { 1,0,0,0.5f }, { 0,0,0 }, { 12,12,0.5f }, false);
			reg.get<Transform>(r2).rotation = { 90,0,0 };
			for (int i = 0; i < 6; ++i) {
				float a = i * 1.047f;
				Entity shield = AddPart(reg, root, GeoShape::Sphere, { 0,1,1,1 }, { sinf(a) * 8, 0, cosf(a) * 8 }, { 1.5f,1.5f,1.5f }, true, true);
				reg.get<EnemyStats>(shield).hp = 300;
			}
		}
		static void BuildBossTitan(Registry& reg, Entity root, EnemyStats& stats, Rigidbody& rb) {
			stats.hp = 5000; stats.maxHp = 5000;
			stats.speed = 3.0f; // 1.5 -> 3.0
			stats.scoreValue = 20000; rb.mass = 100.0f;
			reg.emplace<Collider>(root, Collider::CreateBox(4.0f, 6.0f, 3.0f, Layer::Enemy));
			auto& ai = reg.emplace<BossAI>(root); ai.bossName = "Titan";
			AddPart(reg, root, GeoShape::Cube, { 0.1f, 0.1f, 0.1f, 1 }, { 0, 4, 0 }, { 5, 7, 4 }, false);
			AddPart(reg, root, GeoShape::Diamond, { 1.0f, 0.2f, 0.0f, 1 }, { 0, 5, 2.1f }, { 1.5f, 2.5f, 1.0f }, false);
			Entity head = AddPart(reg, root, GeoShape::Cube, { 0.8f, 0.8f, 0.8f, 1 }, { 0, 8.5f, 0 }, { 2, 2, 2 }, true, true);
			reg.get<EnemyStats>(head).hp = 800;
			Entity rArm = AddPart(reg, root, GeoShape::Cube, { 0.3f, 0.3f, 0.4f, 1 }, { 4.5f, 5, 1 }, { 3, 6, 3 }, true, true);
			reg.get<EnemyStats>(rArm).hp = 1000;
			AddPart(reg, root, GeoShape::Sphere, { 0.3f, 0.3f, 0.4f, 1 }, { 4.0f, 7, 0 }, { 2.5f, 2.5f, 2.5f }, false);
			Entity lArm = AddPart(reg, root, GeoShape::Cube, { 0.3f, 0.3f, 0.4f, 1 }, { -4.5f, 5, 1 }, { 3, 6, 3 }, true, true);
			reg.get<EnemyStats>(lArm).hp = 1000;
			AddPart(reg, root, GeoShape::Sphere, { 0.3f, 0.3f, 0.4f, 1 }, { -4.0f, 7, 0 }, { 2.5f, 2.5f, 2.5f }, false);
			AddPart(reg, root, GeoShape::Cube, { 0.1f, 0.1f, 0.1f, 1 }, { 2.0f, 0, 0 }, { 2.5f, 8, 3.5f }, false);
			AddPart(reg, root, GeoShape::Cube, { 0.1f, 0.1f, 0.1f, 1 }, { -2.0f, 0, 0 }, { 2.5f, 8, 3.5f }, false);
			Entity wing = AddPart(reg, root, GeoShape::Diamond, { 1, 0, 0, 1 }, { 0, 6, -3.0f }, { 8, 5, 1 }, false);
			reg.get<Transform>(wing).rotation.x = -20.0f;
		}
	};
}