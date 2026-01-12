/*****************************************************************//**
 * @file	FontManager.cpp
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

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Renderer/Text/FontManager.h"
#include "Engine/Renderer/Text/PrivateFontLoader.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	FontManager::~FontManager()
	{
		if (m_dwriteFactory && m_collectionLoader)
		{
			m_dwriteFactory->UnregisterFontCollectionLoader(m_collectionLoader);
		}
		if (m_collectionLoader)
		{
			m_collectionLoader->Release();
			m_collectionLoader = nullptr;
		}
	}

	void FontManager::Initialize()
	{
		if (m_isInitialized) return;

		// ---------------------------------------------------------
		// 1. カレントディレクトリをexeの場所に強制移動 (Release対策)
		// ---------------------------------------------------------
		/*char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::filesystem::path exePath = buffer;
		std::filesystem::path exeDir = exePath.parent_path();
		std::filesystem::current_path(exeDir);
		Logger::Log("CWD set to: " + exeDir.string());*/

		// ---------------------------------------------------------
		// 2. Factory作成
		// ---------------------------------------------------------
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));

		// ---------------------------------------------------------
		// 3. ローダーの登録
		// ---------------------------------------------------------
		if (!m_collectionLoader)
		{
			m_collectionLoader = new PrivateFontCollectionLoader();
			m_collectionLoader->AddRef();
			m_dwriteFactory->RegisterFontCollectionLoader(m_collectionLoader);
		}

		// ---------------------------------------------------------
		// 4. フォントロード
		// ---------------------------------------------------------
		if (std::filesystem::exists("Resources/Game/Fonts"))
		{
			LoadFonts("Resources/Game/Fonts");
		}
		else
		{
			Logger::LogError("Resources/Game/Fonts not found at: " + std::filesystem::absolute("Resources/Game/Fonts").string());
		}
	}

	void FontManager::LoadFonts(const std::string& directory)
	{
		namespace fs = std::filesystem;
		std::vector<std::wstring> fontPaths;

		if (!fs::exists(directory))
		{
			Logger::LogError("CRITICAL: Fonts directory not found at: " + fs::absolute(directory).string());
			return;
		}

		// パス収集
		for (const auto& entry : fs::recursive_directory_iterator(directory))
		{
			if (entry.is_regular_file())
			{
				std::string ext = entry.path().extension().string();

				if (ext == ".ttf" || ext == ".otf" || ext == ".TTF" || ext == ".OTF")
				{
					std::error_code ec;
					fs::path absPath = fs::canonical(entry.path(), ec);
					if (!ec)
					{
						fontPaths.push_back(absPath.wstring());
					}
					else
					{
						fontPaths.push_back(fs::absolute(entry.path()).wstring());
					}
				}
			}
		}

		if (fontPaths.empty()) return;

		// コレクション作成
		m_collectionLoader->SetFontPaths(fontPaths);
		const char* key = "MyCustomFonts";
		HRESULT hr = m_dwriteFactory->CreateCustomFontCollection(
			m_collectionLoader, key, (UINT32)strlen(key), &m_customCollection
		);

		m_loadedFontNames.clear();
		m_fontPathMap.clear();

		if (SUCCEEDED(hr) && m_customCollection)
		{
			// ロード成功したフォント名をリスト化
			UINT32 count = m_customCollection->GetFontFamilyCount();
			for (UINT32 i = 0; i < count; ++i)
			{
				ComPtr<IDWriteFontFamily> family;
				m_customCollection->GetFontFamily(i, &family);
				ComPtr<IDWriteLocalizedStrings> names;
				family->GetFamilyNames(&names);

				UINT32 idx = 0;
				BOOL exists = false;
				names->FindLocaleName(L"en-us", &idx, &exists);
				if (!exists) idx = 0;

				UINT32 len = 0; names->GetStringLength(idx, &len);
				std::wstring wname(len + 1, 0);
				names->GetString(idx, &wname[0], len + 1);
				wname.resize(len);

				int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wname[0], (int)wname.size(), NULL, 0, NULL, NULL);
				std::string name(size_needed, 0);
				WideCharToMultiByte(CP_UTF8, 0, &wname[0], (int)wname.size(), &name[0], size_needed, NULL, NULL);

				m_loadedFontNames.push_back(name);
				Logger::Log(">>> Loaded Custom Font: " + name);

				// このフォントのファイルパスを取得してマップに保存
				ComPtr<IDWriteFont> font;
				family->GetFont(0, &font);
				if (font)
				{
					ComPtr<IDWriteFontFace> fontFace;
					font->CreateFontFace(&fontFace);
					if (fontFace)
					{
						UINT32 fileCount = 1;
						ComPtr<IDWriteFontFile> fontFile;
						fontFace->GetFiles(&fileCount, &fontFile);

						if (fontFile)
						{
							// 1. このファイルをロードしたローダーを取得
							ComPtr<IDWriteFontFileLoader> loader;
							fontFile->GetLoader(&loader);

							// 2. ローカルファイルローダー（ディスク上のファイルを扱うもの）か確認
							ComPtr<IDWriteLocalFontFileLoader> localLoader;
							loader.As(&localLoader);

							if (localLoader)
							{
								const void* refKey;
								UINT32 refKeySize;
								fontFile->GetReferenceKey(&refKey, &refKeySize);

								// 3. キーからパスの長さを取得
								UINT32 pathLength = 0;
								if (SUCCEEDED(localLoader->GetFilePathLengthFromKey(refKey, refKeySize, &pathLength)) && pathLength > 0)
								{
									// 4. パスを取得（+1 は終端ヌル文字分）
									std::wstring wPath(pathLength + 1, L'\0');
									if (SUCCEEDED(localLoader->GetFilePathFromKey(refKey, refKeySize, &wPath[0], pathLength + 1)))
									{
										// 末尾のヌル文字を除去してサイズを合わせる
										wPath.resize(pathLength);

										// UTF-8へ変換
										int p_len = WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), -1, NULL, 0, NULL, NULL);
										if (p_len > 0)
										{
											std::string u8Path(p_len, 0);
											WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), -1, &u8Path[0], p_len, NULL, NULL);

											if (!u8Path.empty() && u8Path.back() == '\0') u8Path.pop_back();

											// マップに保存
											m_fontPathMap[name] = u8Path;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			Logger::LogError("Failed to create custom font collection!");
		}

		m_isInitialized = true;
	}

	std::string FontManager::GetFontPath(const std::string& fontName)
	{
		if (m_fontPathMap.find(fontName) != m_fontPathMap.end())
		{
			return m_fontPathMap[fontName];
		}
		return "";
	}

	ComPtr<IDWriteTextFormat> FontManager::GetTextFormat(StringId key, const std::wstring& fontFamily, float fontSize, DWRITE_FONT_WEIGHT fontWeight, DWRITE_FONT_STYLE fontStyle)
	{
		// 単に fontName や key だけでなく、サイズ・太さ・スタイルを含めた文字列をキーにする
		std::string cacheKey = std::string(key.c_str()) + "_"
			+ std::to_string(fontSize) + "_"
			+ std::to_string((int)fontWeight) + "_"
			+ std::to_string((int)fontStyle);

		if (m_textFormats.find(cacheKey) != m_textFormats.end())\
		{
			return m_textFormats[cacheKey];
		}

		ComPtr<IDWriteTextFormat> format;
		HRESULT hr = E_FAIL;

		// A. カスタムコレクションから探す
		if (m_customCollection)
		{
			UINT32 index;
			BOOL exists;
			m_customCollection->FindFamilyName(fontFamily.c_str(), &index, &exists);
			if (exists)
			{
				hr = m_dwriteFactory->CreateTextFormat(
					fontFamily.c_str(), m_customCollection.Get(),
					fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ja_jp", &format
				);
			}
		}

		// B. 無ければシステムフォントから探す（nullptr = System Collection）
		if (FAILED(hr))
		{
			// PCにインストールされているフォントを試す
			hr = m_dwriteFactory->CreateTextFormat(
				fontFamily.c_str(), nullptr,
				fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ja-jp", &format
			);
			if (SUCCEEDED(hr))
			{
				Logger::Log("Fallback to System Font: " + std::string(fontFamily.begin(), fontFamily.end()));
			}
		}

		// C. それでもダメなら「メイリオ」に強制フォールバック
		if (FAILED(hr))
		{
			Logger::LogWarning("Font Not Found. Fallback to Meiryo.");
			hr = m_dwriteFactory->CreateTextFormat(
				L"Meiryo", nullptr,
				DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"ja-jp", &format
			);
		}

		if (SUCCEEDED(hr))
		{
			m_textFormats[key] = format;
			return format;
		}

		return nullptr;
	}

}	// namespace Arche