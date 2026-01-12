/*****************************************************************//**
 * @file	SceneSerializer.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/17	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___SCENE_SERIALIZER_H___
#define ___SCENE_SERIALIZER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"

namespace Arche
{

	class ARCHE_API SceneSerializer
	{
	public:
		static void SaveScene(World& world, const std::string& filepath);
		static void LoadScene(World& world, const std::string& filepath);
		static void RevertPrefab(World& world, Entity entity);
		static void CreateEmptyScene(const std::string& filepath);

		// プレファブ保存・読み込み
		static void SavePrefab(Registry& reg, Entity root, const std::string& filepath);
		static Entity LoadPrefab(World& world, const std::string& filepath);

		// プレファブの変更をシーン上のインスタンスに適用する
		static void ReloadPrefabInstances(World& world, const std::string& filepath);

		// ヘルパー再帰処理用
		static void SerializeEntityRecursive(Registry& reg, Entity entity, json& outJson);
		static void ReconstructEntityRecursive(World& world, Entity parent, const json& jsonNode);

		// エンティティとその子孫を全て削除する
		static void DestroyEntityRecursive(World& world, Entity entity);

		static void ReconstructPrefabChildren(World& world, Entity root, const json& prefabJson);
		// 個別保存用（必要なら実装、使わなければ削除可）
		static void SerializeEntityToJson(Registry& registry, Entity entity, json& outJson);
		static Entity DeserializeEntityFromJson(Registry& registry, const json& inJson);
		static void DeserializeEntityFromJson(Registry& registry, Entity entity, const json& inJson);


		static void CollectAssets(const std::string& filepath, 
			std::vector<std::string>& outModels, 
			std::vector<std::string>& outTextures, 
			std::vector<std::string>& outSounds);

		static Entity DuplicateEntity(World& world, Entity entity);
	};

}	// namespace Arche

#endif // !___SCENE_SERIALIZER_H___