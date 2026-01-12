/*****************************************************************//**
 * @file	AnimatorControllerSerializer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___ANIMATOR_CONTROLLER_SERIALIZER_H___
#define ___ANIMATOR_CONTROLLER_SERIALIZER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Animation/AnimatorController.h"

namespace Arche
{
	class AnimatorControllerSerializer
	{
	public:
		static bool Serialize(const std::string& filepath, const std::shared_ptr<AnimatorController>& controller)
		{
			nlohmann::json root;

			// 1. 基本設定
			root["EntryState"] = controller->entryState.c_str();

			// 2. パラメータ
			root["Parameters"] = nlohmann::json::array();
			for (const auto& param : controller->parameters)
			{
				nlohmann::json pNode;
				pNode["Name"] = param.name;
				pNode["Type"] = (int)param.type;
				pNode["DefaultFloat"] = param.defaultFloat;
				pNode["DefaultInt"] = param.defaultInt;
				pNode["DefaultBool"] = param.defaultBool;
				root["Parameters"].push_back(pNode);
			}

			// 3. ステート
			root["States"] = nlohmann::json::array();
			for (const auto& state : controller->states)
			{
				nlohmann::json sNode;
				sNode["Name"] = state.name.c_str();
				sNode["MotionName"] = state.motionName;
				sNode["Speed"] = state.speed;
				sNode["Loop"] = state.loop;
				sNode["Position"] = { state.position[0], state.position[1] };

				// 遷移 (Transitions)
				sNode["Transitions"] = nlohmann::json::array();
				for (const auto& trans : state.transitions)
				{
					nlohmann::json tNode;
					tNode["TargetState"] = trans.targetState.c_str();
					tNode["HasExitTime"] = trans.hasExitTime;
					tNode["ExitTime"] = trans.exitTime;
					tNode["Duration"] = trans.duration;

					// 条件 (Conditions)
					tNode["Conditions"] = nlohmann::json::array();
					for (const auto& cond : trans.conditions)
					{
						nlohmann::json cNode;
						cNode["Parameter"] = cond.parameter;
						cNode["Mode"] = (int)cond.mode;
						cNode["Threshold"] = cond.threshold;
						tNode["Conditions"].push_back(cNode);
					}
					sNode["Transitions"].push_back(tNode);
				}
				root["States"].push_back(sNode);
			}

			// ファイル書き出し
			std::ofstream fout(filepath);
			if (fout.is_open())
			{
				fout << std::setw(4) << root;
				fout.close();
				return true;
			}
			return false;
		}

		static std::shared_ptr<AnimatorController> Deserialize(const std::string& filepath)
		{
			std::ifstream fin(filepath);
			if (!fin.is_open()) return nullptr;

			nlohmann::json root;
			fin >> root;
			fin.close();

			auto controller = std::make_shared<AnimatorController>();

			// 1. 基本設定
			if (root.contains("EntryState"))
				controller->entryState = root["EntryState"].get<std::string>();

			// 2. パラメータ
			if (root.contains("Parameters"))
			{
				for (const auto& pNode : root["Parameters"])
				{
					AnimatorParameter param;
					param.name = pNode["Name"].get<std::string>();
					param.type = (AnimatorParameterType)pNode["Type"].get<int>();
					param.defaultFloat = pNode["DefaultFloat"].get<float>();
					param.defaultInt = pNode["DefaultInt"].get<int>();
					param.defaultBool = pNode["DefaultBool"].get<bool>();
					controller->parameters.push_back(param);
				}
			}

			// 3. ステート
			if (root.contains("States"))
			{
				for (const auto& sNode : root["States"])
				{
					AnimatorState state;
					state.name = sNode["Name"].get<std::string>();
					state.motionName = sNode.value("MotionName", "");
					state.speed = sNode.value("Speed", 1.0f);
					state.loop = sNode.value("Loop", true);

					if (sNode.contains("Position"))
					{
						state.position[0] = sNode["Position"][0];
						state.position[1] = sNode["Position"][1];
					}

					if (sNode.contains("Transitions"))
					{
						for (const auto& tNode : sNode["Transitions"])
						{
							AnimatorTransition trans;
							trans.targetState = tNode["TargetState"].get<std::string>();
							trans.hasExitTime = tNode.value("HasExitTime", false);
							trans.exitTime = tNode.value("ExitTime", 1.0f);
							trans.duration = tNode.value("Duration", 0.25f);

							if (tNode.contains("Conditions"))
							{
								for (const auto& cNode : tNode["Conditions"])
								{
									AnimatorCondition cond;
									cond.parameter = cNode["Parameter"].get<std::string>();
									cond.mode = (AnimatorConditionMode)cNode["Mode"].get<int>();
									cond.threshold = cNode["Threshold"].get<float>();
									trans.conditions.push_back(cond);
								}
							}
							state.transitions.push_back(trans);
						}
					}
					controller->states.push_back(state);
				}
			}

			return controller;
		}
	};
}

#endif