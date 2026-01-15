#pragma once

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"

namespace Arche
{
	class SurvivalSystem : public ISystem
	{
	public:
		SurvivalSystem()
		{
			m_systemName = "SurvivalSystem";
		}

		void Update(Registry& registry) override
		{
			float dt = Time::DeltaTime();

			// プレイヤーの時間管理
			auto view = registry.view<PlayerTime, Transform>();
			for (auto entity : view)
			{
				auto& timeData = view.get<PlayerTime>(entity);
				auto& trans = view.get<Transform>(entity);

				if (timeData.isDead) continue;

				// 1. 時間の自然減少
				timeData.currentTime -= dt;

				// 2. クライマックス演出 (残り1秒以下でスロー)
				// ヒットストップ中でなければ適用
				if (timeData.currentTime <= 1.0f && timeData.currentTime > 0.0f)
				{
					Time::timeScale = 0.1f; // 世界を遅くする
				}
				else
				{
					Time::timeScale = 1.0f; // 通常速度
				}

				// 3. ゲームオーバー判定
				if (timeData.currentTime <= 0.0f)
				{
					timeData.currentTime = 0.0f;
					timeData.isDead = true;
					Time::timeScale = 0.0f; // 完全停止
					Logger::Log("TIME UP! GAME OVER");
				}

				// 4. 無敵時間の更新
				if (timeData.invincibilityTimer > 0.0f)
				{
					timeData.invincibilityTimer -= dt;
					// 点滅処理 (GeometricDesignがあれば)
					if (registry.has<GeometricDesign>(entity))
					{
						// 0.1秒ごとに表示切替
						registry.get<GeometricDesign>(entity).isWireframe = (int)(timeData.invincibilityTimer * 20.0f) % 2 == 0;
					}
				}
				else
				{
					if (registry.has<GeometricDesign>(entity))
						registry.get<GeometricDesign>(entity).isWireframe = false;
				}

				// 5. 足元の円形ゲージ (リング) の更新
				UpdateRingVisual(registry, entity, timeData);
			}
		}

	private:
		// プレイヤーIDとリングEntityIDの紐づけ
		std::map<Entity, Entity> m_ringMap;

		void UpdateRingVisual(Registry& reg, Entity player, const PlayerTime& timeData)
		{
			// リングが存在しなければ作成
			if (m_ringMap.find(player) == m_ringMap.end() || !reg.valid(m_ringMap[player]))
			{
				Entity ring = reg.create();
				reg.emplace<Transform>(ring);
				auto& geo = reg.emplace<GeometricDesign>(ring);
				geo.shapeType = GeoShape::Torus; // ドーナツ型
				geo.color = { 0.0f, 1.0f, 1.0f, 0.8f }; // シアン

				// プレイヤーの子要素に設定（追従）
				if (!reg.has<Relationship>(player)) reg.emplace<Relationship>(player);
				if (!reg.has<Relationship>(ring)) reg.emplace<Relationship>(ring);

				reg.get<Relationship>(player).children.push_back(ring);
				reg.get<Relationship>(ring).parent = player;

				m_ringMap[player] = ring;
			}

			Entity ring = m_ringMap[player];
			if (reg.has<Transform>(ring) && reg.has<GeometricDesign>(ring))
			{
				auto& t = reg.get<Transform>(ring);
				auto& geo = reg.get<GeometricDesign>(ring);

				// --- 演出ロジック ---
				float ratio = timeData.currentTime / timeData.maxTime; // 割合 (0.0 ~ 1.0)

				// サイズ: 時間が減るとリングが縮む (最小0.3倍)
				float scale = 2.0f * ratio;
				if (scale < 0.3f) scale = 0.3f;
				t.scale = { scale, 0.1f, scale };
				t.position = { 0.0f, -0.0f, 0.0f }; // 足元へ

				// 色: 緑(安全) -> 黄 -> 赤(危険)
				if (ratio > 0.5f) geo.color = { 0.0f, 1.0f, 0.0f, 0.8f };		// Green
				else if (ratio > 0.2f) geo.color = { 1.0f, 1.0f, 0.0f, 0.8f };	// Yellow
				else geo.color = { 1.0f, 0.0f, 0.0f, 1.0f };					// Red
			}
		}
	};
}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::SurvivalSystem, "SurvivalSystem")
