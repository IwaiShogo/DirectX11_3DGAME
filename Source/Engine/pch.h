// ======================================================================
// Procompiled Header for Engine
// ここに追加されたヘッダーは一度だけコンパイルされ、使い回されます。
// 頻繁に変更されるファイルはここに追加しないでください。
// ======================================================================

#ifndef ___PCH_H___
#define ___PCH_H___

// メモリリーク検出用の定義
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

// Windowsのmin/maxマクロと標準ライブラリの競合を防ぐ
#define NOMINMAX

// Windows APIの軽量化（不要なAPIを除外）
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>

// Timeクラスで使用するマルチメディアタイマーAPI用
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// Inputクラス
#include <Xinput.h>
#include <array>
#pragma comment(lib, "xinput.lib")

// ResourceManager
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>
#include <DirectXTex.h>

// AudioManager
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

// Direct2D / DirectWrite
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <stringapiset.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "gdi32.lib")

// --- DirectX ---
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>	// ComPtr
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- WIC ---
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

// --- Standard Library ---
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <typeindex>
#include <type_traits>
#include <chrono>
#include <thread>
#include <mutex>
#include <cmath>
#include <limits>
#include <cassert>
#include <stdexcept>
#include <set>
#include <random>
#include <cstdint>
#include <filesystem>
#include <tuple>
#include <deque>
#include <comdef.h>
#include <future>
#include <list>

#include "Engine/Core/Core.h"

// --- Libraries ---
#include <json.hpp>	// nlohmann/json
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
// #include "imgui_stdlib.h"
#include "ImGuizmo.h"
#include "imnodes.h"

// --- Common Engine Definitions ---
// よく使うComPtrやDirectXMathのnamespce省略など
using Microsoft::WRL::ComPtr;
using json = nlohmann::json;
using namespace DirectX;

#ifdef _DEBUG
	#define new new( _NORMAL_BLOCK, __FILE__, __LINE__ )
#endif // _DEBUG

#endif // !___PCH_H___