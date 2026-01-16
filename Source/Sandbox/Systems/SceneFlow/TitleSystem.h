#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Core/GameSession.h"

namespace Arche
{
	// --------------------------------------------------------------------------
	// 演出用データ
	// --------------------------------------------------------------------------
	struct TitleComponents
	{
		bool isInitialized = false;
		bool isWarping = false;
		float stateTimer = 0.0f;

		Entity camera = NullEntity;

		// ロゴ（3層構造）
		struct GlitchLayer { Entity e; XMFLOAT4 baseColor; };
		std::vector<GlitchLayer> logoLayers;

		// UI
		Entity pressText = NullEntity;
		Entity pressBg = NullEntity;
		Entity scoreText = NullEntity;

		// 背景: サイバー・テライン（波打つ地形・床）
		struct TerrainBar { Entity e; float x, z; };
		std::vector<TerrainBar> terrainBars;

		// ★復活: トンネルオブジェクト（中央から迫ってくるランダムな物体）
		struct TunnelObj { Entity e; float z; float rotSpeed; int type; };
		std::vector<TunnelObj> tunnelObjs;

		// 背景: 星
		struct Star { Entity e; float speed; float z; };
		std::vector<Star> stars;

		// エフェクト: エネルギースフィア
		Entity energyCore = NullEntity;
	};

	// --------------------------------------------------------------------------
	// タイトル画面システム "5 SECONDS - Hyper Warp"
	// --------------------------------------------------------------------------
	class TitleSystem : public ISystem
	{
	public:
		TitleSystem()
		{
			m_systemName = "TitleSystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			static TitleComponents ctx;

			if (!ctx.isInitialized) {
				InitializeScene(registry, ctx);
				ctx.isInitialized = true;
			}

			float dt = Time::DeltaTime();
			float time = Time::TotalTime();
			if (ctx.isWarping) ctx.stateTimer += dt;

			// ワープ係数
			float warpFactor = std::min(ctx.stateTimer / 1.5f, 1.0f);

			// ベーススピード（通常時でも少し速くして疾走感を出す）
			float baseSpeed = ctx.isWarping ? 100.0f : 15.0f;

			// テーマカラー
			XMFLOAT4 themeColor = { 0.0f, 1.0f, 1.0f, 1.0f }; // Cyan
			if (ctx.isWarping) {
				themeColor = { 1.0f, 1.0f - warpFactor, 1.0f, 1.0f }; // Orange -> White
			}

			// =============================================================
			// 1. 無限トンネル (ランダムな物体が迫ってくる)
			// =============================================================
			float tunnelLength = 100.0f;
			for (auto& obj : ctx.tunnelObjs)
			{
				if (registry.valid(obj.e))
				{
					auto& t = registry.get<Transform>(obj.e);
					auto& geo = registry.get<GeometricDesign>(obj.e);

					// 手前に移動
					obj.z -= baseSpeed * dt;

					// ループ処理
					if (obj.z < -15.0f) {
						obj.z += tunnelLength;
						// 戻ったときに位置を再ランダム化して飽きさせない
						t.position.x = ((rand() % 400) / 10.0f) - 20.0f;
						t.position.y = ((rand() % 400) / 10.0f) - 20.0f;
						// 中央付近は空ける（ロゴが見えるように）
						if (abs(t.position.x) < 5.0f && abs(t.position.y) < 5.0f) {
							t.position.x += 10.0f;
						}
					}

					t.position.z = obj.z;

					// 回転
					t.rotation.z += obj.rotSpeed * dt * (1.0f + warpFactor * 5.0f);
					t.rotation.x += obj.rotSpeed * 0.5f * dt;

					// 距離フォグ & ビート点滅
					float distAlpha = std::clamp(1.0f - (abs(obj.z) / 80.0f), 0.0f, 1.0f);
					float beat = 1.0f + 0.2f * sinf(time * 8.0f); // 音楽のようなビート

					t.scale = { 2.0f * beat, 2.0f * beat, 2.0f * beat };
					if (ctx.isWarping) t.scale = { 1.0f, 1.0f, 20.0f }; // ワープ時は伸びる

					geo.color = { themeColor.x, themeColor.y, themeColor.z, distAlpha * 0.8f };
				}
			}

			// =============================================================
			// 2. 地形アニメーション (床)
			// =============================================================
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
					t.position.y = -10.0f + height * 0.5f; // トンネルの邪魔にならないよう少し下げる

					float intensity = (height / 5.0f);
					geo.color = { themeColor.x * intensity, 0.0f, themeColor.z * intensity, 0.5f };
				}
			}

			// =============================================================
			// 3. グリッチ・ロゴ "5 SECONDS"
			// =============================================================
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

