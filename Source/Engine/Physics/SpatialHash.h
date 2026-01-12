/*****************************************************************//**
 * @file	SpatialHash.h
 * @brief	空間分割（Spatial Hashing）による衝突判定最適化
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/15	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___SPATIAL_HASH_H___
#define ___SPATIAL_HASH_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"

namespace Arche
{

	class SpatialHash
	{
	public:
		// セルサイズ（オブジェクトの最大サイズより少し大きく設定する）
		static constexpr float CELL_SIZE = 64.0f;

		// グリッドのリセット
		void Clear()
		{
			grid.clear();
		}

		// オブジェクトの登録
		void Register(Entity entity, const XMFLOAT3& min, const XMFLOAT3& max)
		{
			int startX = (int)std::floor(min.x / CELL_SIZE);
			int startY = (int)std::floor(min.y / CELL_SIZE);
			int startZ = (int)std::floor(min.z / CELL_SIZE);

			int endX = (int)std::floor(max.x / CELL_SIZE);
			int endY = (int)std::floor(max.y / CELL_SIZE);
			int endZ = (int)std::floor(max.z / CELL_SIZE);

			for (int x = startX; x <= endX; x++)
			{
				for (int y = startY; y <= endY; y++)
				{
					for (int z = startZ; z <= endZ; z++)
					{
						int key = GetKey(x, y, z);
						grid[key].push_back(entity);
					}
				}
			}
		}

		// 候補リストの取得
		std::vector<Entity> Query(const XMFLOAT3& min, const XMFLOAT3& max)
		{
			std::vector<Entity> result;

			int startX = (int)std::floor(min.x / CELL_SIZE);
			int startY = (int)std::floor(min.y / CELL_SIZE);
			int startZ = (int)std::floor(min.z / CELL_SIZE);

			int endX = (int)std::floor(max.x / CELL_SIZE);
			int endY = (int)std::floor(max.y / CELL_SIZE);
			int endZ = (int)std::floor(max.z / CELL_SIZE);

			for (int x = startX; x <= endX; x++)
			{
				for (int y = startY; y <= endY; y++)
				{
					for (int z = startZ; z <= endZ; z++)
					{
						int key = GetKey(x, y, z);
						if (grid.find(key) != grid.end())
						{
							const auto& list = grid[key];
							result.insert(result.end(), list.begin(), list.end());
						}
					}
				}
			}

			// 重複削除
			std::sort(result.begin(), result.end());
			result.erase(std::unique(result.begin(), result.end()), result.end());

			return result;
		}

	private:
		std::unordered_map<int, std::vector<Entity>> grid;

		// 座標ハッシュ関数
		// 実際にはもっと衝突しにくいハッシュが良いが、ゲーム用とならこれで十分
		int GetKey(int x, int y, int z) const
		{
			// 素数を使ったハッシュ
			const int p1 = 73856093;
			const int p2 = 19349663;
			const int p3 = 83492791;
			return (x * p1) ^ (y * p2) ^ (z * p3);
		}
	};

}	// namespace Arche

#endif // !___SPATIAL_HASH_H___