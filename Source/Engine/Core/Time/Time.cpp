// ===== インクルード =====
#include "Engine/pch.h"
#include "Time.h"

namespace Arche
{
	// 静的変数の実体定義
	LARGE_INTEGER Time::s_cpuFreq = {};
	LARGE_INTEGER Time::s_lastTime = {};
	LARGE_INTEGER Time::s_startTime = {};
	double Time::s_deltaTime = 0.0;
	bool Time::s_isStepNext = false;
	double Time::s_targetFrameTime = 1.0 / 60.0;
	float Time::timeScale = 1.0f;
	bool Time::isPaused = false;

	void Time::Initialize()
	{
		QueryPerformanceFrequency(&s_cpuFreq);
		QueryPerformanceCounter(&s_startTime);
		s_lastTime = s_startTime;
		timeBeginPeriod(1);
	}

	void Time::Update()
	{
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);

		long long diff = currentTime.QuadPart - s_lastTime.QuadPart;
		s_deltaTime = static_cast<double>(diff) / static_cast<double>(s_cpuFreq.QuadPart);

		s_lastTime = currentTime;
	}

	void Time::StepFrame()
	{
		s_isStepNext = true;
	}

	float Time::DeltaTime()
	{
		if (s_isStepNext)
		{
			s_isStepNext = false;
			return 1.0f / 60.0f;
		}

		if (isPaused)
		{
			return 0.0f;
		}

		return static_cast<float>(s_deltaTime) * timeScale;
	}

	float Time::TotalTime()
	{
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		long long diff = currentTime.QuadPart - s_startTime.QuadPart;
		return static_cast<float>(static_cast<double>(diff) / static_cast<double>(s_cpuFreq.QuadPart));
	}

	void Time::SetFrameRate(int fps)
	{
		if (fps > 0)
		{
			s_targetFrameTime = 1.0f / static_cast<double>(fps);
		}
	}

	void Time::WaitFrame()
	{
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);

		double elapsed = static_cast<double>(currentTime.QuadPart - s_lastTime.QuadPart) / static_cast<double>(s_cpuFreq.QuadPart);

		while (elapsed < s_targetFrameTime)
		{
			double remaining = s_targetFrameTime - elapsed;
			if (remaining > 0.001)
			{
				Sleep(static_cast<DWORD>(remaining * 1000.0));
			}
			QueryPerformanceCounter(&currentTime);
			elapsed = static_cast<double>(currentTime.QuadPart - s_lastTime.QuadPart) / static_cast<double>(s_cpuFreq.QuadPart);
		}
	}
}