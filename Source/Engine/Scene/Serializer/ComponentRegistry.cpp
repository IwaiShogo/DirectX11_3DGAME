#include "Engine/pch.h"
#include "ComponentRegistry.h"
#include "Engine/Core/Base/StringId.h"
#include "Engine/Scene/Components/Components.h"

#ifdef _DEBUG
#include "Editor/Core/InspectorGui.h"
#endif // _DEBUG


namespace Arche
{
	// 実装（これらは全て Engine 側のヒープで実行されます）
	void JsonWrite(json& j, const char* key, int val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, long val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, long long val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, unsigned int val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, unsigned long val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, unsigned long long val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, float val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, double val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, bool val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, char val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, unsigned char val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, const std::string& val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, const char* val) { j[key] = val; }

	// StringId (数値として保存)
	void JsonWrite(json& j, const char* key, const StringId& val) { j[key] = val.c_str(); }

	// Vector系
	void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT2& val) { j[key] = { val.x, val.y }; }
	void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT3& val) { j[key] = { val.x, val.y, val.z }; }
	void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT4& val) { j[key] = { val.x, val.y, val.z, val.w }; }
	void JsonWrite(json& j, const char* key, const std::vector<std::string>& val) { j[key] = val; }
	void JsonWrite(json& j, const char* key, const std::vector<Entity>& val) { j[key] = val; }

	void SerializeComponentHelper(json& parent, const std::string& key, std::function<void(json&)> writer)
	{
		json& compNode = parent[key];
		if (compNode.is_null()) compNode = json::object();
		writer(compNode);
	}

	// ヘルパー関数の実装
	void AddToComponentOrder(Registry& reg, Entity e, const std::string& compName)
	{
		// Tagコンポーネントを持っていれば、リストに追加
		if (reg.has<Tag>(e))
		{
			auto& tag = reg.get<Tag>(e);
			if (std::find(tag.componentOrder.begin(), tag.componentOrder.end(), compName) == tag.componentOrder.end())
			{
				tag.componentOrder.push_back(compName);
			}
		}
	}

	void RemoveFromComponentOrder(Registry& reg, Entity e, const std::string& compName)
	{
		// Tagコンポーネントを持っていれば、リストから削除
		if (reg.has<Tag>(e))
		{
			auto& order = reg.get<Tag>(e).componentOrder;
			auto it = std::find(order.begin(), order.end(), compName);
			if (it != order.end())
			{
				order.erase(it);
			}
		}
	}

	// インスペクター状態管理の実装 (Engine側)
#ifdef _DEBUG
	// 状態保持用マップ（Engine側のヒープにある）
	static std::map<ImGuiID, json> s_inspectorStates;

	void Inspector_Snapshot(ImGuiID id, std::function<void(json&)> saveFn)
	{
		json state;
		saveFn(state); // Engineで作ったjsonを渡して書き込ませる
		s_inspectorStates[id] = state;
	}

	void Inspector_Commit(ImGuiID id, std::function<void(json&)> saveFn, std::function<void(const json&, const json&)> cmdFn)
	{
		if (s_inspectorStates.find(id) == s_inspectorStates.end()) return;

		json oldState = s_inspectorStates[id];
		json newState;
		saveFn(newState);

		if (oldState != newState)
		{
			cmdFn(oldState, newState);
		}
		s_inspectorStates.erase(id);
	}

	bool Inspector_HasState(ImGuiID id)
	{
		return s_inspectorStates.find(id) != s_inspectorStates.end();
	}
#endif // _DEBUG

	ComponentRegistry& Arche::ComponentRegistry::Instance()
	{
		static ComponentRegistry instance;
		return instance;
	}
}