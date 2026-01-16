#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Core/GameSession.h" // データ共有用

namespace Arche
{
	// --------------------------------------------------------------------------
	// ステージデータ定義
	// --------------------------------------------------------------------------
	struct StageInfo
	{
		int id;
		std::string name;
		std::string description;
		std::string enemyType; // UI表示用
		GeoShape iconShape;	   // ステージアイコン
		GeoShape enemyShape;   // 敵プレビュー形状
		int difficulty;		   // 難易度(星の数)
	};

	// --------------------------------------------------------------------------
	// 演出用コンポーネント管理
	// --------------------------------------------------------------------------
	struct StageNode
	{
		Entity root;	   // 回転軸
		Entity visual;	   // ステージアイコン
		Entity ring;	   // 選択リング
		Entity lockedOverlay; // ロック時の黒いカバー
		Entity label;	   // 頭上ラベル

		// 敵プレビュー（ホログラム）
		Entity enemyPreviewRoot;
		Entity enemyBody;
		std::vector<Entity> enemyParts; // 装飾パーツ

		// ステージデータ
		StageInfo info;
		XMFLOAT3 basePos;
		bool isLocked;
	};

	struct SelectContext
	{
		bool isInitialized = false;

		// 状態
		int currentIndex = 0;
		bool isSelected = false;
		float timer = 0.0f;

		// 解放状況 (デバッグ用: 本来はGameSessionから取得)
		int maxClearedStage = 0; // 0ならStage1のみ解放

		Entity camera = NullEntity;

		// オブジェクト
		std::vector<StageNode> nodes;
		Entity centerTable = NullEntity; // 中央の台座

		// UI
		Entity uiPanelBg = NullEntity;
		Entity uiTitle = NullEntity;
		Entity uiDesc = NullEntity;
		Entity uiStatus = NullEntity;

		// 背景
		struct CyberLine { Entity e; float z; float speed; };
		std::vector<CyberLine> bgLines;
		Entity floorGrid = NullEntity;
	};

	// --------------------------------------------------------------------------
	// ステージセレクトシステム "Tactical Hologram"
	// --------------------------------------------------------------------------
	class StageSelectSystem : public ISystem
	{
	public:
		StageSelectSystem()
		{
			m_systemName = "StageSelectSystem";
			m_group = SystemGroup::PlayOnly;
		}

