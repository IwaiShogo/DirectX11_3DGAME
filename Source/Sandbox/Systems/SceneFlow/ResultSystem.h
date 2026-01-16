#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Core/GameSession.h"
#include "Sandbox/Components/Visual/GeometricDesign.h" 
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Arche
{
	struct ResultContext
	{
		float timer = 0.0f;
		int phase = 0;
		int displayScore = 0;
		int targetScore = 0;
		std::string rankStr = "C";

		Entity camera = NullEntity; // 初期化判定用
		Entity titleText = NullEntity;
		Entity scoreText = NullEntity;
		Entity rankText = NullEntity;
		Entity guideText = NullEntity;

		struct BackObj { Entity e; float speed; float z; };
		std::vector<BackObj> bgObjects;
		bool isClear = false;
	};

	class ResultSystem : public ISystem
	{
	public:
		ResultSystem() { m_systemName = "ResultSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& registry) override
		{
			// ★ Static変数
			static ResultContext ctx;

			// ★ エンティティ無効チェックで初期化
			if (!registry.valid(ctx.camera))
			{
				ctx = ResultContext(); // リセット
				InitializeScene(registry, ctx);
			}

			float dt = Time::DeltaTime();
			float time = Time::TotalTime();
			ctx.timer += dt;

			XMFLOAT4 mainColor = ctx.isClear ? XMFLOAT4{ 0.0f, 1.0f, 1.0f, 1.0f } : XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f };

			if (ctx.phase == 0) {
				if (ctx.timer > 1.0f) { ctx.phase = 1; ctx.timer = 0.0f; }
			}
			else if (ctx.phase == 1) {
				float progress = std::min(ctx.timer / 1.5f, 1.0f);
				float ease = 1.0f - powf(1.0f - progress, 3.0f);
				ctx.displayScore = (int)(ctx.targetScore * ease);
				if (progress >= 1.0f) { ctx.displayScore = ctx.targetScore; ctx.phase = 2; ctx.timer = 0.0f; }
			}
			else if (ctx.phase == 2) {
				if (ctx.timer > 0.5f) ctx.phase = 3;
			}

			if (registry.valid(ctx.scoreText)) {
				auto& txt = registry.get<TextComponent>(ctx.scoreText);
				txt.text = "SCORE: " + std::to_string(ctx.displayScore);
				if (ctx.phase == 1) {
					float noise = (rand() % 100) / 100.0f;
					txt.color = { mainColor.x, mainColor.y + noise * 0.5f, mainColor.z, 1.0f };
				}
				else {
					txt.color = mainColor;
				}
			}

			if (registry.valid(ctx.rankText)) {
				auto& t = registry.get<Transform>(ctx.rankText);
				auto& txt = registry.get<TextComponent>(ctx.rankText);
				if (ctx.phase >= 2) {
					float s = 1.0f;
					if (ctx.phase == 2) { float tVal = ctx.timer / 0.5f; s = 5.0f * (1.0f - tVal) + 1.0f; }
					t.scale = { s, s, s };
					txt.text = ctx.rankStr; txt.color = { 1.0f, 0.8f, 0.0f, 1.0f };
					t.rotation.z = sinf(time * 10.0f) * 0.1f;
				}
				else {
					txt.color = { 0,0,0,0 };
				}
			}

			if (registry.valid(ctx.guideText)) {
				auto& txt = registry.get<TextComponent>(ctx.guideText);
				if (ctx.phase == 3) { float alpha = 0.5f + 0.5f * sinf(time * 5.0f); txt.color = { 1, 1, 1, alpha }; }
				else txt.color = { 0,0,0,0 };
			}

			for (auto& obj : ctx.bgObjects) {
				if (registry.valid(obj.e)) {
					auto& t = registry.get<Transform>(obj.e);
					obj.z -= dt * obj.speed;
					if (obj.z < -10.0f) obj.z = 40.0f;
					t.position.z = obj.z;
					t.rotation.x += dt; t.rotation.y += dt * 0.5f;
					t.position.y += dt * (ctx.isClear ? 0.5f : -0.5f);
					if (t.position.y > 10.0f) t.position.y = -10.0f;
					if (t.position.y < -10.0f) t.position.y = 10.0f;
				}
			}

			if (registry.valid(ctx.camera)) {
				auto& t = registry.get<Transform>(ctx.camera);
				float shake = (ctx.phase < 3) ? 0.05f : 0.01f;
				t.position.x = sinf(time * 0.3f) * 0.5f + ((rand() % 100) / 100.0f - 0.5f) * shake;
				t.position.y = cosf(time * 0.2f) * 0.5f;
			}

			if (ctx.phase == 3) {
				if (Input::GetKeyDown(VK_SPACE) || Input::GetButtonDown(Button::A)) {
					SceneManager::Instance().LoadScene("Resources/Game/Scenes/TitleScene.json", new FadeTransition(1.0f, { 0,0,0,1 }));
				}
				else if (Input::GetKeyDown('R') || Input::GetButtonDown(Button::B)) {
					GameSession::ResetResult();
					SceneManager::Instance().LoadScene("Resources/Game/Scenes/GameScene.json", new FadeTransition(0.5f, { 0,0,0,1 }));
				}
			}
		}

	private:
		void InitializeScene(Registry& reg, ResultContext& ctx)
		{
			ctx.timer = 0.0f;
			ctx.phase = 0;
			ctx.isClear = GameSession::isGameClear;
			ctx.targetScore = GameSession::lastScore;
			ctx.displayScore = 0;

			if (ctx.targetScore >= 50000) ctx.rankStr = "S";
			else if (ctx.targetScore >= 30000) ctx.rankStr = "A";
			else if (ctx.targetScore >= 10000) ctx.rankStr = "B";
			else ctx.rankStr = "C";
			if (!ctx.isClear) ctx.rankStr = "FAILED";

			XMFLOAT4 themeCol = ctx.isClear ? XMFLOAT4{ 0,1,1,1 } : XMFLOAT4{ 1,0,0,1 };

			ctx.camera = reg.create();
			reg.emplace<Camera>(ctx.camera);
			reg.emplace<Transform>(ctx.camera);

			ctx.titleText = reg.create();
			reg.emplace<Transform>(ctx.titleText).position = { 0, 3.0f, 0 };
			auto& t = reg.emplace<TextComponent>(ctx.titleText);
			t.text = ctx.isClear ? "MISSION ACCOMPLISHED" : "SIGNAL LOST";
			t.fontKey = "Makinas 4 Square"; t.fontSize = 80.0f; t.isBold = true; t.centerAlign = true; t.color = themeCol;

			ctx.scoreText = reg.create();
			reg.emplace<Transform>(ctx.scoreText).position = { 0, 1.0f, 0 };
			auto& ts = reg.emplace<TextComponent>(ctx.scoreText);
			ts.text = "SCORE: 0"; ts.fontKey = "Makinas 4 Square"; ts.fontSize = 60.0f; ts.centerAlign = true; ts.color = themeCol;

			ctx.rankText = reg.create();
			reg.emplace<Transform>(ctx.rankText).position = { 0, -1.0f, -2.0f };
			auto& tr = reg.emplace<TextComponent>(ctx.rankText);
			tr.text = ""; tr.fontKey = "Makinas 4 Square"; tr.fontSize = 200.0f; tr.isBold = true; tr.centerAlign = true; tr.color = { 0,0,0,0 };

			ctx.guideText = reg.create();
			reg.emplace<Transform>(ctx.guideText).position = { 0, -3.5f, 0 };
			auto& tg = reg.emplace<TextComponent>(ctx.guideText);
			tg.text = "[SPACE] TITLE   [R] RETRY"; tg.fontKey = "Makinas 4 Square"; tg.fontSize = 24.0f; tg.centerAlign = true; tg.color = { 0,0,0,0 };

			for (int i = 0; i < 50; ++i) {
				Entity e = reg.create();
				float x = ((rand() % 400) / 10.0f) - 20.0f;
				float y = ((rand() % 400) / 10.0f) - 20.0f;
				float z = (float)(rand() % 50);
				reg.emplace<Transform>(e).position = { x, y, z };
				float s = ((rand() % 100) / 100.0f) * 0.5f + 0.1f;
				reg.get<Transform>(e).scale = { s, s, s };
				auto& geo = reg.emplace<GeometricDesign>(e);
				geo.shapeType = (rand() % 2 == 0) ? GeoShape::Cube : GeoShape::Pyramid; geo.isWireframe = true; geo.color = { themeCol.x, themeCol.y, themeCol.z, 0.3f };
				ResultContext::BackObj bo; bo.e = e; bo.speed = 2.0f + (rand() % 100) / 10.0f; bo.z = z;
				ctx.bgObjects.push_back(bo);
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::ResultSystem, "ResultSystem")