#include "Engine/pch.h"
#include "SceneSerializer.h"
#include "Engine/Core/Application.h"
#include "Engine/Scene/Systems/Physics/CollisionSystem.h"
#include "Engine/Scene/Serializer/ComponentSerializer.h"
#include "Engine/Scene/Serializer/SystemRegistry.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{
	void SceneSerializer::SaveScene(World& world, const std::string& filepath)
	{
		json sceneJson;
		sceneJson["SceneName"] = "Untitled Scene";

		const auto& env = SceneManager::Instance().GetContext().environment;

		json envJson;
		envJson["SkyboxTexture"] = env.skyboxTexturePath;

		envJson["SkyColorTop"] = { env.skyColorTop.x, env.skyColorTop.y, env.skyColorTop.z, env.skyColorTop.w };
		envJson["SkyColorHorizon"] = { env.skyColorHorizon.x, env.skyColorHorizon.y, env.skyColorHorizon.z, env.skyColorHorizon.w };
		envJson["SkyColorBottom"] = { env.skyColorBottom.x, env.skyColorBottom.y, env.skyColorBottom.z, env.skyColorBottom.w };

		envJson["AmbientColor"] = { env.ambientColor.x, env.ambientColor.y, env.ambientColor.z };
		envJson["AmbientIntensity"] = env.ambientIntensity;

		sceneJson["Environment"] = envJson;

		// 1. レイヤー衝突設定
		sceneJson["Physics"]["LayerCollision"] = json::array();
		for (int i = 0; i < 32; ++i)
		{
			Layer layer = (Layer)(1 << i);
			Layer mask = PhysicsConfig::GetMask(layer);
			if ((int)mask != 0 && (int)mask != -1)
			{
				json layerJson;
				layerJson["Layer"] = (int)layer;
				layerJson["Mask"] = (int)mask;
				sceneJson["Physics"]["LayerCollision"].push_back(layerJson);
			}
		}

		// 2. システム構成
		sceneJson["Systems"] = json::array();
		for (const auto& sys : world.getSystems())
		{
			json sysJson;
			sysJson["Name"] = sys->m_systemName;
			sysJson["Group"] = (int)sys->m_group;
			sceneJson["Systems"].push_back(sysJson);
		}

		// 3. エンティティ
		sceneJson["Entities"] = json::array();
		auto& registry = world.getRegistry();

		registry.each([&](auto entityID)
			{
				Entity entity = entityID;
				// Tagを持たない（内部的な）エンティティは保存しない
				if (!registry.has<Tag>(entity)) return;

				json entityJson;
				entityJson["ID"] = (uint32_t)entity;

				entityJson["IsActive"] = registry.isActiveSelf(entity);

				ComponentSerializer::SerializeEntity(registry, entity, entityJson);
				sceneJson["Entities"].push_back(entityJson);
			});

		std::ofstream fout(filepath);
		if (fout.is_open()) {
			fout << sceneJson.dump(4);
			fout.close();
			Logger::Log("Scene Saved: " + filepath);
		}
		else {
			Logger::LogError("Failed to save scene: " + filepath);
		}
	}

	void SceneSerializer::LoadScene(World& world, const std::string& filepath)
	{
		std::ifstream fin(filepath);
		if (!fin.is_open()) {
			Logger::LogError("Failed to load scene: " + filepath);
			return;
		}

		world.clearSystems();
		world.clearEntities();
		CollisionSystem::Reset();
		PhysicsConfig::Reset();

		json sceneJson;
		try { fin >> sceneJson; }
		catch (json::parse_error& e) {
			Logger::LogError(std::string("JSON Parse Error: ") + e.what());
			return;
		}

		if (sceneJson.contains("Environment"))
		{
			auto& envJson = sceneJson["Environment"];
			auto& ctx = SceneManager::Instance().GetContext(); // 書き換え可能な参照を取得

			if (envJson.contains("SkyboxTexture"))
				ctx.environment.skyboxTexturePath = envJson["SkyboxTexture"].get<std::string>();

			// 色の読み込み (JSON配列 -> XMFLOAT4)
			auto LoadColor4 = [&](const char* key, XMFLOAT4& outVal) {
				if (envJson.contains(key)) {
					auto v = envJson[key];
					outVal = { v[0], v[1], v[2], v[3] };
				}
				};
			LoadColor4("SkyColorTop", ctx.environment.skyColorTop);
			LoadColor4("SkyColorHorizon", ctx.environment.skyColorHorizon);
			LoadColor4("SkyColorBottom", ctx.environment.skyColorBottom);

			if (envJson.contains("AmbientColor")) {
				auto v = envJson["AmbientColor"];
				ctx.environment.ambientColor = { v[0], v[1], v[2] };
			}
			if (envJson.contains("AmbientIntensity")) {
				ctx.environment.ambientIntensity = envJson["AmbientIntensity"].get<float>();
			}
		}

		// Physics
		if (sceneJson.contains("Physics") && sceneJson["Physics"].contains("LayerCollision"))
		{
			for (auto& layerJson : sceneJson["Physics"]["LayerCollision"])
			{
				Layer layer = (Layer)layerJson["Layer"].get<int>();
				Layer mask = (Layer)layerJson["Mask"].get<int>();
				PhysicsConfig::Configure(layer).setMask(mask);
			}
		}

		// Systems
		if (sceneJson.contains("Systems"))
		{
			for (auto& sysJson : sceneJson["Systems"])
			{
				std::string name = sysJson["Name"].get<std::string>();
				SystemGroup group = SystemGroup::PlayOnly;
				if (sysJson.contains("Group")) group = (SystemGroup)sysJson["Group"].get<int>();
				SystemRegistry::Instance().CreateSystem(world, name, group);
			}
		}

		// Entities
		auto& registry = world.getRegistry();
		registry.clear();
		std::unordered_map<uint32_t, Entity> idMap;

		if (sceneJson.contains("Entities")) {
			for (auto& entityJson : sceneJson["Entities"]) {
				Entity newEntity = registry.create();
				uint32_t oldID = entityJson["ID"].get<uint32_t>();
				idMap[oldID] = newEntity;

				if (entityJson.contains("IsActive"))
				{
					registry.setActive(newEntity, entityJson["IsActive"].get<bool>());
				}

				ComponentSerializer::DeserializeEntity(registry, newEntity, entityJson);
			}
		}

		// Relationship Fix
		for (auto const& [oldID, entity] : idMap)
		{
			if (registry.has<Relationship>(entity))
			{
				auto& rel = registry.get<Relationship>(entity);

				// 親IDの解決
				if (rel.parent != NullEntity) {
					uint32_t oldParentID = (uint32_t)rel.parent;
					if (idMap.count(oldParentID)) rel.parent = idMap[oldParentID];
					else rel.parent = NullEntity;
				}

				// 子IDリストの解決
				std::vector<Entity> newChildren;
				for (auto oldChildID : rel.children) {
					uint32_t oldID = (uint32_t)oldChildID;
					if (idMap.count(oldID)) newChildren.push_back(idMap[oldID]);
				}
				rel.children = newChildren;
			}
		}

		Logger::Log("Scene Loaded: " + filepath);
	}

	void SceneSerializer::RevertPrefab(World& world, Entity entity)
	{
		Registry& reg = world.getRegistry();

		// プレファブ情報を持っていなければ何もしない
		if (!reg.has<PrefabInstance>(entity)) return;

		std::string path = reg.get<PrefabInstance>(entity).prefabPath;
		if (!std::filesystem::exists(path))
		{
			Logger::LogError("Prefab file not found: " + path);
			return;
		}

		// ファイル読み込み
		std::ifstream fin(path);
		if (!fin.is_open()) return;
		json prefabJson;
		fin >> prefabJson;
		if (!prefabJson.is_array() || prefabJson.empty()) return;

		// --- 復元処理 ---

		// 1. 親子関係とPrefab情報のバックアップ
		Entity parent = NullEntity;
		if (reg.has<Relationship>(entity)) parent = reg.get<Relationship>(entity).parent;

		// 2. コンポーネントを一度すべて削除（クリーンな状態にする）
		// ComponentRegistryを使って全削除
		for (const auto& [name, iface] : ComponentRegistry::Instance().GetInterfaces())
		{
			iface.remove(reg, entity);
		}

		// 3. プレファブからデータをロード
		ComponentSerializer::DeserializeEntity(reg, entity, prefabJson[0]);

		// 4. バックアップ情報の復元
		// 親子関係
		if (reg.has<Relationship>(entity))
		{
			reg.get<Relationship>(entity).parent = parent;
		}
		else if (parent != NullEntity)
		{
			auto& rel = reg.emplace<Relationship>(entity);
			rel.parent = parent;
		}

		// PrefabInstance (Deserializeで入っているはずだが念のためパスを保証)
		if (!reg.has<PrefabInstance>(entity))
		{
			reg.emplace<PrefabInstance>(entity, path);
		}
		else
		{
			reg.get<PrefabInstance>(entity).prefabPath = path;
		}

		// 5. 子階層の再構築 (プレファブ構造に強制一致させる)
		// まず今の子を全削除
		std::function<void(Entity)> deleteChildren = [&](Entity e) {
			if (reg.has<Relationship>(e)) {
				auto children = reg.get<Relationship>(e).children;
				for (auto c : children) {
					deleteChildren(c);
					reg.destroy(c);
				}
				reg.get<Relationship>(e).children.clear();
			}
			};
		deleteChildren(entity);

		// JSONから子を再生成
		ReconstructPrefabChildren(world, entity, prefabJson);

		Logger::Log("Reverted to Prefab: " + path);
	}

	void SceneSerializer::CreateEmptyScene(const std::string& filepath)
	{
		json sceneJson;
		sceneJson["SceneName"] = "New Scene";
		sceneJson["Physics"]["LayerCollision"] = json::array();

		// 標準システム
		struct SysDef { std::string name; SystemGroup group; };
		std::vector<SysDef> standardSystems = {
			{ "Physics System", SystemGroup::PlayOnly },
			{ "Collision System", SystemGroup::PlayOnly }, { "UI System", SystemGroup::Always },
			{ "Lifetime System", SystemGroup::PlayOnly }, { "Hierarchy System", SystemGroup::Always },
			{"Animation System", SystemGroup::PlayOnly },
			{ "Render System", SystemGroup::Always }, { "Model Render System", SystemGroup::Always },
			{ "Billboard System", SystemGroup::Always }, { "Sprite Render System", SystemGroup::Always },
			{ "Text Render System", SystemGroup::Always }, { "Audio System", SystemGroup::Always },
			{ "Button System", SystemGroup::Always }
		};
		sceneJson["Systems"] = json::array();
		for (const auto& sys : standardSystems) {
			json s; s["Name"] = sys.name; s["Group"] = (int)sys.group;
			sceneJson["Systems"].push_back(s);
		}
		sceneJson["Entities"] = json::array();

		std::ofstream fout(filepath);
		if (fout.is_open()) { fout << sceneJson.dump(4); fout.close(); Logger::Log("Created: " + filepath); }
	}

	// ====================================================================================
	// ヘルパー: エンティティとその子孫を再帰的に削除
	// ====================================================================================
	void SceneSerializer::DestroyEntityRecursive(World& world, Entity entity)
	{
		auto& reg = world.getRegistry();

		if (reg.has<Relationship>(entity))
		{
			// コピーをとってから回す（削除中にイテレータが無効になるのを防ぐ）
			auto children = reg.get<Relationship>(entity).children;
			for (auto child : children)
			{
				DestroyEntityRecursive(world, child);
			}
		}
		world.getRegistry().destroy(entity);
	}

	// ====================================================================================
	// ヘルパー: 再帰的にエンティティと子要素をJSONにシリアライズ
	// ====================================================================================
	void SceneSerializer::SerializeEntityRecursive(Registry& reg, Entity entity, json& outJson)
	{
		// 1. 自身のコンポーネントを保存
		SceneSerializer::SerializeEntityToJson(reg, entity, outJson);

		// 2. 子要素を保存
		outJson["Children"] = json::array();
		if (reg.has<Relationship>(entity))
		{
			auto& rel = reg.get<Relationship>(entity);
			for (auto child : rel.children)
			{
				if (reg.valid(child))
				{
					json childJson;
					SerializeEntityRecursive(reg, child, childJson);
					outJson["Children"].push_back(childJson);
				}
			}
		}
	}

	// ====================================================================================
	// ヘルパー: 再帰的にJSONからエンティティと子要素を構築
	// ====================================================================================
	void SceneSerializer::ReconstructEntityRecursive(World& world, Entity parent, const json& jsonNode)
	{
		auto& reg = world.getRegistry();

		// 子エンティティを作成 (World::create_entity() を使用)
		Entity child = world.create_entity().id();

		// デフォルトでTagをつける
		if (!reg.has<Tag>(child)) reg.emplace<Tag>(child, "Child");

		// コンポーネント復元
		DeserializeEntityFromJson(reg, child, jsonNode);

		// 親子付け
		if (parent != NullEntity)
		{
			if (!reg.has<Relationship>(child)) reg.emplace<Relationship>(child);
			if (!reg.has<Relationship>(parent)) reg.emplace<Relationship>(parent);

			auto& childRel = reg.get<Relationship>(child);
			childRel.parent = parent;

			auto& parentRel = reg.get<Relationship>(parent);
			parentRel.children.push_back(child);
		}

		// さらにその子要素を再帰的に生成
		if (jsonNode.contains("Children") && jsonNode["Children"].is_array())
		{
			for (const auto& childNode : jsonNode["Children"])
			{
				ReconstructEntityRecursive(world, child, childNode);
			}
		}
	}

	// ====================================================================================
	// プレファブ保存
	// ====================================================================================
	void SceneSerializer::SavePrefab(Registry& reg, Entity root, const std::string& filepath)
	{
		json prefabJson;

		// ルートエンティティをオブジェクトとして保存 (配列にはしない)
		SerializeEntityRecursive(reg, root, prefabJson);

		std::ofstream o(filepath);
		o << std::setw(4) << prefabJson << std::endl;
	}

	// ====================================================================================
	// プレファブのロード (インスタンス化用)
	// ====================================================================================
	Entity SceneSerializer::LoadPrefab(World& world, const std::string& filepath)
	{
		std::ifstream i(filepath);
		if (!i.is_open()) return NullEntity;

		json prefabJson;
		try { i >> prefabJson; }
		catch (...) { return NullEntity; }
		i.close();

		if (prefabJson.is_null()) return NullEntity;

		// ★修正: 配列かオブジェクトかを判定し、ルートノードを特定する
		const json* rootNode = &prefabJson;
		if (prefabJson.is_array())
		{
			if (prefabJson.empty()) return NullEntity;
			rootNode = &prefabJson[0];
		}

		// ルートエンティティ作成
		Entity root = world.create_entity().id();

		// コンポーネント復元
		DeserializeEntityFromJson(world.getRegistry(), root, *rootNode);

		// PrefabInstance コンポーネントを付与してリンク情報を残す
		if (!world.getRegistry().has<PrefabInstance>(root))
		{
			world.getRegistry().emplace<PrefabInstance>(root, filepath);
		}

		// 子要素の再構築
		if (rootNode->contains("Children"))
		{
			for (const auto& childNode : (*rootNode)["Children"])
			{
				ReconstructEntityRecursive(world, root, childNode);
			}
		}

		return root;
	}

	// ====================================================================================
	// パス比較用ヘルパー
	// ====================================================================================
	static bool IsSamePath(const std::string& pathA, const std::string& pathB)
	{
		namespace fs = std::filesystem;
		try
		{
			// 1. ファイルが存在する場合、OSレベルで同一ファイルかチェック (最も確実)
			if (fs::exists(pathA) && fs::exists(pathB))
			{
				return fs::equivalent(pathA, pathB);
			}

			// 2. 文字列として正規化して比較 (存在しない場合やパス形式違い対策)
			fs::path a = fs::u8path(pathA).lexically_normal();
			fs::path b = fs::u8path(pathB).lexically_normal();

			// 小文字に統一して比較 (Windows対策)
			std::string strA = a.string();
			std::string strB = b.string();
			std::transform(strA.begin(), strA.end(), strA.begin(), ::tolower);
			std::transform(strB.begin(), strB.end(), strB.begin(), ::tolower);

			return strA == strB;
		}
		catch (...)
		{
			// エラー時は単純な文字列比較へフォールバック
			return pathA == pathB;
		}
	}

	// ====================================================================================
	// プレファブの変更を全インスタンスに適用 (プロパゲーション)
	// ====================================================================================
	void SceneSerializer::ReloadPrefabInstances(World& world, const std::string& filepath)
	{
		std::ifstream i(filepath);
		if (!i.is_open()) return;

		json prefabJson;
		try { i >> prefabJson; }
		catch (...) { return; }
		i.close();

		if (prefabJson.is_null()) return;

		// 配列かオブジェクトかを判定
		const json* rootNode = &prefabJson;
		if (prefabJson.is_array())
		{
			if (prefabJson.empty()) return;
			rootNode = &prefabJson[0];
		}

		auto& reg = world.getRegistry();
		std::vector<Entity> targets;

		// 対象を収集
		reg.view<PrefabInstance>().each([&](Entity e, PrefabInstance& pref) {
			if (IsSamePath(pref.prefabPath, filepath)) {
				targets.push_back(e);
			}
			});

		for (auto entity : targets)
		{
			// --- 1. 現状の維持したいデータをバックアップ ---
			Transform backupTransform;
			if (reg.has<Transform>(entity)) backupTransform = reg.get<Transform>(entity);

			std::string backupName = "Entity";
			if (reg.has<Tag>(entity)) backupName = reg.get<Tag>(entity).name.c_str();

			Entity parent = NullEntity;
			if (reg.has<Relationship>(entity)) parent = reg.get<Relationship>(entity).parent;

			// --- 2. 古い子要素をすべて削除 ---
			if (reg.has<Relationship>(entity))
			{
				std::function<void(Entity)> destroyRecursive = [&](Entity e) {
					if (reg.has<Relationship>(e)) {
						auto children = reg.get<Relationship>(e).children;
						for (auto child : children) destroyRecursive(child);
					}
					world.getRegistry().destroy(e);
					};

				auto children = reg.get<Relationship>(entity).children; // コピー
				for (auto child : children)
				{
					destroyRecursive(child);
				}
				reg.get<Relationship>(entity).children.clear();
			}

			// --- 3. 自身のコンポーネントをクリア ---
			for (auto& [name, iface] : ComponentRegistry::Instance().GetInterfaces())
			{
				iface.remove(reg, entity);
			}

			// --- 4. プレファブから復元 ---
			DeserializeEntityFromJson(reg, entity, *rootNode);

			// --- 5. バックアップデータを書き戻す (オーバーライド) ---
			// Transform
			if (reg.has<Transform>(entity)) {
				reg.get<Transform>(entity) = backupTransform;
			}
			else {
				reg.emplace<Transform>(entity, backupTransform);
			}

			// Tag
			if (reg.has<Tag>(entity)) {
				reg.get<Tag>(entity).name = backupName;
			}
			else {
				reg.emplace<Tag>(entity, backupName);
			}

			// PrefabInstance
			if (reg.has<PrefabInstance>(entity)) {
				reg.get<PrefabInstance>(entity).prefabPath = filepath;
			}
			else {
				reg.emplace<PrefabInstance>(entity, filepath);
			}

			// 親子関係
			if (parent != NullEntity)
			{
				if (!reg.has<Relationship>(entity)) reg.emplace<Relationship>(entity);
				reg.get<Relationship>(entity).parent = parent;
			}

			// --- 6. 子要素の再構築 ---
			if (rootNode->contains("Children"))
			{
				for (const auto& childNode : (*rootNode)["Children"])
				{
					ReconstructEntityRecursive(world, entity, childNode);
				}
			}
		}

		Logger::Log("Reloaded prefab instances: " + filepath);
	}

	void SceneSerializer::SerializeEntityToJson(Registry& registry, Entity entity, json& outJson) {
		outJson["ID"] = (uint32_t)entity;
		ComponentSerializer::SerializeEntity(registry, entity, outJson);
	}

	Entity SceneSerializer::DeserializeEntityFromJson(Registry& registry, const json& inJson) {
		Entity entity = registry.create();
		ComponentSerializer::DeserializeEntity(registry, entity, inJson);
		return entity;
	}

	void SceneSerializer::DeserializeEntityFromJson(Registry& registry, Entity entity, const json& inJson)
	{
		ComponentSerializer::DeserializeEntity(registry, entity, inJson);
	}

	void SceneSerializer::ReconstructPrefabChildren(World& world, Entity root, const json& prefabJson)
	{
		Registry& reg = world.getRegistry();
		std::map<uint32_t, Entity> idMap; // 旧ID -> 新ID

		// ルートのIDマップ登録 (JSONの最初の要素がルートと仮定)
		uint32_t rootOldID = prefabJson[0]["ID"].get<uint32_t>();
		idMap[rootOldID] = root;

		// JSONの2つ目以降（子要素）を生成
		for (size_t i = 1; i < prefabJson.size(); ++i)
		{
			const auto& eJson = prefabJson[i];
			uint32_t oldID = eJson["ID"].get<uint32_t>();

			// 新規作成
			Entity newEntity = world.create_entity().id();
			ComponentSerializer::DeserializeEntity(reg, newEntity, eJson);

			idMap[oldID] = newEntity;
		}

		// 親子関係の再構築
		// rootとその子要素の関係のみを復元する
		for (auto const& [oldID, newEntity] : idMap)
		{
			if (reg.has<Relationship>(newEntity))
			{
				auto& rel = reg.get<Relationship>(newEntity);

				// 親IDの解決
				if (rel.parent != NullEntity)
				{
					uint32_t oldParentID = (uint32_t)rel.parent;
					if (idMap.count(oldParentID))
					{
						rel.parent = idMap[oldParentID];
					}
					else
					{
						// マップにない親（プレファブ外）の場合はリンクを切るか、rootにする
						// 基本的にプレファブ内の子はプレファブ内の親を持つはず
						rel.parent = NullEntity;
					}
				}

				// 子IDリストの解決
				std::vector<Entity> newChildren;
				for (auto oldChildID : rel.children)
				{
					uint32_t oldCID = (uint32_t)oldChildID;
					if (idMap.count(oldCID))
					{
						newChildren.push_back(idMap[oldCID]);
					}
				}
				rel.children = newChildren;
			}
		}
	}

	void SceneSerializer::CollectAssets(const std::string& filepath, std::vector<std::string>& outModels, std::vector<std::string>& outTextures, std::vector<std::string>& outSounds)
	{
		std::ifstream stream(filepath);
		if (!stream.is_open()) return;

		json root;
		try {
			stream >> root;
		}
		catch (...) { return; }
		stream.close();

		if (!root.contains("Entities")) return;

		for (auto& entity : root["Entities"])
		{
			// MeshComponent -> Model
			if (entity.contains("MeshComponent")) {
				std::string path = entity["MeshComponent"].value("modelKey", "");
				if (!path.empty()) outModels.push_back(path);
			}

			// SpriteComponent -> Texture
			if (entity.contains("SpriteComponent")) {
				std::string path = entity["SpriteComponent"].value("textureKey", "");
				if (!path.empty()) outTextures.push_back(path);
			}

			// BillboardComponent -> Texture
			if (entity.contains("BillboardComponent")) {
				std::string path = entity["BillboardComponent"].value("textureKey", "");
				if (!path.empty()) outTextures.push_back(path);
			}

			// ParticleSystemComponent -> Texture
			if (entity.contains("ParticleSystemComponent")) {
				std::string path = entity["ParticleSystemComponent"].value("textureKey", "");
				if (!path.empty()) outTextures.push_back(path);
			}

			// AudioSource -> Sound
			if (entity.contains("AudioSource")) {
				std::string path = entity["AudioSource"].value("soundKey", "");
				if (!path.empty()) outSounds.push_back(path);
			}

			// Animator
			if (entity.contains("Animator"))
			{
				std::string ctrlPath = entity["Animator"].value("controllerPath", "");
				if (!ctrlPath.empty() && std::filesystem::exists(ctrlPath))
				{
					// コントローラーJSONを開く
					std::ifstream ctrlStream(ctrlPath);
					if (ctrlStream.is_open())
					{
						json ctrlJson;
						try {
							ctrlStream >> ctrlJson;
							if (ctrlJson.contains("States"))
							{
								for (const auto& state : ctrlJson["States"])
								{
									std::string motion = state.value("MotionName", "");
									// モーションファイルをモデルとしてプリロードリストに追加
									if (!motion.empty()) outModels.push_back(motion);
								}
							}
						}
						catch (...) {}
					}
				}
			}
		}

		// 重複削除
		auto Unique = [](std::vector<std::string>& v) {
			std::sort(v.begin(), v.end());
			v.erase(std::unique(v.begin(), v.end()), v.end());
			};
		Unique(outModels);
		Unique(outTextures);
		Unique(outSounds);
	}
	
	Entity SceneSerializer::DuplicateEntity(World& world, Entity entity)
	{
		Registry& reg = world.getRegistry();
		if (!reg.valid(entity)) return NullEntity;

		// 1. JSONにシリアライズ (子孫含む)
		json entityJson;
		SerializeEntityRecursive(reg, entity, entityJson);

		// 2. 親を取得
		Entity parent = NullEntity;
		if (reg.has<Relationship>(entity))
		{
			parent = reg.get<Relationship>(entity).parent;
		}

		// 3. 復元 (複製)
		// ルートエンティティを作成
		Entity newEntity = world.create_entity().id();
		DeserializeEntityFromJson(reg, newEntity, entityJson);

		// 名前を変更 (Unity風に "Name (Copy)" とする)
		if (reg.has<Tag>(newEntity))
		{
			std::string name = reg.get<Tag>(newEntity).name.c_str();
			name += " (Copy)";
			reg.get<Tag>(newEntity).name = name;
		}

		// 親子付け
		if (parent != NullEntity)
		{
			// 自分のRelationship
			if (!reg.has<Relationship>(newEntity)) reg.emplace<Relationship>(newEntity);
			reg.get<Relationship>(newEntity).parent = parent;

			// 親のRelationship
			if (reg.has<Relationship>(parent))
			{
				reg.get<Relationship>(parent).children.push_back(newEntity);
			}
		}

		// 子要素の再構築 (再帰処理)
		if (entityJson.contains("Children"))
		{
			for (const auto& childNode : entityJson["Children"])
			{
				ReconstructEntityRecursive(world, newEntity, childNode);
			}
		}

		Logger::Log("Duplicated Entity: " + std::to_string(entity) + " -> " + std::to_string(newEntity));
		return newEntity;
	}
}