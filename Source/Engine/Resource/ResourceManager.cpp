/*****************************************************************//**
 * @file	ResourceManager.cpp
 * @brief	リソースマネージャー
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/11/25	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Core/Base/Logger.h"
#include "Engine/Renderer/RHI/Texture.h"
#include "Engine/Renderer/Data/Model.h"
#include "Engine/Audio/Sound.h"

namespace Arche
{
	void ResourceManager::Initialize(ID3D11Device* device)
	{
		m_device = device;
		CreateSystemTextures();
	}

	void ResourceManager::Clear()
	{
		m_textures.clear();
		m_models.clear();
		m_sounds.clear();
		m_tasks.clear(); // タスクもクリア
	}

	// --------------------------------------------------------
	// 非同期ロードリクエスト
	// --------------------------------------------------------
	void ResourceManager::LoadModelAsync(const std::string& path)
	{
		std::string key = ResolvePath(path, m_modelDirs, m_modelExts);
		if (key.empty() || m_models.count(key)) return; // 既存

		// 重複チェック (ポインタ経由でアクセス)
		for (const auto& t : m_tasks) if (t->key == key && t->type == AsyncTask::TaskType::ModelType) return;

		// ポインタとして作成
		auto t = std::make_unique<AsyncTask>();
		t->type = AsyncTask::TaskType::ModelType;
		t->key = key;
		t->modelData = std::make_shared<Model>();

		// 別スレッドでロード開始
		std::shared_ptr<Model> ptr = t->modelData;
		t->task = std::async(std::launch::async, [ptr, key]() -> bool {
			return ptr->LoadCPU(key);
			});

		// ポインタをリストに移動
		m_tasks.push_back(std::move(t));
		m_totalTasks++;
	}

	void ResourceManager::LoadTextureAsync(const std::string& path)
	{
		std::string key = ResolvePath(path, m_textureDirs, m_imgExts);
		if (key.empty() || m_textures.count(key)) return;

		for (const auto& t : m_tasks) if (t->key == key && t->type == AsyncTask::TaskType::TextureType) return;

		auto t = std::make_unique<AsyncTask>();
		t->type = AsyncTask::TaskType::TextureType;
		t->key = key;
		t->textureData = std::make_shared<Texture>();

		std::shared_ptr<Texture> ptr = t->textureData;
		t->task = std::async(std::launch::async, [ptr, key]() -> bool {
			return ptr->LoadCPU(key);
			});

		m_tasks.push_back(std::move(t));
		m_totalTasks++;
	}

	void ResourceManager::LoadSoundAsync(const std::string& path)
	{
		std::string key = ResolvePath(path, m_soundDirs, m_soundExts);
		if (key.empty() || m_sounds.count(key)) return;

		for (const auto& t : m_tasks) if (t->key == key && t->type == AsyncTask::TaskType::SoundType) return;

		auto t = std::make_unique<AsyncTask>();
		t->type = AsyncTask::TaskType::SoundType;
		t->key = key;
		t->soundData = std::make_shared<Sound>();

		std::shared_ptr<Sound> ptr = t->soundData;
		t->task = std::async(std::launch::async, [ptr, key]() -> bool {
			return ptr->LoadCPU(key);
			});

		m_tasks.push_back(std::move(t));
		m_totalTasks++;
	}

	// --------------------------------------------------------
	// 更新処理（メインスレッド）
	// --------------------------------------------------------
	void ResourceManager::Update()
	{
		if (m_tasks.empty()) return;

		// 先頭のタスクを取得 (ポインタへの参照)
		auto& taskPtr = m_tasks.front();

		// タスク完了確認 (ブロックしない)
		if (taskPtr->task.valid() && taskPtr->task.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			bool success = taskPtr->task.get(); // 結果取得

			if (success)
			{
				// GPUリソース生成 / 初期化
				if (taskPtr->type == AsyncTask::TaskType::TextureType)
				{
					if (taskPtr->textureData->UploadGPU(m_device)) {
						m_textures[taskPtr->key] = taskPtr->textureData;
						Logger::Log("Async Loaded Texture: " + taskPtr->key);
					}
				}
				else if (taskPtr->type == AsyncTask::TaskType::ModelType)
				{
					taskPtr->modelData->UploadGPU();
					m_models[taskPtr->key] = taskPtr->modelData;
					Logger::Log("Async Loaded Model: " + taskPtr->key);
				}
				else if (taskPtr->type == AsyncTask::TaskType::SoundType)
				{
					taskPtr->soundData->Initialize();
					m_sounds[taskPtr->key] = taskPtr->soundData;
					Logger::Log("Async Loaded Sound: " + taskPtr->key);
				}
			}
			else
			{
				Logger::LogError("Async Load Failed: " + taskPtr->key);
			}

			// リストから削除（unique_ptrなのでここでデストラクタが呼ばれ、futureも破棄される）
			m_tasks.pop_front();
			m_completedTasks++;

			if (m_tasks.empty()) {
				m_totalTasks = 0;
				m_completedTasks = 0;
			}
		}
	}

	float ResourceManager::GetProgress() const
	{
		if (m_totalTasks == 0) return 1.0f;
		return (float)m_completedTasks / (float)m_totalTasks;
	}

	// --------------------------------------------------------
	// 取得（同期ロードフォールバック）
	// --------------------------------------------------------
	std::shared_ptr<Texture> ResourceManager::GetTexture(const std::string& keyName)
	{
		if (m_textures.count(keyName)) return m_textures[keyName];

		std::string path = ResolvePath(keyName, m_textureDirs, m_imgExts);
		if (path.empty()) {
			if (std::filesystem::exists(keyName)) path = keyName;
			else return m_textures["White"];
		}
		// 同期ロード
		auto tex = LoadTextureSync(path);
		if (tex) m_textures[keyName] = tex;
		else m_textures[keyName] = m_textures["White"];
		return m_textures[keyName];
	}

	std::shared_ptr<Model> ResourceManager::GetModel(const std::string& keyName)
	{
		if (m_models.count(keyName)) return m_models[keyName];

		std::string path = ResolvePath(keyName, m_modelDirs, m_modelExts);
		if (path.empty()) path = keyName;

		auto model = LoadModelSync(path);
		if (model) m_models[keyName] = model;
		return model;
	}

	std::shared_ptr<Sound> ResourceManager::GetSound(const std::string& keyName)
	{
		if (m_sounds.count(keyName)) return m_sounds[keyName];

		std::string path = ResolvePath(keyName, m_soundDirs, m_soundExts);
		if (path.empty()) return nullptr;

		auto sound = LoadSoundSync(path);
		if (sound) m_sounds[keyName] = sound;
		return sound;
	}

	// --------------------------------------------------------
	// 同期ロード実装 (CPUロード -> GPUアップロードを連続で行う)
	// --------------------------------------------------------
	std::shared_ptr<Texture> ResourceManager::LoadTextureSync(const std::string& path)
	{
		auto tex = std::make_shared<Texture>();
		if (!tex->LoadCPU(path)) return nullptr;
		if (!tex->UploadGPU(m_device)) return nullptr;
		return tex;
	}

	std::shared_ptr<Model> ResourceManager::LoadModelSync(const std::string& path)
	{
		auto model = std::make_shared<Model>();
		if (!model->LoadSync(path)) return nullptr;
		return model;
	}

	std::shared_ptr<Sound> ResourceManager::LoadSoundSync(const std::string& path)
	{
		auto sound = std::make_shared<Sound>();
		if (!sound->LoadCPU(path)) return nullptr;
		sound->Initialize();
		return sound;
	}

	// --------------------------------------------------------
	// 互換性維持のための関数
	// --------------------------------------------------------
	void ResourceManager::ReloadTexture(const std::string& keyName)
	{
		if (m_textures.find(keyName) != m_textures.end())
		{
			std::string path = m_textures[keyName]->filepath;
			if (path.find("System::") == std::string::npos && std::filesystem::exists(path))
			{
				auto newTex = LoadTextureSync(path);
				if (newTex) m_textures[keyName] = newTex;
				Logger::Log("Reloaded Texture: " + keyName);
			}
		}
	}

	void ResourceManager::AddResource(const std::string& key, std::shared_ptr<Texture> resource)
	{
		m_textures[key] = resource;
	}

	// --------------------------------------------------------
	// 内部処理
	// --------------------------------------------------------
	void ResourceManager::CreateSystemTextures() {
		uint32_t pixel = 0xFFFFFFFF;
		D3D11_SUBRESOURCE_DATA initData = { &pixel, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = 1; desc.Height = 1; desc.MipLevels = 1; desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		ComPtr<ID3D11Texture2D> tex2D;
		m_device->CreateTexture2D(&desc, &initData, tex2D.GetAddressOf());
		ComPtr<ID3D11ShaderResourceView> srv;
		m_device->CreateShaderResourceView(tex2D.Get(), nullptr, srv.GetAddressOf());
		auto tex = std::make_shared<Texture>();
		tex->filepath = "System::White"; tex->width = 1; tex->height = 1; tex->srv = srv;
		m_textures["White"] = tex;
	}

	std::string ResourceManager::ResolvePath(const std::string& keyName, const std::vector<std::string>& directories, const std::vector<std::string>& extensions) {
		if (std::filesystem::exists(keyName)) return keyName;
		for (const auto& dir : directories) {
			std::string tryPath = dir + keyName;
			if (std::filesystem::exists(tryPath)) return tryPath;
			for (const auto& ext : extensions) {
				std::string tryPathExt = tryPath + ext;
				if (std::filesystem::exists(tryPathExt)) return tryPathExt;
			}
		}
		return "";
	}
}