#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h" 
#include "Engine/Core/Window/Input.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace DirectX;

namespace Arche
{
	// 演出用ヘルパー
	inline float Rnd(float min, float max) { return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min)); }

	struct ResultTag {};

	struct ResultContext
	{
		// UI系
		Entity uiRoot = NullEntity;
		Entity resultLabel = NullEntity;
		Entity scoreLabel = NullEntity;

		Entity btnRetry = NullEntity;
		Entity btnSelect = NullEntity;
		Entity cursorObj = NullEntity;

		// 演出用オブジェクト
		struct Particle { Entity e; XMFLOAT3 velocity; XMFLOAT3 rotAxis; };
		std::vector<Particle> particles;
		std::vector<Entity> rings;
		std::vector<Entity> floorGrids;

		// 状態
		int selectIndex = 0;
		float timer = 0.0f;
		float globalTime = 0.0f;
		bool isDecided = false;
		bool isClear = false;
	};

	class ResultSystem : public ISystem
	{
	public:
		ResultSystem() { m_systemName = "ResultSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static ResultContext ctx;

			bool init = false;
			for (auto e : reg.view<ResultTag>()) { init = true; break; }
			if (!init) {
				ctx = ResultContext();
				ctx.isClear = GameSession::isGameClear;

				InitVisuals(reg, ctx);
				InitUI(reg, ctx);

				if (ctx.isClear) AudioManager::Instance().PlayBGM("bgm_result.wav", 0.5f, false);
				else AudioManager::Instance().PlayBGM("bgm_result.wav", 0.5f, false);

				reg.emplace<ResultTag>(reg.create());
			}

			float dt = Time::DeltaTime();
			ctx.timer += dt;
			ctx.globalTime += dt;

			if (!ctx.isDecided)
			{
				// 入力
				if (Input::GetButtonDown(Button::Up) || Input::GetKeyDown('W') || Input::GetKeyDown(VK_UP)) {
					ctx.selectIndex--; if (ctx.selectIndex < 0) ctx.selectIndex = 1;
					AudioManager::Instance().PlaySE("se_dash.wav", 0.5f);
				}
				else if (Input::GetButtonDown(Button::Down) || Input::GetKeyDown('S') || Input::GetKeyDown(VK_DOWN)) {
					ctx.selectIndex++; if (ctx.selectIndex > 1) ctx.selectIndex = 0;
					AudioManager::Instance().PlaySE("se_dash.wav", 0.5f);
				}
				else if (Input::GetButtonDown(Button::A) || Input::GetKeyDown(VK_SPACE) || Input::GetKeyDown(VK_RETURN)) {
					ctx.isDecided = true;
					AudioManager::Instance().PlaySE("se_start.wav", 0.4f);
					ctx.timer = 0.0f;
				}

				UpdateUIAnim(reg, ctx, dt);
			}
			else
			{
				// 遷移
				if (ctx.timer > 1.0f) {
					AudioManager::Instance().StopBGM(0.5f);
					if (ctx.selectIndex == 0) {
						SceneManager::Instance().LoadScene("Resources/Game/Scenes/GameScene.json", new FadeTransition(0.5f, { 0,0,0,1 }));
					}
					else {
						// タイトルかステージセレクトへ
						SceneManager::Instance().LoadScene("Resources/Game/Scenes/TitleScene.json", new FadeTransition(0.5f, { 0,0,0,1 }));
					}
				}
			}

			// 常時背景アニメーション
			UpdateVisuals(reg, ctx, dt);
		}

	private:
		// ------------------------------------------------------------
		// 初期化 (Visual)
		// ------------------------------------------------------------
		void InitVisuals(Registry& reg, ResultContext& ctx)
		{
			// カメラ
			Entity cam = reg.create();
			reg.emplace<Camera>(cam);
			auto& ct = reg.emplace<Transform>(cam);
			ct.position = { 0, 0, -20 };
			ct.rotation.x = 0.1f;

			// テーマカラー決定
			XMFLOAT4 baseCol = ctx.isClear ? XMFLOAT4{ 0, 1, 1, 1 } : XMFLOAT4{ 1, 0, 0, 1 };
			XMFLOAT4 subCol = ctx.isClear ? XMFLOAT4{ 1, 1, 0, 1 } : XMFLOAT4{ 0.5f, 0, 0, 1 };

			// 1. 背景リング (巨大な回転構造体)
			for (int i = 0; i < 5; ++i) {
				Entity e = reg.create();
				auto& t = reg.emplace<Transform>(e);
				t.position = { 0, 0, 10.0f + i * 5.0f };
				t.scale = { 20.0f + i * 8.0f, 20.0f + i * 8.0f, 0.5f };

				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = (i % 2 == 0) ? GeoShape::Torus : GeoShape::Cylinder;
				g.color = { baseCol.x, baseCol.y, baseCol.z, 0.1f + i * 0.05f };
				g.isWireframe = true;

				ctx.rings.push_back(e);
			}

			// 2. 浮遊パーティクル (成功なら上昇、失敗なら下降・拡散)
			int pCount = 100;
			for (int i = 0; i < pCount; ++i) {
				Entity e = reg.create();
				auto& t = reg.emplace<Transform>(e);
				t.position = { Rnd(-40, 40), Rnd(-20, 20), Rnd(-10, 50) };
				float s = Rnd(0.2f, 0.8f);
				t.scale = { s, s, s };

				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = (rand() % 2 == 0) ? GeoShape::Cube : GeoShape::Diamond;
				g.color = (rand() % 3 == 0) ? subCol : baseCol;
				g.color.w = Rnd(0.3f, 0.8f);
				g.isWireframe = true;

				ResultContext::Particle p;
				p.e = e;
				if (ctx.isClear) {
					p.velocity = { 0, Rnd(2.0f, 5.0f), 0 }; // 上昇
				}
				else {
					p.velocity = { Rnd(-1,1), Rnd(-2.0f, -5.0f), 0 }; // 下降＋揺れ
				}
				p.rotAxis = { Rnd(-1,1), Rnd(-1,1), Rnd(-1,1) };
				ctx.particles.push_back(p);
			}

			// 3. 床グリッド (失敗時は赤く燃える地面、成功時は輝く水面)
			for (int x = -10; x <= 10; ++x) {
				Entity e = reg.create();
				auto& t = reg.emplace<Transform>(e);
				t.position = { x * 4.0f, -10.0f, 20.0f };
				t.scale = { 0.1f, 0.1f, 100.0f };
				t.rotation.x = XM_PIDIV2; // 寝かせる

				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cylinder;
				g.color = { baseCol.x, baseCol.y, baseCol.z, 0.2f };
				g.isWireframe = false;
				ctx.floorGrids.push_back(e);
			}
		}

		// ------------------------------------------------------------
		// 初期化 (UI)
		// ------------------------------------------------------------
		void InitUI(Registry& reg, ResultContext& ctx)
		{
			// タイトル
			std::string titleStr = ctx.isClear ? "MISSION COMPLETE" : "MISSION FAILED";
			XMFLOAT4 titleCol = ctx.isClear ? XMFLOAT4{ 0, 1, 1, 1 } : XMFLOAT4{ 1, 0, 0, 1 };

			ctx.resultLabel = CreateText(reg, titleStr, 0, 4.0f, 80.0f, titleCol);
			reg.get<TextComponent>(ctx.resultLabel).isBold = true;

			// スコア
			std::string scoreStr = "TOTAL SCORE: " + std::to_string(GameSession::killCount * 100 + (ctx.isClear ? 1000 : 0));
			ctx.scoreLabel = CreateText(reg, scoreStr, 0, 1.5f, 40.0f, { 1,1,1,1 });

			// ボタン
			ctx.btnRetry = CreateText(reg, "RETRY MISSION", 0, -2.0f, 30.0f, { 0.5f, 0.5f, 0.5f, 1 });
			ctx.btnSelect = CreateText(reg, "RETURN TO TITLE", 0, -3.5f, 30.0f, { 0.5f, 0.5f, 0.5f, 1 });

			// カーソル
			ctx.cursorObj = reg.create();
			reg.emplace<Transform>(ctx.cursorObj).scale = { 0.8f, 0.8f, 0.8f };
			auto& g = reg.emplace<GeometricDesign>(ctx.cursorObj);
			g.shapeType = GeoShape::Pyramid;
			g.color = ctx.isClear ? XMFLOAT4{ 1, 1, 0, 1 } : XMFLOAT4{ 1, 0, 0, 1 };
			reg.get<Transform>(ctx.cursorObj).rotation.z = -XM_PIDIV2;
		}

		// ------------------------------------------------------------
		// 更新処理
		// ------------------------------------------------------------
		void UpdateVisuals(Registry& reg, ResultContext& ctx, float dt)
		{
			// リング回転
			for (int i = 0; i < ctx.rings.size(); ++i) {
				if (reg.valid(ctx.rings[i])) {
					auto& t = reg.get<Transform>(ctx.rings[i]);
					float speed = (i % 2 == 0 ? 1 : -1) * (0.2f + i * 0.1f);
					t.rotation.z += dt * speed;
					// 失敗時は不安定に揺らす
					if (!ctx.isClear) t.rotation.x = sinf(ctx.globalTime * 2.0f + i) * 0.1f;
				}
			}

			// パーティクル移動
			for (auto& p : ctx.particles) {
				if (reg.valid(p.e)) {
					auto& t = reg.get<Transform>(p.e);
					t.position.x += p.velocity.x * dt;
					t.position.y += p.velocity.y * dt;
					t.position.z += p.velocity.z * dt;

					t.rotation.x += p.rotAxis.x * dt * 2.0f;
					t.rotation.y += p.rotAxis.y * dt * 2.0f;

					// ループ処理
					if (t.position.y > 30.0f) t.position.y = -30.0f;
					if (t.position.y < -30.0f) t.position.y = 30.0f;
				}
			}

			// 床の波打ち
			for (int i = 0; i < ctx.floorGrids.size(); ++i) {
				if (reg.valid(ctx.floorGrids[i])) {
					auto& t = reg.get<Transform>(ctx.floorGrids[i]);
					float wave = sinf(ctx.globalTime * 3.0f + i * 0.5f);
					t.scale.y = 0.1f + (wave * 0.5f + 0.5f) * 0.5f; // 太さ変化
					reg.get<GeometricDesign>(ctx.floorGrids[i]).color.w = 0.1f + (wave * 0.5f + 0.5f) * 0.3f;
				}
			}
		}

		void UpdateUIAnim(Registry& reg, ResultContext& ctx, float dt)
		{
			// 選択肢のハイライト
			auto UpdateBtn = [&](Entity e, bool active) {
				if (reg.valid(e)) {
					auto& t = reg.get<TextComponent>(e);
					auto& tr = reg.get<Transform>(e);
					if (active) {
						t.color = { 1, 1, 0, 1 };
						tr.scale = { 1.2f + sinf(ctx.globalTime * 10.0f) * 0.05f, 1.2f, 1.0f };
					}
					else {
						t.color = { 0.5f, 0.5f, 0.5f, 1 };
						tr.scale = { 1.0f, 1.0f, 1.0f };
					}
				}
				};
			UpdateBtn(ctx.btnRetry, ctx.selectIndex == 0);
			UpdateBtn(ctx.btnSelect, ctx.selectIndex == 1);

			// カーソル移動
			if (reg.valid(ctx.cursorObj)) {
				auto& t = reg.get<Transform>(ctx.cursorObj);
				float targetY = (ctx.selectIndex == 0) ? -2.0f : -3.5f;
				t.position.y = std::lerp(t.position.y, targetY, dt * 15.0f);
				t.position.x = -5.0f + sinf(ctx.globalTime * 8.0f) * 0.3f;
				t.rotation.x += dt * 2.0f;
			}

			// タイトルの明滅
			if (reg.valid(ctx.resultLabel)) {
				float a = 0.8f + sinf(ctx.globalTime * 3.0f) * 0.2f;
				reg.get<TextComponent>(ctx.resultLabel).color.w = a;
			}
		}

		Entity CreateText(Registry& reg, std::string text, float x, float y, float size, XMFLOAT4 color) {
			Entity e = reg.create();
			reg.emplace<Transform>(e).position = { x, y, 0 };
			auto& t = reg.emplace<TextComponent>(e);
			t.text = text; t.fontSize = size; t.color = color;
			t.fontKey = "Makinas 4 Square"; t.centerAlign = true;
			return e;
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::ResultSystem, "ResultSystem")