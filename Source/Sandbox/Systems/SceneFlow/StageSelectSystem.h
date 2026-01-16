#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <DirectXMath.h>

using namespace DirectX;

// --------------------------------------------------------------------------
// 演出用ヘルパー
// --------------------------------------------------------------------------
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float RandF(float min, float max) { return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min)); }

namespace Arche
{
	struct StageSelectTag {};

	enum class SelectState { Boot, Idle, Switching, Locked, Decided };

	struct StageInfo {
		int id;
		std::string name;
		std::string desc;
		std::string enemyName;
		GeoShape icon;
		GeoShape bossShape;
		int difficulty;
	};

	// 文字送り演出
	struct Typewriter {
		std::string targetText;
		float progress = 0.0f;

		void Set(const std::string& text) {
			if (targetText != text) { targetText = text; progress = 0.0f; }
		}
		void Update(float dt) {
			if (progress < targetText.length()) progress += dt * 50.0f;
		}
		std::string Get() const {
			size_t len = std::min((size_t)progress, targetText.length());
			return targetText.substr(0, len) + (fmod(progress, 15.0f) > 7.0f ? "_" : "");
		}
	};

	struct SelectContext
	{
		SelectState state = SelectState::Boot;
		float stateTimer = 0.0f;
		float globalTime = 0.0f;

		int currentIndex = 0;
		float currentAngle = 0.0f;
		int maxClearedStage = 0;

		Entity camera = NullEntity;

		// ステージノード
		struct StageNode {
			Entity root;
			Entity visual;
			Entity ring;
			Entity label;
			StageInfo info;
			bool isLocked;
		};
		std::vector<StageNode> nodes;

		// 中央ホログラム
		Entity centerHoloRoot = NullEntity;
		struct HoloPart { Entity e; XMFLOAT3 basePos; XMFLOAT3 scale; float rotSpeed; };
		std::vector<HoloPart> holoParts;
		Entity scanPlane = NullEntity;

		// ロックオン・レティクル
		struct ReticlePart { Entity e; XMFLOAT3 offset; };
		Entity reticleRoot = NullEntity;
		std::vector<ReticlePart> reticleParts;

		// UI (HUD)
		Entity uiRoot = NullEntity;
		Entity uiHeaderBg = NullEntity;
		Entity uiFooterBg = NullEntity;
		Entity uiTitle = NullEntity;
		Entity uiDesc = NullEntity;
		Entity uiThreatLabel = NullEntity;
		std::vector<Entity> uiThreatBars;

		Typewriter twTitle;
		Typewriter twDesc;

		// 背景・環境 (豪華版)
		struct StreamLine { Entity e; float speed; float y; };
		std::vector<StreamLine> streamLines;

		struct Debris { Entity e; XMFLOAT3 axis; float speed; }; // 浮遊デブリ
		std::vector<Debris> debrisList;

		struct BgRing { Entity e; XMFLOAT3 axis; float speed; }; // 背景リング
		std::vector<BgRing> bgRings;

		std::vector<Entity> floorHexes;
	};

	class StageSelectSystem : public ISystem
	{
	public:
		StageSelectSystem() { m_systemName = "StageSelectSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static SelectContext ctx;

			// 初期化チェック
			bool init = false;
			for (auto e : reg.view<StageSelectTag>()) { init = true; break; }
			if (!init) {
				ctx = SelectContext();
				ctx.maxClearedStage = 4; // TODO: GameSession連携
				InitScene(reg, ctx);

				AudioManager::Instance().PlayBGM("bgm_stageselect.wav", 0.5f);
				reg.emplace<StageSelectTag>(reg.create());
			}

			float dt = Time::DeltaTime();
			ctx.globalTime += dt;
			ctx.stateTimer += dt;

			UpdateInput(reg, ctx);
			UpdateCarousel(reg, ctx, dt);
			UpdateCenterHologram(reg, ctx, dt);
			UpdateReticle(reg, ctx, dt);
			UpdateBackground(reg, ctx, dt);
			UpdateUI(reg, ctx, dt);
			UpdateCamera(reg, ctx, dt);

			// 遷移
			if (ctx.state == SelectState::Decided && ctx.stateTimer > 1.5f)
			{
				GameSession::selectedStageId = ctx.nodes[ctx.currentIndex].info.id;
				AudioManager::Instance().StopBGM();
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/GameScene.json", new FadeTransition(0.3f, { 1,1,1,1 }));
			}
		}

