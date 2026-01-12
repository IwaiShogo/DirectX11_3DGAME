#ifndef ___RESOURCE_MANAGER_H___
#define ___RESOURCE_MANAGER_H___

#include "Engine/pch.h"
#include <future>
#include <list>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

// クラスの前方宣言
namespace Arche {
	class Texture;
	class Model;
	class Sound;
}

namespace Arche
{
	// 非同期タスク構造体
	struct AsyncTask {
		enum class TaskType {
			ModelType,
			TextureType,
			SoundType
		} type;

		std::string key;

		std::shared_ptr<Arche::Model> modelData;
		std::shared_ptr<Arche::Texture> textureData;
		std::shared_ptr<Arche::Sound> soundData;

		std::future<bool> task;

		AsyncTask() = default;
		// std::futureを持つためコピー禁止
		AsyncTask(const AsyncTask&) = delete;
		AsyncTask& operator=(const AsyncTask&) = delete;
		// ムーブは許可
		AsyncTask(AsyncTask&&) = default;
		AsyncTask& operator=(AsyncTask&&) = default;
	};

	class ARCHE_API ResourceManager
	{
	public:
		static ResourceManager& Instance() { static ResourceManager instance; return instance; }

		// シングルトンなのでコピー・ムーブを明示的に禁止する
		// これがないと、std::list<unique_ptr> を持つクラスのコピーコンストラクタを
		// コンパイラが生成しようとしてエラーになります。
		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;
		ResourceManager(ResourceManager&&) = delete;
		ResourceManager& operator=(ResourceManager&&) = delete;

		// 初期化・終了
		void Initialize(ID3D11Device* device);
		void Clear();
		void Update();

		// 非同期ロード要求
		void LoadModelAsync(const std::string& path);
		void LoadTextureAsync(const std::string& path);
		void LoadSoundAsync(const std::string& path);

		// 取得
		std::shared_ptr<Texture> GetTexture(const std::string& keyName);
		std::shared_ptr<Model>	 GetModel(const std::string& keyName);
		std::shared_ptr<Sound>	 GetSound(const std::string& keyName);

		// 状況確認
		bool IsLoading() const { return !m_tasks.empty(); }
		float GetProgress() const;

		// ユーティリティ
		const std::unordered_map<std::string, std::shared_ptr<Texture>>& GetTextureMap() const { return m_textures; }
		const std::unordered_map<std::string, std::shared_ptr<Model>>& GetModelMap() const { return m_models; }
		const std::unordered_map<std::string, std::shared_ptr<Sound>>& GetSoundMap() const { return m_sounds; }

		void ReloadTexture(const std::string& keyName);
		void AddResource(const std::string& key, std::shared_ptr<Texture> resource);

	private:
		// コンストラクタはprivate
		ResourceManager() = default;
		~ResourceManager() = default;

		void CreateSystemTextures();
		std::string ResolvePath(const std::string& keyName, const std::vector<std::string>& directories, const std::vector<std::string>& extensions);

		std::shared_ptr<Texture> LoadTextureSync(const std::string& path);
		std::shared_ptr<Model>	 LoadModelSync(const std::string& path);
		std::shared_ptr<Sound>	 LoadSoundSync(const std::string& path);

	private:
		ID3D11Device* m_device = nullptr;

		std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
		std::unordered_map<std::string, std::shared_ptr<Model>>	  m_models;
		std::unordered_map<std::string, std::shared_ptr<Sound>>	  m_sounds;

		// std::unique_ptr のリスト（安全のため）
		std::list<std::unique_ptr<AsyncTask>> m_tasks;

		int m_totalTasks = 0;
		int m_completedTasks = 0;

		const std::vector<std::string> m_textureDirs = { "Resources/Game/Textures/", "Resources/Engine/Textures/" };
		const std::vector<std::string> m_modelDirs = { "Resources/Game/Models/",   "Resources/Engine/Models/" };
		const std::vector<std::string> m_soundDirs = { "Resources/Game/Sounds/",   "Resources/Engine/Sounds/" };
		const std::vector<std::string> m_imgExts = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
		const std::vector<std::string> m_modelExts = { ".fbx", ".obj", ".gltf", ".glb" };
		const std::vector<std::string> m_soundExts = { ".wav", ".mp3", ".ogg" };
	};
}
#endif