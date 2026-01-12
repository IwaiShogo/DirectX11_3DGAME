/*****************************************************************//**
 * @file	InspectorGui.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___INSPECTOR_GUI_H___
#define ___INSPECTOR_GUI_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Base/Reflection.h"
#include "Engine/Core/Base/StringId.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Scene/Components/ComponentDefines.h"
#include "Engine/Renderer/Text/FontManager.h"

namespace Arche
{
	ARCHE_API void Inspector_Snapshot(ImGuiID id, std::function<void(json&)> saveFn);
	ARCHE_API void Inspector_Commit(ImGuiID id, std::function<void(json&)> saveFn, std::function<void(const json&, const json&)> cmdFn);
	ARCHE_API bool Inspector_HasState(ImGuiID id);

	inline std::unordered_map<std::string, ImFont*> g_InspectorFontMap;

	// ------------------------------------------------------------
	// InspectorGuiVisitor
	// 型に応じたImGuiウィジェットを描画する
	// ------------------------------------------------------------
	struct InspectorGuiVisitor
	{
		std::function<void(json&)> serializeFunc;
		std::function<void(const json&, const json&)> commandFunc;

		// 編集開始時の状態を保持する
		//static inline std::map<ImGuiID, json> s_startStates;

		InspectorGuiVisitor(std::function<void(json&)> sFunc, std::function<void(json, json)> cFunc)
			: serializeFunc(sFunc), commandFunc(cFunc) {}

		// 比較用ヘルパー
		// ------------------------------------------------------------
		template<typename T> bool IsChanged(const T& a, const T& b) { return a != b; }
		bool IsChanged(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b) { return a.x != b.x || a.y != b.y; }
		bool IsChanged(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return a.x != b.x || a.y != b.y || a.z != b.z; }
		bool IsChanged(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b) { return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w; }

		// 共通処理ラッパー
		// ImGuiのウィジェットを描画し、編集開始・終了を検知する
		// ------------------------------------------------------------
		template<typename T, typename Func>
		void DrawWidget(const char* label, T& currentVal, Func func)
		{
			// 1. 描画前の値をバックアップ
			T oldVal = currentVal;

			ImGui::PushID(label);
			ImGuiID id = ImGui::GetID(label);

			// 2. ウィジェット描画実行
			func();

			// 3. 編集開始検知
			if (ImGui::IsItemActivated() || (ImGui::IsItemActive() && !Inspector_HasState(id)))
			{
				if (!Inspector_HasState(id))
				{
					if (IsChanged(oldVal, currentVal))
					{
						T temp = currentVal;
						currentVal = oldVal;

						// Engine側でスナップショット作成
						Inspector_Snapshot(id, serializeFunc);

						currentVal = temp;
					}
					else
					{
						Inspector_Snapshot(id, serializeFunc);
					}
				}
			}

			// 4. 編集終了検知
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (Inspector_HasState(id))
				{
					Inspector_Commit(id, serializeFunc, commandFunc);
				}
			}

			ImGui::PopID();
		}

		// 各型の描画実装
		// ------------------------------------------------------------

		// 基本型
		// ------------------------------------------------------------
		// bool
		void operator()(bool& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				ImGui::Checkbox(name, &val);
			});
		}

		// int
		void operator()(int& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				ImGui::DragInt(name, &val);
			});
		}

		// float
		void operator()(float& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				float speed = 0.1f;
				float min = 0.0f;
				float max = 0.0f;

				// 名前による挙動の変化
				if (strstr(name, "volume") || strstr(name, "Volume"))
				{
					speed = 0.01f;
					max = 1.0f;
				}
				else if (strstr(name, "size") || strstr(name, "Size"))
				{
					speed = 1.0f;
				}

				ImGui::DragFloat(name, &val, speed, min, max);
			});
		}

		// 文字列（std::string）
		// ------------------------------------------------------------
		void operator()(std::string& val, const char* name)
		{
			std::string label = name;

			// --------------------------------------------------------
			// 1. テキスト本文 (複数行入力)
			// --------------------------------------------------------
			if (label == "text" || label == "Content")
			{
				ImGui::LabelText(name, "Text Content");
				char buf[1024];
				strncpy_s(buf, val.c_str(), sizeof(buf));
				// 複数行入力ボックス
				if (ImGui::InputTextMultiline(name, buf, sizeof(buf), ImVec2(-1, 60)))
				{
					val = buf;
				}
				return;
			}

			// --------------------------------------------------------
			// 2. フォント選択 (.ttf / .otf)
			// --------------------------------------------------------
			if (label == "fontKey")
			{
				const auto& loadedFonts = FontManager::Instance().GetLoadedFontNames();

				if (ImGui::BeginCombo(name, val.c_str()))
				{
					// カスタムフォント一覧
					for (const auto& fontName : loadedFonts)
					{
						bool isSelected = (val == fontName);
						bool pushed = false;
						if (g_InspectorFontMap.find(fontName) != g_InspectorFontMap.end())
						{
							ImGui::PushFont(g_InspectorFontMap[fontName]);
							pushed = true;
						}

						if (ImGui::Selectable(fontName.c_str(), isSelected))
						{
							val = fontName;
						}

						if (pushed) ImGui::PopFont();
						if (isSelected) ImGui::SetItemDefaultFocus();
					}

					// システムフォントの代表例
					const char* systemFonts[] = { "Meiryo", "Yu Gothic", "MS Gothic", "Arial" };
					for (const char* sysFont : systemFonts)
					{
						bool isSelected = (val == sysFont);
						if (ImGui::Selectable(sysFont, isSelected))
						{
							val = sysFont;
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				return;
			}

			// --------------------------------------------------------
			// 3. リソースファイル選択 (サブフォルダ対応・検索機能付き)
			// --------------------------------------------------------

			// 検索対象のディレクトリと拡張子を決定
			std::string searchDir = "";
			std::vector<std::string> extensions;

			if (label == "modelKey" || strstr(name, "Model")) {
				searchDir = "Resources/Game/Models";
				extensions = { ".fbx", ".obj", ".gltf", ".glb" };
			}
			else if (label == "textureKey" || strstr(name, "Texture") || strstr(name, "Sprite") || strstr(name, "Image")) {
				searchDir = "Resources/Game/Textures";
				extensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
			}
			else if (label == "controllerPath" || strstr(name, "Animation")) {
				searchDir = "Resources/Game/Animations";
				extensions = { ".json", ".fbx" }; // FBXのアニメーションも含む場合
			}
			else if (label == "soundKey" || strstr(name, "Sound")) {
				searchDir = "Resources/Game/Sounds";
				extensions = { ".wav", ".mp3", ".ogg" };
			}

			// 対象のリソース変数でなければ、通常のテキスト入力 (フォールバック)
			if (searchDir.empty())
			{
				char buf[256];
				strncpy_s(buf, val.c_str(), sizeof(buf));
				if (ImGui::InputText(name, buf, sizeof(buf)))
				{
					val = buf;
				}
				// ドラッグ&ドロップ受け入れ (絶対パス対応)
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* droppedPathW = (const wchar_t*)payload->Data;
						std::filesystem::path p(droppedPathW);
						std::string fullPath = p.string();
						std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

						// Resources以下なら相対パスにする
						size_t found = fullPath.find("Resources");
						if (found != std::string::npos) val = fullPath.substr(found);
						else val = fullPath;
					}
					ImGui::EndDragDropTarget();
				}
				return;
			}

			// === ファイル選択用プルダウン描画 ===

			static std::unordered_map<std::string, std::vector<std::string>> s_fileCache;
			bool needScan = (s_fileCache[searchDir].empty());

			ImGui::PushID(name);

			float btnWidth = 24.0f;
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - btnWidth - 8.0f);

			std::string preview = val.empty() ? "(None)" : val;

			if (ImGui::BeginCombo(name, preview.c_str()))
			{
				// 検索フィルター
				static char filterBuf[64] = "";
				ImGui::InputTextWithHint("##Filter", "Search...", filterBuf, 64);
				std::string filterStr = filterBuf;
				bool hasFilter = !filterStr.empty();

				if (!hasFilter)
				{
					if (ImGui::Selectable("(None)", val.empty())) val = "";
				}

				for (const auto& file : s_fileCache[searchDir])
				{
					// フィルター (大文字小文字区別簡易版)
					if (hasFilter && file.find(filterStr) == std::string::npos) continue;

					bool isSelected = (val == file);
					if (ImGui::Selectable(file.c_str(), isSelected))
					{
						val = file;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::SameLine();
			// リフレッシュボタン
			if (ImGui::Button("R", ImVec2(btnWidth, 0)))
			{
				needScan = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh file list");

			ImGui::PopID();

			// ファイルスキャン (再帰的)
			if (needScan)
			{
				auto& list = s_fileCache[searchDir];
				list.clear();

				if (std::filesystem::exists(searchDir))
				{
					// recursive_directory_iterator でサブフォルダも走査
					for (const auto& entry : std::filesystem::recursive_directory_iterator(searchDir))
					{
						if (entry.is_regular_file())
						{
							std::string ext = entry.path().extension().string();
							bool match = false;
							for (const auto& e : extensions) if (e == ext) match = true;

							if (match)
							{
								// ルートからの相対パスを作成
								std::string relative = std::filesystem::relative(entry.path(), searchDir).string();
								std::replace(relative.begin(), relative.end(), '\\', '/');
								list.push_back(relative);
							}
						}
					}
				}
			}
		}

		// リソースキー（StringId） -> ドラッグ&ドロップ対応
		// ------------------------------------------------------------
		void operator()(StringId& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
					std::string s = val.c_str();
					char buf[256];
					strcpy_s(buf, sizeof(buf), s.c_str());

					ImGui::InputText(name, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
			});
		}

		// DirectX Math型
		// ------------------------------------------------------------
		// XMFLOAT2
		void operator()(DirectX::XMFLOAT2& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				float v[2] = { val.x, val.y };
				if (ImGui::DragFloat2(name, v, 0.1f))
				{
					val = { v[0], v[1] };
				}
			});
		}

		// XMFLOAT3
		void operator()(DirectX::XMFLOAT3& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				float v[3] = { val.x, val.y, val.z };
				if (strstr(name, "color") || strstr(name, "Color"))
				{
					if (ImGui::ColorEdit3(name, v)) val = { v[0], v[1], v[2] };
				}
				else
				{
					if (ImGui::DragFloat3(name, v, 0.1f)) val = { v[0], v[1], v[2] };
				}
			});
		}

		// XMFLOAT4型（空ピッカー）
		void operator()(DirectX::XMFLOAT4& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				// 変数名に Color が含まれていなくても、XMFLOAT4は基本的に色として扱う
				float v[4] = { val.x, val.y, val.z, val.w };
				if (ImGui::ColorEdit4(name, v))
				{
					val = { v[0], v[1], v[2], v[3] };
				}
			});
		}

		// Enum型
		// ------------------------------------------------------------
		// ColliderType
		void operator()(ColliderType& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				// プルダウンの中身
				const char* items[] = { "Box", "Sphere", "Capsule", "Cylinder" };
				int item = (int)val;

				if (ImGui::Combo(name, &item, items, IM_ARRAYSIZE(items))) val = (ColliderType)item;
			});
		}

		// BodyType
		void operator()(BodyType& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				const char* items[] = { "Static", "Dynamic", "Kinematic" };
				int item = (int)val;
				if (ImGui::Combo(name, &item, items, IM_ARRAYSIZE(items))) val = (BodyType)item;
			});
		}

		// Layer
		void operator()(Layer& val, const char* name)
		{
			DrawWidget(name, val, [&]()
			{
				int layerVal = (int)val;
				if (ImGui::InputInt(name, &layerVal)) val = (Layer)layerVal;
			});
		}

		// その他（Entity等）
		// ------------------------------------------------------------
		// Entity
		void operator()(Entity& val, const char* name)
		{
			if (val == NullEntity) ImGui::Text("%s: (None)", name);
			else ImGui::Text("%s: Entity(%d)", name, (uint32_t)val);
		}

		// Vector (Sprcialized for Entity)
		void operator()(std::vector<Entity>& val, const char* name)
		{
			// 折り畳み式のツリーで子要素を表示
			if (ImGui::TreeNode(name))
			{
				if (val.empty())
				{
					ImGui::TextDisabled("Empty");
				}
				else
				{
					for (size_t i = 0; i < val.size(); ++i)
					{
						ImGui::Text("[%d] Entity(ID: %d)", i, (uint32_t)val[i]);
					}
				}
				ImGui::TreePop();
			}
			else
			{
				// 閉じているときは要素数だけ表示
				ImGui::SameLine();
				ImGui::TextDisabled("(%d items)", val.size());
			}
		}

		// Fallback
		template<typename T>
		void operator()(T& val, const char* name)
		{
			ImGui::TextDisabled("%s: (Not Supported)", name);
		}
	};

	// ------------------------------------------------------------
	// ヘルパー関数：コンポーネントを描画する
	// ------------------------------------------------------------
	template<typename T>
	void DrawComponent(
		const char* label,
		T& component,
		Entity entity,
		Registry& registry,
		bool& removed,
		std::function<void(json&)> serializeFunc,
		std::function<void(const json&, const json&)> commandFunc,
		int index = -1,
		std::function<void(int, int)> onReorder = nullptr
	)
	{
		// D&D用のID
		ImGui::PushID(label);

		// チェックボックスの状態を取得
		bool isEnabled = registry.isComponentEnabled<T>(entity);

		float headerX = ImGui::GetCursorPosX();

		ImGui::Checkbox("##Enabled", &isEnabled);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			registry.setComponentEnabled<T>(entity, isEnabled);
		}
		ImGui::SameLine();

		// ヘッダー描画
		bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

		// ドラッグ&ドロップ処理
		// ------------------------------------------------------------
		if (onReorder && index >= 0)
		{
			// 1. ドラッグ元（Source）
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				// 並び変え元のインデックスを渡す
				ImGui::SetDragDropPayload("COMPONENT_ORDER", &index, sizeof(int));
				ImGui::Text("Move %s", label);
				ImGui::EndDragDropSource();
			}

			// 2. ドロップ先（Target）
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMPONENT_ORDER"))
				{
					int srcIndex = *(const int*)payload->Data;
					// 自分自身へのドロップでなければ並び替え実行
					if (srcIndex != index)
					{
						onReorder(srcIndex, index);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		if (open)
		{
			// 受け取った関数を使ってVisitorを正しく構築する
			InspectorGuiVisitor visitor(serializeFunc, commandFunc);
			Reflection::VisitMembers(component, visitor);

			ImGui::Spacing();
			if (ImGui::Button("Remove Component"))
			{
				removed = true;
			}
		}

		ImGui::PopID();
	}

}	// namespace Arche

#endif // _DEBUG

#endif // !___INSPECTOR_GUI_H___