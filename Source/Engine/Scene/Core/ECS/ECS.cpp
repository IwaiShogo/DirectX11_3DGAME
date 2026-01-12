/*****************************************************************//**
 * @file	ECS.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "ECS.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	std::size_t ComponentTypeManager::GetID(const char* typeName)
	{
		static std::unordered_map<std::string, std::size_t> types;
		static std::size_t count = 0;

		std::string key = typeName;
		if (types.find(key) == types.end())
		{
			types[key] = count++;
		}
		return types[key];
	}

	EntityHandle& EntityHandle::setParent(Entity parentId)
	{
		// 1. 自分にRelationshipを追加
		if (!registry->has<Relationship>(entity))
		{
			registry->emplace<Relationship>(entity);
		}
		auto& myRel = registry->get<Relationship>(entity);
		myRel.parent = parentId;

		// 2. 親のRelationshipを取得
		if (!registry->has<Relationship>(parentId))
		{
			registry->emplace<Relationship>(parentId);
		}
		auto& parentRel = registry->get<Relationship>(parentId);

		// 3. 親の子リストに自分を追加
		parentRel.children.push_back(entity);

		return *this;	// チェーン出来るように自分を返す
	}

}	// namespace Arche