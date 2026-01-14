/*****************************************************************//**
 * @file	PrefabManager.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#include "Engine/pch.h"
#include "PrefabManager.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	void PrefabManager::Initialize(const std::string& directory)
	{
		m_directory = directory;
		ReloadPrefabs();
	}

	void PrefabManager::ReloadPrefabs()
	{
		m_prefabs.clear();
		if (!std::filesystem::exists(m_directory))
		{
			std::filesystem::create_directories(m_directory);
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_directory))
		{
			if (entry.path().extension() == ".json")
			{
				std::ifstream file(entry.path());
				if (file.is_open())
				{
					nlohmann::json data;
					file >> data;

					// ファイル名をキーにする
					std::string name = entry.path().stem().string();
					m_prefabs[name] = data;

					Logger::Log("Loaded Prefab: " + name);
				}
			}
		}
	}

	const nlohmann::json* PrefabManager::GetPrefabData(const std::string& name)
	{
		if (m_prefabs.find(name) != m_prefabs.end())
		{
			return &m_prefabs[name];
		}
		Logger::LogError("Prefab not found: " + name);
		return nullptr;
	}

	Entity PrefabManager::Spawn(Registry& registry, const std::string& prefabName, const XMFLOAT3& position, const XMFLOAT3& rotation)
	{
		const auto* data = GetPrefabData(prefabName);
		if (!data)
		{
			Logger::LogError("Prefab not found: " + prefabName);
			return NullEntity;
		}

		const nlohmann::json& entityJson = *data;

		// 1. エンティティ作成
		Entity entity = registry.create();

		auto& baseTrans = registry.emplace<Transform>(entity, position, rotation, XMFLOAT3(1, 1, 1));
		XMStoreFloat4x4(&baseTrans.worldMatrix, baseTrans.GetLocalMatrix());

		// JSONルートまたはComponentsオブジェクトを取得
		const auto& components = entityJson.contains("Components") ? entityJson.at("Components") : entityJson;

		// --------------------------------------------------------
		// Step A: Tagコンポーネントを最優先で作成 (強制手動設定)
		// --------------------------------------------------------
		// まず空のTagコンポーネントを確実に作成する
		auto& tagComp = registry.emplace<Tag>(entity);

		// デフォルトはプレファブ名をセット
		tagComp.name = prefabName;
		tagComp.tag = "Untagged";

		// JSONにTag定義があれば上書きする
		if (components.contains("Tag"))
		{
			const auto& tagJson = components.at("Tag");

			// nameフィールドがあれば読み込む
			if (tagJson.contains("name") && tagJson["name"].is_string())
			{
				tagComp.name = tagJson["name"].get<std::string>();
			}

			// tagフィールドがあれば読み込む
			if (tagJson.contains("tag") && tagJson["tag"].is_string())
			{
				tagComp.tag = tagJson["tag"].get<std::string>();
			}
		}

		// --------------------------------------------------------
		// Step B: その他のコンポーネントを復元 (ComponentRegistry使用)
		// --------------------------------------------------------
		auto& interfaces = ComponentRegistry::Instance().GetInterfaces();

		for (auto it = components.begin(); it != components.end(); ++it)
		{
			std::string typeName = it.key();

			// 無視するキー (Tagは処理済みなのでスキップ)
			if (typeName == "Tag" || typeName == "ID" || typeName == "Children" || typeName == "Parent" || typeName == "IsActive")
			{
				continue;
			}

			if (typeName == "Transform")
			{
				auto itInterface = interfaces.find(typeName);
				if (itInterface != interfaces.end())
				{
					itInterface->second.deserialize(registry, entity, components);

					auto& trans = registry.get<Transform>(entity);
					trans.position = position;
					trans.rotation = rotation;

					XMStoreFloat4x4(&trans.worldMatrix, trans.GetLocalMatrix());
				}
				continue;
			}

			auto itInterface = interfaces.find(typeName);
			if (itInterface != interfaces.end())
			{
				const auto& iface = itInterface->second;
				iface.add(registry, entity);
				iface.deserialize(registry, entity, components);
			}
		}

		return entity;
	}
}