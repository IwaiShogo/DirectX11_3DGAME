/*****************************************************************//**
 * @file	HierarchySystem.h
 * @brief	親から順に座標を計算し、コライダーのワールド位置も更新するシステム
 *********************************************************************/

#ifndef ___HIERARCHY_SYSTEM_H___
#define ___HIERARCHY_SYSTEM_H___

 // ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include <functional>
#include <algorithm> // max等のために必要
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{

	class HierarchySystem : public ISystem
	{
	public:
		HierarchySystem() { m_systemName = "Hierarchy System"; }

		void Update(Registry& registry) override
		{
			// ワールド行列とワールドコライダーを更新する再帰関数
			std::function<void(Entity, const XMMATRIX&)> updateEntity =
				[&](Entity entity, const XMMATRIX& parentMatrix)
				{
					if (registry.has<Transform>(entity)) {
						auto& t = registry.get<Transform>(entity);

						// 1. ローカル行列作成 (Scale * Rotation * Translation)
						XMMATRIX localMat = t.GetLocalMatrix();

						// 2. 親行列を掛けてワールド行列作成
						XMMATRIX worldMat = localMat * parentMatrix;

						// 3. 結果を保存
						XMStoreFloat4x4(&t.worldMatrix, worldMat);

						// ★重要: コライダーがある場合、ワールド座標に合わせて形状データを更新する
						if (registry.has<Collider>(entity)) {
							UpdateWorldCollider(registry, entity, worldMat);
						}

						// 4. 子へ伝播
						if (registry.has<Relationship>(entity)) {
							for (Entity child : registry.get<Relationship>(entity).children) {
								// 子エンティティが無効でないか確認
								if (registry.valid(child)) {
									updateEntity(child, worldMat);
								}
							}
						}
					}
				};

			// --- ルート（親なし）から更新開始 ---
			auto view = registry.view<Transform>();
			for (auto e : view) {
				bool isRoot = true;
				// Relationshipを持っていて、かつ親が存在する場合はルートではない
				if (registry.has<Relationship>(e)) {
					if (registry.valid(registry.get<Relationship>(e).parent)) {
						isRoot = false;
					}
				}

				if (isRoot) {
					updateEntity(e, XMMatrixIdentity());
				}
			}
		}

	private:
		void UpdateWorldCollider(Registry& reg, Entity e, const XMMATRIX& worldMat)
		{
			if (!reg.has<Collider>(e)) return;

			// 1. 行列からワールド座標を抽出
			XMVECTOR worldPosVec = worldMat.r[3];

			// 2. 物理システムが参照している WorldCollider キャッシュを直接更新
			if (!reg.has<WorldCollider>(e)) {
				reg.emplace<WorldCollider>(e);
			}
			auto& worldCol = reg.get<WorldCollider>(e);

			// 計算結果を直接キャッシュに叩き込む
			XMStoreFloat3(&worldCol.center, worldPosVec);

			// OBB用の軸（回転）も同期
			XMVECTOR scale, rotQuat, trans;
			XMMatrixDecompose(&scale, &rotQuat, &trans, worldMat);
			XMMATRIX rotMat = XMMatrixRotationQuaternion(rotQuat);
			XMFLOAT4X4 m; XMStoreFloat4x4(&m, rotMat);
			worldCol.axes[0] = { m._11, m._12, m._13 };
			worldCol.axes[1] = { m._21, m._22, m._23 };
			worldCol.axes[2] = { m._31, m._32, m._33 };

			// ★重要: 物理エンジンに「計算済み」であることを伝え、上書きを防ぐ
			worldCol.isDirty = false;

			// 3. Colliderのoffsetも念のため同期
			auto& col = reg.get<Collider>(e);
			XMStoreFloat3(&col.offset, worldPosVec);
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::HierarchySystem, "Hierarchy System")

#endif // !___HIERARCHY_SYSTEM_H___