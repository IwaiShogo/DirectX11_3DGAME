/*****************************************************************//**
 * @file	SystemRegistry.h
 * @brief	システムの動的生成・登録
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___SYSTEM_REGISTRY_H___
#define ___SYSTEM_REGISTRY_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"

namespace Arche
{
	class ARCHE_API SystemRegistry
	{
	public:
		// システム生成関数の型定義
		using SystemCreator = std::function<ISystem* (World&, SystemGroup)>;

		// シングルトン取得
		static SystemRegistry& Instance();

		static SystemRegistry*& GetInstancePtr();

		static void Destroy();

		/**
		 * @brief	システムを型として登録する
		 * @tparam	T		システムのクラス（ISystem継承）
		 * @param	name	システム名（JSON保存用キー、例: "PhysicsSystem"）
		 */
		template<typename T>
		void Register(const std::string& name)
		{
			m_creators[name] = [](World& world, SystemGroup group) -> ISystem*
			{
				if(group == SystemGroup::Unspecified)
				{
					return world.registerSystem<T>();
				}
				else
				{
					return world.registerSystem<T>(group);
				}
			};
		}

		/**
		 * @brief	名前からシステムを生成してWorldに登録する
		 */
		ISystem* CreateSystem(World& world, const std::string& name, SystemGroup group = SystemGroup::Unspecified)
		{
			if (m_creators.find(name) != m_creators.end())
			{
				return m_creators[name](world, group);
			}
			Logger::LogWarning("Unknown System: " + name);
			return nullptr;
		}

		void Clear()
		{
			m_creators.clear();
		}

		// 登録されている全システム名を取得（エディタの追加用メニュー用）
		const std::unordered_map<std::string, SystemCreator>& GetCreators() const { return m_creators; }

	private:
		std::unordered_map<std::string, SystemCreator> m_creators;
	};

	// マクロ用のヘルパー
	#define ARCHE_CONCAT_IMPL(x, y) x##y
	#define ARCHE_CONCAT(x, y) ARCHE_CONCAT_IMPL(x, y)

	// 自動登録用マクロ
	// 使い方: ヘッダーまたはcppの最後で ARCHE_REGISTER_SYSTEM(MySystem) と書く
	#define ARCHE_REGISTER_SYSTEM(Type, Name) \
		namespace { \
			static const bool ARCHE_CONCAT(SystemRegistered_, __COUNTER__) = [](){ \
				Arche::SystemRegistry::Instance().Register<Type>(Name); \
				return true; \
			}(); \
		}

}	// namespace Arche

#endif // !___SYSTEM_REGISTRY_H___