	private:
		void InitScene(Registry& reg, SelectContext& ctx)
		{
			// カメラ (少し遠くへ)
			ctx.camera = reg.create();
			reg.emplace<Camera>(ctx.camera);
			reg.emplace<Transform>(ctx.camera).position = { 0, 8, -20 };

			// ステージデータ
			std::vector<StageInfo> data = {
				{ 1, "SECTOR 01: ORIGIN", "Training Simulation.\nTarget: Drone Swarm", "DRONE", GeoShape::Cube, GeoShape::Cube, 1 },
				{ 2, "SECTOR 02: FORGE", "Heavy Industry Zone.\nTarget: Armored Core", "HEAVY", GeoShape::Pyramid, GeoShape::Cube, 2 },
				{ 3, "SECTOR 03: STREAM", "High-speed Data Lane.\nTarget: Speeder", "SPEEDER", GeoShape::Torus, GeoShape::Cylinder, 3 },
				{ 4, "SECTOR 04: HIVE", "Bio-Digital Cluster.\nTarget: Hive Mind", "SWARM", GeoShape::Diamond, GeoShape::Sphere, 4 },
				{ 5, "SECTOR 05: CORE", "Central Processor.\nTarget: OMEGA", "OMEGA", GeoShape::Sphere, GeoShape::Sphere, 5 },
			};

			// ノード生成
			for (const auto& d : data) {
				SelectContext::StageNode node;
				node.info = d;
				node.isLocked = (d.id > ctx.maxClearedStage + 1);

				node.root = reg.create();
				reg.emplace<Transform>(node.root);

				// ビジュアル
				node.visual = reg.create();
				reg.emplace<Transform>(node.visual);
				auto& g = reg.emplace<GeometricDesign>(node.visual);
				g.shapeType = d.icon;
				g.isWireframe = true;

				// リング
				node.ring = reg.create();
				reg.emplace<Transform>(node.ring);
				auto& gr = reg.emplace<GeometricDesign>(node.ring);
				gr.shapeType = GeoShape::Torus;
				gr.isWireframe = true;

				// ラベル
				node.label = reg.create();
				reg.emplace<Transform>(node.label);
				auto& txt = reg.emplace<TextComponent>(node.label);
				txt.text = std::to_string(d.id);
				txt.fontKey = "Makinas 4 Square";
				txt.fontSize = 24.0f;
				txt.centerAlign = true;

				ctx.nodes.push_back(node);
			}

			// 中央ホログラムルート
			ctx.centerHoloRoot = reg.create();
			reg.emplace<Transform>(ctx.centerHoloRoot);

			// スキャン光
			ctx.scanPlane = reg.create();
			reg.emplace<Transform>(ctx.scanPlane).scale = { 4.0f, 0.05f, 4.0f };
			auto& sp = reg.emplace<GeometricDesign>(ctx.scanPlane);
			sp.shapeType = GeoShape::Cylinder;
			sp.color = { 0, 1, 1, 0.3f };
			sp.isWireframe = false;

			// ロックオン・レティクル
			ctx.reticleRoot = reg.create();
			reg.emplace<Transform>(ctx.reticleRoot);
			float rOffset = 1.8f; // 少し広げる
			for (int i = 0; i < 4; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e);
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube;
				g.color = { 1, 0.5f, 0, 1 };

				SelectContext::ReticlePart p;
				p.e = e;
				float angle = XM_PIDIV2 * i + XM_PIDIV4;
				p.offset = { cosf(angle) * rOffset, sinf(angle) * rOffset, 0 };
				ctx.reticleParts.push_back(p);
			}

			// UI構築 (HUD)
			ctx.uiRoot = reg.create();
			reg.emplace<Transform>(ctx.uiRoot);

			// ヘッダー背景
			ctx.uiHeaderBg = reg.create();
			reg.emplace<Transform>(ctx.uiHeaderBg).scale = { 12.0f, 1.5f, 0.1f };
			auto& hbg = reg.emplace<GeometricDesign>(ctx.uiHeaderBg);
			hbg.shapeType = GeoShape::Cube;
			hbg.color = { 0.0f, 0.05f, 0.1f, 0.9f };

