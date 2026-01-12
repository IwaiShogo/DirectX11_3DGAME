/*****************************************************************//**
 * @file	ComponentDefines.h
 * @brief	コンポーネントで使用する列挙型や、定数の定義
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___COMPONENT_DEFINES_H___
#define ___COMPONENT_DEFINES_H___

// ===== インクルード =====
#include "Engine/pch.h"

namespace Arche
{
	// 物理関連の列挙型
	// ------------------------------------------------------------
	/**
	 * @enum	BodyType
	 * @brief	物理挙動の種類
	 */
	enum class BodyType
	{
		Static,		// 動かない（質量無限大）。壁、地面など。
		Dynamic,	// 物理演算で動く。重力や衝突の影響を受ける。プレイヤー、敵、箱など。
		Kinematic,	// 物理演算を無視し、プログラムで動かす。移動床、エレベーターなど。
	};

	/**
	 * @enum	ColliderType
	 * @brief	当たり判定の形状
	 */
	enum class ColliderType
	{
		Box,		// ボックス
		Sphere,		// 球体
		Capsule,	// カプセル
		Cylinder,	// 円柱
	};

	// レイヤーシステム
	// ------------------------------------------------------------
	/**
	 * @enum	Layer
	 * @brief	レイヤー定義（ビットマスク）
	 */
	enum class Layer : uint32_t
	{
		None = 0,
		Default = 1 << 0,
		Player = 1 << 1,
		Enemy = 1 << 2,
		Wall = 1 << 3,
		Item = 1 << 4,
		Projectile = 1 << 5,
		// 必要に応じて追加（最大32レイヤー）

		All = 0xFFFFFFFF
	};

	// 演算子オーバーロード（Layer同士のビット演算用）
	constexpr Layer operator|(Layer a, Layer b) { return static_cast<Layer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
	constexpr Layer operator&(Layer a, Layer b) { return static_cast<Layer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }
	constexpr Layer operator~(Layer a) { return static_cast<Layer>(~static_cast<uint32_t>(a)); }
	constexpr Layer& operator|=(Layer& a, Layer b) { a = a | b; return a; }
	constexpr Layer& operator&=(Layer& a, Layer b) { a = a & b; return a; }
	// bool判定用
	constexpr bool operator&&(Layer a, Layer b) { return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0; }
	constexpr bool operator||(Layer a, Layer b) { return (static_cast<uint32_t>(a) | static_cast<uint32_t>(b)) != 0; }
	constexpr bool operator!(Layer a) { return static_cast<uint32_t>(a) == 0; }
}

#endif // !___COMPONENT_DEFINES_H___