#include "Engine/pch.h"
#include "Engine/Audio/Sound.h"
#include "Engine/Core/Base/Logger.h"
#include <cstdio> // for sprintf_s

namespace Arche
{
	// WAVファイルパース用構造体
	struct RIFF_HEADER { char chunkId[4]; unsigned int chunkSize; char format[4]; };
	struct CHUNK_HEADER { char chunkId[4]; unsigned int chunkSize; };

	bool Sound::LoadCPU(const std::string& path)
	{
		this->filepath = path;
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			OutputDebugStringA(("Sound Load Error: File not found -> " + path + "\n").c_str());
			return false;
		}

		RIFF_HEADER riff;
		file.read((char*)&riff, sizeof(RIFF_HEADER));

		// RIFFヘッダチェック
		if (strncmp(riff.chunkId, "RIFF", 4) != 0 || strncmp(riff.format, "WAVE", 4) != 0) {
			OutputDebugStringA(("Sound Load Error: Invalid WAV format -> " + path + "\n").c_str());
			return false;
		}

		// チャンク探索
		bool fmtFound = false;
		bool dataFound = false;

		while (file.good())
		{
			CHUNK_HEADER chunk;
			file.read((char*)&chunk, sizeof(CHUNK_HEADER));
			if (file.eof()) break;

			// デバッグログ: 見つけたチャンクを表示
			char logBuf[256];
			sprintf_s(logBuf, "WAV Chunk: [%c%c%c%c] Size: %u (%s)\n",
				chunk.chunkId[0], chunk.chunkId[1], chunk.chunkId[2], chunk.chunkId[3],
				chunk.chunkSize, path.c_str());
			OutputDebugStringA(logBuf);

			// 現在位置を保存
			std::streampos chunkDataStart = file.tellg();

			// [fmt ] チャンク (フォーマット情報)
			if (strncmp(chunk.chunkId, "fmt ", 4) == 0)
			{
				memset(&wfx, 0, sizeof(WAVEFORMATEX));
				unsigned int readSize = (chunk.chunkSize < sizeof(WAVEFORMATEX)) ? chunk.chunkSize : sizeof(WAVEFORMATEX);
				file.read((char*)&wfx, readSize);

				// 補正: 16バイト形式(PCMWAVEFORMAT)の場合のケア
				if (readSize < 18 && wfx.wFormatTag == WAVE_FORMAT_PCM) {
					wfx.cbSize = 0;
				}
				fmtFound = true;
			}
			// [data] チャンク (波形データ)
			else if (strncmp(chunk.chunkId, "data", 4) == 0)
			{
				if (chunk.chunkSize > 0) {
					buffer.resize(chunk.chunkSize);
					file.read((char*)buffer.data(), chunk.chunkSize);
					dataFound = true;
				}
				else {
					OutputDebugStringA("Sound Warning: Empty data chunk.\n");
				}
				// 音声データを見つけたら読み込み終了とする
				break;
			}

			// --- 次のチャンクへスキップ ---
			std::streampos currentPos = file.tellg();
			long long bytesRead = currentPos - chunkDataStart;
			long long bytesToSkip = chunk.chunkSize - bytesRead;

			if (bytesToSkip > 0) {
				file.seekg(bytesToSkip, std::ios::cur);
			}

			// パディングスキップ (奇数サイズの場合)
			if (chunk.chunkSize % 2 != 0) {
				file.seekg(1, std::ios::cur);
			}
		}

		if (!fmtFound) {
			OutputDebugStringA(("Sound Error: 'fmt ' chunk not found -> " + path + "\n").c_str());
			return false;
		}
		if (!dataFound || buffer.empty()) {
			OutputDebugStringA(("Sound Error: 'data' chunk not found or empty -> " + path + "\n").c_str());
			return false;
		}

		return true;
	}

	void Sound::Initialize()
	{
		// XAudio2バッファ設定
		xBuffer.pAudioData = buffer.data();
		xBuffer.Flags = XAUDIO2_END_OF_STREAM;
		xBuffer.AudioBytes = (UINT32)buffer.size();

		// ★補正: nAvgBytesPerSec が 0 の場合、自分で計算する
		if (wfx.nAvgBytesPerSec == 0 && wfx.nSamplesPerSec > 0 && wfx.nBlockAlign > 0) {
			wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		}

		// 再生時間計算
		if (wfx.nAvgBytesPerSec > 0) {
			duration = (float)xBuffer.AudioBytes / wfx.nAvgBytesPerSec;
		}
		else {
			duration = 0.0f;
			OutputDebugStringA("Sound Warning: Duration could not be calculated (nAvgBytesPerSec is 0)\n");
		}
	}
}