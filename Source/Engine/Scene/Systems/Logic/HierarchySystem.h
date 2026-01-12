/*****************************************************************//**
 * @file	HierarchySystem.h
 * @brief	親から順に座標を計算していくシステム（ヒエラルキー）
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/11/27	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___HIERARCHY_SYSTEM_H___
#define ___HIERARCHY_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include <functional>

namespace Arche
{

	class HierarchySystem
		: public ISystem
	{
	public:
		HierarchySystem() { m_systemName = "Hierarchy System"; }

		void Update(Registry& registry) override
		{
			// 再帰的に行列を更新する関数
			std::function<void(Entity, const DirectX::XMMATRIX&)> updateMatrix =
				[&](Entity entity, const DirectX::XMMATRIX& parentMatrix)
				{
					if (registry.has<Transform>(entity)) {
						auto& t = registry.get<Transform>(entity);

						// 1. ローカル行列を作る (S * R * T)
						DirectX::XMMATRIX localMat = t.GetLocalMatrix();

						// 2. 親の行列を掛けてワールド行列にする
						DirectX::XMMATRIX worldMat = localMat * parentMatrix;

						// 計算結果をメンバ変数にストアする
						DirectX::XMStoreFloat4x4(&t.worldMatrix, worldMat);

						// 3. 子供たちにも「今計算したワールド行列」を渡して更新させる
						if (registry.has<Relationship>(entity)) {
							for (Entity child : registry.get<Relationship>(entity).children) {
								// 計算済みの worldMat を渡す
								updateMatrix(child, worldMat);
							}
						}
					}
				};

			// --- ルート（親を持たないエンティティ）から更新開始 ---

			// 1. 全エンティティから「Relationshipを持つが、Parentが無効」なものを探す
			//	  もしくは「Relationshipを持っていない」エンティティもルート扱い

			// ここでは全エンティティを走査してルートを探す簡易実装
			registry.view<Transform>().each([&](Entity e, Transform& t) {
				bool isRoot = true;
				if (registry.has<Relationship>(e)) {
					// 親がいるならルートではない
					if (registry.get<Relationship>(e).parent != NullEntity) {
						isRoot = false;
					}
				}

				if (isRoot) {
					// ルートの親行列は「単位行列」
					updateMatrix(e, DirectX::XMMatrixIdentity());
				}
				});
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::HierarchySystem, "Hierarchy System")

#endif // !___HIERARCHY_SYSTEM_H___