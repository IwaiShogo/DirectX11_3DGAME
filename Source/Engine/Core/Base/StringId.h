/*****************************************************************//**
 * @file	StringId.h
 * @brief	文字列をコンパイル時（または実行時）に32bit/64bit整数に変換する仕組み
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 *********************************************************************/

#ifndef ___STRING_ID_H___
#define ___STRING_ID_H___

// ===== インクルード =====
#include "Engine/pch.h"

/**
 * @class	StringId
 * @brief	文字列をハッシュ化して保持するクラス（高速比較用）
 */
class StringId
{
public:
	using ValueType = uint32_t;

	// --- コンストラクタ ---

	// デフォルトコンストラクタ
	StringId() : m_hash(0)
	{
#ifdef _DEBUG
		m_debugString = "Empty";
#endif // _DEBUG
	}

	// 文字列から生成
	StringId(const char* str) : m_hash(Hash(str))
	{
#ifdef _DEBUG
		m_debugString = str;	// デバッグ時は元の文字列も保持
#endif // _DEBUG
	}

	// std::stringから生成
	StringId(const std::string& str) : StringId(str.c_str()) {}

	// 数値から直接作成（シリアライズ復元用）
	explicit StringId(ValueType hash) : m_hash(hash)
	{
#ifdef _DEBUG
		m_debugString = "HashDirect:" + std::to_string(hash);
#endif // _DEBUG
	}

	// --- 演算子オーバーロード ---
	bool operator==(const StringId& other) const { return m_hash == other.m_hash; }
	bool operator!=(const StringId& other) const { return m_hash != other.m_hash; }
	bool operator<(const StringId& other) const { return m_hash < other.m_hash; }

	// --- アクセサ ---
	ValueType GetHash() const { return m_hash; }
	const char* c_str() const
	{
#ifdef _DEBUG
		return m_debugString.c_str();
#else
		return "";	// Releaseでは文字列を持たない
#endif

	}

private:
	// CRC32ハッシュ関数
	static constexpr ValueType Hash(const char* str)
	{
		ValueType hash = 0xFFFFFFFF;
		for (; *str; ++str)
		{
			hash = (hash ^ *str) * 0xED888320;
		}
		return hash;
	}

private:
	ValueType m_hash;

#ifdef _DEBUG
	std::string m_debugString;	// デバッグ用文字列
#endif // _DEBUG
};

// std::unordered_mapのキーとして使うためのハッシュ定義
namespace std
{
	template<>
	struct hash<StringId>
	{
		std::size_t operator()(const StringId& k) const
		{
			return k.GetHash();
		}
	};
}

#endif // !___STRING_ID_H___
