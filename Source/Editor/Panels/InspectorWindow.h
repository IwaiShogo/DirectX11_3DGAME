/*****************************************************************//**
 * @file	InspectorWindow.h
 * @brief	インスペクターウィンドウ
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/11/27	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	
 * コンポーネントを追加した際：
 * 1. Draw()内、コンポーネント一覧に追加
 * 2. AddComponentPopupに追加
 *********************************************************************/

#ifndef ___INSPECTOR_WINDOW_H___
#define ___INSPECTOR_WINDOW_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Editor/Core/Editor.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"
#include "Editor/Core/CommandHistory.h"
#include "Editor/Core/EditorCommands.h"

namespace Arche
{

	class InspectorWindow : public EditorWindow
	{
	public:
		InspectorWindow()
		{
			m_windowName = "Inspector";
		}

		void Draw(World& world, std::vector<Entity>& selection, Context& ctx) override
		{
			if (!m_isOpen) return;

			ImGui::Begin(m_windowName.c_str(), &m_isOpen);

			// 選択がない場合
			if (selection.empty())
			{
				ImGui::End();
				return;
			}

			Registry& reg = world.getRegistry();

			// プライマリ（最後に追加されたもの）を基準にする
			Entity primary = selection.back();
			if (!reg.valid(primary))
			{
				ImGui::End();
				return;
			}

			// ------------------------------------------------------------
			// 1. ヘッダー情報 (Active / Tag / Name)
			// ------------------------------------------------------------
			bool isSingle = (selection.size() == 1);

			// Activeチェックボックス (プライマリの状態を表示)
			bool isActive = reg.isActiveSelf(primary);
			if (ImGui::Checkbox("##Active", &isActive))
			{
				// 全員に適用
				for (Entity e : selection)
				{
					if (reg.valid(e)) reg.setActive(e, isActive);
				}
			}
			ImGui::SameLine();

			// 名前とタグ
			if (reg.has<Tag>(primary))
			{
				auto& tagComp = reg.get<Tag>(primary);

				// 1. Name (ヒエラルキー表示名)
				char nameBuf[256];
				memset(nameBuf, 0, sizeof(nameBuf));
				strcpy_s(nameBuf, sizeof(nameBuf), tagComp.name.c_str());

				// Enterキーで確定したら反映
				if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					tagComp.name = nameBuf;
				}
				// 編集中も反映したい場合はこちら（お好みで）
				else if (ImGui::IsItemDeactivatedAfterEdit())
				{
					tagComp.name = nameBuf;
				}

				// 2. Tag (ロジック判定用)
				char tagBuf[256];
				memset(tagBuf, 0, sizeof(tagBuf));
				strcpy_s(tagBuf, sizeof(tagBuf), tagComp.tag.c_str());

				if (ImGui::InputText("Tag", tagBuf, sizeof(tagBuf)))
				{
					tagComp.tag = tagBuf;
				}

				ImGui::Separator();
			}

			// ------------------------------------------------------------
			// 2. コンポーネント描画 (共通のものだけ表示)
			// ------------------------------------------------------------
			if (reg.has<Tag>(primary))
			{
				// プライマリの持っているコンポーネント順序を取得
				auto& primaryOrder = reg.get<Tag>(primary).componentOrder;
				auto& interfaces = ComponentRegistry::Instance().GetInterfaces();

				for (const std::string& compName : primaryOrder)
				{
					if (compName == "Tag") continue; // Tagは上で表示済み

					// インターフェースが存在するか
					if (interfaces.find(compName) == interfaces.end()) continue;

					// "全員が" このコンポーネントを持っているかチェック
					bool allHave = true;
					for (Entity e : selection)
					{
						if (!interfaces.at(compName).has(reg, e))
						{
							allHave = false;
							break;
						}
					}

					// 全員持っている場合のみ表示
					if (allHave)
					{
						auto& iface = interfaces.at(compName);

						// --- コールバック定義 ---

						// 値変更時: 全員に適用
						auto onCommand = [&](json oldVal, json newVal)
							{
								// 変更があった場合、選択されている全エンティティに適用
								for (Entity e : selection)
								{
									// 各エンティティの現在の値を「Old」として保存すべきだが、
									// 簡易実装として「プライマリの変更差分」を全員に適用する形にする
									// ※厳密には個別にChangeComponentValueCommandを作る

									// 現在のそのエンティティの値をOldとして取得
									json targetOldVal;
									iface.serialize(reg, e, targetOldVal);

									// 新しい値を作成（変更箇所だけマージするのは複雑なので、
									// ここでは単純に「プライマリと同じ値になる」挙動とする）
									// ※TransformのPositionなどを一括変更すると全員同じ場所になる挙動
									//	 Unityのような「相対的な加算」はさらに高度な実装が必要

									CommandHistory::Execute(std::make_shared<ChangeComponentValueCommand>(
										world, e, compName, targetOldVal, newVal));
								}
							};

						// 削除時: 全員から削除
						auto onRemove = [&]()
							{
								for (Entity e : selection)
								{
									CommandHistory::Execute(std::make_shared<RemoveComponentCommand>(world, e, compName));
								}
							};

						// 並び替え時: とりあえずプライマリのみ（複雑なため）
						auto onReorder = [&](int src, int dst)
							{
								// 単体選択時のみ許可するのが安全
								if (isSingle)
								{
									// 既存のReorder処理 (Editor側で実装が必要かも)
								}
							};

						// 描画実行 (プライマリの値を表示)
						if (iface.drawInspectorDnD)
						{
							// IDが被らないようにPushIDする
							ImGui::PushID(compName.c_str());
							iface.drawInspectorDnD(reg, primary, 0, onReorder, onRemove, onCommand);
							ImGui::PopID();
						}
					}
				}
			}

			ImGui::Separator();

			// ------------------------------------------------------------
			// 3. コンポーネント追加ボタン (全員に追加)
			// ------------------------------------------------------------
			float width = ImGui::GetContentRegionAvail().x;
			float btnW = 200.0f;
			ImGui::SetCursorPosX((width - btnW) * 0.5f);

			if (ImGui::Button("Add Component", ImVec2(btnW, 0)))
			{
				ImGui::OpenPopup("AddComponentPopup");
			}

			if (ImGui::BeginPopup("AddComponentPopup"))
			{
				static char searchBuf[64] = "";
				if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
				ImGui::InputTextWithHint("##Search", "Search...", searchBuf, sizeof(searchBuf));
				ImGui::Separator();

				auto& interfaces = ComponentRegistry::Instance().GetInterfaces();

				// コンポーネント一覧を表示
				for (auto& [name, iface] : interfaces)
				{
					if (name == "Tag" || name == "NativeScriptComponent") continue; // 除外

					// 検索フィルタ
					if (searchBuf[0] != '\0')
					{
						std::string n = name;
						std::string s = searchBuf;
						
						// 両方を小文字に変換
						std::transform(n.begin(), n.end(), n.begin(), ::tolower);
						std::transform(s.begin(), s.end(), s.begin(), ::tolower);

						if (n.find(s) == std::string::npos) continue;
					}

					if (ImGui::MenuItem(name.c_str()))
					{
						// 全員に追加
						for (Entity e : selection)
						{
							if (!iface.has(reg, e))
							{
								CommandHistory::Execute(std::make_shared<AddComponentCommand>(world, e, name));
							}
						}
						ImGui::CloseCurrentPopup();
						searchBuf[0] = '\0';
					}
				}
				ImGui::EndPopup();
			}

			ImGui::End();
		}
	};

}	// namespace Arche

#endif // _DEBUG

#endif	//!___INSPECTOR_WINDOW_H___