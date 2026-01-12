/*****************************************************************//**
 * @file	AnimationSystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___ANIMATION_SYSTEM_H___
#define ___ANIMATION_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Scene/Animation/AnimatorController.h"
#include "Engine/Scene/Serializer/AnimatorControllerSerializer.h"

namespace Arche
{
	class AnimationSystem : public ISystem
	{
	public:
		AnimationSystem()
		{
			m_systemName = "Animation System";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();
			auto view = registry.view<MeshComponent, Animator>();

			for (auto entity : view)
			{
				auto& mesh = view.get<MeshComponent>(entity);
				auto& animator = view.get<Animator>(entity);

				if (!mesh.pModel) continue;

				// 1. ロード
				if (!animator.controller && !animator.controllerPath.empty()) {
					animator.controller = AnimatorControllerSerializer::Deserialize(animator.controllerPath);
				}
				if (!animator.controller) continue;

				auto& controller = animator.controller;

				// 2. 初期ステート
				const AnimatorState* currentState = controller->FindState(animator.currentState);
				if (!currentState) {
					// (前回と同じ初期化処理)
					const AnimatorState* entry = nullptr;
					if (controller->entryState.GetHash() != 0) entry = controller->FindState(controller->entryState);
					else if (!controller->states.empty()) entry = &controller->states[0];

					if (entry) ChangeState(animator, mesh, *entry, 0.0f); // 初期は即時遷移
					continue;
				}

				// 3. 遷移判定
				for (const auto& transition : currentState->transitions)
				{
					if (CheckConditions(animator, mesh, transition))
					{
						const AnimatorState* nextState = controller->FindState(transition.targetState);
						if (nextState)
						{
							ConsumeTriggers(animator, transition);
							// ★修正: Durationを渡して遷移
							ChangeState(animator, mesh, *nextState, transition.duration);
							break;
						}
					}
				}

				// 4. 更新
				if (animator.isPlaying) mesh.pModel->Step(dt);
				animator.stateTime += dt;
			}
		}

	private:
		// duration引数を追加
		void ChangeState(Animator& animator, MeshComponent& mesh, const AnimatorState& nextState, float duration)
		{
			animator.currentState = nextState.name;
			animator.stateTime = 0.0f;

			if (!nextState.motionName.empty())
			{
				// Model側のPlayにDurationを渡す
				mesh.pModel->Play(nextState.motionName, nextState.loop, nextState.speed, duration);
			}
		}

		bool CheckConditions(Animator& animator, MeshComponent& mesh, const AnimatorTransition& transition)
		{
			// ExitTimeチェック
			if (transition.hasExitTime)
			{
				// Modelから正確な時間と長さを取得
				float length = mesh.pModel->GetCurrentAnimationLength();
				float time = mesh.pModel->GetCurrentAnimationTime();

				// 長さが0なら即遷移 (安全策)
				if (length <= 0.001f) return true;

				// 正規化時間
				float normalizedTime = time / length;

				// 指定時間に達していないなら遷移しない
				if (normalizedTime < transition.exitTime) return false;
			}

			// ExitTime以外に条件がなければ、ExitTimeを満たした時点でOK
			if (transition.conditions.empty()) return true;

			// 条件チェック
			for (const auto& cond : transition.conditions)
			{
				if (!EvaluateCondition(animator, cond)) return false;
			}
			return true;
		}

		bool EvaluateCondition(Animator& animator, const AnimatorCondition& cond)
		{
			switch (cond.mode)
			{
			case AnimatorConditionMode::If:		 return animator.GetBool(cond.parameter) == true;
			case AnimatorConditionMode::IfNot:	 return animator.GetBool(cond.parameter) == false;
			case AnimatorConditionMode::Greater: return animator.GetFloat(cond.parameter) > cond.threshold;
			case AnimatorConditionMode::Less:	 return animator.GetFloat(cond.parameter) < cond.threshold;
			case AnimatorConditionMode::Equals:	 return animator.GetInt(cond.parameter) == (int)cond.threshold;
			case AnimatorConditionMode::NotEqual:return animator.GetInt(cond.parameter) != (int)cond.threshold;
			default: if (animator.triggers.count(cond.parameter)) return animator.triggers[cond.parameter]; return false;
			}
		}

		void ConsumeTriggers(Animator& animator, const AnimatorTransition& transition)
		{
			for (const auto& cond : transition.conditions) {
				if (animator.triggers.count(cond.parameter)) animator.triggers[cond.parameter] = false;
			}
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::AnimationSystem, "Animation System")

#endif // !___ANIMATION_SYSTEM_H___