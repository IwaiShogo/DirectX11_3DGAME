#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Visual/FloatingTextAnimation.h"

namespace Arche
{
	class FloatingTextSystem : public ISystem
	{
	public:
		FloatingTextSystem()
		{
			m_systemName = "FloatingTextSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// Transform(3D座標) と TextComponent, Animation を持つエンティティを対象
			auto view = registry.view<FloatingTextAnimation, Transform, TextComponent>();

			for (auto entity : view)
			{
				auto& anim = view.get<FloatingTextAnimation>(entity);
				auto& trans = view.get<Transform>(entity);
				auto& text = view.get<TextComponent>(entity);

				// 1. 上昇 (Y座標を加算)
				trans.position.y += anim.moveSpeedY * dt;

				// 2. フェードアウト (Alpha値を減算)
				text.color.w -= anim.fadeSpeed * dt;

				// 負の値にならないようにクランプ
				if (text.color.w < 0.0f) text.color.w = 0.0f;
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::FloatingTextSystem, "FloatingTextSystem")
