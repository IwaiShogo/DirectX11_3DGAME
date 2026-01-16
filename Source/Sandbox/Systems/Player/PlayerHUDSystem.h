#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Components/Player/PlayerTime.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Core/GameSession.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	struct PlayerHUDTag {};

	struct ReticlePartData {
		Entity e; XMFLOAT3 localOffset; XMFLOAT3 localScale;
	};

	struct PlayerHUDContext
	{
		Entity ringMain = NullEntity; Entity ringSub = NullEntity; Entity ticks[4] = { NullEntity };
		Entity timerText = NullEntity; Entity timerBarBg = NullEntity; Entity timerBarFill = NullEntity;
		Entity scoreText = NullEntity; Entity warningText = NullEntity;
		Entity waveText = NullEntity; Entity killText = NullEntity;
		Entity dangerFrames[4] = { NullEntity };

		Entity reticleRoot = NullEntity;
		std::vector<ReticlePartData> reticleParts;

		float displayTime = 5.0f;
		float warningTimer = 0.0f;
		XMFLOAT3 hudShake = { 0,0,0 };

		std::vector<Entity> indicatorPool;
	};

	class PlayerHUDSystem : public ISystem
	{
	public:
		PlayerHUDSystem() { m_systemName = "PlayerHUDSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static PlayerHUDContext ctx;
			bool isInitialized = false;
			for (auto e : reg.view<PlayerHUDTag>()) { isInitialized = true; break; }

			if (!isInitialized) {
				ctx = PlayerHUDContext();
				InitHUD(reg, ctx);
				reg.emplace<PlayerHUDTag>(reg.create());
			}

			float pTime = 0.0f; float pMaxTime = 5.0f; XMFLOAT3 pPos = { 0,0,0 };
			for (auto e : reg.view<PlayerTime, Transform>()) {
				auto& pt = reg.get<PlayerTime>(e); auto& t = reg.get<Transform>(e);
				pTime = pt.currentTime; pMaxTime = pt.maxTime; pPos = t.position;
				break;
			}

			Entity cam = NullEntity; for (auto e : reg.view<Camera>()) { cam = e; break; }
			if (cam == NullEntity) return;
			auto& camT = reg.get<Transform>(cam);

			float dt = Time::DeltaTime(); float time = Time::TotalTime();

			// ボス警告
			bool bossExists = false;
			for (auto e : reg.view<EnemyStats>()) {
				auto type = reg.get<EnemyStats>(e).type;
				if (type == EnemyType::Boss_Omega || type == EnemyType::Boss_Tank ||
					type == EnemyType::Boss_Prism || type == EnemyType::Boss_Carrier || type == EnemyType::Boss_Construct || type == EnemyType::Boss_Titan) {
					bossExists = true; break;
				}
			}
			static bool wasBossExists = false;
			if (bossExists && !wasBossExists) ctx.warningTimer = 4.0f;
			wasBossExists = bossExists;
			if (ctx.warningTimer > 0.0f) ctx.warningTimer -= dt;

			// UI配置計算
			float hudDist = 6.0f;
			XMMATRIX camRotMat = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, camT.rotation.z);
			XMVECTOR camPosVec = XMLoadFloat3(&camT.position);

			auto SetUI = [&](Entity e, float x, float y, float z, float sx, float sy) {
				if (!reg.valid(e)) return;
				float shakeX = x + ctx.hudShake.x; float shakeY = y + ctx.hudShake.y;
				XMVECTOR local = XMVectorSet(shakeX, shakeY, hudDist + z, 0);
				XMVECTOR world = camPosVec + XMVector3TransformNormal(local, camRotMat);
				auto& t = reg.get<Transform>(e);
				XMStoreFloat3(&t.position, world);
				t.rotation = camT.rotation;
				t.scale = { sx, sy, 0.01f };
				};

			float ratio = std::clamp(pTime / 5.0f, 0.0f, 1.0f);
			XMFLOAT4 baseColor = (ratio > 0.5f) ? XMFLOAT4(0, 1, 1, 1) : XMFLOAT4(1, ratio * 2, 0, 1);
			ctx.hudShake = { 0,0,0 };
			if (ratio < 0.2f) { ctx.hudShake.x = (rand() % 100 / 1000.0f) * 0.2f; ctx.hudShake.y = (rand() % 100 / 1000.0f) * 0.2f; }

			if (reg.valid(ctx.timerText)) {
				ctx.displayTime += (pTime - ctx.displayTime) * dt * 10.0f;
				char buf[32]; sprintf_s(buf, "%04.2f", std::max(0.0f, ctx.displayTime));
				reg.get<TextComponent>(ctx.timerText).text = buf;
				reg.get<TextComponent>(ctx.timerText).color = baseColor;
				SetUI(ctx.timerText, 0, 1.8f, 0, 1, 1);
			}
			if (reg.valid(ctx.timerBarFill)) {
				reg.get<GeometricDesign>(ctx.timerBarFill).color = baseColor;
				SetUI(ctx.timerBarFill, 0, 2.1f, 0, 2.0f * (pTime / pMaxTime), 0.1f);
			}
			if (reg.valid(ctx.timerBarBg)) SetUI(ctx.timerBarBg, 0, 2.1f, 0.02f, 2.05f, 0.15f);

			if (reg.valid(ctx.scoreText)) {
				reg.get<TextComponent>(ctx.scoreText).text = "SCORE: " + std::to_string(GameSession::lastScore);
				SetUI(ctx.scoreText, 2.5f, 1.8f, 0, 1, 1);
			}
			if (reg.valid(ctx.waveText)) {
				reg.get<TextComponent>(ctx.waveText).text = "WAVE " + std::to_string(GameSession::currentWave) + "/" + std::to_string(GameSession::totalWaves);
				SetUI(ctx.waveText, -2.5f, 1.8f, 0, 1, 1);
			}
			if (reg.valid(ctx.killText)) {
				reg.get<TextComponent>(ctx.killText).text = "KILLS: " + std::to_string(GameSession::killCount);
				SetUI(ctx.killText, -2.5f, 1.5f, 0, 1, 1);
			}
			if (reg.valid(ctx.warningText)) {
				float a = (ctx.warningTimer > 0 && (int)(time * 15) % 2 == 0) ? 1.0f : 0.0f;
				reg.get<TextComponent>(ctx.warningText).color = { 1,0,0,a };
				SetUI(ctx.warningText, 0, 0, 0, 1, 1);
			}

			float da = (pTime < 1.5f) ? 0.2f + 0.1f * sinf(time * 15) : 0.0f;
			struct F { float x, y, w, h; };
			F frames[] = { {0, 3.5f, 14, 1.5f}, {0, -3.5f, 14, 1.5f}, {-6.0f, 0, 1.5f, 9}, {6.0f, 0, 1.5f, 9} };
			for (int i = 0; i < 4; ++i) {
				if (reg.valid(ctx.dangerFrames[i])) {
					reg.get<GeometricDesign>(ctx.dangerFrames[i]).color = { 1,0,0,da };
					SetUI(ctx.dangerFrames[i], frames[i].x, frames[i].y, -1.0f, frames[i].w, frames[i].h);
				}
			}

			UpdateRings(reg, ctx, pPos, ratio, baseColor, dt);
			UpdateReticle(reg, ctx, pPos, camT, dt);
			UpdateOffscreenIndicators(reg, ctx, camT);
		}

	private:
		void InitHUD(Registry& reg, PlayerHUDContext& ctx) {
			auto MkText = [&](Entity& e, float s, bool c) { e = reg.create(); reg.emplace<Transform>(e); auto& t = reg.emplace<TextComponent>(e); t.fontKey = "Makinas 4 Square"; t.fontSize = s; t.centerAlign = c; };
			MkText(ctx.timerText, 50, true);
			MkText(ctx.scoreText, 15, true);
			MkText(ctx.waveText, 15, true);
			MkText(ctx.killText, 12, true);
			MkText(ctx.warningText, 60, true);
			reg.get<TextComponent>(ctx.warningText).text = "WARNING\nBOSS APPROACHING";
			reg.get<TextComponent>(ctx.warningText).color = { 1,0,0,0 };

			auto MkBar = [&](Entity& e, XMFLOAT4 c) { e = reg.create(); reg.emplace<Transform>(e); auto& g = reg.emplace<GeometricDesign>(e); g.shapeType = GeoShape::Cube; g.color = c; };
			MkBar(ctx.timerBarFill, { 0,1,1,1 });
			MkBar(ctx.timerBarBg, { 0,0,0,0.5f });

			for (int i = 0; i < 4; ++i) MkBar(ctx.dangerFrames[i], { 0,0,0,0 });

			auto MkRing = [&](Entity& e, GeoShape s) { e = reg.create(); reg.emplace<Transform>(e); auto& g = reg.emplace<GeometricDesign>(e); g.shapeType = s; g.isWireframe = true; };
			MkRing(ctx.ringMain, GeoShape::Cylinder);
			MkRing(ctx.ringSub, GeoShape::Cylinder);
			for (int i = 0; i < 4; ++i) {
				ctx.ticks[i] = reg.create(); reg.emplace<Transform>(ctx.ticks[i]);
				reg.emplace<GeometricDesign>(ctx.ticks[i]).shapeType = GeoShape::Cube;
			}

			ctx.reticleRoot = reg.create(); reg.emplace<Transform>(ctx.reticleRoot);
			for (int i = 0; i < 4; ++i) {
				Entity e = reg.create(); reg.emplace<Transform>(e);
				auto& g = reg.emplace<GeometricDesign>(e);
				g.shapeType = GeoShape::Cube; g.color = { 1,0.5f,0,0.8f };
				ReticlePartData p; p.e = e;
				float ang = i * XM_PIDIV2;
				p.localOffset = { cosf(ang), sinf(ang), 0 };
				p.localScale = (i % 2 == 0) ? XMFLOAT3(0.1f, 0.5f, 0.05f) : XMFLOAT3(0.5f, 0.1f, 0.05f);
				ctx.reticleParts.push_back(p);
			}

			for (int i = 0; i < 30; ++i) {
				Entity e = reg.create();
				reg.emplace<Transform>(e).scale = { 0,0,0 };
				auto& g = reg.emplace<GeometricDesign>(e);
				g.isWireframe = false;
				ctx.indicatorPool.push_back(e);
			}
		}

		// ★修正版: コンパス方式で方向を表示
		void UpdateOffscreenIndicators(Registry& reg, PlayerHUDContext& ctx, Transform& camT) {
			XMVECTOR camPos = XMLoadFloat3(&camT.position);
			// HUD配置用の行列 (カメラの回転全適用)
			XMMATRIX camRotFull = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, camT.rotation.z);
			XMVECTOR camFwdFull = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), camRotFull);

			// コンパス計算用: Y軸回転(Yaw)のみの逆回転行列
			// これを使って敵の位置を「カメラが北を向いている状態」の座標系に戻す
			XMMATRIX invYawRot = XMMatrixRotationY(-camT.rotation.y);

			int used = 0;
			auto enemies = reg.view<EnemyStats, Transform>();

			for (auto e : enemies) {
				if (used >= ctx.indicatorPool.size()) break;

				auto& eT = reg.get<Transform>(e);
				XMVECTOR ePos = XMLoadFloat3(&eT.position);
				XMVECTOR toEnemy = ePos - camPos;
				float dist = XMVectorGetX(XMVector3Length(toEnemy));
				if (dist < 10.0f) continue; // ある程度遠い敵のみ対象

				XMVECTOR dir = XMVector3Normalize(toEnemy);

				// 画面外判定 (実際の視線との内積)
				float dot = XMVectorGetX(XMVector3Dot(camFwdFull, dir));

				// 視野角外 (0.6以下) なら表示
				if (dot < 0.6f) {
					Entity ind = ctx.indicatorPool[used++];
					auto& t = reg.get<Transform>(ind);
					auto& g = reg.get<GeometricDesign>(ind);
					auto& stats = reg.get<EnemyStats>(e);

					// --- コンパスロジック ---
					// カメラのYaw回転を打ち消して、相対位置を計算
					XMVECTOR localPos = XMVector3TransformCoord(toEnemy, invYawRot);

					// X(左右) と Z(前後) を使って角度を出す
					float lx = XMVectorGetX(localPos);
					float lz = XMVectorGetZ(localPos);

					// atan2(x, z) -> 前(Z+)が0度、右(X+)が90度、後(Z-)が180度
					// これを画面上の配置角に変換
					float angle = atan2f(lx, lz);

					// 画面端（円周）に配置
					// sin(angle) -> X座標, cos(angle) -> Y座標
					// 前(0度) -> (0, 1) = 上
					// 右(90度) -> (1, 0) = 右
					// 後(180度) -> (0, -1) = 下
					float r = 3.5f; // 半径
					float hudX = sinf(angle) * r;
					float hudY = cosf(angle) * r;

					// ワールド座標へ変換 (HUD平面上に配置)
					XMVECTOR hudPosLocal = XMVectorSet(hudX, hudY, 5.0f, 1.0f);
					XMVECTOR hudPosWorld = camPos + XMVector3TransformNormal(hudPosLocal, camRotFull);

					XMStoreFloat3(&t.position, hudPosWorld);

					// 矢印の回転 (外側を向くように)
					t.rotation = camT.rotation;
					t.rotation.z = -angle; // 時計回り/反時計回りの整合

					// ボスと雑魚の区別
					bool isBoss = (stats.type == EnemyType::Boss_Tank || stats.type == EnemyType::Boss_Prism ||
						stats.type == EnemyType::Boss_Carrier || stats.type == EnemyType::Boss_Construct ||
						stats.type == EnemyType::Boss_Omega || stats.type == EnemyType::Boss_Titan);

					if (isBoss) {
						g.shapeType = GeoShape::Diamond;
						g.color = { 1.0f, 0.0f, 1.0f, 1.0f }; // 紫
						t.scale = { 0.8f, 0.8f, 0.1f };
					}
					else {
						g.shapeType = GeoShape::Pyramid;
						g.color = { 1.0f, 0.0f, 0.0f, 0.8f }; // 赤
						t.scale = { 0.4f, 0.4f, 0.1f };
					}
				}
			}

			for (int i = used; i < ctx.indicatorPool.size(); ++i) {
				reg.get<Transform>(ctx.indicatorPool[i]).scale = { 0, 0, 0 };
			}
		}

		void UpdateRings(Registry& reg, PlayerHUDContext& ctx, XMFLOAT3 pPos, float ratio, XMFLOAT4 col, float dt) {
			float y = pPos.y - 0.9f;
			if (reg.valid(ctx.ringMain)) {
				auto& t = reg.get<Transform>(ctx.ringMain);
				t.position = { pPos.x, y, pPos.z }; t.scale = { 2.0f * ratio + 0.5f, 0.02f, 2.0f * ratio + 0.5f }; t.rotation.y += dt * 2.0f;
				reg.get<GeometricDesign>(ctx.ringMain).color = col;
			}
			if (reg.valid(ctx.ringSub)) {
				auto& t = reg.get<Transform>(ctx.ringSub);
				t.position = { pPos.x, y, pPos.z }; t.scale = { 3.0f, 0.02f, 3.0f }; t.rotation.y -= dt * 4.0f;
				col.w = 0.3f; reg.get<GeometricDesign>(ctx.ringSub).color = col;
			}
			for (int i = 0; i < 4; ++i) {
				if (reg.valid(ctx.ticks[i])) {
					auto& t = reg.get<Transform>(ctx.ticks[i]);
					float ang = Time::TotalTime() * 3.0f + i * XM_PIDIV2; float r = 2.5f * ratio + 0.8f;
					t.position = { pPos.x + cosf(ang) * r, y, pPos.z + sinf(ang) * r };
					t.scale = { 0.1f,0.1f,0.1f }; t.rotation.y = -ang;
					reg.get<GeometricDesign>(ctx.ticks[i]).color = { col.x,col.y,col.z,1.0f };
				}
			}
		}

		void UpdateReticle(Registry& reg, PlayerHUDContext& ctx, XMFLOAT3 pPos, Transform& camT, float dt) {
			Entity target = NullEntity; float minD = 999.0f;
			for (auto e : reg.view<EnemyStats, Transform>()) {
				float d = XMVectorGetX(XMVector3Length(XMLoadFloat3(&reg.get<Transform>(e).position) - XMLoadFloat3(&pPos)));
				if (d < minD && d < 20.0f) { minD = d; target = e; }
			}
			if (reg.valid(ctx.reticleRoot)) {
				auto& t = reg.get<Transform>(ctx.reticleRoot);
				if (target != NullEntity) {
					XMFLOAT3 tPos = reg.get<Transform>(target).position;
					t.position.x += (tPos.x - t.position.x) * dt * 20.0f;
					t.position.y += (tPos.y - t.position.y) * dt * 20.0f;
					t.position.z += (tPos.z - t.position.z) * dt * 20.0f;
					t.rotation.z += dt * 5.0f; t.rotation.x = camT.rotation.x; t.rotation.y = camT.rotation.y;
					t.scale = { 1.0f, 1.0f, 1.0f };
					for (auto& p : ctx.reticleParts) if (reg.valid(p.e)) reg.get<GeometricDesign>(p.e).color = { 1,0,0,0.8f };
				}
				else {
					XMMATRIX rot = XMMatrixRotationRollPitchYaw(camT.rotation.x, camT.rotation.y, camT.rotation.z);
					XMVECTOR pos = XMLoadFloat3(&camT.position) + XMVector3TransformCoord({ 0,0,5 }, rot);
					XMStoreFloat3(&t.position, pos); t.rotation = camT.rotation; t.scale = { 0.5f, 0.5f, 0.5f };
					for (auto& p : ctx.reticleParts) if (reg.valid(p.e)) reg.get<GeometricDesign>(p.e).color = { 1,1,1,0.1f };
				}
				for (auto& part : ctx.reticleParts) {
					if (reg.valid(part.e)) {
						auto& pt = reg.get<Transform>(part.e);
						XMMATRIX mRot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
						XMVECTOR vOffset = XMLoadFloat3(&part.localOffset);
						XMVECTOR vPos = XMLoadFloat3(&t.position) + XMVector3Transform(vOffset, mRot);
						XMStoreFloat3(&pt.position, vPos);
						pt.rotation = t.rotation; pt.scale = part.localScale;
					}
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerHUDSystem, "PlayerHUDSystem")