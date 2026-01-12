/*****************************************************************//**
 * @file	PhysicsSystem.h
 * @brief	物理挙動
 * 
 * @details	
 * 物理挙動を計算し、エンティティの動きを管理します。
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/01	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/12/16	最終更新日
 * 			作業内容：	- 物理システムの基本的なフレームワークを追加
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___PHYSICS_SYSTEM_H___
#define ___PHYSICS_SYSTEM_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{

	// 接触情報
	namespace Physics
	{
		struct Contact
		{
			Entity a, b;				// 衝突した2つのエンティティ
			DirectX::XMFLOAT3 normal;	// A -> Bの法線
			float depth;				// めり込み量
		};
	}

	/**
	 * @class	PhysicsSystem
	 * @brief	物理挙動
	 */
	class PhysicsSystem
		: public ISystem
	{
	public:
		PhysicsSystem() { m_systemName = "Physics System"; }

		// 物理シミュレーション更新（重力、速度更新）
		void Update(Registry& registry) override;

		// 衝突解決（CollisionSystemから呼ばれる）
		static void Solve(Registry& registry, const std::vector<Physics::Contact>& contacts);
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PhysicsSystem, "Physics System")

#endif // !___PHYSICS_SYSTEM_H___