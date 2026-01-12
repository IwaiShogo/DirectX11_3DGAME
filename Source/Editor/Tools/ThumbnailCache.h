/*****************************************************************//**
 * @file	ThumbnailCache.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___THUMBNAIL_CACHE_H___
#define ___THUMBNAIL_CACHE_H___

#include "Engine/pch.h"
#include "Engine/Resource/ResourceManager.h"
#include "Editor/Tools/ThumbnailGenerator.h"

namespace Arche
{
	class ThumbnailCache
	{
	public:
		ThumbnailCache() = default;
		~ThumbnailCache() = default;

		// パスに応じたサムネイル（SRV）を返す
		ID3D11ShaderResourceView* GetIcon(const std::filesystem::path& path)
		{
			std::string pathStr = path.string();
			std::shared_ptr<Texture> texture = nullptr;

			// 1. ディレクトリの場合
			if (std::filesystem::is_directory(path))
			{
				texture = ResourceManager::Instance().GetTexture("Resources/Editor/Icons/folder.png");
			}
			// 2. ファイルの場合
			else
			{
				std::string ext = path.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				// A. 画像ファイルなら中身を表示
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds")
				{
					texture = ResourceManager::Instance().GetTexture(pathStr);
				}
				// B. モデルまたはプレファブなら、サムネイル生成を試みる
				else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".json")
				{
					// Sceneファイルは除外
					bool isScene = (ext == ".json" && pathStr.find("Scene") != std::string::npos);

					if (!isScene)
					{
						// 生成処理
						std::string thumbPath = ThumbnailGenerator::Generate(path);

						if (!thumbPath.empty())
						{
							texture = ResourceManager::Instance().GetTexture(thumbPath);
						}

						// 生成失敗
						if (!texture && m_iconMap.count(ext))
						{
							texture = ResourceManager::Instance().GetTexture(m_iconMap.at(ext));
						}
					}
				}
				// C. その他登録済みアイコン
				else if (m_iconMap.count(ext))
				{
					texture = ResourceManager::Instance().GetTexture(m_iconMap.at(ext));
				}
				// D. 汎用アイコン
				else
				{
					texture = ResourceManager::Instance().GetTexture("Resources/Editor/Icons/file.png");
				}
			}

			// 失敗時
			if (!texture)
			{
				texture = ResourceManager::Instance().GetTexture("White");
			}

			return texture ? texture->srv.Get() : nullptr;
		}

	private:
		// 拡張子とアイコンパスの対応マップ
		const std::unordered_map<std::string, std::string> m_iconMap = {
			// コード
			{ ".h",	   "Resources/Editor/Icons/header.png" },
			{ ".hpp",  "Resources/Editor/Icons/header.png" },
			{ ".cpp",  "Resources/Editor/Icons/cpp.png" },
			{ ".cxx",  "Resources/Editor/Icons/cpp.png" },
			// データ
			{ ".json", "Resources/Editor/Icons/json.png" },
			{ ".txt",  "Resources/Editor/Icons/txt.png" },
			// 3Dモデル
			{ ".fbx",  "Resources/Editor/Icons/model.png" },
			{ ".obj",  "Resources/Editor/Icons/model.png" },
			{ ".gltf", "Resources/Editor/Icons/model.png" },
			{ ".glb",  "Resources/Editor/Icons/model.png" },
			// フォント
			{ ".ttf",  "Resources/Editor/Icons/font.png" },
			{ ".otf",  "Resources/Editor/Icons/font.png" },
			// サウンド
			{ ".wav",  "Resources/Editor/Icons/audio.png" },
			{ ".mp3",  "Resources/Editor/Icons/audio.png" },
			// シェーダー
			{ ".hlsl", "Resources/Editor/Icons/shader.png" },
		};
	};

}	// namespace Arche

#endif // !___THUMBNAIL_CACHE_H___