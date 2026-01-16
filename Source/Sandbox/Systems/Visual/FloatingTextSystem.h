#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Renderer/Text/TextRenderer.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>

namespace Arche
{
	// フローティングテキスト成分
	struct FloatingText {
		float life = 1.0f;
		float maxLife = 1.0f;
		float speed = 2.0f;
		DirectX::XMFLOAT3 velocity = { 0, 1, 0 };
	};

	class FloatingTextSystem : public ISystem
	{
	public:
		FloatingTextSystem() { m_systemName = "FloatingTextSystem"; m_group = SystemGroup::PlayOnly; }

		// ダメージポップアップ生成ヘルパー (staticにして他から呼べるようにする簡易実装)
		static void Spawn(Registry& reg, const DirectX::XMFLOAT3& pos, int damage, const DirectX::XMFLOAT4& color, float scale = 1.0f)
		{
			Entity e = reg.create();
			auto& t = reg.emplace<Transform>(e);
			t.position = pos;
			// カメラの向きに合わせるビルボード処理はUpdateで行うか、生成時にカメラ取得が必要
			// ここでは簡易的にY軸回転のみランダムなどで誤魔化さず、Updateでカメラを向かせる

			// 少し散らす
			t.position.x += (rand() % 100 / 100.0f - 0.5f) * 0.5f;
			t.position.y += 1.0f;
			t.position.z += (rand() % 100 / 100.0f - 0.5f) * 0.5f;
			t.scale = { scale, scale, scale };

			auto& txt = reg.emplace<TextComponent>(e);
			char buf[16]; sprintf_s(buf, "%d", damage);
			txt.text = buf;
			txt.fontKey = "Makinas 4 Square";
			txt.fontSize = 40.0f * scale;
			txt.color = color;
			txt.centerAlign = true;

			auto& ft = reg.emplace<FloatingText>(e);
			ft.life = 0.8f;
			ft.maxLife = 0.8f;
			ft.velocity = { 0, 3.0f, 0 };
		}

		void Update(Registry& reg) override
		{
			float dt = Time::DeltaTime();

			// カメラ取得（ビルボード用）
			Entity cam = NullEntity;
			for (auto e : reg.view<Camera>()) { cam = e; break; }
			DirectX::XMFLOAT3 camRot = { 0,0,0 };
			if (reg.valid(cam)) camRot = reg.get<Transform>(cam).rotation;

			// テキスト更新
			auto view = reg.view<FloatingText, Transform, TextComponent>();
			std::vector<Entity> deadList;

			for (auto e : view)
			{
				auto& ft = view.get<FloatingText>(e);
				auto& t = view.get<Transform>(e);
				auto& txt = view.get<TextComponent>(e);

				// 移動
				t.position.x += ft.velocity.x * dt;
				t.position.y += ft.velocity.y * dt;
				t.position.z += ft.velocity.z * dt;

				// 減速
				ft.velocity.y -= dt * 5.0f; // 重力

				// ビルボード（カメラを向く）
				if (reg.valid(cam)) t.rotation = camRot;

				// 寿命
				ft.life -= dt;
				float alpha = ft.life / ft.maxLife;
				txt.color.w = alpha;

				// スケールアニメーション（ポンと出てスッと消える）
				float s = 1.0f;
				if (ft.life > ft.maxLife * 0.8f) s = 1.5f; // 出現時拡大
				t.scale = { s, s, s };

				if (ft.life <= 0.0f) {
					deadList.push_back(e);
				}
			}

			for (auto e : deadList) reg.destroy(e);
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::FloatingTextSystem, "FloatingTextSystem")