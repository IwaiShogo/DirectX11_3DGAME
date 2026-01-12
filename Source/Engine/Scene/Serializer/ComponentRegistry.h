/*****************************************************************//**
 * @file	ComponentRegistry.h
 * @brief	コンポーネントの自動登録・管理・シリアライズ機能
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___COMPONENT_REGISTRY_H___
#define ___COMPONENT_REGISTRY_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Base/Reflection.h"
#include "Engine/Core/Base/StringId.h"

// エディタ機能（InspectorGui）を利用可能にする
#ifdef _DEBUG
#include "Editor/Core/InspectorGui.h"
#endif // _DEBUG

namespace Arche
{
	// ------------------------------------------------------------
	// ヘルパー関数の宣言（型を網羅する）
	// ------------------------------------------------------------
	ARCHE_API void JsonWrite(json& j, const char* key, int val);
	ARCHE_API void JsonWrite(json& j, const char* key, long val);
	ARCHE_API void JsonWrite(json& j, const char* key, long long val);
	ARCHE_API void JsonWrite(json& j, const char* key, unsigned int val);	   // Entity等はここ
	ARCHE_API void JsonWrite(json& j, const char* key, unsigned long val);
	ARCHE_API void JsonWrite(json& j, const char* key, unsigned long long val); // size_tはここ
	ARCHE_API void JsonWrite(json& j, const char* key, float val);
	ARCHE_API void JsonWrite(json& j, const char* key, double val);
	ARCHE_API void JsonWrite(json& j, const char* key, bool val);
	ARCHE_API void JsonWrite(json& j, const char* key, char val);
	ARCHE_API void JsonWrite(json& j, const char* key, unsigned char val);
	ARCHE_API void JsonWrite(json& j, const char* key, const std::string& val);
	ARCHE_API void JsonWrite(json& j, const char* key, const char* val);
	ARCHE_API void JsonWrite(json& j, const char* key, const StringId& val);

	// Vector系
	ARCHE_API void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT2& val);
	ARCHE_API void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT3& val);
	ARCHE_API void JsonWrite(json& j, const char* key, const DirectX::XMFLOAT4& val);
	ARCHE_API void JsonWrite(json& j, const char* key, const std::vector<std::string>& val);
	ARCHE_API void JsonWrite(json& j, const char* key, const std::vector<Entity>& val);

	ARCHE_API void SerializeComponentHelper(json& parent, const std::string& key, std::function<void(json&)> writer);

	// シリアライズ用ビジター（保存）
	// ------------------------------------------------------------
	struct SerializeVisitor
	{
		json& j;

		// 基本型のマッピング
		void operator()(int val, const char* name) { JsonWrite(j, name, val); }
		void operator()(long val, const char* name) { JsonWrite(j, name, val); }
		void operator()(long long val, const char* name) { JsonWrite(j, name, val); }
		void operator()(unsigned int val, const char* name) { JsonWrite(j, name, val); }
		void operator()(unsigned long val, const char* name) { JsonWrite(j, name, val); }
		void operator()(unsigned long long val, const char* name) { JsonWrite(j, name, val); }
		void operator()(float val, const char* name) { JsonWrite(j, name, val); }
		void operator()(double val, const char* name) { JsonWrite(j, name, val); }
		void operator()(bool val, const char* name) { JsonWrite(j, name, val); }
		void operator()(char val, const char* name) { JsonWrite(j, name, val); }
		void operator()(unsigned char val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const std::string& val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const char* val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const StringId& val, const char* name) { JsonWrite(j, name, val); }

		// Vector系
		void operator()(const XMFLOAT2& val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const XMFLOAT3& val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const XMFLOAT4& val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const std::vector<std::string>& val, const char* name) { JsonWrite(j, name, val); }
		void operator()(const std::vector<Entity>& val, const char* name) { JsonWrite(j, name, val); }

		template<typename T> requires std::is_enum_v<T>
		void operator()(const T& val, const char* name) { JsonWrite(j, name, static_cast<int>(val)); }
	};

	// デシリアライズ用ビジター
	// ------------------------------------------------------------
	struct DeserializeVisitor
	{
		const json& j;
		template<typename T>
		void operator()(T& val, const char* name) { if (j.contains(name)) val = j[name].get<T>(); }
		void operator()(DirectX::XMFLOAT2& v, const char* name) { if (j.contains(name)) { auto a = j[name]; if (a.size() >= 2) { v.x = a[0]; v.y = a[1]; } } }
		void operator()(DirectX::XMFLOAT3& v, const char* name) { if (j.contains(name)) { auto a = j[name]; if (a.size() >= 3) { v.x = a[0]; v.y = a[1]; v.z = a[2]; } } }
		void operator()(DirectX::XMFLOAT4& v, const char* name) { if (j.contains(name)) { auto a = j[name]; if (a.size() >= 4) { v.x = a[0]; v.y = a[1]; v.z = a[2]; v.w = a[3]; } } }
		void operator()(StringId& v, const char* name)
		{
			if (j.contains(name))
			{
				const auto& val = j[name];
				if (val.is_number())
				{
					// 数値（ハッシュ）なら、直接ハッシュ値から復元
					v = StringId(val.get<uint32_t>());
				}
				else if (val.is_string())
				{
					// 文字列なら、文字列からハッシュ化して復元（旧データや手書きJSON用）
					v = StringId(val.get<std::string>().c_str());
				}
			}
		}
		void operator()(Entity& v, const char* name) { if (j.contains(name)) v = (Entity)j[name].get<uint32_t>(); }
		template<typename T>requires std::is_enum_v<T>
		void operator()(T& v, const char* name) { if (j.contains(name)) v = (T)j[name].get<int>(); }
		void operator()(std::vector<Entity>& v, const char* name)
		{
			if (j.contains(name)) { v.clear(); for (auto& id : j[name]) v.push_back((Entity)id.get<uint32_t>()); }
		}
	};

	ARCHE_API void AddToComponentOrder(Registry& reg, Entity e, const std::string& compName);
	ARCHE_API void RemoveFromComponentOrder(Registry& reg, Entity e, const std::string& compName);

	// コンポーネントレジストリ
	// ------------------------------------------------------------
	class ARCHE_API ComponentRegistry
	{
	public:
		// コマンド発行用コールバック
		using CommandCallback = std::function<void(const json&, const json&)>;

		struct Interface
		{
			std::string name;
			std::function<void(Registry&, Entity, json&)> serialize;
			std::function<void(Registry&, Entity, const json&)> deserialize;
			std::function<void(Registry&, Entity)> add;
			std::function<void(Registry&, Entity)> remove;
			std::function<bool(Registry&, Entity)> has;
			std::function<void(Registry&, Entity, CommandCallback)> drawInspector;
			std::function<void(Registry&, Entity, int, std::function<void(int, int)>, std::function<void()>, CommandCallback)> drawInspectorDnD;
		};

		static ComponentRegistry& Instance();

		// コンポーネント登録関数
		template<typename T>
		void Register(const char* name)
		{
			std::string nameStr = name;

			// 既に登録済みならスキップ
			if (m_interfaces.find(nameStr) != m_interfaces.end()) return;

			Interface iface;
			iface.name = nameStr;

			// Serialize
			iface.serialize = [nameStr](Registry& reg, Entity e, json& j)
				{
					if (reg.has<T>(e))
					{
						SerializeComponentHelper(j, nameStr, [&](json& compNode)
							{
								compNode["Enabled"] = reg.isComponentEnabled<T>(e);
								Reflection::VisitMembers(reg.get<T>(e), SerializeVisitor{ compNode });
							});
					}
				};

			// Deserialize
			iface.deserialize = [nameStr](Registry& reg, Entity e, const json& j)
				{
					if (j.contains(nameStr))
					{
						T& comp = reg.has<T>(e) ? reg.get<T>(e) : reg.emplace<T>(e);
						const auto& compNode = j[nameStr];

						if (compNode.contains("Enabled"))
						{
							reg.setComponentEnabled<T>(e, compNode["Enabled"].get<bool>());
						}

						Reflection::VisitMembers(comp, DeserializeVisitor{ compNode });
					}
				};

			iface.add = [nameStr](Registry& reg, Entity e)
				{
					// コンポーネント追加
					if (!reg.has<T>(e))
					{
						reg.emplace<T>(e);

						AddToComponentOrder(reg, e, nameStr);
					}
				};

			iface.remove = [nameStr](Registry& reg, Entity e)
				{
					if (reg.has<T>(e))
					{
						reg.remove<T>(e);

						RemoveFromComponentOrder(reg, e, nameStr);
					}
				};

			// Has (変更なし)
			iface.has = [](Registry& reg, Entity e)
				{
					return reg.has<T>(e);
				};

			// DrawInspector (変更なし)
			iface.drawInspectorDnD = [nameStr, iface](Registry& reg, Entity e, int index, std::function<void(int, int)> onReorder, std::function<void()> onRemove, CommandCallback onCommand)
				{
#ifdef _DEBUG
					if (reg.has<T>(e))
					{
						bool removed = false;
						auto serializeFunc = [&](json& outJson) { iface.serialize(reg, e, outJson); };

						// DrawComponent関数内でImGuiの描画を行う
						DrawComponent(nameStr.c_str(), reg.get<T>(e), e, reg, removed, serializeFunc, onCommand, index, onReorder);

						if (removed && onRemove)
						{
							onRemove();
						}
					}
#endif // _DEBUG
				};

			m_interfaces[nameStr] = iface;
		}

		void Clear()
		{
			m_interfaces.clear();
		}

		const std::map<std::string, Interface>& GetInterfaces() const { return m_interfaces; }

	private:
		std::map<std::string, Interface> m_interfaces;
	};

}	// namespace Arche

#endif // !___COMPONENT_REGISTRY_H___