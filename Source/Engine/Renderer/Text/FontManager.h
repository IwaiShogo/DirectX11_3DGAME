/*****************************************************************//**
 * @file	FontManager.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___FONT_MANAGER_H___
#define ___FONT_MANAGER_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Base/StringId.h"

namespace Arche
{
	// ===== 前方宣言 =====
	class PrivateFontCollectionLoader;

	class ARCHE_API FontManager
	{
	public:
		static FontManager& Instance() { static FontManager i; return i; }

		void Initialize();

		ID2D1Factory* GetD2DFactory() const { return m_d2dFactory.Get(); }

		// テキストフォーマット取得
		ComPtr<IDWriteTextFormat> GetTextFormat(
			StringId key, const std::wstring& fontFamily, float fontSize,
			DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL);

		// ロードされたフォント名のリストを取得
		const std::vector<std::string>& GetLoadedFontNames() const { return m_loadedFontNames; }

		// フォント名からファイルパスを取得する
		std::string GetFontPath(const std::string& fontName);

	private:
		FontManager() = default;
		~FontManager();

		// フォントディレクトリ内の全フォントをメモリにロード
		void LoadFonts(const std::string& directory);

		bool m_isInitialized = false;

		ComPtr<ID2D1Factory> m_d2dFactory;
		ComPtr<IDWriteFactory> m_dwriteFactory;

		// カスタムフォント用
		PrivateFontCollectionLoader* m_collectionLoader = nullptr;
		ComPtr<IDWriteFontCollection> m_customCollection;

		// キャッシュ: キー(StringId) -> TextFormat
		// 同じ設定のフォントを何度も作らないようにする
		std::unordered_map<StringId, ComPtr<IDWriteTextFormat>> m_textFormats;
		// GUI表示用
		std::vector<std::string> m_loadedFontNames;
		// フォント名 -> ファイルパス
		std::unordered_map<std::string, std::string> m_fontPathMap;
	};

}	// namespace Arche

#endif // !___FONT_MANAGER_H___