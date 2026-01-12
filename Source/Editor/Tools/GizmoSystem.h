/*****************************************************************//**
 * @file	GizmoSystem.h
 * @brief	ギズモの描画ロジック（2D / 3D）
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___GIZMO_SYSTEM_H___
#define ___GIZMO_SYSTEM_H___

#ifdef _DEBUG

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Core/Context.h"
#include "Engine/Config.h"

// コマンド関連のインクルード
#include "Editor/Core/CommandHistory.h"
#include "Editor/Core/EditorCommands.h"
#include "Engine/Scene/Serializer/ComponentRegistry.h"

namespace Arche
{

	class GizmoSystem
	{
	public:
		static void Draw(World& world, Entity& selected, const XMMATRIX& view, const XMMATRIX& proj, float x, float y, float width, float height)
		{
			if (selected == NullEntity) return;

			Registry& reg = world.getRegistry();

			// スナップ設定
			bool useGridSnap = Input::GetKey(VK_CONTROL);	// Ctrlでグリッドスナップ
			bool useEntitySnap = Input::GetKey(VK_SHIFT);	// Shiftでエンティティ吸着

			// スナップ値
			float snapTranslate[3] = { 0.5f, 0.5f, 0.5f };	// 0.5m単位
			float snapRotate[3] = { 45.0f, 45.0f, 45.0f };	// 45度単位
			float snapScale[3] = { 0.1f, 0.1f, 0.1f };		// 0.1倍単位

			float* snapValues = nullptr;

			// 操作モード制御
			static ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
			if (Input::GetKeyDown('1')) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
			if (Input::GetKeyDown('2')) mCurrentGizmoOperation = ImGuizmo::ROTATE;
			if (Input::GetKeyDown('3')) mCurrentGizmoOperation = ImGuizmo::SCALE;

			// グリッドスナップの値選択
			if (useGridSnap)
			{
				if (mCurrentGizmoOperation == ImGuizmo::TRANSLATE) snapValues = snapTranslate;
				else if (mCurrentGizmoOperation == ImGuizmo::ROTATE) snapValues = snapRotate;
				else if (mCurrentGizmoOperation == ImGuizmo::SCALE) snapValues = snapScale;
			}

			// =================================================================================
			// 1. Transform2D (UI / 2D) の場合
			// =================================================================================
			if (reg.has<Transform2D>(selected))
			{
				ImGuizmo::SetOrthographic(true); // 2Dモード
				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(x, y, width, height);

				// --------------------------------------------------------
				// カメラ設定 (Z=-10から原点を見る)
				// --------------------------------------------------------
				XMVECTOR eye	= XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f);
				XMVECTOR at		= XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
				XMVECTOR up		= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				XMMATRIX view2D = XMMatrixLookAtLH(eye, at, up);

				float screenW = (float)Config::SCREEN_WIDTH;
				float screenH = (float)Config::SCREEN_HEIGHT;
				XMMATRIX proj2D = XMMatrixOrthographicLH(screenW, screenH, 0.1f, 1000.0f);

				float viewM[16], projM[16], worldM[16];
				MatrixToFloat16(view2D, viewM);
				MatrixToFloat16(proj2D, projM);

				auto& t2 = reg.get<Transform2D>(selected);

				// --------------------------------------------------------
				// ギズモに渡す行列の作成 (D2D WorldMatrix -> DirectX 4x4 Matrix)
				// --------------------------------------------------------
				auto& m = t2.worldMatrix;
				XMMATRIX matWorld = XMMatrixSet(
					m._11, m._12, 0.0f, 0.0f,
					m._21, m._22, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					m._31, m._32, 0.0f, 1.0f
				);
				MatrixToFloat16(matWorld, worldM);

				// --------------------------------------------------------
				// ギズモ操作実行
				// --------------------------------------------------------
				if (ImGuizmo::Manipulate(viewM, projM, mCurrentGizmoOperation, ImGuizmo::WORLD, worldM))
				{
					// 操作後の新しいワールド行列
					XMMATRIX newWorldMat = XMLoadFloat4x4((XMFLOAT4X4*)worldM);

					// ----------------------------------------------------
					// 親の行列を考慮して「ローカル座標」に戻す
					// ----------------------------------------------------
					XMMATRIX parentMat = XMMatrixIdentity();
					float anchorX = 0.0f;
					float anchorY = 0.0f;

					// 親矩形のデフォルト
					float halfW = screenW * 0.5f;
					float halfH = screenH * 0.5f;
					D2D1_RECT_F parentRect = { -halfW, halfH, halfW, -halfH };

					// 親がいるか確認
					if (reg.has<Relationship>(selected))
					{
						Entity parent = reg.get<Relationship>(selected).parent;
						if (parent != NullEntity && reg.has<Transform2D>(parent))
						{
							// 親のワールド行列を取得して 4x4 に変換
							auto& pm = reg.get<Transform2D>(parent).worldMatrix;
							parentMat = XMMatrixSet(
								pm._11, pm._12, 0.0f, 0.0f,
								pm._21, pm._22, 0.0f, 0.0f,
								0.0f, 0.0f, 1.0f, 0.0f,
								pm._31, pm._32, 0.0f, 1.0f
							);

							// 親のローカル矩形を計算 (UISystemと同じロジック)
							auto& pt = reg.get<Transform2D>(parent);
							float pl = -pt.size.x * pt.pivot.x;
							float pr = pl + pt.size.x;
							float pb = -pt.size.y * pt.pivot.y;
							float pt_top = pb + pt.size.y;
							parentRect = { pl, pt_top, pr, pb };
						}
					}

					// アンカー位置計算
					float pW = parentRect.right - parentRect.left;
					float pH = parentRect.bottom - parentRect.top;
					anchorX = parentRect.left + (pW * (t2.anchorMin.x + t2.anchorMax.x) * 0.5f);
					anchorY = parentRect.top + (pH * (t2.anchorMin.y + t2.anchorMax.y) * 0.5f);

					// --------------------------------------------------------
					// 変換: World -> Local -> Position
					// --------------------------------------------------------
					// 親の逆行列で「ローカル座標（アンカー込み）」に変換
					DirectX::XMVECTOR det;
					DirectX::XMMATRIX invParent = DirectX::XMMatrixInverse(&det, parentMat);
					DirectX::XMMATRIX localMat = newWorldMat * invParent;

					float localM[16];
					MatrixToFloat16(localMat, localM);

					float translation[3], rotation[3], scale[3];
					ImGuizmo::DecomposeMatrixToComponents(localM, translation, rotation, scale);

					// 分解された translation は「アンカー + position」なので、アンカーを引く
					t2.position.x = translation[0] - anchorX;
					t2.position.y = translation[1] - anchorY;

					t2.rotation.z = rotation[2];
					t2.scale.x = scale[0];
					t2.scale.y = scale[1];
				}
			}
			// 2. Transform（3D）の場合
			else if (reg.has<Transform>(selected))
			{
				ImGuizmo::SetOrthographic(false);	// 3Dモード（透視投影）
				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(x, y, width, height);

				// 行列をfloat配列に変換 (Transpose含む)
				float viewM[16], projM[16], worldM[16];
				MatrixToFloat16(view, viewM);
				MatrixToFloat16(proj, projM);

				// 対象のTransformを取得
				Transform& t = reg.get<Transform>(selected);
				MatrixToFloat16(t.GetWorldMatrix(), worldM);

				// ギズモ描画と操作判定
				ImGuizmo::Manipulate(viewM, projM, mCurrentGizmoOperation, ImGuizmo::LOCAL, worldM, nullptr, snapValues);

				// Undo / Redo
				bool isUsingNow = ImGuizmo::IsUsing();

				// 1. 操作開始
				if (isUsingNow && !s_isUsing)
				{
					// 現在の値をシリアライズして保存
					ComponentRegistry::Instance().GetInterfaces().at("Transform")
						.serialize(reg, selected, s_startValue);
				}
				
				// 2. 操作中
				if (isUsingNow)
				{
					// 値を分解して適用
					float translation[3], rotation[3], scale[3];
					ImGuizmo::DecomposeMatrixToComponents(worldM, translation, rotation, scale);


					// エンティティ吸着処理
					if (mCurrentGizmoOperation == ImGuizmo::TRANSLATE && useEntitySnap)
					{
						XMFLOAT3 currentPos = { translation[0], translation[1], translation[2] };
						XMFLOAT3 snappedPos = currentPos;
						float minDistSq = 1.0f; // 吸着距離の閾値（の2乗）
						bool snapped = false;

						// 全エンティティ走査
						auto viewT = reg.view<Transform>();
						for (auto otherEntity : viewT)
						{
							if (otherEntity == selected) continue; // 自分自身は無視

							auto& otherT = viewT.get<Transform>(otherEntity);

							// 距離計算
							float dx = currentPos.x - otherT.position.x;
							float dy = currentPos.y - otherT.position.y;
							float dz = currentPos.z - otherT.position.z;
							float distSq = dx * dx + dy * dy + dz * dz;

							if (distSq < minDistSq)
							{
								minDistSq = distSq;
								snappedPos = otherT.position;
								snapped = true;
							}
						}

						if (snapped)
						{
							// 吸着適用
							translation[0] = snappedPos.x;
							translation[1] = snappedPos.y;
							translation[2] = snappedPos.z;

							// 吸着した場所にガイド線を表示（視覚フィードバック）
							// ワールド座標 -> スクリーン座標に変換してImGuiで線を描く
							ImVec2 p1 = WorldToScreen(currentPos, view, proj, x, y, width, height);
							ImVec2 p2 = WorldToScreen(snappedPos, view, proj, x, y, width, height);

							// 黄色い線を引く
							ImGui::GetWindowDrawList()->AddLine(p1, p2, IM_COL32(255, 255, 0, 255), 2.0f);
							// 吸着点に丸を表示
							ImGui::GetWindowDrawList()->AddCircleFilled(p2, 5.0f, IM_COL32(255, 255, 0, 255));
						}
					}

					t.position = { translation[0], translation[1], translation[2] };
					t.rotation = { rotation[0], rotation[1], rotation[2] };
					t.scale = { scale[0], scale[1], scale[2] };
					
					XMMATRIX localMat = t.GetLocalMatrix();
					XMMATRIX parentMat = XMMatrixIdentity();

					if (reg.has<Relationship>(selected))
					{
						Entity parent = reg.get<Relationship>(selected).parent;
						if (parent != NullEntity && reg.has<Transform>(parent))
						{
							parentMat = reg.get<Transform>(parent).GetWorldMatrix();
						}
					}

					// ワールド行列を更新
					XMStoreFloat4x4(&t.worldMatrix, localMat * parentMat);
				}

				// 3. 操作終了
				if (!isUsingNow && s_isUsing)
				{
					json endValue;
					ComponentRegistry::Instance().GetInterfaces().at("Transform")
						.serialize(reg, selected, endValue);

					// 値が変わっていればコマンドスタックに積む
					if (s_startValue != endValue)
					{
						CommandHistory::Execute(std::make_shared<ChangeComponentValueCommand>(
							world, selected, "Transform", s_startValue, endValue
						));
					}
				}

				// 状態更新
				s_isUsing = isUsingNow;
			}
		}

	private:
		// ヘルパー: XMMATRIX -> float[16]
		static void MatrixToFloat16(const XMMATRIX& m, float* out) {
			XMStoreFloat4x4((XMFLOAT4X4*)out, m);
		}

		// ワールド座標 -> スクリーン座標変換（可視化用）
		static ImVec2 WorldToScreen(const XMFLOAT3& worldPos, const XMMATRIX& view, const XMMATRIX& proj, float viewportX, float viewportY, float viewportW, float viewportH)
		{
			XMVECTOR pos = XMLoadFloat3(&worldPos);
			XMVECTOR screenPos = XMVector3Project(
				pos,
				viewportX, viewportY, viewportW, viewportH,
				0.0f, 1.0f,
				proj, view, XMMatrixIdentity()
			);

			XMFLOAT3 s;
			XMStoreFloat3(&s, screenPos);
			return ImVec2(s.x, s.y);
		}

	private:
		// 状態保存用
		static inline bool s_isUsing = false;
		static inline json s_startValue;	// 操作開始時の値
	};

}	// namespace Arche

#endif // _DEBUG

#endif // !___GIZMO_SYSTEM_H___