					if (doGlitch)
					{
						t.position.x = baseX + ((rand() % 100) / 500.0f - 0.1f);
						t.position.y = baseY + ((rand() % 100) / 500.0f - 0.1f);
						if (i == 1) txt.color = { 0,1,1,0.8f };
						else if (i == 2) txt.color = { 1,0,0,0.8f };
					}
					else
					{
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

			// =============================================================
			// 4. UI配置 (被り修正)
			// =============================================================

			// PRESS SPACE (枠付き)
			if (registry.valid(ctx.pressText))
			{
				auto& t = registry.get<Transform>(ctx.pressText);
				auto& txt = registry.get<TextComponent>(ctx.pressText);

				// ★修正: Y座標を -1.0 に設定
				t.position = { 0, -1.0f, -3.0f };

				float alpha = 0.7f + 0.3f * sinf(time * 8.0f);
				if (ctx.isWarping) alpha = 0.0f;
				txt.color = { 1.0f, 1.0f, 1.0f, alpha };

				// 背景
				if (registry.valid(ctx.pressBg)) {
					auto& bgT = registry.get<Transform>(ctx.pressBg);
					bgT.position = { t.position.x, t.position.y, t.position.z + 0.01f };
					registry.get<GeometricDesign>(ctx.pressBg).color = { 0, 0, 0, alpha * 0.6f };
				}
			}

			// HIGHSCORE (位置調整)
			if (registry.valid(ctx.scoreText))
			{
				auto& t = registry.get<Transform>(ctx.scoreText);
				auto& txt = registry.get<TextComponent>(ctx.scoreText);

				// ★修正: Y座標を -2.5 に下げ、少し手前(-3.0)に持ってくることで重なり回避
				// またフォントサイズを少し小さくします
				t.position = { 0, -2.5f, -3.0f };

				if (ctx.isWarping) txt.color.w = 0.0f;
			}

			// =============================================================
			// 5. エネルギースフィア (背景)
			// =============================================================
			if (registry.valid(ctx.energyCore))
			{
				auto& t = registry.get<Transform>(ctx.energyCore);
				t.position = { 0, 2.0f, 40.0f };
				t.rotation.y += dt * 5.0f;
				float scale = 15.0f + sinf(time) * 1.0f + warpFactor * 50.0f;
				t.scale = { scale, scale, scale };
				auto& geo = registry.get<GeometricDesign>(ctx.energyCore);
				geo.color = { themeColor.x, 0.0f, themeColor.z, 0.3f };
			}

			// =============================================================
			// 6. 星
			// =============================================================
			for (auto& star : ctx.stars) {
				if (registry.valid(star.e)) {
					auto& t = registry.get<Transform>(star.e);
					star.z -= star.speed * dt * (ctx.isWarping ? 20.0f : 1.0f);
					if (star.z < -15.0f) star.z = 60.0f;
					t.position.z = star.z;
				}
			}

			// =============================================================
			// 7. カメラ (固定)
			// =============================================================
			if (registry.valid(ctx.camera))
			{
				auto& t = registry.get<Transform>(ctx.camera);
				float shake = ctx.isWarping ? (rand() % 100 / 200.0f) : sinf(time * 0.5f) * 0.05f;
				t.position = { shake, 1.0f + shake, -12.0f };
				t.rotation = { 0, 0, 0 };
			}

			// =============================================================
			// 8. 遷移
			// =============================================================
			if (!ctx.isWarping && (Input::GetKeyDown(VK_SPACE) || Input::GetButtonDown(Button::A)))
			{
				ctx.isWarping = true;
				ctx.stateTimer = 0.0f;
			}

			if (ctx.isWarping && ctx.stateTimer > 1.2f)
			{
				ctx.isInitialized = false;
				ctx.terrainBars.clear();
				ctx.tunnelObjs.clear(); // クリア
				ctx.stars.clear();
				ctx.logoLayers.clear();

				SceneManager::Instance().LoadScene("Resources/Game/Scenes/StageSelectScene.json", new FadeTransition(0.5f, { 1,1,1,1 }));
			}
		}

