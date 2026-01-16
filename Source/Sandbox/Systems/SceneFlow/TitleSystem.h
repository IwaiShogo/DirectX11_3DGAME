#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Core/GameSession.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

namespace Arche
{
	// 初期化判定用のタグ
	struct TitleSceneTag {};

	struct TitleComponents
	{
		bool isWarping = false;
		float stateTimer = 0.0f;

		Entity camera = NullEntity;

		struct GlitchLayer { Entity e; XMFLOAT4 baseColor; };
		std::vector<GlitchLayer> logoLayers;

		Entity pressText = NullEntity;
		Entity pressBg = NullEntity;
		Entity scoreText = NullEntity;

		struct TerrainBar { Entity e; float x, z; };
		std::vector<TerrainBar> terrainBars;

		struct TunnelObj { Entity e; float z; float rotSpeed; int type; };
		std::vector<TunnelObj> tunnelObjs;

		struct Star { Entity e; float speed; float z; };
		std::vector<Star> stars;

		Entity energyCore = NullEntity;
	};

	class TitleSystem : public ISystem
	{
	public:
		TitleSystem() { m_systemName = "TitleSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& registry) override
		{
			static TitleComponents ctx;

			// ★ 修正箇所: 範囲for文でタグの存在を確認
			bool isInitialized = false;
			for (auto e : registry.view<TitleSceneTag>()) {
				isInitialized = true;
				break;
			}

			if (!isInitialized)
			{
				ctx = TitleComponents(); // データをリセット
				InitializeScene(registry, ctx);

				// 初期化済みタグを生成
				registry.emplace<TitleSceneTag>(registry.create());
			}

			float dt = Time::DeltaTime();
			float time = Time::TotalTime();
			if (ctx.isWarping) ctx.stateTimer += dt;

			float warpFactor = std::min(ctx.stateTimer / 1.5f, 1.0f);
			float baseSpeed = ctx.isWarping ? 100.0f : 15.0f;

			XMFLOAT4 themeColor = { 0.0f, 1.0f, 1.0f, 1.0f };
			if (ctx.isWarping) themeColor = { 1.0f, 1.0f - warpFactor, 1.0f, 1.0f };

			// 1. トンネル
			float tunnelLength = 100.0f;
			for (auto& obj : ctx.tunnelObjs)
			{
				if (registry.valid(obj.e))
				{
					auto& t = registry.get<Transform>(obj.e);
					auto& geo = registry.get<GeometricDesign>(obj.e);

					obj.z -= baseSpeed * dt;
					if (obj.z < -15.0f) {
						obj.z += tunnelLength;
						t.position.x = ((rand() % 400) / 10.0f) - 20.0f;
						t.position.y = ((rand() % 400) / 10.0f) - 20.0f;
						if (abs(t.position.x) < 5.0f && abs(t.position.y) < 5.0f) t.position.x += 10.0f;
					}
					t.position.z = obj.z;
					t.rotation.z += obj.rotSpeed * dt * (1.0f + warpFactor * 5.0f);
					t.rotation.x += obj.rotSpeed * 0.5f * dt;

					float distAlpha = 1.0f - (abs(obj.z) / 80.0f);
					distAlpha = std::clamp(distAlpha, 0.0f, 1.0f);

					float beat = 1.0f + 0.2f * sinf(time * 8.0f);

					t.scale = { 2.0f * beat, 2.0f * beat, 2.0f * beat };
					if (ctx.isWarping) t.scale = { 1.0f, 1.0f, 20.0f };

					geo.color = { themeColor.x, themeColor.y, themeColor.z, distAlpha * 0.8f };
				}
			}

			// 2. 地形
			float scroll = time * (ctx.isWarping ? 20.0f : 2.0f);
			for (auto& bar : ctx.terrainBars)
			{
				if (registry.valid(bar.e))
				{
					auto& t = registry.get<Transform>(bar.e);
					auto& geo = registry.get<GeometricDesign>(bar.e);
					float noise = sinf(bar.x * 0.2f + scroll) + cosf(bar.z * 0.2f + scroll * 0.7f);
					float height = 2.0f + noise * 1.5f;
					if (ctx.isWarping) height *= (1.0f + warpFactor * 2.0f);
					t.scale.y = height;
					t.position.y = -10.0f + height * 0.5f;
					float intensity = (height / 5.0f);
					geo.color = { themeColor.x * intensity, 0.0f, themeColor.z * intensity, 0.5f };
				}
			}

			// 3. ロゴ
			bool doGlitch = (fmod(time, 4.0f) < 0.1f) || ctx.isWarping;
			for (int i = 0; i < ctx.logoLayers.size(); ++i)
			{
				auto& layer = ctx.logoLayers[i];
				if (registry.valid(layer.e))
				{
					auto& t = registry.get<Transform>(layer.e);
					auto& txt = registry.get<TextComponent>(layer.e);
					float baseX = 0.0f;
					float baseY = 1.5f + sinf(time * 0.8f) * 0.1f;

					if (doGlitch) {
						t.position.x = baseX + ((rand() % 100) / 500.0f - 0.1f);
						t.position.y = baseY + ((rand() % 100) / 500.0f - 0.1f);
						if (i == 1) txt.color = { 0,1,1,0.8f }; else if (i == 2) txt.color = { 1,0,0,0.8f };
					}
					else {
						float drift = (i == 0) ? 0 : (i == 1 ? 0.01f : -0.01f);
						t.position = { baseX + drift, baseY, 0.0f };
						txt.color = layer.baseColor;
					}
					if (ctx.isWarping) {
						t.scale = { 1.0f + warpFactor * 3.0f, 1.0f + warpFactor * 3.0f, 1.0f };
						txt.color.w = std::max(0.0f, 1.0f - warpFactor);
					}
				}
			}

			// 4. UI
			if (registry.valid(ctx.pressText))
			{
				auto& t = registry.get<Transform>(ctx.pressText);
				t.position = { 0, -1.0f, -3.0f };
				float alpha = 0.7f + 0.3f * sinf(time * 8.0f);
				if (ctx.isWarping) alpha = 0.0f;
				registry.get<TextComponent>(ctx.pressText).color = { 1.0f, 1.0f, 1.0f, alpha };

				if (registry.valid(ctx.pressBg)) {
					auto& bgT = registry.get<Transform>(ctx.pressBg);
					bgT.position = { t.position.x, t.position.y, t.position.z + 0.01f };
					registry.get<GeometricDesign>(ctx.pressBg).color = { 0, 0, 0, alpha * 0.6f };
				}
			}
			if (registry.valid(ctx.scoreText))
			{
				registry.get<Transform>(ctx.scoreText).position = { 0, -2.5f, -3.0f };
				if (ctx.isWarping) registry.get<TextComponent>(ctx.scoreText).color.w = 0.0f;
			}

			// 5. エフェクト
			if (registry.valid(ctx.energyCore))
			{
				auto& t = registry.get<Transform>(ctx.energyCore);
				t.rotation.y += dt * 5.0f;
				float scale = 15.0f + sinf(time) * 1.0f + warpFactor * 50.0f;
				t.scale = { scale, scale, scale };
				registry.get<GeometricDesign>(ctx.energyCore).color = { themeColor.x, 0.0f, themeColor.z, 0.3f };
			}

			// 6. 星
			for (auto& star : ctx.stars) {
				if (registry.valid(star.e)) {
					auto& t = registry.get<Transform>(star.e);
					star.z -= star.speed * dt * (ctx.isWarping ? 20.0f : 1.0f);
					if (star.z < -15.0f) star.z = 60.0f;
					t.position.z = star.z;
				}
			}

			// 7. カメラ
			if (registry.valid(ctx.camera))
			{
				auto& t = registry.get<Transform>(ctx.camera);
				float shake = ctx.isWarping ? (rand() % 100 / 200.0f) : sinf(time * 0.5f) * 0.05f;
				t.position = { shake, 1.0f + shake, -12.0f };
			}

			// 遷移
			if (!ctx.isWarping && (Input::GetKeyDown(VK_SPACE) || Input::GetButtonDown(Button::A)))
			{
				ctx.isWarping = true;
				ctx.stateTimer = 0.0f;
			}

			if (ctx.isWarping && ctx.stateTimer > 1.2f)
			{
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/StageSelectScene.json", new FadeTransition(0.5f, { 1,1,1,1 }));
			}
		}

	private:
		void InitializeScene(Registry& reg, TitleComponents& ctx)
		{
			// カメラ
			{
				ctx.camera = reg.create();
				reg.emplace<Camera>(ctx.camera);
				reg.emplace<Transform>(ctx.camera);
			}

			// トンネル
			for (int i = 0; i < 30; ++i)
			{
				Entity e = reg.create();
				float z = 80.0f - (i * 3.5f);
				float x = ((rand() % 400) / 10.0f) - 20.0f;
				float y = ((rand() % 400) / 10.0f) - 20.0f;
				if (abs(x) < 5.0f && abs(y) < 5.0f) x += 10.0f;

				reg.emplace<Transform>(e).position = { x, y, z };
				reg.get<Transform>(e).scale = { 2.0f, 2.0f, 2.0f };

				auto& geo = reg.emplace<GeometricDesign>(e);
				int type = rand() % 3;
				geo.shapeType = (type == 0) ? GeoShape::Cube : (type == 1 ? GeoShape::Diamond : GeoShape::Torus);
				geo.isWireframe = true;
				geo.color = { 0, 1, 1, 1 };

				TitleComponents::TunnelObj obj;
				obj.e = e; obj.z = z; obj.rotSpeed = ((rand() % 100) / 10.0f) - 5.0f; obj.type = type;
				ctx.tunnelObjs.push_back(obj);
			}

			// 地形
			int rows = 25; int cols = 30; float spacing = 2.0f;
			for (int z = 0; z < rows; ++z) {
				for (int x = -cols / 2; x < cols / 2; ++x) {
					if (abs(x) < 2) continue;
					Entity e = reg.create();
					float posX = x * spacing; float posZ = z * spacing;
					reg.emplace<Transform>(e).position = { posX, -10.0f, posZ };
					reg.get<Transform>(e).scale = { 1.8f, 1.0f, 1.8f };
					auto& geo = reg.emplace<GeometricDesign>(e);
					geo.shapeType = GeoShape::Cube;
					geo.isWireframe = true;
					geo.color = { 0, 1, 1, 1 };
					TitleComponents::TerrainBar bar; bar.e = e; bar.x = posX; bar.z = posZ;
					ctx.terrainBars.push_back(bar);
				}
			}

			// 星
			for (int i = 0; i < 60; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { ((rand() % 600) / 5.0f) - 60.0f, ((rand() % 400) / 5.0f) - 20.0f, (float)(rand() % 60) };
				reg.get<Transform>(e).scale = { 0.1f, 0.1f, 0.5f };
				reg.emplace<GeometricDesign>(e).shapeType = GeoShape::Cube;
				reg.get<GeometricDesign>(e).color = { 1,1,1,1 };
				TitleComponents::Star s; s.e = e; s.z = 0; s.speed = 10.0f + rand() % 20;
				ctx.stars.push_back(s);
			}

			// エネルギーコア
			{
				ctx.energyCore = reg.create();
				reg.emplace<Transform>(ctx.energyCore).position = { 0, 2.0f, 40.0f };
				reg.get<Transform>(ctx.energyCore).scale = { 15.0f, 15.0f, 15.0f };
				auto& geo = reg.emplace<GeometricDesign>(ctx.energyCore);
				geo.shapeType = GeoShape::Sphere;
				geo.isWireframe = true;
				geo.color = { 1, 0, 1, 0.5f };
			}

			// ロゴ
			auto CreateText = [&](const std::string& txt, XMFLOAT4 col) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { 0, 10.0f, 0 };
				auto& t = reg.emplace<TextComponent>(e);
				t.text = txt; t.fontKey = "Makinas 4 Square"; t.fontSize = 80.0f; t.isBold = true; t.centerAlign = true; t.color = col;
				TitleComponents::GlitchLayer l; l.e = e; l.baseColor = col;
				ctx.logoLayers.push_back(l);
				};
			CreateText("5 SECONDS", { 1,1,1,1 });
			CreateText("5 SECONDS", { 0,1,1,0.5f });
			CreateText("5 SECONDS", { 1,0,1,0.5f });

			// UI
			{
				ctx.pressText = reg.create();
				reg.emplace<Transform>(ctx.pressText).position = { 0, -1.0f, -3.0f };
				auto& t = reg.emplace<TextComponent>(ctx.pressText);
				t.text = "PRESS SPACE"; t.fontKey = "Makinas 4 Square"; t.fontSize = 32.0f; t.centerAlign = true; t.color = { 1, 1, 1, 1 }; t.isBold = true;

				ctx.pressBg = reg.create();
				reg.emplace<Transform>(ctx.pressBg).scale = { 4.0f, 0.8f, 0.01f };
				auto& geo = reg.emplace<GeometricDesign>(ctx.pressBg);
				geo.shapeType = GeoShape::Cube; geo.color = { 0, 0, 0, 0.5f };
			}
			{
				ctx.scoreText = reg.create();
				reg.emplace<Transform>(ctx.scoreText).position = { 0, -2.5f, -3.0f };
				auto& t = reg.emplace<TextComponent>(ctx.scoreText);
				t.text = "HIGHSCORE: " + std::to_string(GameSession::lastScore);
				t.fontKey = "Makinas 4 Square";
				t.fontSize = 20.0f; t.centerAlign = true; t.color = { 1.0f, 0.8f, 0.0f, 1.0f };
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TitleSystem, "TitleSystem")