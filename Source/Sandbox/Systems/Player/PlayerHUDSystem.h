#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	// 初期化判定用タグ（これを追加して管理します）
	struct PlayerHUDTag {};

	struct ReticlePartData {
		Entity e;
		XMFLOAT3 localOffset;
		XMFLOAT3 localScale;
	};

	struct PlayerHUDContext
	{
		// メインタイマー
		Entity timerRoot = NullEntity;
		Entity timerText = NullEntity;
		Entity timerRing = NullEntity;
		Entity timerBarBg = NullEntity;
		Entity timerBarFill = NullEntity;

		// 照準（レティクル）
		Entity reticleRoot = NullEntity;
		std::vector<ReticlePartData> reticleParts;

		// スコア・ウェーブ
		Entity scoreText = NullEntity;
		Entity waveText = NullEntity;

		// 警告表示
		Entity dangerOverlay = NullEntity;

		float currentDisplayTime = 5.0f;
		float targetTime = 5.0f;
	};

	class PlayerHUDSystem : public ISystem
	{
	public:
		PlayerHUDSystem() { m_systemName = "PlayerHUDSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static PlayerHUDContext ctx;

			// --- 初期化チェック (タグ方式へ変更) ---
			// シーン内にタグが存在するか確認
			bool isInitialized = false;
			for (auto e : reg.view<PlayerHUDTag>()) { isInitialized = true; break; }

			// タグがなければ（＝シーン開始時）、データをリセットして初期化
			if (!isInitialized) {
				ctx = PlayerHUDContext(); // ★ここで古いIDを完全に消去
				InitHUD(reg, ctx);
				reg.emplace<PlayerHUDTag>(reg.create()); // 初期化済みタグを生成
			}

			// プレイヤー情報取得
			float pTime = 0.0f;
			float pMaxTime = 5.0f;
			XMFLOAT3 pPos = { 0,0,0 };

			bool playerFound = false;
			for (auto e : reg.view<PlayerTime, Transform>()) {
				auto& pt = reg.get<PlayerTime>(e);
				auto& t = reg.get<Transform>(e);
				pTime = pt.currentTime;
				pMaxTime = pt.maxTime;
				pPos = t.position;
				playerFound = true;
				break;
			}
			if (!playerFound) return;

			// カメラ取得
			Entity cam = NullEntity;
			for (auto e : reg.view<Camera>()) { cam = e; break; }
			if (cam == NullEntity) return;
			auto& camT = reg.get<Transform>(cam);

			float dt = Time::DeltaTime();
			float time = Time::TotalTime();

			// --- UI座標系セットアップ (カメラ前方に追従) ---
			auto GetHUDPos = [&](float x, float y, float z) -> XMFLOAT3 {
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, camT.rotation.z);
				XMVECTOR offset = XMVectorSet(x, y, z, 0);
				XMVECTOR pos = XMLoadFloat3(&camT.position) + XMVector3Transform(offset, rot);
				XMFLOAT3 res; XMStoreFloat3(&res, pos);
				return res;
				};

			// --- 1. タイマー更新 (画面中央上部) ---
			if (reg.valid(ctx.timerText)) {
				// 表示数値の慣性
				ctx.currentDisplayTime += (pTime - ctx.currentDisplayTime) * dt * 10.0f;

				auto& txt = reg.get<TextComponent>(ctx.timerText);
				char buf[32]; sprintf_s(buf, "%04.2f", std::max(0.0f, ctx.currentDisplayTime));
				txt.text = buf;

				// 危険色
				if (pTime < 2.0f) {
					txt.color = { 1, 0, 0, 1 };
					float shake = sinf(time * 50.0f) * 0.02f;
					// 位置更新
					XMFLOAT3 p = GetHUDPos(shake, 3.5f, 8.0f);
					reg.get<Transform>(ctx.timerText).position = p;
					reg.get<Transform>(ctx.timerText).rotation = camT.rotation;
				}
				else {
					txt.color = { 0, 1, 1, 1 };
					XMFLOAT3 p = GetHUDPos(0, 3.5f, 8.0f);
					reg.get<Transform>(ctx.timerText).position = p;
					reg.get<Transform>(ctx.timerText).rotation = camT.rotation;
				}
			}

			// タイマーバー（ゲージ）
			if (reg.valid(ctx.timerBarFill)) {
				float ratio = std::clamp(ctx.currentDisplayTime / pMaxTime, 0.0f, 1.0f);
				auto& t = reg.get<Transform>(ctx.timerBarFill);
				// バーの長さ制御
				t.scale.x = 4.0f * ratio;

				XMFLOAT3 p = GetHUDPos(0, 3.8f, 8.0f); // テキストの上
				reg.get<Transform>(ctx.timerBarFill).position = p;
				reg.get<Transform>(ctx.timerBarFill).rotation = camT.rotation;

				// 背景バーも同期
				if (reg.valid(ctx.timerBarBg)) {
					reg.get<Transform>(ctx.timerBarBg).position = GetHUDPos(0, 3.8f, 8.05f); // 少し奥
					reg.get<Transform>(ctx.timerBarBg).rotation = camT.rotation;
				}
			}

			// --- 2. レティクル (敵をロックオン) ---
			// 一番近い敵を探す
			Entity target = NullEntity;
			float minD = 999.0f;
			for (auto e : reg.view<EnemyStats, Transform>()) {
				auto& et = reg.get<Transform>(e);
				float d = sqrt(pow(et.position.x - pPos.x, 2) + pow(et.position.z - pPos.z, 2));
				if (d < minD) { minD = d; target = e; }
			}

			if (target != NullEntity && minD < 20.0f) {
				// ターゲット位置にレティクル移動
				auto& et = reg.get<Transform>(target);
				XMFLOAT3 targetPos = et.position;

				if (reg.valid(ctx.reticleRoot)) {
					auto& t = reg.get<Transform>(ctx.reticleRoot);
					// 補間移動
					t.position.x += (targetPos.x - t.position.x) * dt * 20.0f;
					t.position.y += (targetPos.y - t.position.y) * dt * 20.0f;
					t.position.z += (targetPos.z - t.position.z) * dt * 20.0f;
					t.rotation.z += dt * 5.0f; // 回転

					// カメラの方を向く
					t.rotation.x = camT.rotation.x;
					t.rotation.y = camT.rotation.y;

					// スケールアニメーション
					float s = 1.0f + sinf(time * 10.0f) * 0.1f;
					t.scale = { s, s, s };

					// 各パーツの更新 (手動親子関係)
					for (auto& part : ctx.reticleParts) {
						if (reg.valid(part.e)) {
							// ★念のためコンポーネントチェック
							if (reg.has<GeometricDesign>(part.e)) {
								reg.get<GeometricDesign>(part.e).color.w = 0.8f;
							}

							auto& pt = reg.get<Transform>(part.e);
							// 親の回転(Z軸)を考慮してオフセットを計算
							float rot = t.rotation.z;
							float c = cosf(rot); float s = sinf(rot);
							float ox = part.localOffset.x * c - part.localOffset.y * s;
							float oy = part.localOffset.x * s + part.localOffset.y * c;

							// 簡易的にワールド座標で加算
							XMMATRIX mRot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, 0);
							XMVECTOR vOffset = XMVectorSet(ox, oy, 0, 0); // Z回転適用済みのオフセット
							XMVECTOR vWorldOffset = XMVector3Transform(vOffset, mRot);

							XMFLOAT3 worldOffset; XMStoreFloat3(&worldOffset, vWorldOffset);
							pt.position = { t.position.x + worldOffset.x, t.position.y + worldOffset.y, t.position.z + worldOffset.z };
							pt.rotation = t.rotation;
							pt.scale = part.localScale;
						}
					}
				}
			}
			else {
				// ターゲットなし：プレイヤーの前方に薄く表示
				if (reg.valid(ctx.reticleRoot)) {
					XMMATRIX rot = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, camT.rotation.z);
					XMVECTOR fwd = XMVector3TransformCoord({ 0,0,1 }, rot);
					XMVECTOR pos = XMLoadFloat3(&camT.position) + fwd * 5.0f;
					XMFLOAT3 res; XMStoreFloat3(&res, pos);

					auto& t = reg.get<Transform>(ctx.reticleRoot);
					t.position = res;
					t.rotation = camT.rotation;

					for (auto& part : ctx.reticleParts) {
						if (reg.valid(part.e) && reg.has<GeometricDesign>(part.e)) {
							reg.get<GeometricDesign>(part.e).color.w = 0.1f;
							// 簡易配置
							XMMATRIX mRot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
							XMVECTOR vOffset = XMLoadFloat3(&part.localOffset);
							XMVECTOR vPos = XMLoadFloat3(&t.position) + XMVector3Transform(vOffset, mRot);
							XMStoreFloat3(&reg.get<Transform>(part.e).position, vPos);
							reg.get<Transform>(part.e).rotation = t.rotation;
						}
					}
				}
			}

			// --- 3. 瀕死エフェクト (Danger Overlay) ---
			if (reg.valid(ctx.dangerOverlay)) {
				auto& g = reg.get<GeometricDesign>(ctx.dangerOverlay);
				if (pTime < 1.5f) {
					float alpha = 0.3f + 0.3f * sinf(time * 10.0f);
					g.color = { 1, 0, 0, alpha };

					auto& t = reg.get<Transform>(ctx.dangerOverlay);
					XMFLOAT3 p = GetHUDPos(0, 0, 1.0f);
					t.position = p;
					t.rotation = camT.rotation;
				}
				else {
					g.color = { 0,0,0,0 };
				}
			}
		}

	private:
		void InitHUD(Registry& reg, PlayerHUDContext& ctx)
		{
			// --- タイマー ---
			ctx.timerText = reg.create();
			reg.emplace<Transform>(ctx.timerText);
			auto& txt = reg.emplace<TextComponent>(ctx.timerText);
			txt.fontKey = "Makinas 4 Square";
			txt.fontSize = 60.0f;
			txt.centerAlign = true;

			// タイマーバー
			ctx.timerBarFill = reg.create();
			reg.emplace<Transform>(ctx.timerBarFill).scale = { 4.0f, 0.15f, 0.01f };
			auto& gFill = reg.emplace<GeometricDesign>(ctx.timerBarFill);
			gFill.shapeType = GeoShape::Cube;
			gFill.color = { 0, 1, 1, 0.8f };

			ctx.timerBarBg = reg.create();
			reg.emplace<Transform>(ctx.timerBarBg).scale = { 4.1f, 0.2f, 0.01f };
			auto& gBg = reg.emplace<GeometricDesign>(ctx.timerBarBg);
			gBg.shapeType = GeoShape::Cube;
			gBg.color = { 0, 0, 0, 0.5f };

			// --- レティクル ---
			ctx.reticleRoot = reg.create();
			reg.emplace<Transform>(ctx.reticleRoot);

			// 4つのパーツで囲む
			for (int i = 0; i < 4; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e);

				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube; // バー
				g.color = { 1, 0.5f, 0, 0.8f };
				g.isWireframe = false;

				ReticlePartData part;
				part.e = e;
				float ang = i * XM_PIDIV2;
				float dist = 1.0f;
				// 上下左右に配置
				part.localOffset = { cosf(ang) * dist, sinf(ang) * dist, 0 };
				// 向きに合わせて細長く
				if (i % 2 == 0) part.localScale = { 0.1f, 0.5f, 0.05f };
				else part.localScale = { 0.5f, 0.1f, 0.05f };

				ctx.reticleParts.push_back(part);
			}

			// --- 瀕死オーバーレイ ---
			ctx.dangerOverlay = reg.create();
			reg.emplace<Transform>(ctx.dangerOverlay).scale = { 16.0f, 9.0f, 0.1f };
			auto& gDanger = reg.emplace<GeometricDesign>(ctx.dangerOverlay);
			gDanger.shapeType = GeoShape::Cube;
			gDanger.color = { 0,0,0,0 };
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerHUDSystem, "PlayerHUDSystem")