	private:
		void InitializeScene(Registry& reg, TitleComponents& ctx)
		{
			// 1. カメラ
			{
				bool found = false;
				for (auto e : reg.view<Camera>()) { ctx.camera = e; found = true; break; }
				if (!found) {
					ctx.camera = reg.create();
					reg.emplace<Camera>(ctx.camera);
					reg.emplace<Transform>(ctx.camera);
				}
			}

			// 2. ★復活: トンネルオブジェクト (迫ってくるモノ)
			int tunnelCount = 30;
			for (int i = 0; i < tunnelCount; ++i)
			{
				Entity e = reg.create();
				float z = 80.0f - (i * 3.5f); // 手前から奥へ配置
				float x = ((rand() % 400) / 10.0f) - 20.0f;
				float y = ((rand() % 400) / 10.0f) - 20.0f;

				// 中央（ロゴエリア）は避ける
				if (abs(x) < 5.0f && abs(y) < 5.0f) x += 10.0f;

				reg.emplace<Transform>(e).position = { x, y, z };
				reg.get<Transform>(e).scale = { 2.0f, 2.0f, 2.0f };

				auto& geo = reg.emplace<GeometricDesign>(e);
				int type = rand() % 3;
				if (type == 0) geo.shapeType = GeoShape::Cube;
				else if (type == 1) geo.shapeType = GeoShape::Diamond;
				else geo.shapeType = GeoShape::Torus;

				geo.isWireframe = true;
				geo.color = { 0, 1, 1, 1 };

				TitleComponents::TunnelObj obj;
				obj.e = e; obj.z = z; obj.rotSpeed = ((rand() % 100) / 10.0f) - 5.0f; obj.type = type;
				ctx.tunnelObjs.push_back(obj);
			}

			// 3. 地形バー (床)
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

			// 4. 星
			for (int i = 0; i < 60; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { ((rand() % 600) / 5.0f) - 60.0f, ((rand() % 400) / 5.0f) - 20.0f, (float)(rand() % 60) };
				reg.get<Transform>(e).scale = { 0.1f, 0.1f, 0.5f };
				reg.emplace<GeometricDesign>(e).shapeType = GeoShape::Cube;
				reg.get<GeometricDesign>(e).color = { 1,1,1,1 };
				TitleComponents::Star s; s.e = e; s.z = 0; s.speed = 10.0f + rand() % 20;
				ctx.stars.push_back(s);
			}

			// 5. エネルギースフィア
			{
				ctx.energyCore = reg.create();
				reg.emplace<Transform>(ctx.energyCore).position = { 0, 2.0f, 40.0f };
				reg.get<Transform>(ctx.energyCore).scale = { 15.0f, 15.0f, 15.0f };
				auto& geo = reg.emplace<GeometricDesign>(ctx.energyCore);
				geo.shapeType = GeoShape::Sphere;
				geo.isWireframe = true;
				geo.color = { 1, 0, 1, 0.5f };
			}

			// 6. ロゴ "5 SECONDS"
			auto CreateText = [&](const std::string& txt, XMFLOAT4 col) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).position = { 0, 1.5f, 0 };
				auto& t = reg.emplace<TextComponent>(e);
				t.text = txt;
				t.fontKey = "Consolas";
				t.fontSize = 80.0f;
				t.isBold = true;
				t.centerAlign = true;
				t.color = col;
				TitleComponents::GlitchLayer l; l.e = e; l.baseColor = col;
				ctx.logoLayers.push_back(l);
				};
			CreateText("5 SECONDS", { 1,1,1,1 });
			CreateText("5 SECONDS", { 0,1,1,0.5f });
			CreateText("5 SECONDS", { 1,0,1,0.5f });

			// 7. UI: PRESS SPACE (位置修正済み)
			{
				ctx.pressText = reg.create();
				reg.emplace<Transform>(ctx.pressText).position = { 0, -1.0f, -3.0f };
				auto& t = reg.emplace<TextComponent>(ctx.pressText);
				t.text = "PRESS SPACE";
				t.fontKey = "Consolas";
				t.fontSize = 32.0f;
				t.centerAlign = true;
				t.color = { 1, 1, 1, 1 };
				t.isBold = true;

				ctx.pressBg = reg.create();
				reg.emplace<Transform>(ctx.pressBg).scale = { 4.0f, 0.8f, 0.01f };
				auto& geo = reg.emplace<GeometricDesign>(ctx.pressBg);
				geo.shapeType = GeoShape::Cube;
				geo.color = { 0, 0, 0, 0.5f };
			}

			// 8. ハイスコア (位置修正済み)
			{
				ctx.scoreText = reg.create();
				// Press Space(-1.0) よりさらに下(-2.5)へ配置
				reg.emplace<Transform>(ctx.scoreText).position = { 0, -2.5f, -3.0f };
				auto& t = reg.emplace<TextComponent>(ctx.scoreText);
				t.text = "HIGHSCORE: " + std::to_string(GameSession::lastScore);
				t.fontSize = 20.0f; // 少し小さくして控えめに
				t.centerAlign = true;
				t.color = { 1.0f, 0.8f, 0.0f, 1.0f };
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TitleSystem, "TitleSystem")
