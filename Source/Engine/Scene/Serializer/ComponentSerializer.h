/*****************************************************************//**
 * @file	ComponentSerializer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/20	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___COMPONENT_SERIALIZER_H___
#define ___COMPONENT_SERIALIZER_H___

// ===== インクルード =====
#include "Engine/Scene/Serializer/ComponentRegistry.h"

namespace Arche
{
	class ComponentSerializer
	{
	public:
		// エンティティが持っている全コンポーネントをJSONに変換
		static void SerializeEntity(Registry& reg, Entity entity, json& outJson)
		{
			// レジストリに登録された全コンポーネント型を走査
			for (auto& [name, iface] : ComponentRegistry::Instance().GetInterfaces())
			{
				// 持っていればJSONに書き込む
				iface.serialize(reg, entity, outJson);
			}
		}

		// JSONからコンポーネントを復元
		static void DeserializeEntity(Registry& reg, Entity entity, const json& inJson)
		{
			// レジストリに登録された全コンポーネント型を走査
			for (auto& [name, iface] : ComponentRegistry::Instance().GetInterfaces())
			{
				// JSONに含まれていれば復元
				iface.deserialize(reg, entity, inJson);
			}
		}
	};

}	// namespace Arche

#endif // !___COMPONENT_SERIALIZER_H___