			// フッター背景（幅広に）
			ctx.uiFooterBg = reg.create();
			reg.emplace<Transform>(ctx.uiFooterBg).scale = { 14.0f, 3.5f, 0.1f };
			auto& fbg = reg.emplace<GeometricDesign>(ctx.uiFooterBg);
			fbg.shapeType = GeoShape::Cube;
			fbg.color = { 0.0f, 0.05f, 0.1f, 0.9f };

			auto CreateText = [&](const std::string& key, float size) {
				Entity e = reg.create();
				reg.emplace<Transform>(e);
				auto& t = reg.emplace<TextComponent>(e);
				t.fontKey = key; t.fontSize = size; t.centerAlign = true;
				return e;
				};

			ctx.uiTitle = CreateText("Makinas 4 Square", 42.0f);
			reg.get<TextComponent>(ctx.uiTitle).isBold = true;

			ctx.uiDesc = CreateText("Makinas 4 Square", 24.0f);

			ctx.uiThreatLabel = CreateText("Makinas 4 Square", 18.0f);
			reg.get<TextComponent>(ctx.uiThreatLabel).text = "THREAT LEVEL";
			reg.get<TextComponent>(ctx.uiThreatLabel).color = { 0.5f, 0.5f, 0.5f, 1.0f };

			// 難易度バー
			for (int i = 0; i < 5; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e);
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube;
				g.color = { 0.2f, 0.2f, 0.2f, 1 };
				ctx.uiThreatBars.push_back(e);
			}

			// --- 豪華背景エフェクト構築 ---

			// 1. データストリーム (数増加)
			for (int i = 0; i < 50; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).scale = { 0.05f, RandF(3.0f, 8.0f), 0.05f };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cylinder;
				g.color = { 0, 0.5f, 1.0f, 0.2f };
				g.isWireframe = true;

