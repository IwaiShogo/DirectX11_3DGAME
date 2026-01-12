#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Core/Time/Time.h"

#include "Sandbox/Components/PlayerMoveData.h"

#include "Engine/Scene/Core/SceneManager.h"

namespace Arche
{
	class PlayerMoveSystem : public ISystem
	{
	public:
		PlayerMoveSystem()
		{
			m_systemName = "PlayerMoveSystem";
		}

		void Update(Registry& registry) override
		{
			// "PlayerMoveData" と "Transform" の両方を持っているエンティティを全て取得
			auto view = registry.view<PlayerMoveData, Transform, Animator>();

			for (auto entity : view)
			{
				// 参照（&）で取得して書き換えられるようにする
				auto& moveData = view.get<PlayerMoveData>(entity);
				auto& transform = view.get<Transform>(entity);
				auto& animator = view.get<Animator>(entity);

				// --- 移動ロジック ---
				float dt = Time::DeltaTime();
				float speed = moveData.speed;

				bool isMoving = false;

				// Inputクラスを使って入力を取得
				if (Input::GetKey(VK_LEFT))		{ transform.position.x -= speed * dt; isMoving = true; }
				if (Input::GetKey(VK_RIGHT))	{ transform.position.x += speed * dt; isMoving = true; }
				if (Input::GetKey(VK_UP))		{ transform.position.z += speed * dt; isMoving = true; }
				if (Input::GetKey(VK_DOWN))		{ transform.position.z -= speed * dt; isMoving = true; }

				if (Input::GetKeyDown(VK_SPACE)) transform.position.y += 10.0f;

				float animSpeed = isMoving ? 1.0f : 0.0f;
				animator.SetFloat("Speed", animSpeed);

				// テスト用
				if (Input::GetKeyDown('T')) SceneManager::Instance().LoadSceneAsync("Resources/Game/Scenes/TitleScene.json", new FadeTransition(2.0f));
				if (Input::GetKeyDown('G')) SceneManager::Instance().LoadSceneAsync("Resources/Game/Scenes/GameScene.json", new SlideTransition(2.0f));
			}
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerMoveSystem, "PlayerMoveSystem")