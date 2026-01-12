/*****************************************************************//**
 * @file	Texture.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2026/01/10	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2026/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#include "Engine/pch.h"
#include "Engine/Renderer/RHI/Texture.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	bool Texture::LoadCPU(const std::string& path)
	{
		this->filepath = path;
		std::wstring wpath = std::filesystem::path(path).wstring();
		std::string ext = std::filesystem::path(path).extension().string();

		// 小文字変換などの処理を入れるとより安全ですが、ここでは簡易実装
		HRESULT hr;

		if (ext == ".dds")
			hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, scratchImage);
		else if (ext == ".tga")
			hr = DirectX::LoadFromTGAFile(wpath.c_str(), nullptr, scratchImage);
		else
			hr = DirectX::LoadFromWICFile(wpath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, scratchImage);

		if (FAILED(hr))
		{
			Logger::LogError("Failed to load texture file: " + path + " (HRESULT: " + std::to_string(hr) + ")");
			return false;
		}

		this->width = (int)scratchImage.GetMetadata().width;
		this->height = (int)scratchImage.GetMetadata().height;

		return true;
	}

	bool Texture::UploadGPU(ID3D11Device* device)
	{
		if (scratchImage.GetImageCount() == 0) return false;

		HRESULT hr = DirectX::CreateShaderResourceView(
			device,
			scratchImage.GetImages(),
			scratchImage.GetImageCount(),
			scratchImage.GetMetadata(),
			srv.GetAddressOf()
		);

		// VRAM転送完了したらCPUメモリは解放
		scratchImage.Release();

		return SUCCEEDED(hr);
	}
}