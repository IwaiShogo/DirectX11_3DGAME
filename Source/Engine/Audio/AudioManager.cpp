/*****************************************************************//**
 * @file	AudioManager.cpp
 * @brief	オーディオマネージャー
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/11/26	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Core/Time/Time.h"

namespace Arche
{

	void AudioManager::Initialize()
	{
		HRESULT hr;

		// 1. COM初期化 (既に行われている場合はS_FALSEが返る)
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		// 2. XAudio2エンジンの作成
		hr = XAudio2Create(&m_xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to init XAudio2\n");
			return;
		}

		// 3. マスターボイス（最終出力）の作成
		hr = m_xAudio2->CreateMasteringVoice(&m_masterVoice);
		if (FAILED(hr)) return;

		// 4. サブミックス（グループ）の作成
		// SE用
		hr = m_xAudio2->CreateSubmixVoice(&m_seSubmix, 1, 44100, 0, 0, 0, 0);
		// BGM用
		hr = m_xAudio2->CreateSubmixVoice(&m_bgmSubmix, 1, 44100, 0, 0, 0, 0);

		m_masterVoice->SetVolume(m_masterVolume);
		m_seSubmix->SetVolume(m_seVolume);
		m_bgmSubmix->SetVolume(m_bgmVolume);
	}

	void AudioManager::Update()
	{
		float dt = Time::DeltaTime();

		// 履歴の更新（時間が経ったら消す）
		{
			auto it = m_soundEvents.begin();
			while (it != m_soundEvents.end())
			{
				it->time -= dt;
				if (it->time <= 0.0f)
				{
					it = m_soundEvents.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		// 再生が終了したSEボイスを検出し、破棄する
		{
			auto it = m_seVoices.begin();
			while (it != m_seVoices.end()) {
				XAUDIO2_VOICE_STATE state;
				it->voice->GetState(&state);

				// バッファを使い切った = 再生終了
				if (state.BuffersQueued == 0) {
					it->voice->DestroyVoice();
					it = m_seVoices.erase(it);
				}
				else {
					++it;
				}
			}
		}
	}

	void AudioManager::Finalize()
	{
		if (m_currentBgmVoice) {
			m_currentBgmVoice->DestroyVoice();
			m_currentBgmVoice = nullptr;
		}
		for (auto& v : m_seVoices) {
			v.voice->DestroyVoice();
		}
		m_seVoices.clear();

		if (m_seSubmix) m_seSubmix->DestroyVoice();
		if (m_bgmSubmix) m_bgmSubmix->DestroyVoice();
		if (m_masterVoice) m_masterVoice->DestroyVoice();

		m_xAudio2.Reset();
		CoUninitialize();
	}

	void AudioManager::PlaySE(std::string& key, float volume, float pitch)
	{
		// 1. データ取得 (ResourceManager経由)
		auto sound = ResourceManager::Instance().GetSound(key);
		if (!sound) return;

		IXAudio2SourceVoice* sourceVoice = nullptr;

		// SEサブミックスへ出力するように指定
		XAUDIO2_SEND_DESCRIPTOR send = { 0, m_seSubmix };
		XAUDIO2_VOICE_SENDS sendList = { 1, &send };

		HRESULT hr = m_xAudio2->CreateSourceVoice(&sourceVoice, &sound->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList);
		if (FAILED(hr)) return;

		sourceVoice->SetVolume(volume);
		sourceVoice->SetFrequencyRatio(pow(2.0f, pitch)); // ピッチ変更

		hr = sourceVoice->SubmitSourceBuffer(&sound->xBuffer);
		if (FAILED(hr)) {
			sourceVoice->DestroyVoice();
			return;
		}

		sourceVoice->Start(0);
		m_seVoices.push_back({ sourceVoice, false });

	}

	void AudioManager::Play3DSE(std::string& key, const XMFLOAT3& emitterPos, const XMFLOAT3& listenerPos, float range, float volume)
	{
		float dx = emitterPos.x - listenerPos.x;
		float dy = emitterPos.y - listenerPos.y;
		float dz = emitterPos.z - listenerPos.z;
		float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

		if (dist > range) return;	// 聞こえない

		float attenuation = 1.0f - (dist / range);
		PlaySE(key, volume * attenuation);

		// デバッグ用に位置を更新
		m_soundEvents.push_back({ key, emitterPos, 1.5f });
	}

	void AudioManager::PlayBGM(std::string& key, float volume, bool loop)
	{
		// 既に再生中なら止める
		StopBGM();

		// データ取得 (ResourceManager経由)
		auto sound = ResourceManager::Instance().GetSound(key);
		if (!sound) return;

		// 仮の実装：ResourceManager連携前のためコメントアウト

		// BGMサブミックスへ出力
		XAUDIO2_SEND_DESCRIPTOR send = { 0, m_bgmSubmix };
		XAUDIO2_VOICE_SENDS sendList = { 1, &send };

		HRESULT hr = m_xAudio2->CreateSourceVoice(&m_currentBgmVoice, &sound->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList);
		if (FAILED(hr)) return;

		m_currentBgmVoice->SetVolume(volume);

		// ループ設定
		XAUDIO2_BUFFER buffer = sound->xBuffer;
		if (loop) {
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		m_currentBgmVoice->SubmitSourceBuffer(&buffer);
		m_currentBgmVoice->Start(0);
	}

	void AudioManager::StopBGM(float fadeOutSeconds)
	{
		if (m_currentBgmVoice) {
			m_currentBgmVoice->Stop();
			m_currentBgmVoice->FlushSourceBuffers();
			m_currentBgmVoice->DestroyVoice();
			m_currentBgmVoice = nullptr;
		}
	}

	void AudioManager::SetMasterVolume(float volume)
	{
		if (m_masterVoice) m_masterVoice->SetVolume(volume);
	}
	void AudioManager::SetSEVolume(float volume)
	{
		if (m_seSubmix) m_seSubmix->SetVolume(volume);
	}
	void AudioManager::SetBGMVolume(float volume)
	{
		if (m_bgmSubmix) m_bgmSubmix->SetVolume(volume);
	}

	void AudioManager::OnInspector() {
		ImGui::Begin("Audio Mixer");

		// マスター音量
		if (ImGui::SliderFloat("Master Volume", &m_masterVolume, 0.0f, 1.0f)) {
			SetMasterVolume(m_masterVolume);
		}

		// BGM音量
		if (ImGui::SliderFloat("BGM Volume", &m_bgmVolume, 0.0f, 1.0f)) {
			SetBGMVolume(m_bgmVolume);
		}

		// SE音量
		if (ImGui::SliderFloat("SE Volume", &m_seVolume, 0.0f, 1.0f)) {
			SetSEVolume(m_seVolume);
		}

		ImGui::Separator();

		// アクティブなSEの数
		ImGui::Text("Active SE Voices: %d", m_seVoices.size());

		// BGM再生状況
		if (m_currentBgmVoice) {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "BGM Playing");
			if (ImGui::Button("Stop BGM")) StopBGM(0.5f);
		}
		else {
			ImGui::TextDisabled("BGM Stopped");
		}

		ImGui::End();
	}

}	// namespace Arche