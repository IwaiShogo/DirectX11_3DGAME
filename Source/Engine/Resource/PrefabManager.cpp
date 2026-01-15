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

		// Transformを初期化 (位置・回転は引数から設定)
		auto& baseTrans = registry.emplace<Transform>(entity, position, rotation, XMFLOAT3(1, 1, 1));
		XMStoreFloat4x4(&baseTrans.worldMatrix, baseTrans.GetLocalMatrix());

		// JSONルートまたはComponentsオブジェクトを取得
		const auto& components = entityJson.contains("Components") ? entityJson.at("Components") : entityJson;

		// --------------------------------------------------------
		// Step A: Tagコンポーネントを最優先で作成
		// --------------------------------------------------------
		auto& tagComp = registry.emplace<Tag>(entity);
		tagComp.name = prefabName;
		tagComp.tag = "Untagged";

		// JSONにTag定義があれば上書き
		if (components.contains("Tag"))
		{
			const auto& tagJson = components.at("Tag");

			if (tagJson.contains("name") && tagJson["name"].is_string()) tagComp.name = tagJson["name"].get<std::string>();
			if (tagJson.contains("tag") && tagJson["tag"].is_string())	 tagComp.tag = tagJson["tag"].get<std::string>();

			// componentOrder (Inspectorの並び順) を復元する
			if (tagJson.contains("componentOrder") && tagJson["componentOrder"].is_array())
			{
				tagComp.componentOrder.clear();
				for (const auto& compName : tagJson["componentOrder"])
				{
					tagComp.componentOrder.push_back(compName.get<std::string>());
				}
			}
		}

		// --------------------------------------------------------
		// Step B: その他のコンポーネントを復元
		// --------------------------------------------------------
		auto& interfaces = ComponentRegistry::Instance().GetInterfaces();

		for (auto it = components.begin(); it != components.end(); ++it)
		{
			std::string typeName = it.key();

			// 無視するキー
			if (typeName == "Tag" || typeName == "ID" || typeName == "Children" || typeName == "Parent" || typeName == "IsActive")
			{
				continue;
			}

			// Transformの手動処理とリスト登録
			if (typeName == "Transform")
			{
				auto& trans = registry.get<Transform>(entity);

				// 位置と回転は引数を優先維持、スケールのみJSONから読み取る
				trans.position = position;
				trans.rotation = rotation;

				if (it.value().contains("scale"))
				{
					auto& s = it.value()["scale"];
					if (s.is_array() && s.size() == 3)
					{
						trans.scale = { (float)s[0], (float)s[1], (float)s[2] };
					}
				}

				XMStoreFloat4x4(&trans.worldMatrix, trans.GetLocalMatrix());

				// Inspector表示用リストになければ追加 (JSONから読み込めていない場合の保険)
				if (std::find(tagComp.componentOrder.begin(), tagComp.componentOrder.end(), "Transform") == tagComp.componentOrder.end())
				{
					tagComp.componentOrder.push_back("Transform");
				}
				continue;
			}

			// その他のコンポーネント
			auto itInterface = interfaces.find(typeName);
			if (itInterface != interfaces.end())
			{
				const auto& iface = itInterface->second;
				iface.add(registry, entity); // これが自動的にcomponentOrderに追加するはず
				iface.deserialize(registry, entity, components);
			}
		}

		return entity;
	}
}