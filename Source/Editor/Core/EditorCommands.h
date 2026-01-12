/*****************************************************************//**
 * @file	EditorCommands.h
 * @brief	具体的な編集コマンド定義
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___EDITOR_COMMANDS_H___
#define ___EDITOR_COMMANDS_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/CommandHistory.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"

namespace Arche
{
	static void RemoveChildFromParent(Registry& reg, Entity parent, Entity child)
	{
		if (parent != NullEntity && reg.valid(parent) && reg.has<Relationship>(parent))
		{
			auto& children = reg.get<Relationship>(parent).children;
			children.erase(
				std::remove(children.begin(), children.end(), child),
				children.end()
			);
		}
	}

	// エンティティ削除コマンド
	// ------------------------------------------------------------
	class DeleteEntityCommand
		: public ICommand
	{
	public:
		DeleteEntityCommand(World& world, Entity entity)
			: m_world(world), m_targetEntity(entity), m_parentOfTarget(NullEntity)
		{
			Registry& reg = m_world.getRegistry();

			// 1. 削除対象の親を覚えておく（Undoでの再結合用）
			if (reg.has<Relationship>(entity))
			{
				m_parentOfTarget = reg.get<Relationship>(entity).parent;
			}

			// 2. 自分と全子孫を再帰的にバックアップ
			CollectDescendants(reg, entity);
		}

		void Execute() override
		{
			Registry& reg = m_world.getRegistry();

			// 1. 親から切り離す
			if (m_parentOfTarget != NullEntity)
			{
				RemoveChildFromParent(reg, m_parentOfTarget, m_targetEntity);
			}

			// 2. 収集した全エンティティを削除（リストは親->子の順なので、逆順(子->親)で消すと安全だがEnTTは順序不問）
			// IDが無効になる前に全て削除
			for (const auto& backup : m_backups)
			{
				Entity e = (Entity)backup.originalID;
				if (reg.valid(e))
				{
					reg.destroy(e);
				}
			}
		}

		void Undo() override
		{
			Registry& reg = m_world.getRegistry();

			// 旧ID -> 新Entity の変換マップ
			std::map<uint32_t, Entity> idMap;

			// 1. 全エンティティを復元（コンポーネントデータ含む）
			Entity newTargetEntity = NullEntity;

			for (const auto& backup : m_backups)
			{
				// 生成 & デシリアライズ
				Entity newEntity = SceneSerializer::DeserializeEntityFromJson(reg, backup.data);

				// マップに登録
				idMap[backup.originalID] = newEntity;

				// ターゲット（削除の起点となったエンティティ）の新しいIDを保持
				if (backup.originalID == (uint32_t)m_targetEntity)
				{
					newTargetEntity = newEntity;
				}
			}

			// ターゲットのIDを更新（次回Redoのため）
			m_targetEntity = newTargetEntity;

			// 2. 親子関係のIDリンクを修復（IDリマッピング）
			for (auto& [oldId, newEntity] : idMap)
			{
				if (!reg.has<Relationship>(newEntity)) continue;
				auto& rel = reg.get<Relationship>(newEntity);

				// Parentの書き換え
				if (rel.parent != NullEntity)
				{
					uint32_t oldParentID = (uint32_t)rel.parent;
					// 復元されたグループ内に親がいるならIDを書き換え
					if (idMap.count(oldParentID))
					{
						rel.parent = idMap[oldParentID];
					}
					// 親が削除対象外（生き残っている親）なら、IDはそのまま
				}

				// Childrenの書き換え
				for (auto& child : rel.children)
				{
					uint32_t oldChildID = (uint32_t)child;
					if (idMap.count(oldChildID))
					{
						child = idMap[oldChildID];
					}
				}
			}

			// 3. 元の親（削除されずに残っていた親）に、復元したターゲットを再接続
			if (m_parentOfTarget != NullEntity && reg.valid(m_parentOfTarget))
			{
				if (!reg.has<Relationship>(m_parentOfTarget))
					reg.emplace<Relationship>(m_parentOfTarget);

				auto& parentRel = reg.get<Relationship>(m_parentOfTarget);
				// 重複チェックして追加
				if (std::find(parentRel.children.begin(), parentRel.children.end(), m_targetEntity) == parentRel.children.end())
				{
					parentRel.children.push_back(m_targetEntity);
				}

				// ターゲット側の親も確実に設定
				if (!reg.has<Relationship>(m_targetEntity)) reg.emplace<Relationship>(m_targetEntity);
				reg.get<Relationship>(m_targetEntity).parent = m_parentOfTarget;
			}
		}

	private:
		// 再帰的に子孫を集める（親 -> 子 の順でリストに追加）
		void CollectDescendants(Registry& reg, Entity root)
		{
			// 自分のバックアップ
			json data;
			SceneSerializer::SerializeEntityToJson(reg, root, data);
			m_backups.push_back({ (uint32_t)root, data });

			// 子がいれば再帰
			if (reg.has<Relationship>(root))
			{
				for (auto child : reg.get<Relationship>(root).children)
				{
					CollectDescendants(reg, child);
				}
			}
		}

	private:
		struct EntityBackupData
		{
			uint32_t originalID;	// 元のID
			json data;				// シリアライズデータ
		};

		World& m_world;
		Entity m_targetEntity; // 削除対象（ルート）
		Entity m_parentOfTarget; // 削除対象の親ID（Undo復帰用）

		// 復元用データリスト
		std::vector<EntityBackupData> m_backups;
	};

	// エンティティ生成コマンド
	// ------------------------------------------------------------
	class CreateEntityCommand
		: public ICommand
	{
	public:
		CreateEntityCommand(World& world) : m_world(world) {}

		void Execute() override
		{
			// 生成
			m_createdEntity = m_world.create_entity()
				.add<Tag>("New Entity")
				.add<Transform>().id();
		}

		void Undo() override
		{
			// 生成の取り消し = 削除
			if (m_world.getRegistry().valid(m_createdEntity))
			{
				m_world.getRegistry().destroy(m_createdEntity);
			}
		}
		
	private:
		World& m_world;
		Entity m_createdEntity = NullEntity;
	};

	// 親子関係変更コマンド
	// ------------------------------------------------------------
	class ReparentEntityCommand
		: public ICommand
	{
	public:
		ReparentEntityCommand(World& world, Entity child, Entity newParent)
			: m_world(world), m_child(child), m_newParent(newParent), m_oldParent(NullEntity)
		{
			Registry& reg = m_world.getRegistry();
			if (reg.has<Relationship>(child))
			{
				m_oldParent = reg.get<Relationship>(child).parent;
			}
		}

		void Execute() override
		{
			SetParent(m_child, m_newParent);
		}

		void Undo() override
		{
			SetParent(m_child, m_oldParent);
		}

	private:
		void SetParent(Entity child, Entity parent)
		{
			Registry& reg = m_world.getRegistry();
			if (!reg.valid(child)) return;

			// 1. 旧親から離脱
			if (reg.has<Relationship>(child))
			{
				Entity currParent = reg.get<Relationship>(child).parent;
				RemoveChildFromParent(reg, currParent, child);
			}

			// 2. 新規へ所属
			if (!reg.has<Relationship>(child)) reg.emplace<Relationship>(child);
			reg.get<Relationship>(child).parent = parent;

			if (parent != NullEntity && reg.valid(parent))
			{
				if (!reg.has<Relationship>(parent)) reg.emplace<Relationship>(parent);
				// 重複防止
				auto& children = reg.get<Relationship>(parent).children;
				if (std::find(children.begin(), children.end(), child) == children.end())
				{
					children.push_back(child);
				}
			}
		}

	private:
		World& m_world;
		Entity m_child;
		Entity m_newParent;
		Entity m_oldParent;
	};

	// コンポーネント追加コマンド
	// ------------------------------------------------------------
	class AddComponentCommand
		: public ICommand
	{
	public:
		AddComponentCommand(World& world, Entity entity, const std::string& compName)
			: m_world(world), m_entity(entity), m_componentName(compName) {}

		void Execute() override
		{
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).add(reg, m_entity);
			}
		}

		void Undo()
		{
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).remove(reg, m_entity);
			}
		}

	private:
		World& m_world;
		Entity m_entity;
		std::string m_componentName;
	};

	// コンポーネント削除コマンド
	// ------------------------------------------------------------
	class RemoveComponentCommand
		: public ICommand
	{
	public:
		RemoveComponentCommand(World& world, Entity entity, const std::string& compName)
			: m_world(world), m_entity(entity), m_componentName(compName)
		{
			// 削除前に現在の状態を保存
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).serialize(reg, m_entity, m_backup);
			}
		}

		void Execute() override
		{
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).remove(reg, m_entity);
			}
		}

		void Undo() override
		{
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).deserialize(reg, m_entity, m_backup);
			}
		}

	private:
		World& m_world;
		Entity m_entity;
		std::string m_componentName;
		json m_backup;
	};

	// コンポーネント値変更コマンド
	// ------------------------------------------------------------
	class ChangeComponentValueCommand
		: public ICommand
	{
	public:
		ChangeComponentValueCommand(World& world, Entity entity, const std::string& compName, const json& oldVal, const json& newVal)
			: m_world(world), m_entity(entity), m_componentName(compName), m_oldState(oldVal), m_newState(newVal) {}

		void Execute() override
		{
			ApplyState(m_newState);
		}

		void Undo() override
		{
			ApplyState(m_oldState);
		}

	private:
		void ApplyState(const json& state)
		{
			Registry& reg = m_world.getRegistry();
			auto& interfaces = ComponentRegistry::Instance().GetInterfaces();
			if (interfaces.count(m_componentName))
			{
				interfaces.at(m_componentName).deserialize(reg, m_entity, state);
			}
		}

	private:
		World& m_world;
		Entity m_entity;
		std::string m_componentName;
		json m_oldState;
		json m_newState;
	};

	// コンポーネント順序変更コマンド
	// ------------------------------------------------------------
	class ReorderComponentCommand
		: public ICommand
	{
	public:
		ReorderComponentCommand(World& world, Entity entity, const std::vector<std::string>& oldOrder, const std::vector<std::string>& newOrder)
			: m_world(world), m_entity(entity), m_oldOrder(oldOrder), m_newOrder(newOrder) {}

		void Execute() override
		{
			if (m_world.getRegistry().has<Tag>(m_entity))
			{
				m_world.getRegistry().get<Tag>(m_entity).componentOrder = m_newOrder;
			}
		}

		void Undo() override
		{
			if (m_world.getRegistry().has<Tag>(m_entity))
			{
				m_world.getRegistry().get<Tag>(m_entity).componentOrder = m_oldOrder;
			}
		}

	private:
		World& m_world;
		Entity m_entity;
		std::vector<std::string> m_oldOrder;
		std::vector<std::string> m_newOrder;
	};

	class MoveEntityCommand : public ICommand
	{
	public:
		// newIndex が -1 の場合は末尾に追加
		MoveEntityCommand(World& world, Entity entity, Entity newParent, int newIndex = -1)
			: m_world(world), m_entity(entity), m_newParent(newParent), m_newIndex(newIndex)
		{
			auto& reg = m_world.getRegistry();

			// 現在の親とインデックスを保存（Undo用）
			if (reg.has<Relationship>(entity))
			{
				m_oldParent = reg.get<Relationship>(entity).parent;

				// 旧親の子リストから自分の位置を探す
				if (m_oldParent != NullEntity)
				{
					auto& children = reg.get<Relationship>(m_oldParent).children;
					auto it = std::find(children.begin(), children.end(), entity);
					if (it != children.end()) m_oldIndex = (int)std::distance(children.begin(), it);
				}
				else
				{
					m_oldIndex = 0;
				}
			}
		}

		void Execute() override
		{
			Move(m_newParent, m_newIndex);
		}

		void Undo() override
		{
			Move(m_oldParent, m_oldIndex);
		}

	private:
		void Move(Entity parent, int index)
		{
			auto& reg = m_world.getRegistry();
			if (!reg.valid(m_entity)) return;

			// 1. 現在の親から削除
			Entity currentParent = NullEntity;
			if (reg.has<Relationship>(m_entity))
			{
				currentParent = reg.get<Relationship>(m_entity).parent;
			}

			if (currentParent != NullEntity && reg.valid(currentParent))
			{
				auto& children = reg.get<Relationship>(currentParent).children;
				auto it = std::find(children.begin(), children.end(), m_entity);
				if (it != children.end()) children.erase(it);
			}

			// 2. 新しい親に登録
			if (parent != NullEntity && reg.valid(parent))
			{
				if (!reg.has<Relationship>(parent)) reg.emplace<Relationship>(parent);
				auto& children = reg.get<Relationship>(parent).children;

				// インデックスの正規化
				if (index < 0 || index >(int)children.size()) index = (int)children.size();

				children.insert(children.begin() + index, m_entity);

				// コンポーネント更新
				if (!reg.has<Relationship>(m_entity)) reg.emplace<Relationship>(m_entity);
				reg.get<Relationship>(m_entity).parent = parent;
			}
			else
			{
				// 親なし（ルート）にする場合
				if (!reg.has<Relationship>(m_entity)) reg.emplace<Relationship>(m_entity);
				reg.get<Relationship>(m_entity).parent = NullEntity;
			}
		}

		World& m_world;
		Entity m_entity;
		Entity m_newParent;
		Entity m_oldParent = NullEntity;
		int m_newIndex;
		int m_oldIndex = 0;
	};

}	// namespace Arche

#endif // !___EDITOR_COMMANDS_H___