/*****************************************************************//**
 * @file	CollisionSystem.h
 * @brief	衝突検出
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/24	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___COLLISION_SYSTEM_H___
#define ___COLLISION_SYSTEM_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Scene/Systems/Physics/PhysicsSystem.h"

namespace Arche
{

	/**
	 * @namespace	Physics
	 * @brief		形状データの定義（ワールド空間用）
	 */
	namespace Physics
	{
		/**
		 * @struct	Sphere
		 * @brief	球
		 */
		struct Sphere
		{
			XMFLOAT3 center;
			float radius;
		};

		/**
		 * @struct	OBB
		 * @brief	箱
		 */
		struct OBB
		{
			XMFLOAT3 center;
			XMFLOAT3 extents;	// 各軸の「半径」（幅 / 2, 高さ / 2, 奥行 / 2）
			XMFLOAT3 axes[3];	// ローカル軸 (右, 上, 前) - 回転行列から取得
		};

		/**
		 * @struct	Capsule
		 * @brief	カプセル
		 */
		struct Capsule
		{
			XMFLOAT3 start;	// 線分の始点
			XMFLOAT3 end;	// 線分の終点
			float radius;
		};

		/**
		 * @struct	Cylinder
		 * @brief	円柱
		 */
		struct Cylinder
		{
			XMFLOAT3 center;
			XMFLOAT3 axis;		// 軸の向き（正規化済み）
			float height;
			float radius;
		};
	}

	class CollisionSystem
		: public ISystem
	{
	public:
		CollisionSystem() { m_systemName = "Collision System"; }

		// 初期化（Observerの接続など）
		void Initialize(Registry& registry);

		void Update(Registry& registry) override;

		static Entity Raycast(Registry& registry, const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, float& outDist);

		static void Reset();

	private:
		// --- 内部処理 ---
		void UpdateWorldCollider(Registry& registry, Entity e, const Transform& t, const Collider& c, WorldCollider& wc);

		// --- 判定関数群（回転対応） ---
		// 球 vs ...
		bool CheckSphereSphere(const Physics::Sphere& a, const Physics::Sphere& b, Physics::Contact& outContact);
		bool CheckSphereOBB(const Physics::Sphere& s, const Physics::OBB& b, Physics::Contact& outContact);
		bool CheckSphereCapsule(const Physics::Sphere& s, const Physics::Capsule& c, Physics::Contact& outContact);
		bool CheckSphereCylinder(const Physics::Sphere& s, const Physics::Cylinder& c, Physics::Contact& outContact);


		// OOB（箱）vs ...
		bool CheckOBBOBB(const Physics::OBB& a, const Physics::OBB& b, Physics::Contact& outContact);
		bool CheckOBBCapsule(const Physics::OBB& box, const Physics::Capsule& cap, Physics::Contact& outContact);
		bool CheckOBBCylinder(const Physics::OBB& box, const Physics::Cylinder& cyl, Physics::Contact& outContact);

		// Capsule vs ...
		bool CheckCapsuleCapsule(const Physics::Capsule& a, const Physics::Capsule& b, Physics::Contact& outContact);
		bool CheckCapsuleCylinder(const Physics::Capsule& cap, const Physics::Cylinder& cyl, Physics::Contact& outContact);
		bool CheckCylinderCylinder(const Physics::Cylinder& a, const Physics::Cylinder& b, Physics::Contact& outContact);

	private:
		// 変更検知用
		static Observer m_observer;
		static bool m_isInitialized;
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::CollisionSystem, "Collision System")

#endif // !___COLLISION_SYSTEM_H___