#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/PlayerData.h"

namespace Arche
{
	class TimeManagerSystem : public ISystem
	{
	public:
		// 外部からのヒットストップ要求
		static void RequestHitStop(float durationSec)
		{
			s_hitStopDuration = durationSec;
			s_isHitStopActive = true;
		}

		TimeManagerSystem()
		{
			m_systemName = "TimeManagerSystem";
			m_group = SystemGroup::Always;
		}

		void Update(Registry& registry) override
		{
			// 1. ヒットストップ
			ProcessHitStop();

			// 2. 寿命管理
			if (!s_isHitStopActive)
			{
				ProcessLifeTimers(registry);
			}
		}

	private:
		static inline float s_hitStopDuration = 0.0f;
		static inline bool s_isHitStopActive = false;

		// 寿命減少
		void ProcessLifeTimers(Registry& reg)
		{
			float dt = Time::DeltaTime();

			reg.view<LifeTimer>().each([&](Entity e, LifeTimer& timer) {
				if (timer.isDead) return;

				timer.currentInfo -= dt;

				// クライマックスタイム演出
				if (timer.currentInfo <= 1.0f && timer.currentInfo > 0.0f)
				{
					// ヒットストップ中でなければスローを適用
					if (!s_isHitStopActive) Time::timeScale = 0.5f;
				}
				else if (!s_isHitStopActive)
				{
					Time::timeScale = 1.0f;
				} 

				if (timer.currentInfo <= 0.0f)
				{
					timer.currentInfo = 0.0f;
					timer.isDead = true;
					Logger::Log("Player Died due to Time Limit!");
				}
				});
		}

		// ヒットストップ
		void ProcessHitStop()
		{
			if (s_isHitStopActive)
			{
				// 時間をほぼ止める
				Time::timeScale = 0.001f;

				s_hitStopDuration -= 0.016f;

				if (s_hitStopDuration <= 0.0f)
				{
					s_isHitStopActive = false;
					Time::timeScale = 1.0f;
				}
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TimeManagerSystem, "TimeManagerSystem")