		void Update(Registry& registry) override
		{
			static SelectContext ctx;

			if (!ctx.isInitialized) {
				// セッションからクリア状況を取得（なければ初期値）
				// ※GameSession側に int clearedStage があると仮定
				// ctx.maxClearedStage = GameSession::clearedStage; 
				ctx.maxClearedStage = 4; // ★デバッグ: 全開放 (本番は0にする)

				InitScene(registry, ctx);
				ctx.isInitialized = true;
			}

			float dt = Time::DeltaTime();
			float time = Time::TotalTime();

			if (ctx.isSelected) ctx.timer += dt;

			// =============================================================
			// 1. 入力処理 (指定された形式)
			// =============================================================
			if (!ctx.isSelected)
			{
				if (Input::GetButtonDown(Button::Right) || Input::GetKeyDown('D'))
				{
					ctx.currentIndex = (ctx.currentIndex + 1) % 5;
				}
				else if (Input::GetButtonDown(Button::Left) || Input::GetKeyDown('A'))
				{
					ctx.currentIndex = (ctx.currentIndex - 1 + 5) % 5;
				}
				else if (Input::GetButtonDown(Button::A) || Input::GetKeyDown(VK_SPACE))
				{
					// ロックされているステージは選択不可
					if (!ctx.nodes[ctx.currentIndex].isLocked)
					{
						ctx.isSelected = true;
						ctx.timer = 0.0f;
					}
				}
			}

			// =============================================================
			// 2. カメラ制御 (ダイナミック・パン)
			// =============================================================
			if (registry.valid(ctx.camera))
			{
				auto& tCam = registry.get<Transform>(ctx.camera);

				// 現在選択中のステージ座標
				XMFLOAT3 targetPos = ctx.nodes[ctx.currentIndex].basePos;

				// ターゲットへの角度
				float angle = atan2f(targetPos.x, targetPos.z);

				// カメラ位置計算 (ターゲットの手前)
				// ロック中は少し遠く、解放済みは近く
				bool isLocked = ctx.nodes[ctx.currentIndex].isLocked;
				float dist = isLocked ? 9.0f : 7.0f;
				float height = isLocked ? 4.0f : 2.5f;

				// 決定時のズーム
				if (ctx.isSelected) {
					dist = std::lerp(dist, 2.0f, ctx.timer * 0.8f); // 急接近
					height = std::lerp(height, targetPos.y, ctx.timer * 0.8f);
				}

				float camX = sinf(angle) * dist;
				float camZ = cosf(angle) * dist;

				// Lerpで移動
				tCam.position.x = std::lerp(tCam.position.x, camX, dt * 5.0f);
				tCam.position.y = std::lerp(tCam.position.y, height, dt * 5.0f);
				tCam.position.z = std::lerp(tCam.position.z, camZ, dt * 5.0f);

				// LookAt (ラジアン計算)
				float dx = targetPos.x - tCam.position.x;
				float dy = targetPos.y - tCam.position.y;
				float dz = targetPos.z - tCam.position.z;
				float xzDist = sqrtf(dx * dx + dz * dz);

				tCam.rotation.x = atan2f(-dy, xzDist); // Pitch
				tCam.rotation.y = atan2f(dx, dz);	   // Yaw
				tCam.rotation.z = 0.0f;
			}

			// =============================================================
			// 3. ステージノード & ホログラム更新
			// =============================================================
			for (int i = 0; i < 5; ++i)
			{
				auto& node = ctx.nodes[i];
				bool isTarget = (i == ctx.currentIndex);
				bool locked = node.isLocked;

				// --- A. ステージアイコン (台座) ---
				if (registry.valid(node.visual))
				{
					auto& t = registry.get<Transform>(node.visual);
					auto& geo = registry.get<GeometricDesign>(node.visual);

					// 選択中は高速回転、非選択はゆっくり
					float rotSpeed = isTarget ? 2.0f : 0.5f;
					if (ctx.isSelected && isTarget) rotSpeed = 20.0f;
					t.rotation.y += dt * rotSpeed;

					// 浮遊アニメーション
					t.position.y = node.basePos.y + sinf(time * 2.0f + i) * 0.1f;

					// 色制御
					if (locked) {
						geo.color = { 0.1f, 0.1f, 0.1f, 0.8f }; // 黒
						geo.isWireframe = true;
					}
					else {
						// 解放済み: Stage 5は赤、他はシアン
						XMFLOAT4 baseCol = (node.info.id == 5) ? XMFLOAT4{ 1, 0, 0.2f, 1 } : XMFLOAT4{ 0, 1, 1, 1 };
						if (!isTarget) baseCol = { baseCol.x, baseCol.y, baseCol.z, 0.3f }; // 非選択は薄く
						if (ctx.isSelected && isTarget) baseCol = { 1, 1, 1, 1 }; // 出撃時は白
						geo.color = baseCol;
						geo.isWireframe = true;
					}
				}

				// --- B. 敵プレビュー (選択中のみホログラム投影) ---
				if (registry.valid(node.enemyPreviewRoot))
				{
					auto& tRoot = registry.get<Transform>(node.enemyPreviewRoot);

					// ターゲットかつ非ロック時のみ表示
					float targetScale = (isTarget && !locked) ? 1.0f : 0.0f;
					float curS = tRoot.scale.x;
					float newS = std::lerp(curS, targetScale, dt * 10.0f);
					tRoot.scale = { newS, newS, newS };

					if (newS > 0.01f)
					{
						// ホログラム特有のノイズ回転
						tRoot.rotation.y -= dt * 1.5f;
						tRoot.position.y = node.basePos.y + 1.2f + sinf(time * 5.0f) * 0.05f; // ビビリ振動

						// パーツのアニメーション
						for (auto& part : node.enemyParts) {
							if (registry.valid(part)) {
								registry.get<Transform>(part).rotation.z += dt * 3.0f;
								registry.get<Transform>(part).rotation.x += dt * 2.0f;
								// ホログラム色
								registry.get<GeometricDesign>(part).color =
									(node.info.id == 5) ? XMFLOAT4{ 1,0,0, 0.6f } : XMFLOAT4{ 0,1,0, 0.6f };
							}
						}
					}
				}

				// --- C. 選択リング ---
				if (registry.valid(node.ring))
				{
					auto& t = registry.get<Transform>(node.ring);
					float s = isTarget ? 3.0f : 0.0f;
					float cur = t.scale.x;
					t.scale = { std::lerp(cur, s, dt * 15.0f), std::lerp(cur, s, dt * 15.0f), 0.1f };
					t.rotation.z += dt * 5.0f; // 回転

					auto& geo = registry.get<GeometricDesign>(node.ring);
					geo.color = locked ? XMFLOAT4{ 1,0,0,0.5f } : XMFLOAT4{ 0,1,1,0.5f };
				}

				// --- D. ラベル ---
				if (registry.valid(node.label))
				{
					auto& t = registry.get<Transform>(node.label);
					// カメラの方を向く
					if (registry.valid(ctx.camera)) t.rotation = registry.get<Transform>(ctx.camera).rotation;

					auto& txt = registry.get<TextComponent>(node.label);
					if (locked) {
						txt.text = "LOCKED";
						txt.color = { 0.5f, 0.5f, 0.5f, (isTarget ? 1.0f : 0.0f) };
					}
					else {
						txt.text = node.info.name;
						txt.color = { 1.0f, 1.0f, 1.0f, (isTarget ? 1.0f : 0.0f) };
					}
				}
			}

			// =============================================================
			// 4. UIパネル (画面手前に固定)
			// =============================================================
			if (registry.valid(ctx.uiTitle) && registry.valid(ctx.camera))
			{
				// カメラ座標系を取得
				auto& camT = registry.get<Transform>(ctx.camera);
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, 0);
				XMVECTOR fwd = XMVector3TransformCoord({ 0,0,1 }, rot);
				XMVECTOR right = XMVector3TransformCoord({ 1,0,0 }, rot);
				XMVECTOR up = XMVector3TransformCoord({ 0,1,0 }, rot);
				XMVECTOR camPos = XMLoadFloat3(&camT.position);

				// パネル位置: 右下、手前
				XMVECTOR basePos = camPos + fwd * 3.0f + right * 1.2f + up * -0.8f;

				auto UpdateUI = [&](Entity e, float yOff, float scale) {
					if (!registry.valid(e)) return;
					auto& t = registry.get<Transform>(e);
					XMVECTOR p = basePos + up * yOff;
					XMStoreFloat3(&t.position, p);
					t.rotation = camT.rotation;
					t.scale = { scale, scale, scale };
					};

				// 背景パネル
				UpdateUI(ctx.uiPanelBg, 0.0f, 1.0f);
				registry.get<Transform>(ctx.uiPanelBg).scale = { 1.8f, 1.2f, 0.01f }; // 板にする

				// テキスト更新
				StageNode& current = ctx.nodes[ctx.currentIndex];
				bool locked = current.isLocked;

				// タイトル
				UpdateUI(ctx.uiTitle, 0.3f, 0.5f);
				auto& txtTitle = registry.get<TextComponent>(ctx.uiTitle);
				txtTitle.text = locked ? "ACCESS DENIED" : current.info.name;
				txtTitle.color = locked ? XMFLOAT4{ 1,0,0,1 } : XMFLOAT4{ 0,1,1,1 };

				// 説明
				UpdateUI(ctx.uiDesc, 0.0f, 0.3f);
				auto& txtDesc = registry.get<TextComponent>(ctx.uiDesc);
				txtDesc.text = locked ? "Clear previous sector\nto unlock data." : current.info.description;

				// ステータス (難易度など)
				UpdateUI(ctx.uiStatus, -0.3f, 0.3f);
				auto& txtStat = registry.get<TextComponent>(ctx.uiStatus);
				if (locked) {
					txtStat.text = "STATUS: LOCKED";
				}
				else {
					std::string diff = "";
					for (int i = 0; i < current.info.difficulty; ++i) diff += "*";
					txtStat.text = "THREAT LEVEL: " + diff + "\nUNIT: " + current.info.enemyType;
				}

				// 出撃時はUIを消す
				float uiAlpha = ctx.isSelected ? 0.0f : 1.0f;
				txtTitle.color.w = uiAlpha;
				txtDesc.color.w = uiAlpha;
				txtStat.color.w = uiAlpha;
				registry.get<GeometricDesign>(ctx.uiPanelBg).color.w = uiAlpha * 0.5f;
			}

