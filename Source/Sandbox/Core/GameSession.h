#pragma once
#include <string>

namespace Arche
{
	// シーンをまたいで生存するデータ
	class GameSession
	{
	public:
		// --- ゲーム結果データ ---
		static int lastScore;			// スコア
		static float timeSurvived;		// 生存時間
		static int enemiesKilled;		// 撃破数
		static bool isGameClear;		// クリアか死亡か

		// --- 設定データ ---
		static int selectedStageId;		// 選択されたステージ (1, 2, 3...)

		// リセット用
		static void ResetResult()
		{
			lastScore = 0;
			timeSurvived = 0.0f;
			enemiesKilled = 0;
			isGameClear = false;
		}
	};

	// 実体定義 (本来は.cppに書くべきですが、簡易的にここで初期化)
	// ※リンクエラーが出る場合は .cpp に移動してください
	inline int GameSession::lastScore = 0;
	inline float GameSession::timeSurvived = 0.0f;
	inline int GameSession::enemiesKilled = 0;
	inline bool GameSession::isGameClear = false;
	inline int GameSession::selectedStageId = 1;
}