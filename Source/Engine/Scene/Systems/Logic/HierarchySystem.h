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
		// ★追加: コライダーのワールド情報を計算してキャッシュする関数
		void UpdateWorldCollider(Registry& reg, Entity e, const XMMATRIX& worldMat)
		{
			const auto& col = reg.get<Collider>(e);

			// WorldColliderコンポーネントがなければ作成、あれば取得
			if (!reg.has<WorldCollider>(e)) {
				reg.emplace<WorldCollider>(e);
			}
			auto& worldCol = reg.get<WorldCollider>(e);

			// --- 行列からスケール・回転・平行移動を抽出 ---
			XMVECTOR scale, rotQuat, trans;
			XMMatrixDecompose(&scale, &rotQuat, &trans, worldMat);
			XMFLOAT3 s; XMStoreFloat3(&s, scale);

			// --- センター位置の計算 (オフセット適用) ---
			// コライダーのローカルオフセットをワールド座標へ変換
			XMVECTOR offsetVec = XMLoadFloat3(&col.offset);
			// オフセットにワールド行列を適用することで、親の移動・回転・スケール全てが反映された絶対座標になる
			XMVECTOR centerVec = XMVector3Transform(offsetVec, worldMat);
			XMStoreFloat3(&worldCol.center, centerVec);

			// --- 形状情報の更新 ---
			worldCol.isDirty = false; // 更新済みとする

			// ボックスの場合
			if (col.type == ColliderType::Box) {
				// 回転軸の抽出
				XMMATRIX rotMat = XMMatrixRotationQuaternion(rotQuat);
				XMFLOAT4X4 rot4x4; XMStoreFloat4x4(&rot4x4, rotMat);

				worldCol.axes[0] = { rot4x4._11, rot4x4._12, rot4x4._13 };
				worldCol.axes[1] = { rot4x4._21, rot4x4._22, rot4x4._23 };
				worldCol.axes[2] = { rot4x4._31, rot4x4._32, rot4x4._33 };

				// ハーフサイズ (スケール適用)
				worldCol.extents = {
					col.boxSize.x * s.x * 0.5f,
					col.boxSize.y * s.y * 0.5f,
					col.boxSize.z * s.z * 0.5f
				};
			}
			// 球の場合
			else if (col.type == ColliderType::Sphere) {
				// スケールの最大値を半径に適用
				float maxScale = std::max({ s.x, s.y, s.z });
				worldCol.radius = col.radius * maxScale;
			}
			// カプセル・円柱の場合
			else {
				// 高さ方向(Y)と半径方向(X,Z)のスケール適用
				float maxRadScale = std::max(s.x, s.z);
				worldCol.radius = col.radius * maxRadScale;
				worldCol.height = col.height * s.y;

				// 軸方向（Y軸基準）をワールド回転に合わせて更新
				XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), worldMat);
				up = XMVector3Normalize(up);
				XMStoreFloat3(&worldCol.axis, up);
			}
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::HierarchySystem, "Hierarchy System")

#endif // !___HIERARCHY_SYSTEM_H___