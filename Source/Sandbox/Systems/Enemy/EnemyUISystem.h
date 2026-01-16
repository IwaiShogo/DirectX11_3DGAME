#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include <unordered_map>

namespace Arche
{
	class EnemyUISystem : public ISystem
	{
		std::unordered_map<Entity, Entity> m_enemyToUI;

	public:
		EnemyUISystem() { m_systemName = "EnemyUISystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			Entity cam = NullEntity;
			for (auto e : reg.view<Camera>()) { cam = e; break; }

			// 1. 削除チェック & クリーンアップ
			// 敵がいない、または無効になったらUIを消してリストから外す
			for (auto it = m_enemyToUI.begin(); it != m_enemyToUI.end(); ) {
				Entity enemy = it->first;
				Entity ui = it->second;

				// 敵が存在しないならUIも道連れ
				if (!reg.valid(enemy)) {
					if (reg.valid(ui)) reg.destroy(ui);
					it = m_enemyToUI.erase(it);
				}
				else {
					++it;
				}
			}

			// 2. 新規作成 & 更新
			auto view = reg.view<EnemyStats, Transform>();
			for (auto enemy : view)
			{
				// ★追加: 念のための生存確認 (viewに含まれていても無効な場合があるため)
				if (!reg.valid(enemy)) continue;

				// マップに登録されているか検索
				auto it = m_enemyToUI.find(enemy);
				Entity ui = NullEntity;

				if (it == m_enemyToUI.end()) {
					// 新規作成
					ui = reg.create();
					reg.emplace<Transform>(ui);
					auto& g = reg.emplace<GeometricDesign>(ui);
					g.shapeType = GeoShape::Cube;
					g.color = { 0, 0, 0, 1 };
					g.isWireframe = false;

					m_enemyToUI[enemy] = ui;
				}
				else {
					ui = it->second;
				}

				// ★追加: UI自体の生存確認とコンポーネント確認
				if (!reg.valid(ui)) continue;
				if (!reg.has<Transform>(ui) || !reg.has<GeometricDesign>(ui)) continue;

				// 敵のコンポーネント取得 (view経由なので基本あるはずだが念のため)
				auto& eStats = view.get<EnemyStats>(enemy);
				auto& eTrans = view.get<Transform>(enemy);

				auto& uiTrans = reg.get<Transform>(ui);
				auto& uiGeo = reg.get<GeometricDesign>(ui); // ★ここで落ちていたのを防ぐ

				// 位置更新
				float yOffset = (eStats.type == EnemyType::Boss_Omega) ? 6.0f : 2.5f;
				XMVECTOR ePos = XMLoadFloat3(&eTrans.position);
				XMVECTOR uiPos = ePos + XMVectorSet(0, yOffset, 0, 0);
				XMStoreFloat3(&uiTrans.position, uiPos);

				if (cam != NullEntity && reg.has<Transform>(cam)) {
					uiTrans.rotation = reg.get<Transform>(cam).rotation;
				}

				// HPバー更新
				float hpRatio = eStats.hp / eStats.maxHp;
				if (hpRatio < 0) hpRatio = 0;
				if (hpRatio > 0.5f) uiGeo.color = { 1.0f - (hpRatio - 0.5f) * 2, 1, 0, 1 };
				else uiGeo.color = { 1, hpRatio * 2, 0, 1 };

				uiTrans.scale = { 1.5f * hpRatio, 0.15f, 0.05f };
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::EnemyUISystem, "EnemyUISystem")