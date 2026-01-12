#ifndef ___ANIMATOR_CONTROLLER_H___
#define ___ANIMATOR_CONTROLLER_H___

#include "Engine/pch.h"
#include "Engine/Core/Base/StringId.h"
#include "Engine/Core/Base/Reflection.h"

namespace Arche
{
	// パラメータの型
	enum class AnimatorParameterType
	{
		Float,
		Int,
		Bool,
		Trigger
	};

	// 比較演算子
	enum class AnimatorConditionMode
	{
		If,				// Bool true
		IfNot,			// Bool false
		Greater,		// Float/Int >
		Less,			// Float/Int <
		Equals,			// Int ==
		NotEqual		// Int !=
	};

	// 遷移条件
	struct AnimatorCondition
	{
		AnimatorConditionMode mode;
		std::string parameter; // パラメータ名
		float threshold;	   // 閾値 (int/boolもここにキャストして格納)
	};

	// ステート遷移
	struct AnimatorTransition
	{
		StringId targetState;			// 遷移先ステート名
		bool hasExitTime = false;		// モーション終了時に自動遷移するか
		float exitTime = 1.0f;			// 遷移開始タイミング (0.0-1.0)
		float duration = 0.25f;			// クロスフェード時間
		std::vector<AnimatorCondition> conditions; // 遷移条件リスト
	};

	// ステート (1つのノード)
	struct AnimatorState
	{
		StringId name;				// ステート名 (例: "Idle", "Run")
		std::string motionName;		// 再生するモーション名 (Model内のキー)
		float speed = 1.0f;			// 再生速度
		bool loop = true;			// ループするか

		// エディタ用座標 (ImNodesで位置を復元するため)
		float position[2] = { 0.0f, 0.0f };

		std::vector<AnimatorTransition> transitions;
	};

	// パラメータ定義
	struct AnimatorParameter
	{
		std::string name;
		AnimatorParameterType type;
		float defaultFloat = 0.0f;
		int defaultInt = 0;
		bool defaultBool = false;
	};

	// アニメーターコントローラー (全体のアセットデータ)
	struct AnimatorController
	{
		std::vector<AnimatorParameter> parameters;
		std::vector<AnimatorState> states;
		StringId entryState; // 初期ステート

		// ヘルパー: 名前からステート検索
		const AnimatorState* FindState(const StringId& name) const
		{
			for (const auto& state : states)
			{
				if (state.name == name) return &state;
			}
			return nullptr;
		}
	};

	// リフレクション定義 (シリアライズ用)
	REFLECT_STRUCT_BEGIN(Arche::AnimatorCondition)
	REFLECT_VAR(mode) REFLECT_VAR(parameter) REFLECT_VAR(threshold)
	REFLECT_STRUCT_END()

	REFLECT_STRUCT_BEGIN(Arche::AnimatorTransition)
	REFLECT_VAR(targetState) REFLECT_VAR(hasExitTime) REFLECT_VAR(exitTime) REFLECT_VAR(duration) REFLECT_VAR(conditions)
	REFLECT_STRUCT_END()

	REFLECT_STRUCT_BEGIN(Arche::AnimatorState)
	REFLECT_VAR(name) REFLECT_VAR(motionName) REFLECT_VAR(speed) REFLECT_VAR(loop) REFLECT_VAR(position) REFLECT_VAR(transitions)
	REFLECT_STRUCT_END()

	REFLECT_STRUCT_BEGIN(Arche::AnimatorParameter)
	REFLECT_VAR(name) REFLECT_VAR(type) REFLECT_VAR(defaultFloat) REFLECT_VAR(defaultInt) REFLECT_VAR(defaultBool)
	REFLECT_STRUCT_END()

	REFLECT_STRUCT_BEGIN(Arche::AnimatorController)
	REFLECT_VAR(parameters) REFLECT_VAR(states) REFLECT_VAR(entryState)
	REFLECT_STRUCT_END()

}	// namespace Arche

#endif