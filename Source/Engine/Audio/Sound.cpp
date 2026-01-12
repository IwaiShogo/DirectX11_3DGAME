#include "Engine/pch.h"
#include "Engine/Audio/Sound.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	// WAVファイルパース用構造体
	struct RIFF_HEADER { char chunkId[4]; unsigned int chunkSize; char format[4]; };
	struct CHUNK_HEADER { char chunkId[4]; unsigned int chunkSize; };

	bool Sound::LoadCPU(const std::string& path)
	{
		this->filepath = path;
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) return false;

		RIFF_HEADER riff;
		file.read((char*)&riff, sizeof(RIFF_HEADER));

		if (strncmp(riff.chunkId, "RIFF", 4) != 0 || strncmp(riff.format, "WAVE", 4) != 0) {
			return false;
		}

		// チャンク探索
		while (true)
		{
			CHUNK_HEADER chunk;
			file.read((char*)&chunk, sizeof(CHUNK_HEADER));
			if (file.eof()) break;

			// fmt チャンク
			if (strncmp(chunk.chunkId, "fmt ", 4) == 0)
			{
				file.read((char*)&wfx, sizeof(WAVEFORMATEX));
				int skip = chunk.chunkSize - sizeof(WAVEFORMATEX);
				if (skip > 0) file.seekg(skip, std::ios::cur);
			}
			// data チャンク
			else if (strncmp(chunk.chunkId, "data", 4) == 0)
			{
				buffer.resize(chunk.chunkSize);
				file.read((char*)buffer.data(), chunk.chunkSize);

				// ここではバッファに読むだけ
				break;
			}
			else
			{
				file.seekg(chunk.chunkSize, std::ios::cur);
			}
		}
		return true;
	}

	void Sound::Initialize()
	{
		// XAudio2バッファ設定
		xBuffer.pAudioData = buffer.data();
		xBuffer.Flags = XAUDIO2_END_OF_STREAM;
		xBuffer.AudioBytes = (UINT32)buffer.size();

		// 再生時間計算
		if (wfx.nAvgBytesPerSec > 0)
			duration = (float)xBuffer.AudioBytes / wfx.nAvgBytesPerSec;
	}
}