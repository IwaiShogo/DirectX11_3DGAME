/*****************************************************************//**
 * @file	SceneViewPanel.cpp
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#include "Engine/pch.h"
#ifdef _DEBUG
#include "SceneViewPanel.h"
#include "Editor/Core/Editor.h" // ここでEditorをインクルードすればOK
#include "Engine/Scene/Components/Components.h"
#include "Engine/Scene/Systems/Physics/CollisionSystem.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"
#include "Engine/Resource/ResourceManager.h"
#include "Editor/Core/EditorPrefs.h"
#include "Editor/Tools/GizmoSystem.h"

namespace Arche
{
	SceneViewPanel::SceneViewPanel()
	{
		m_camera.Initialize(XM_PIDIV4, (float)Config::SCREEN_WIDTH / (float)Config::SCREEN_HEIGHT, 0.1f, 1000.0f);
		auto& prefs = EditorPrefs::Instance().camera;
		m_camera.SetPosition({ prefs.x, prefs.y, prefs.z });
		m_camera.SetRotation(prefs.pitch, prefs.yaw);
	}

	SceneViewPanel::~SceneViewPanel()
	{
		auto pos = m_camera.GetPositionFloat3();
		auto& prefs = EditorPrefs::Instance().camera;
		prefs.x = pos.x;
		prefs.y = pos.y;
		prefs.z = pos.z;
		prefs.pitch = m_camera.GetPitch();
		prefs.yaw = m_camera.GetYaw();

		EditorPrefs::Instance().Save();
	}

	void SceneViewPanel::Draw(World& world, std::vector<Entity>& selection)
	{
		// Editorクラスへのアクセスが可能になる
		World* activeWorld = &world;
		if (Editor::Instance().GetMode() == Editor::EditorMode::Prefab)
		{
			activeWorld = Editor::Instance().GetActiveWorld();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		bool open = ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);

		if (!open)
		{
			ImGui::End();
			ImGui::PopStyleVar();
			return;
		}

		// 0. Deleteキーでの削除ショートカット (複数選択対応)
		if (ImGui::IsWindowFocused() && !selection.empty())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				// コピーしてから回す（イテレータ破壊防止）
				auto targets = selection;
				for (Entity e : targets)
				{
					CommandHistory::Execute(std::make_shared<DeleteEntityCommand>(world, e));
				}
				Editor::Instance().ClearSelection();
			}
		}

		// 1. サイズチェック
		ImVec2 panelSize = ImGui::GetContentRegionAvail();
		if (panelSize.x < 1.0f || panelSize.y < 1.0f)
		{
			ImGui::End();
			ImGui::PopStyleVar();
			return;
		}

		panelSize.x = std::floor(panelSize.x);
		panelSize.y = std::floor(panelSize.y);

		float targetAspect = (float)Config::SCREEN_WIDTH / (float)Config::SCREEN_HEIGHT;
		ImVec2 imageSize = panelSize;

		if (panelSize.x / panelSize.y > targetAspect)
		{
			imageSize.x = std::floor(panelSize.y * targetAspect);
			imageSize.y = panelSize.y;
		}
		else
		{
			imageSize.x = panelSize.x;
			imageSize.y = std::floor(panelSize.x / targetAspect);
		}

		ImVec2 cursorStart = ImGui::GetCursorScreenPos();
		float offsetX = (panelSize.x - imageSize.x) * 0.5f;
		float offsetY = (panelSize.y - imageSize.y) * 0.5f;
		ImVec2 imageStart = ImVec2(cursorStart.x + offsetX, cursorStart.y + offsetY);

		ImGui::SetCursorScreenPos(imageStart);

		// 2. リサイズ処理
		static ImVec2 lastSize = { 0, 0 };
		if ((int)imageSize.x != (int)lastSize.x || (int)imageSize.y != (int)lastSize.y)
		{
			Application::Instance().ResizeSceneRenderTarget((float)imageSize.x, (float)imageSize.y);
			lastSize = imageSize;
		}

		if (ImGui::IsWindowHovered())
		{
			m_camera.SetAspect(imageSize.x / imageSize.y);
			m_camera.Update(); // 元のUpdate関数を使用
		}

		// 3. 画像描画
		auto srv = Application::Instance().GetSceneSRV();
		if (srv) {
			ImGui::Image((void*)srv, imageSize);
		}

		// 7. シーンビューへのドラッグ&ドロップ
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* droppedPath = (const char*)payload->Data;
				std::string relativePath = droppedPath;
				std::string rootPath = "";
				std::string fullPath = (relativePath.find(rootPath) == 0) ? relativePath : rootPath + relativePath;
				std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

				std::filesystem::path fpath = fullPath;
				std::string ext = fpath.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();

				float contentX = viewportOffset.x + viewportMinRegion.x;
				float contentY = viewportOffset.y + viewportMinRegion.y;
				auto mousePos = ImGui::GetMousePos();
				float vecX = mousePos.x - contentX;
				float vecY = mousePos.y - contentY;
				float width = viewportMaxRegion.x - viewportMinRegion.x;
				float height = viewportMaxRegion.y - viewportMinRegion.y;

				XMMATRIX viewMat = m_camera.GetViewMatrix();
				XMMATRIX projMat = m_camera.GetProjectionMatrix();

				XMVECTOR mouseNear = XMVector3Unproject(XMVectorSet(vecX, vecY, 0.0f, 1.0f),
					0, 0, width, height, 0.0f, 1.0f, projMat, viewMat, XMMatrixIdentity());
				XMVECTOR mouseFar = XMVector3Unproject(XMVectorSet(vecX, vecY, 1.0f, 1.0f),
					0, 0, width, height, 0.0f, 1.0f, projMat, viewMat, XMMatrixIdentity());

				XMVECTOR rayDir = XMVector3Normalize(mouseFar - mouseNear);
				XMVECTOR rayOrigin = mouseNear;

				float t = -XMVectorGetZ(rayOrigin) / XMVectorGetZ(rayDir);
				XMVECTOR worldPos = rayOrigin + rayDir * t;

				XMFLOAT3 spawnPos;
				XMStoreFloat3(&spawnPos, worldPos);
				spawnPos.z = 0.0f;

				Entity newEntity = NullEntity;

				if (ext == ".json") // Prefab
				{
					newEntity = SceneSerializer::LoadPrefab(world, fullPath);
				}
				else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
				{
					newEntity = world.create_entity().id();
					std::string name = fpath.stem().string();
					world.getRegistry().emplace<Tag>(newEntity, name);
					world.getRegistry().emplace<Transform>(newEntity);

					std::string key(fullPath);
					world.getRegistry().emplace<SpriteComponent>(newEntity, key);

					auto tex = ResourceManager::Instance().GetTexture(key);
					if (tex)
					{
						world.getRegistry().emplace<Transform2D>(newEntity, 0.0f, 0.0f, (float)tex->width, (float)tex->height);
					}
					else
					{
						world.getRegistry().emplace<Transform2D>(newEntity, 0.0f, 0.0f, 100.0f, 100.0f);
					}
				}

				if (newEntity != NullEntity)
				{
					if (world.getRegistry().has<Transform>(newEntity))
						world.getRegistry().get<Transform>(newEntity).position = spawnPos;

					if (world.getRegistry().has<Transform2D>(newEntity))
					{
						world.getRegistry().get<Transform2D>(newEntity).position = { spawnPos.x, spawnPos.y };
					}

					// 複数選択対応のセッターを使用
					Editor::Instance().SetSelectedEntity(newEntity);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImVec2 imageMin = ImGui::GetItemRectMin();

		XMMATRIX viewMat = m_camera.GetViewMatrix();
		XMMATRIX projMat = m_camera.GetProjectionMatrix();
		bool cameraFound = true;

		// ギズモ描画
		// 選択されている最後のエンティティ（プライマリ）を表示対象とする
		Entity primarySelected = selection.empty() ? NullEntity : selection.back();

		if (cameraFound && primarySelected != NullEntity)
		{
			// ワールドの切り替えに対応するため、引数の world を使用する
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(imageStart.x, imageStart.y, imageSize.x, imageSize.y);
			// primarySelected を渡す
			GizmoSystem::Draw(*activeWorld, primarySelected, viewMat, projMat, imageStart.x, imageStart.y, imageSize.x, imageSize.y);
		}

		// マウスピッキング
		if (!ImGuizmo::IsUsing() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ImVec2 mousePos = ImGui::GetMousePos();
			float x = mousePos.x - imageMin.x;
			float y = mousePos.y - imageMin.y;

			if (x >= 0 && x < imageSize.x && y >= 0 && y < imageSize.y)
			{
				float halfW = imageSize.x * 0.5f;
				float halfH = imageSize.y * 0.5f;
				float uiMouseX = x - halfW;
				float uiMouseY = halfH - y;

				float scaleX = (float)Config::SCREEN_WIDTH / imageSize.x;
				float scaleY = (float)Config::SCREEN_HEIGHT / imageSize.y;
				uiMouseX *= scaleX;
				uiMouseY *= scaleY;

				bool uiHit = false;
				Entity uiHitEntity = NullEntity;

				std::vector<Entity> uiEntities;
				activeWorld->getRegistry().view<Transform2D>().each([&](Entity e, Transform2D& t) {
					uiEntities.push_back(e);
					});

				for (auto it = uiEntities.rbegin(); it != uiEntities.rend(); ++it)
				{
					Entity e = *it;
					if (activeWorld->getRegistry().has<Canvas>(e)) continue;
					auto& t = activeWorld->getRegistry().get<Transform2D>(e);
					auto& r = t.calculatedRect;

					if (uiMouseX >= r.x && uiMouseX <= r.z &&
						uiMouseY <= r.y && uiMouseY >= r.w)
					{
						uiHitEntity = e;
						uiHit = true;
						break;
					}
				}

				// ヒットしたエンティティ
				Entity hitResult = NullEntity;

				if (uiHit)
				{
					hitResult = uiHitEntity;
				}
				else
				{
					XMVECTOR rayOrigin = m_camera.GetPosition();
					XMVECTOR rayTarget = XMVector3Unproject(
						XMVectorSet(x, y, 1.0f, 0.0f),
						0, 0, imageSize.x, imageSize.y, 0.0f, 1.0f,
						projMat, viewMat, XMMatrixIdentity()
					);

					XMVECTOR rayDir = XMVector3Normalize(rayTarget - rayOrigin);
					XMFLOAT3 origin, dir;
					XMStoreFloat3(&origin, rayOrigin);
					XMStoreFloat3(&dir, rayDir);

					float dist = 0.0f;
					Entity hit = CollisionSystem::Raycast(activeWorld->getRegistry(), origin, dir, dist);

					if (hit != NullEntity) {
						hitResult = hit;
					}
				}

				bool ctrlPressed = ImGui::GetIO().KeyCtrl;

				if (hitResult != NullEntity)
				{
					if (ctrlPressed)
					{
						// Ctrlキーが押されている場合：選択トグル
						auto it = std::find(selection.begin(), selection.end(), hitResult);
						if (it != selection.end())
						{
							Editor::Instance().RemoveFromSelection(hitResult);
						}
						else
						{
							Editor::Instance().AddToSelection(hitResult);
						}
					}
					else
					{
						// Ctrlキーがない場合：単一選択
						// すでに選択済みなら何もしない（ドラッグ移動のため）
						if (selection.size() != 1 || selection[0] != hitResult)
						{
							Editor::Instance().SetSelectedEntity(hitResult);
						}
					}
				}
				else
				{
					// 何もクリックしなかった場合：選択解除（Ctrlなし時）
					if (!ctrlPressed && !ImGuizmo::IsOver())
					{
						Editor::Instance().ClearSelection();
					}
				}
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void SceneViewPanel::FocusEntity(Entity entity, World& world)
	{
		if (entity == NullEntity) return;
		auto& reg = world.getRegistry();

		if (reg.valid(entity))
		{
			if (reg.has<Transform>(entity))
			{
				const auto& t = reg.get<Transform>(entity);
				m_camera.Focus(t.position);
			}
		}
	}
}
#endif