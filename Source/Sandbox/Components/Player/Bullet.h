#pragma once
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	// 攻撃の所有者（フレンドリーファイア防止用）
	enum class EntityType {
		None,
		Player,
		Enemy
	};

	// 攻撃属性（ダメージや報酬倍率）
	struct AttackAttribute {
		float damage = 10.0f;
		float rewardRate = 1.0f; // 撃破時の時間回復倍率
		bool isPenetrate = false;
	};

	// 弾丸コンポーネント
	struct Bullet
	{
		float speed = 20.0f;
		float damage = 1.0f;
		float lifeTime = 2.0f; // 生存時間
		EntityType owner = EntityType::None;

		Bullet() = default;
	};

	ARCHE_COMPONENT(Bullet,
		REFLECT_VAR(speed)
		REFLECT_VAR(damage)
		REFLECT_VAR(lifeTime)
	)

		// AttackAttributeもコンポーネントとして登録（必要に応じて）
		// ARCHE_COMPONENT(AttackAttribute, REFLECT_VAR(damage)...) 
}