/*****************************************************************//**
 * @file	Reflection.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/19	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___REFLECTION_H___
#define ___REFLECTION_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{

	// リフレクションシステム内部用
	namespace Reflection
	{
		// 基本テンプレート
		template<typename T>
		struct Meta
		{
			static constexpr const char* Name = "Unknown";
			template<typename Visitor>
			static void Visit(T& instance, Visitor&& visitor) {}
		};

		// メンバ訪問用関数
		template <typename T, typename Visitor>
		void VisitMembers(T& instance, Visitor&& visitor)
		{
			Meta<T>::Visit(instance, std::forward<Visitor>(visitor));
		}

	}	// namespace Reflection

}	// namespace Arche

// ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
// ユーザー利用用マクロ
// ============================================================

/**
 * @brief	構造体の外部でリフレクション情報を定義するためのマクロ
 * @code
 * 使い方：
 *	REFLECT_STRUCT_BEGIN(Transform)
 *		REFLECT_VAR(position)
 *		REFLECT_VAR(rotation)
 *	REFLECT_STRUCT_END()
 */
#define REFLECT_STRUCT_BEGIN(Type) \
	namespace Reflection { \
		template <> struct Meta<Type> { \
			static constexpr const char* Name = #Type; \
			template <typename Visitor> \
			static void Visit(Type& obj, Visitor&& visitor) { \

 // 変数登録: REFLECT_VAR(position)
#define REFLECT_VAR(Name) \
				visitor(obj.Name, #Name); \

// 定義終了
#define REFLECT_STRUCT_END() \
			} \
		}; \
	}

// ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

#endif // !___REFLECTION_H___