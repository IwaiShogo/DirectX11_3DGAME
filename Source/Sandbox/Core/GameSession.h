#pragma once

namespace Arche
{
	/**
	 * @struct GameSession
	 * @brief  シーン間で共有するゲーム進行データ
	 * C++17の inline static を使用して、ヘッダーのみで実体を定義・共有します。
	 */
	struct GameSession
	{
		// 選択されたステージID (デフォルト1)
		inline static int selectedStageId = 1;

		// 現在のウェーブ数
		inline static int currentWave = 1;
		// 全ウェーブ数
		inline static int totalWaves = 1;

		// 撃破数
		inline static int killCount = 0;

		// 直前のスコア
		inline static int lastScore = 0;

		// ゲームクリアフラグ
		inline static bool isGameClear = false;
	};
}