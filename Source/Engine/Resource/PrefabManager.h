/*****************************************************************//**
 * @file	PrefabManager.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___PREFAB_MANAGER_H___
#define ___PREFAB_MANAGER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"

namespace Arche
{
	class ARCHE_API PrefabManager
	{
	public:
		static PrefabManager& Instance() { static PrefabManager instance; return instance; }

		// 初期化（指定フォルダ内の全JSONをロード）
		void Initialize(const std::string& directory = "Resources/Game/Prefabs");

		// プレファブからエンティティを生成
		Entity Spawn(Registry& registry, const std::string& prefabName, const XMFLOAT3& position = { 0,0,0 }, const XMFLOAT3& rotation = { 0,0,0 });

		// プレファブデータの取得
		const json* GetPrefabData(const std::string& name);

		// リロード（エディタ用）
		void ReloadPrefabs();

	private:
		PrefabManager() = default;
		~PrefabManager() = default;

		std::string m_directory;
		std::unordered_map<std::string, json> m_prefabs;
	};
}

#endif // !___PREFAB_MANAGER_H___