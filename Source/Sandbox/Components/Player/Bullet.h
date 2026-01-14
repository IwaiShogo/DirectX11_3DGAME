#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	/**
	 * @struct Bullet
	 * @brief  弾のパラメータ
	 */
	struct Bullet
	{
		float speed = 20.0f;
		float damage = 1.0f;
		std::string ownerTag = "Player";

		Bullet() = default;
	};

	ARCHE_COMPONENT(Bullet,
		REFLECT_VAR(speed)
		REFLECT_VAR(damage)
		REFLECT_VAR(ownerTag)
	)
}