			// =============================================================
			// 5. 背景演出 (データストリーム)
			// =============================================================
			for (auto& line : ctx.bgLines)
			{
				if (registry.valid(line.e))
				{
					auto& t = registry.get<Transform>(line.e);
					line.z -= dt * line.speed * (ctx.isSelected ? 20.0f : 1.0f);
					if (line.z < -20.0f) line.z = 40.0f;
					t.position.z = line.z;
					t.scale.z = 2.0f + (ctx.isSelected ? 5.0f : 0.0f); // 加速表現
				}
			}

			// =============================================================
			// 6. 遷移処理
			// =============================================================
			if (ctx.isSelected && ctx.timer > 1.2f)
			{
				// セッションにステージID保存
				GameSession::selectedStageId = ctx.nodes[ctx.currentIndex].info.id;

				ctx.isInitialized = false;
				ctx.nodes.clear();
				ctx.bgLines.clear();

				// インゲームへ
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/GameScene.json", new FadeTransition(0.5f, { 1,1,1,1 }));
			}
		}

	private:
		void InitScene(Registry& reg, SelectContext& ctx)
		{
			// 1. カメラ
			{
				bool f = false; for (auto e : reg.view<Camera>()) { ctx.camera = e; f = true; break; }
				if (!f) {
					ctx.camera = reg.create();
					reg.emplace<Camera>(ctx.camera);
					reg.emplace<Transform>(ctx.camera);
				}
			}

			// 2. 中央テーブル (プロジェクター)
			{
				ctx.centerTable = reg.create();
				reg.emplace<Transform>(ctx.centerTable).position = { 0, -2.0f, 0 };
				reg.get<Transform>(ctx.centerTable).scale = { 3.0f, 0.2f, 3.0f };
				auto& geo = reg.emplace<GeometricDesign>(ctx.centerTable);
				geo.shapeType = GeoShape::Cylinder;
				geo.isWireframe = true;
				geo.color = { 0, 0.5f, 1, 0.5f };
			}

			// 3. ステージノード生成ヘルパー
			auto CreateStage = [&](StageInfo info, float angle, float radius, float y) {
				StageNode node;
				node.info = info;
				node.basePos = { sinf(angle) * radius, y, cosf(angle) * radius };
				// アンロック判定: ステージ1は常に解放。それ以外はクリア状況による
				// maxClearedStage=0 なら stage1解放, 2ロック...
				node.isLocked = (info.id > ctx.maxClearedStage + 1);

				// A. Visual (地形アイコン)
				node.visual = reg.create();
				reg.emplace<Transform>(node.visual).position = node.basePos;
				auto& geo = reg.emplace<GeometricDesign>(node.visual);
				geo.shapeType = info.iconShape;
				geo.isWireframe = true;
				geo.color = { 0, 1, 1, 1 };

				// B. Ring (選択カーソル)
				node.ring = reg.create();
				reg.emplace<Transform>(node.ring).position = node.basePos;
				auto& geoR = reg.emplace<GeometricDesign>(node.ring);
				geoR.shapeType = GeoShape::Torus;
				geoR.isWireframe = true;
				geoR.color = { 0, 1, 1, 0 };

				// C. Enemy Preview (敵ホログラム) - 親子構造
				node.enemyPreviewRoot = reg.create();
				reg.emplace<Transform>(node.enemyPreviewRoot).position = node.basePos;
				reg.get<Transform>(node.enemyPreviewRoot).scale = { 0,0,0 };

				// 本体
				node.enemyBody = reg.create();
				reg.emplace<Transform>(node.enemyBody); // Rootの子にする想定だがECS構造上別管理
				// 親子付け省略（座標計算で追従させる簡易実装）

				// パーツ追加 (形状を組み合わせて複雑に見せる)
				auto AddPart = [&](GeoShape s, float sx, float sy, float sz) {
					Entity p = reg.create();
					reg.emplace<Transform>(p).scale = { sx, sy, sz };
					reg.get<Transform>(p).position = node.basePos; // Rootと同じ位置
					auto& g = reg.emplace<GeometricDesign>(p);
					g.shapeType = s;
					g.isWireframe = true;
					node.enemyParts.push_back(p);
					// note: UpdateでRootに合わせて動かす処理が必要
					};

				// 敵タイプ別パーツ構成
				if (info.id == 5) { // Boss
					AddPart(GeoShape::Sphere, 1.0f, 1.0f, 1.0f); // Core
					AddPart(GeoShape::Torus, 2.0f, 2.0f, 0.2f);	 // Ring
					AddPart(GeoShape::Torus, 1.5f, 1.5f, 0.2f);	 // Inner Ring
				}
				else if (info.id == 2) { // Armored
					AddPart(GeoShape::Cube, 0.8f, 0.8f, 0.8f);
					AddPart(GeoShape::Torus, 1.2f, 1.2f, 0.5f);
				}
				else { // Standard
					AddPart(info.enemyShape, 0.8f, 0.8f, 0.8f);
				}

				// D. Label
				node.label = reg.create();
				reg.emplace<Transform>(node.label).position = { node.basePos.x, node.basePos.y + 1.5f, node.basePos.z };
				auto& txt = reg.emplace<TextComponent>(node.label);
				txt.text = info.name;
				txt.fontKey = "Consolas"; // ★修正
				txt.fontSize = 20.0f;
				txt.centerAlign = true;
				txt.color = { 1,1,1,0 };

				ctx.nodes.push_back(node);
				};

			// --- ステージ定義 ---
			// 1. Basic
			CreateStage({ 1, "SECTOR-01", "Training Grounds.\nWeak resistance expected.", "CUBE_DROID", GeoShape::Cube, GeoShape::Cube, 1 },
				0.0f, 6.0f, 0.0f);

			// 2. Armored
			CreateStage({ 2, "SECTOR-02", "Heavy Industry Zone.\nArmored units detected.", "HEAVY_GUARD", GeoShape::Diamond, GeoShape::Cube, 2 },
				XM_PIDIV2, 6.0f, 0.0f);

			// 3. Speed
			CreateStage({ 3, "SECTOR-03", "Data Highway.\nFast moving targets.", "SPEEDER", GeoShape::Torus, GeoShape::Cylinder, 3 },
				XM_PI, 6.0f, 0.0f);

			// 4. Swarm
			CreateStage({ 4, "SECTOR-04", "Hive Cluster.\nMassive enemy count.", "SWARM_CORE", GeoShape::Sphere, GeoShape::Sphere, 4 },
				-XM_PIDIV2, 6.0f, 0.0f);

			// 5. BOSS (中央上空)
			CreateStage({ 5, "CORE SYSTEM", "Central Processor.\nELIMINATE TARGET.", "OMEGA_BOSS", GeoShape::Pyramid, GeoShape::Sphere, 5 },
				0.0f, 0.0f, 3.0f); // 半径0, 高さ3

			// 4. 背景 (デジタルレイン/ライン)
			for (int i = 0; i < 50; ++i) {
				Entity e = reg.create();
				float x = ((rand() % 600) / 10.0f) - 30.0f;
				float y = ((rand() % 400) / 10.0f) - 20.0f;
				float z = (float)(rand() % 60);

				reg.emplace<Transform>(e).position = { x, y, z };
				reg.get<Transform>(e).scale = { 0.05f, 0.05f, 2.0f }; // 長細い

				auto& geo = reg.emplace<GeometricDesign>(e);
				geo.shapeType = GeoShape::Cube;
				geo.color = { 0, 0.3f, 0.5f, 0.5f };

				SelectContext::CyberLine l; l.e = e; l.z = z; l.speed = 10.0f + rand() % 20;
				ctx.bgLines.push_back(l);
			}

			// 5. UIパネル生成
			auto CreateUI = [&](float fontSize, XMFLOAT4 col) {
				Entity e = reg.create();
				reg.emplace<Transform>(e);
				auto& t = reg.emplace<TextComponent>(e);
				t.fontKey = "Consolas";
				t.fontSize = fontSize;
				t.color = col;
				return e;
				};
			ctx.uiPanelBg = reg.create();
			reg.emplace<Transform>(ctx.uiPanelBg);
			reg.emplace<GeometricDesign>(ctx.uiPanelBg).shapeType = GeoShape::Cube;
			reg.get<GeometricDesign>(ctx.uiPanelBg).color = { 0,0,0,0.7f };

			ctx.uiTitle = CreateUI(30.0f, { 0,1,1,1 });
			ctx.uiDesc = CreateUI(20.0f, { 1,1,1,1 });
			ctx.uiStatus = CreateUI(18.0f, { 1,0.5f,0,1 });
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::StageSelectSystem, "StageSelectSystem")
