#pragma once
#include <vector>
#include <cmath>
#include "Sandbox/Components/Enemy/EnemyStats.h"

namespace Arche
{
	struct SpawnEvent
	{
		float time;			// ゲーム開始からの秒数
		EnemyType type;		// 敵タイプ
		float angle;		// 出現角度 (プレイヤー中心の円周上)
		float dist;			// 距離
	};

	class StageDataProvider
	{
	public:
		static std::vector<SpawnEvent> GetStageData(int stageId)
		{
			std::vector<SpawnEvent> events;

			// 簡易ヘルパー: ウェーブ追加
			auto AddWave = [&](float startTime, int count, EnemyType type, float interval) {
				for (int i = 0; i < count; ++i) {
					float angle = (i * (360.0f / count)); // 円形に配置
					events.push_back({ startTime + i * interval, type, angle, 20.0f });
				}
				};

			// ステージごとの構成
			switch (stageId)
			{
			case 1: // Basic Training
				AddWave(2.0f, 5, EnemyType::Zako_Cube, 1.0f);
				AddWave(10.0f, 8, EnemyType::Zako_Cube, 0.5f);
				// Boss
				events.push_back({ 20.0f, EnemyType::Boss_Tank, 0.0f, 25.0f });
				break;

			case 2: // Armored
				AddWave(2.0f, 5, EnemyType::Zako_Cube, 1.0f);
				AddWave(8.0f, 4, EnemyType::Zako_Armored, 2.0f);
				// Boss
				events.push_back({ 25.0f, EnemyType::Boss_Prism, 180.0f, 25.0f });
				break;

			case 3: // Speed
				AddWave(2.0f, 10, EnemyType::Zako_Speed, 0.3f);
				AddWave(8.0f, 10, EnemyType::Zako_Speed, 0.3f);
				// Boss
				events.push_back({ 20.0f, EnemyType::Boss_Carrier, 90.0f, 30.0f });
				break;

			case 4: // Swarm
				AddWave(2.0f, 20, EnemyType::Zako_Cube, 0.1f); // 大量
				AddWave(10.0f, 5, EnemyType::Zako_Armored, 1.0f);
				// Boss
				events.push_back({ 25.0f, EnemyType::Boss_Construct, 0.0f, 25.0f });
				break;

			case 5: // Final
				AddWave(2.0f, 5, EnemyType::Zako_Armored, 1.0f);
				AddWave(5.0f, 10, EnemyType::Zako_Speed, 0.5f);
				AddWave(15.0f, 4, EnemyType::Boss_Tank, 2.0f); // 中ボスラッシュ
				// BIG BOSS
				events.push_back({ 30.0f, EnemyType::Boss_Omega, 0.0f, 40.0f });
				break;
			}

			return events;
		}
	};
}