				SelectContext::StreamLine l;
				l.e = e; l.speed = RandF(8.0f, 20.0f); l.y = RandF(-20.0f, 20.0f);
				float r = RandF(15.0f, 30.0f); float ang = RandF(0, XM_2PI);
				reg.get<Transform>(e).position = { cosf(ang) * r, l.y, sinf(ang) * r };
				ctx.streamLines.push_back(l);
			}

			// 2. 浮遊デブリ (空間の密度上げ)
			for (int i = 0; i < 60; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { RandF(-30,30), RandF(-15,15), RandF(-10, 40) };
				float s = RandF(0.2f, 0.8f);
				reg.get<Transform>(e).scale = { s, s, s };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = (rand() % 2 == 0) ? GeoShape::Cube : GeoShape::Pyramid;
				g.color = { 0.1f, 0.3f, 0.5f, 0.3f };
				g.isWireframe = true;

				SelectContext::Debris d;
				d.e = e;
				d.speed = RandF(0.5f, 2.0f);
				d.axis = { RandF(-1,1), RandF(-1,1), RandF(-1,1) };
				ctx.debrisList.push_back(d);
			}

			// 3. 背景ジャイロリング (巨大構造物)
			for (int i = 0; i < 3; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { 0, 0, 20.0f }; // 奥に配置
				float s = 25.0f + i * 5.0f;
				reg.get<Transform>(e).scale = { s, s, 0.5f };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Torus;
				g.color = { 0.0f, 0.1f, 0.2f, 0.2f }; // 薄く
				g.isWireframe = true;

				SelectContext::BgRing r;
				r.e = e;
				r.speed = (i % 2 == 0 ? 1 : -1) * RandF(0.1f, 0.3f);
				r.axis = { (i == 0 ? 1.0f : 0.0f), (i == 1 ? 1.0f : 0.0f), (i == 2 ? 1.0f : 0.0f) }; // 軸をずらす
				ctx.bgRings.push_back(r);
			}

			// 4. 床ヘックス (広範囲に)
			for (int x = -8; x <= 8; ++x) {
				for (int z = -5; z <= 10; ++z) { // 奥まで
					if (abs(x) + abs(z) < 4) continue;
					Entity e = reg.create();
					reg.emplace<Transform>(e).position = { x * 2.0f + (z % 2) * 1.0f, -6.0f, z * 1.8f }; // Yを下げる
					reg.get<Transform>(e).scale = { 1.0f, 0.1f, 1.0f };
					auto& g = reg.emplace<GeometricDesign>(e);
					g.shapeType = GeoShape::Cylinder;
					g.isWireframe = true;
					g.color = { 0, 0.1f, 0.2f, 0.1f };
					ctx.floorHexes.push_back(e);
				}
			}
		}

		void UpdateInput(Registry& reg, SelectContext& ctx)
		{
			if (ctx.state == SelectState::Boot) {
				if (ctx.stateTimer > 1.0f) { ctx.state = SelectState::Idle; ctx.stateTimer = 0.0f; }
				return;
			}
			if (ctx.state == SelectState::Decided) return;
			if (ctx.state == SelectState::Locked && ctx.stateTimer > 0.3f) ctx.state = SelectState::Idle;

			if (ctx.state == SelectState::Idle)
			{
				int n = (int)ctx.nodes.size();
				if (Input::GetButtonDown(Button::Right) || Input::GetKeyDown('D') || Input::GetKeyDown(VK_RIGHT)) {
					ctx.currentIndex = (ctx.currentIndex + 1) % n;
					ctx.state = SelectState::Switching; ctx.stateTimer = 0.0f;
				}
				else if (Input::GetButtonDown(Button::Left) || Input::GetKeyDown('A') || Input::GetKeyDown(VK_LEFT)) {
					ctx.currentIndex = (ctx.currentIndex - 1 + n) % n;
					ctx.state = SelectState::Switching; ctx.stateTimer = 0.0f;
				}
				else if (Input::GetButtonDown(Button::A) || Input::GetKeyDown(VK_SPACE)) {
					AudioManager::Instance().PlaySE("se_start.wav", 0.5f);
					if (ctx.nodes[ctx.currentIndex].isLocked) {
						ctx.state = SelectState::Locked; ctx.stateTimer = 0.0f;
					}
					else {
						ctx.state = SelectState::Decided; ctx.stateTimer = 0.0f;
					}
				}
			}
			else if (ctx.state == SelectState::Switching) {
				AudioManager::Instance().PlaySE("se_dash.wav", 0.3f);
				if (ctx.stateTimer > 0.15f) ctx.state = SelectState::Idle;
			}
		}

		void UpdateCarousel(Registry& reg, SelectContext& ctx, float dt)
		{
			float step = XM_2PI / ctx.nodes.size();
			float target = ctx.currentIndex * step;
			float diff = target - ctx.currentAngle;
			while (diff <= -XM_PI) diff += XM_2PI;
			while (diff > XM_PI) diff -= XM_2PI;
			ctx.currentAngle += diff * dt * 8.0f;

			float radius = 7.0f;
			if (ctx.state == SelectState::Decided) radius += dt * 30.0f;

			XMFLOAT3 camRot = reg.get<Transform>(ctx.camera).rotation;

			// カルーセル全体を上に持ち上げる (Y=2.0f) ことでUIとの被りを回避
			float carouselBaseY = 2.0f;

			for (int i = 0; i < ctx.nodes.size(); ++i)
			{
				auto& node = ctx.nodes[i];
				bool isSelected = (i == ctx.currentIndex);
				float angle = i * step - ctx.currentAngle - XM_PIDIV2;
				float x = cosf(angle) * radius;
				float z = sinf(angle) * radius;

				// ルート
				if (reg.valid(node.root)) {
					auto& t = reg.get<Transform>(node.root);
					t.position = { x, carouselBaseY, z };
					t.rotation.y = -angle - XM_PIDIV2;
					if (isSelected && ctx.state == SelectState::Locked) t.position.x += RandF(-0.2f, 0.2f);
				}

				// ビジュアル
				if (reg.valid(node.visual)) {
					auto& t = reg.get<Transform>(node.visual);
					t.position = { x, carouselBaseY, z };
					auto& g = reg.get<GeometricDesign>(node.visual);

					if (isSelected) {
						t.scale = { 1.5f, 1.5f, 1.5f };
						t.rotation.y += dt * 2.0f;
						g.color = node.isLocked ? XMFLOAT4{ 1,0,0,1 } : XMFLOAT4{ 0,1,1,1 };
						g.isWireframe = node.isLocked;
					}
					else {
						t.scale = { 1.0f, 1.0f, 1.0f };
						t.rotation.y = -angle;
						g.color = { 0.2f, 0.4f, 0.6f, 0.3f };
						g.isWireframe = true;
					}
				}

				// リング
				if (reg.valid(node.ring)) {
					auto& t = reg.get<Transform>(node.ring);
					t.position = { x, carouselBaseY, z };
					float s = isSelected ? 2.5f : 0.0f;
					t.scale = { s, s, 0.1f };
					t.rotation.z += dt;
					reg.get<GeometricDesign>(node.ring).color = node.isLocked ? XMFLOAT4{ 1,0,0,0.3f } : XMFLOAT4{ 0,1,1,0.3f };
				}

				// ラベル
				if (reg.valid(node.label)) {
					auto& t = reg.get<Transform>(node.label);
					t.position = { x, carouselBaseY + 2.5f, z };
					t.rotation = camRot;
					auto& txt = reg.get<TextComponent>(node.label);
					txt.color.w = isSelected ? 1.0f : 0.3f;
				}
			}
		}

		void UpdateCenterHologram(Registry& reg, SelectContext& ctx, float dt)
		{
			static int lastIdx = -1;
			auto& stage = ctx.nodes[ctx.currentIndex];

			// 再構築
			if (lastIdx != ctx.currentIndex || ctx.state == SelectState::Boot)
			{
				lastIdx = ctx.currentIndex;
				for (auto& p : ctx.holoParts) reg.destroy(p.e);
				ctx.holoParts.clear();

				int parts = stage.isLocked ? 20 : 10;
				GeoShape shape = stage.isLocked ? GeoShape::Pyramid : stage.info.bossShape;

				for (int i = 0; i < parts; ++i) {
					Entity e = reg.create();
					reg.emplace<Transform>(e);
					auto& g = reg.emplace<GeometricDesign>(e);
					g.shapeType = shape;
					g.isWireframe = true;
					g.color = stage.isLocked ? XMFLOAT4{ 1,0,0,0.5f } : XMFLOAT4{ 0,1,1,0.8f };

					SelectContext::HoloPart p;
					p.e = e;
					if (i == 0 && !stage.isLocked) {
						p.basePos = { 0,2.0f,0 }; // 中央少し上
						p.scale = { 2.5f, 2.5f, 2.5f }; p.rotSpeed = 0.5f;
					}
					else {
						float r = RandF(1.5f, 3.5f); float th = RandF(0, XM_2PI);
						p.basePos = { cosf(th) * r, 2.0f + RandF(-1.5f, 1.5f), sinf(th) * r };
						p.scale = { 0.2f, 0.2f, 0.2f }; p.rotSpeed = RandF(1.0f, 3.0f);
					}
					ctx.holoParts.push_back(p);
				}
			}

			// アニメーション
			if (reg.valid(ctx.centerHoloRoot))
			{
				auto& rootT = reg.get<Transform>(ctx.centerHoloRoot);
				rootT.position = { 0, 0, 0 };

				if (ctx.state == SelectState::Decided) rootT.position.y += dt * 15.0f;
				else rootT.position.y = sinf(ctx.globalTime) * 0.2f;

				for (auto& p : ctx.holoParts) {
					if (reg.valid(p.e)) {
						auto& t = reg.get<Transform>(p.e);
						float rot = ctx.globalTime * p.rotSpeed;
						if (stage.isLocked) rot *= 5.0f;

						float c = cosf(rot); float s = sinf(rot);
						float x = p.basePos.x * c - p.basePos.z * s;
						float z = p.basePos.x * s + p.basePos.z * c;

						t.position = { x, rootT.position.y + p.basePos.y, z };
						t.scale = p.scale;
						t.rotation.y += dt * p.rotSpeed;

						if (stage.isLocked) {
							t.position.x += RandF(-0.1f, 0.1f);
							t.scale.x *= RandF(0.8f, 1.2f);
						}
					}
				}
			}

			if (reg.valid(ctx.scanPlane)) {
				auto& t = reg.get<Transform>(ctx.scanPlane);
				t.position.y = 0.0f + fmod(ctx.globalTime * 3.0f, 5.0f);
				reg.get<GeometricDesign>(ctx.scanPlane).color = stage.isLocked ? XMFLOAT4{ 1,0,0,0.2f } : XMFLOAT4{ 0,1,1,0.2f };
			}
		}

		void UpdateReticle(Registry& reg, SelectContext& ctx, float dt)
		{
			if (reg.valid(ctx.reticleRoot)) {
				auto& t = reg.get<Transform>(ctx.reticleRoot);
				// ホログラム位置（Y=2.0周辺）
				t.position = { 0, 2.0f + sinf(ctx.globalTime) * 0.2f, 0 };

				t.rotation.z += dt * (ctx.nodes[ctx.currentIndex].isLocked ? 5.0f : 0.5f);

				float scale = 1.0f;
				if (ctx.state == SelectState::Decided) scale = std::max(0.0f, 1.0f - ctx.stateTimer * 2.0f);
				t.scale = { scale, scale, scale };

				for (auto& p : ctx.reticleParts) {
					if (reg.valid(p.e)) {
						auto& tp = reg.get<Transform>(p.e);
						float rot = t.rotation.z;
						float c = cosf(rot); float s = sinf(rot);
						float x = p.offset.x * c - p.offset.y * s;
						float y = p.offset.x * s + p.offset.y * c;

						tp.position = { t.position.x + x, t.position.y + y, t.position.z };
						tp.rotation.z = rot;
						tp.scale = { 0.5f * scale, 0.05f * scale, 0.05f * scale };

						bool locked = ctx.nodes[ctx.currentIndex].isLocked;
						reg.get<GeometricDesign>(p.e).color = locked ? XMFLOAT4{ 1,0,0,1 } : XMFLOAT4{ 1,0.6f,0,1 };
					}
				}
			}
		}

		void UpdateBackground(Registry& reg, SelectContext& ctx, float dt)
		{
			// 1. ストリームライン (背景)
			for (auto& l : ctx.streamLines) {
				if (reg.valid(l.e)) {
					auto& t = reg.get<Transform>(l.e);
					l.y += l.speed * dt;
					if (l.y > 20.0f) l.y = -20.0f;
					t.position.y = l.y;
				}
			}

			// 2. 浮遊デブリ (回転移動)
			for (auto& d : ctx.debrisList) {
				if (reg.valid(d.e)) {
					auto& t = reg.get<Transform>(d.e);
					t.rotation.x += dt * d.axis.x;
					t.rotation.y += dt * d.axis.y;
					t.position.y += sinf(ctx.globalTime * d.speed) * dt * 0.5f;
				}
			}

			// 3. 背景リング (ゆっくり回転)
			for (auto& r : ctx.bgRings) {
				if (reg.valid(r.e)) {
					auto& t = reg.get<Transform>(r.e);
					t.rotation.z += dt * r.speed;
					t.rotation.x = XM_PIDIV2 * 0.5f + sinf(ctx.globalTime * 0.1f) * 0.1f;
				}
			}

			// 4. 床ヘックス (明滅)
			for (int i = 0; i < ctx.floorHexes.size(); ++i) {
				if (reg.valid(ctx.floorHexes[i])) {
					auto& g = reg.get<GeometricDesign>(ctx.floorHexes[i]);
					float wave = sinf(ctx.globalTime * 2.0f + i * 0.1f);
					g.color.w = 0.1f + (wave * 0.5f + 0.5f) * 0.2f;
				}
			}
		}

		void UpdateUI(Registry& reg, SelectContext& ctx, float dt)
		{
			if (!reg.valid(ctx.camera)) return;
			auto& camT = reg.get<Transform>(ctx.camera);
			XMFLOAT3 camRot = camT.rotation;

			XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(camRot.x, camRot.y, camRot.z);
			XMVECTOR fwd = XMVector3TransformCoord({ 0, 0, 1 }, rotMat);
			XMVECTOR up = XMVector3TransformCoord({ 0, 1, 0 }, rotMat);
			XMVECTOR right = XMVector3TransformCoord({ 1, 0, 0 }, rotMat);
			XMVECTOR camPos = XMLoadFloat3(&camT.position);

			auto Place = [&](Entity e, float x, float y, float z, float sx, float sy) {
				if (!reg.valid(e)) return;
				auto& t = reg.get<Transform>(e);
				XMVECTOR pos = camPos + fwd * z + right * x + up * y;
				XMStoreFloat3(&t.position, pos);
				t.rotation = camRot;
				t.scale = { sx, sy, 1.0f };

				float a = (ctx.state == SelectState::Decided) ? 0.0f : 1.0f;
				if (reg.has<TextComponent>(e)) reg.get<TextComponent>(e).color.w *= a;
				if (reg.has<GeometricDesign>(e)) reg.get<GeometricDesign>(e).color.w *= a;
				};

			auto& node = ctx.nodes[ctx.currentIndex];
			bool locked = node.isLocked;

			// --- ヘッダー ---
			// Y=3.5f は画面上部
			Place(ctx.uiHeaderBg, 0.0f, 3.5f, 10.0f, 14.0f, 1.5f);
			Place(ctx.uiTitle, 0.0f, 3.5f, 9.9f, 1.0f, 1.0f);

			// --- フッター (大幅に下げる) ---
			// Y=-5.0f まで下げることで、中央の惑星との被りを回避
			Place(ctx.uiFooterBg, 0.0f, -5.0f, 10.0f, 12.0f, 3.0f);

			// テキスト類もそれに合わせて配置
			Place(ctx.uiDesc, 0.0f, -4.5f, 9.9f, 1.0f, 1.0f);
			Place(ctx.uiThreatLabel, -4.0f, -5.8f, 9.9f, 1.0f, 1.0f);

			// テキスト更新
			ctx.twTitle.Set(locked ? "[[ RESTRICTED ]]" : node.info.name);
			ctx.twDesc.Set(locked ? "Authorization Required.\nClear previous mission to unlock." : node.info.desc + "\nTarget ID: " + node.info.enemyName);
			ctx.twTitle.Update(dt);
			ctx.twDesc.Update(dt);

			if (reg.valid(ctx.uiTitle)) {
				auto& t = reg.get<TextComponent>(ctx.uiTitle);
				t.text = ctx.twTitle.Get();
				t.color = locked ? XMFLOAT4{ 1,0,0,1 } : XMFLOAT4{ 0,1,1,1 };
			}
			if (reg.valid(ctx.uiDesc)) {
				reg.get<TextComponent>(ctx.uiDesc).text = ctx.twDesc.Get();
			}

			// 難易度バー (フッター内下部)
			int diff = node.isLocked ? 0 : node.info.difficulty;
			for (int i = 0; i < ctx.uiThreatBars.size(); ++i) {
				Place(ctx.uiThreatBars[i], -1.5f + i * 0.8f, -5.8f, 9.9f, 0.7f, 0.15f);
				if (reg.valid(ctx.uiThreatBars[i])) {
					auto& g = reg.get<GeometricDesign>(ctx.uiThreatBars[i]);
					if (i < diff) {
						g.color = { 1.0f, i * 0.2f, 0.0f, 1.0f };
						g.color.w = 0.8f + sinf(ctx.globalTime * 15.0f) * 0.2f;
					}
					else {
						g.color = { 0.2f, 0.2f, 0.2f, 0.5f };
					}
				}
			}
		}

		void UpdateCamera(Registry& reg, SelectContext& ctx, float dt)
		{
			if (!reg.valid(ctx.camera)) return;
			auto& t = reg.get<Transform>(ctx.camera);

			if (ctx.state == SelectState::Decided) {
				float p = ctx.stateTimer * ctx.stateTimer;
				t.position.z = Lerp(-20.0f, 5.0f, p);
				t.position.x = RandF(-0.1f, 0.1f) * p;
			}
			else {
				t.position.z = Lerp(t.position.z, -20.0f, dt * 2.0f);
				t.position.y = 8.0f + sinf(ctx.globalTime * 0.2f) * 0.3f;
				t.position.x = cosf(ctx.globalTime * 0.15f) * 0.5f;
				t.rotation.x = 0.35f;
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::StageSelectSystem, "StageSelectSystem")