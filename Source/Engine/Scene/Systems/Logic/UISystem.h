/*****************************************************************//**
 * @file	UISystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date	2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___UI_SYSTEM_H___
#define ___UI_SYSTEM_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"

namespace Arche
{

	class UISystem : public ISystem
	{
	public:
		UISystem() { m_systemName = "UI System"; }

		void Update(Registry& registry) override
		{
			// 1. 画面サイズ（Canvas）の取得
			float screenW = 1920.0f;
			float screenH = 1080.0f;

			auto canvasView = registry.view<Canvas>();
			for (auto e : canvasView) {
				const auto& canvas = canvasView.get<Canvas>(e);
				screenW = canvas.referenceSize.x;
				screenH = canvas.referenceSize.y;
				break;
			}

			// 2. 再帰計算用の関数
			std::function<void(Entity, const D2D1_RECT_F&, const D2D1_MATRIX_3X2_F&)> updateNode =
				[&](Entity entity, const D2D1_RECT_F& parentRect, const D2D1_MATRIX_3X2_F& parentMat)
				{
					if (!registry.has<Transform2D>(entity)) return;
					auto& t = registry.get<Transform2D>(entity);

					// --------------------------------------------------------
					// A. 位置の計算（アンカー対応）
					// --------------------------------------------------------
					float parentW = parentRect.right - parentRect.left;
					float parentH = parentRect.bottom - parentRect.top;

					// アンカー基準位置 (親の矩形内での相対位置)
					float anchorX = parentRect.left + (parentW * (t.anchorMin.x + t.anchorMax.x) * 0.5f);
					float anchorY = parentRect.top + (parentH * (t.anchorMin.y + t.anchorMax.y) * 0.5f);

					// 最終的なローカル座標 (親空間での位置)
					// アンカー位置 + オフセット(position)
					float localX = anchorX + t.position.x;
					float localY = anchorY + t.position.y;

					// --------------------------------------------------------
					// B. 行列計算 (Local -> World)
					// --------------------------------------------------------

					// 1. スケール
					D2D1_MATRIX_3X2_F matScale = D2D1::Matrix3x2F::Scale(t.scale.x, t.scale.y);

					// 2. 回転 (Pivot補正済みなので、原点中心の回転でOK)
					D2D1_MATRIX_3X2_F matRot = D2D1::Matrix3x2F::Rotation(t.rotation.z);

					// 3. 移動 (計算したローカル座標へ)
					D2D1_MATRIX_3X2_F matTrans = D2D1::Matrix3x2F::Translation(localX, localY);

					// 合成: [Pivot] -> [Scale] -> [Rot] -> [Trans]
					D2D1_MATRIX_3X2_F localMat = matScale * matRot * matTrans;

					// 親行列と合成してワールド行列決定
					t.worldMatrix = localMat * parentMat;

					// --------------------------------------------------------
					// C. 矩形計算 (World Space AABB) - ピッキング等に使用
					// --------------------------------------------------------
					// 行列適用後の4頂点からAABBを求める
					// 元の矩形 (0,0) ～ (SizeX, SizeY) ※Pivot補正前のローカル空間
					// ではなく、Pivot補正行列の逆を考えないといけないが、
					// ここでは単純に「ワールド行列で変換された位置」を中心に矩形を作る簡易計算、
					// もしくは正しく4頂点を変換する。

					// 簡易版: 行列の平行移動成分を「Pivot位置」として使う
					// worldMatrixのdx, dyは「Pivot点」のワールド座標になっている
					float worldPivotX = t.worldMatrix.dx;
					float worldPivotY = t.worldMatrix.dy;

					// 回転を考慮しないAABB (簡易表示用)
					// サイズはスケール込みにするかどうかは仕様によるが、通常はTransform2DのSizeを使う
					// 実際は回転するとAABBは広がるが、ここでは軸並行として計算
					float l = worldPivotX - (t.size.x * t.pivot.x) * t.scale.x; // スケール考慮
					float r = l + (t.size.x * t.scale.x);
					float b = worldPivotY - (t.size.y * t.pivot.y) * t.scale.y;
					float tp = b + (t.size.y * t.scale.y);

					// Y-Up / Y-Down の整合性に注意。
					// 以前のコードに合わせて Rect { left, top, right, bottom } とする
					t.calculatedRect = { l, tp, r, b };

					// --------------------------------------------------------
					// D. 子へ再帰
					// --------------------------------------------------------
					if (registry.has<Relationship>(entity))
					{
						float myL = -t.size.x * t.pivot.x;
						float myR = myL + t.size.x;
						float myB = -t.size.y * t.pivot.y;
						float myT = myB + t.size.y;

						D2D1_RECT_F myLocalRect = { myL, myT, myR, myB };

						for (Entity child : registry.get<Relationship>(entity).children)
						{
							updateNode(child, myLocalRect, t.worldMatrix);
						}
					}
				};

			// もし座標系を完全に中心基準にするなら以下のように設定
			float halfW = screenW * 0.5f;
			float halfH = screenH * 0.5f;
			// Y-Up (Top > Bottom)
			D2D1_RECT_F rootRect = { -halfW, halfH, halfW, -halfH };

			D2D1_MATRIX_3X2_F identity = D2D1::IdentityMatrix();

			bool canvasFound = false;
			for (auto e : canvasView)
			{
				// Canvas自体は親Rectの中央に配置、あるいは(0,0)に配置
				// Canvasコンポーネントを持つエンティティ自体もTransform2Dを持っているので、
				// そのPositionが(0,0)なら画面中央になる
				updateNode(e, rootRect, identity);
				canvasFound = true;
			}

			// Canvas外のルート要素
			if (!canvasFound) {
				registry.view<Transform2D>().each([&](Entity e, Transform2D& t)
				{
					bool isRoot = true;
					if (registry.has<Relationship>(e)) {
						Entity p = registry.get<Relationship>(e).parent;
						if (p != NullEntity && registry.has<Transform2D>(p)) isRoot = false;
					}
					if (isRoot && !registry.has<Canvas>(e)) {
						updateNode(e, rootRect, identity);
					}
				});
			}
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::UISystem, "UI System")

#endif // !___UI_SYSTEM_H___