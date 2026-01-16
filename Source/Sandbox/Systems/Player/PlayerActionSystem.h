#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Core/Window/Input.h"
#include "Engine/Audio/AudioManager.h" // ★追加
#include "Sandbox/Components/Player/PlayerController.h"
#include "Sandbox/Components/Player/PlayerMoveData.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Enemy/EnemyStats.h"
#include "Sandbox/Components/Player/Bullet.h"
#include "Sandbox/Systems/Visual/FloatingTextSystem.h" 
#include <cmath>
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

namespace Arche
{
	struct ActionState {
		float cooldown = 0.0f;
		float shootCooldown = 0.0f;
		float hitStopTimer = 0.0f;
		bool isDashing = false;
		float dashTimer = 0.0f;
		XMFLOAT3 dashDir = { 0,0,0 };
		struct Effect { Entity e; float life; float maxLife; };
		std::vector<Effect> effects;
	};

	class PlayerActionSystem : public ISystem
	{
	public:
		PlayerActionSystem() { m_systemName = "PlayerActionSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			static ActionState state;
			float dt = Time::DeltaTime();

			// ヒットストップ
			if (state.hitStopTimer > 0.0f) {
				state.hitStopTimer -= dt; if (state.hitStopTimer > 0.0f) return;
			}
			if (state.cooldown > 0) state.cooldown -= dt;
			if (state.shootCooldown > 0) state.shootCooldown -= dt;

			auto view = reg.view<PlayerController, Transform>();
			Entity player = NullEntity;
			for (auto e : view) { player = e; break; }
			if (player == NullEntity) return;

			auto& t = reg.get<Transform>(player);
			auto& ctrl = reg.get<PlayerController>(player);

			// ダッシュ
			if (state.isDashing) {
				state.dashTimer -= dt;
				float dashSpeed = 120.0f;
				t.position.x += state.dashDir.x * dashSpeed * dt;
				t.position.z += state.dashDir.z * dashSpeed * dt;
				if ((int)(state.dashTimer * 100) % 3 == 0) SpawnEffect(reg, t.position, GeoShape::Pyramid, { 0,1,1,0.5f }, 0.2f, state);
				SpawnHitbox(reg, t.position, 4.0f, 60.0f, 3.0f, state);
				if (state.dashTimer <= 0.0f) state.isDashing = false;
				return;
			}

			// 入力
			bool inputAttack = Input::GetButton(Button::X) || Input::GetMouseLeftButton();
			bool inputDashCut = (Input::GetButton(Button::LShoulder) || Input::GetMouseRightButton()) && inputAttack;
			bool inputSpin = Input::GetButtonDown(Button::B) || Input::GetKeyDown('F');
			bool inputShock = Input::GetButtonDown(Button::Y) || (Input::GetKey(VK_SHIFT) && Input::GetKeyDown('E'));

			// アクション実行 & サウンド再生
			if (inputDashCut && state.cooldown <= 0) {
				DoDashCut(reg, t, state);
				PlaySound(reg, "se_dash.wav", t.position); // ★ダッシュ音
				state.cooldown = 0.6f;
			}
			else if (inputShock && state.cooldown <= 0) {
				DoShockwave(reg, t, state);
				PlaySound(reg, "se_laser.wav", t.position, 0.7f); // ★衝撃波音
				state.cooldown = 1.2f;
			}
			else if (inputSpin && state.cooldown <= 0) {
				DoSpinAttack(reg, t.position, state);
				PlaySound(reg, "se_swing.wav", t.position, 0.5f); // ★回転斬り音
				state.cooldown = 0.5f;
			}
			else if (inputAttack && state.shootCooldown <= 0 && state.cooldown <= 0) {
				DoShoot(reg, t, ctrl, state);
				PlaySound(reg, "se_shot.wav", t.position, 0.5f); // ★射撃音
				state.shootCooldown = 0.08f;
			}

			UpdateEffects(reg, state, dt);
		}

	private:
		void UpdateWorldMatrix(Transform& t) {
			XMMATRIX worldMat = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
				XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
				XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
			XMStoreFloat4x4(&t.worldMatrix, worldMat);
		}

		void PlaySound(Registry& reg, std::string key, XMFLOAT3 pos, float vol = 1.0f) {
			XMFLOAT3 listenerPos = pos;
			for (auto e : reg.view<AudioListener, Transform>()) {
				listenerPos = reg.get<Transform>(e).position; break;
			}
			AudioManager::Instance().Play3DSE(key, pos, listenerPos, 50.0f, vol);
		}

		void DestroyRecursive(Registry& reg, Entity e) {
			if (reg.has<Relationship>(e)) {
				for (auto child : reg.get<Relationship>(e).children)
					if (reg.valid(child)) DestroyRecursive(reg, child);
			}
			reg.destroy(e);
		}

		void DoShoot(Registry& reg, Transform& t, const PlayerController& ctrl, ActionState& state) {
			Entity b = reg.create();
			XMFLOAT3 spawnPos = t.position; spawnPos.y += 1.0f;
			XMFLOAT3 aimDir = { 0,0,1 };
			if (reg.valid(ctrl.focusTarget) && reg.has<Transform>(ctrl.focusTarget)) {
				XMVECTOR pPos = XMLoadFloat3(&spawnPos);
				XMVECTOR tPos = XMLoadFloat3(&reg.get<Transform>(ctrl.focusTarget).position);
				tPos += XMVectorSet(0, 1.0f, 0, 0);
				XMStoreFloat3(&aimDir, XMVector3Normalize(tPos - pPos));
			}
			else {
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
				XMStoreFloat3(&aimDir, XMVector3TransformCoord({ 0,0,1 }, rot));
			}
			spawnPos.x += aimDir.x * 1.5f; spawnPos.z += aimDir.z * 1.5f;

			auto& bt = reg.emplace<Transform>(b);
			bt.position = spawnPos;
			bt.scale = { 0.3f, 0.3f, 1.5f };
			float pitch = -asinf(aimDir.y); float yaw = atan2f(aimDir.x, aimDir.z);
			bt.rotation = { pitch, yaw, 0 };

			auto& g = reg.emplace<GeometricDesign>(b);
			g.shapeType = GeoShape::Cube; g.color = { 0, 1, 1, 1 }; g.isWireframe = false;
			auto& attr = reg.emplace<AttackAttribute>(b);
			attr.damage = 8.0f; attr.rewardRate = 0.4f; attr.isPenetrate = false;
			auto& bul = reg.emplace<Bullet>(b);
			bul.damage = 8.0f; bul.speed = 80.0f; bul.lifeTime = 2.0f; bul.owner = EntityType::Player;

			// ★行列更新
			UpdateWorldMatrix(bt);
		}

		void DoDashCut(Registry& reg, Transform& t, ActionState& state) {
			XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
			XMVECTOR fwd = XMVector3TransformCoord({ 0, 0, 1 }, rot);
			XMStoreFloat3(&state.dashDir, fwd);
			state.isDashing = true; state.dashTimer = 0.15f;
			SpawnEffect(reg, t.position, GeoShape::Pyramid, { 0, 1, 1, 0.8f }, 0.5f, state);
			ShakeCamera(reg, 0.5f);
		}

		void DoSpinAttack(Registry& reg, XMFLOAT3 pos, ActionState& state) {
			SpawnHitbox(reg, pos, 8.0f, 15.0f, 1.0f, state);
			SpawnEffect(reg, pos, GeoShape::Torus, { 1, 0.5f, 0, 1 }, 0.4f, state);
		}

		void DoShockwave(Registry& reg, Transform& t, ActionState& state) {
			XMMATRIX rot = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
			XMVECTOR fwd = XMVector3TransformCoord({ 0, 0, 1 }, rot);
			XMFLOAT3 dir; XMStoreFloat3(&dir, fwd);
			Entity wave = reg.create();
			XMFLOAT3 pos = t.position; pos.y += 1.0f; pos.x += dir.x * 2.0f; pos.z += dir.z * 2.0f;

			auto& wt = reg.emplace<Transform>(wave);
			wt.position = pos;
			wt.rotation = t.rotation;
			wt.scale = { 4.0f, 4.0f, 4.0f };

			auto& g = reg.emplace<GeometricDesign>(wave);
			g.shapeType = GeoShape::Sphere; g.color = { 1, 0, 1, 0.6f }; g.isWireframe = true;
			auto& attr = reg.emplace<AttackAttribute>(wave);
			attr.damage = 30.0f; attr.rewardRate = 1.5f; attr.isPenetrate = true;
			auto& bul = reg.emplace<Bullet>(wave);
			bul.damage = 30.0f; bul.speed = 40.0f; bul.lifeTime = 1.5f; bul.owner = EntityType::Player;
			ShakeCamera(reg, 0.8f);

			// ★行列更新
			UpdateWorldMatrix(wt);
		}

		void SpawnHitbox(Registry& reg, XMFLOAT3 center, float radius, float damage, float rewardRate, ActionState& state) {
			std::vector<Entity> deadEnemies; bool hitAny = false;
			auto enemies = reg.view<EnemyStats, Transform>();
			for (auto e : enemies) {
				auto& et = reg.get<Transform>(e);
				float dx = et.position.x - center.x; float dz = et.position.z - center.z;
				if (dx * dx + dz * dz < radius * radius) {
					auto& stats = reg.get<EnemyStats>(e);
					stats.hp -= damage; hitAny = true;
					FloatingTextSystem::Spawn(reg, et.position, (int)damage, { 1, 1, 0, 1 }, 1.0f);
					PlaySound(reg, "se_hit", et.position, 0.6f);
					if (stats.hp <= 0) {
						float reward = stats.killReward * rewardRate;
						for (auto p : reg.view<PlayerTime>()) {
							auto& pt = reg.get<PlayerTime>(p);
							pt.currentTime += reward; if (pt.currentTime > pt.maxTime) pt.currentTime = pt.maxTime;
							FloatingTextSystem::Spawn(reg, reg.get<Transform>(p).position, (int)reward, { 0, 1, 0, 1 }, 1.5f);
						}
						GameSession::lastScore += stats.scoreValue;
						deadEnemies.push_back(e);
						PlaySound(reg, "se_explosion", et.position, 1.0f);
					}
				}
			}
			for (auto e : deadEnemies) DestroyRecursive(reg, e);
			if (hitAny) state.hitStopTimer = 0.05f;
		}

		void ShakeCamera(Registry& reg, float intensity) {
			for (auto e : reg.view<Camera, Transform>()) { auto& t = reg.get<Transform>(e); t.position.y -= intensity; }
		}

		void SpawnEffect(Registry& reg, XMFLOAT3 pos, GeoShape shape, XMFLOAT4 col, float life, ActionState& state) {
			Entity e = reg.create();
			auto& t = reg.emplace<Transform>(e);
			t.position = pos; t.scale = { 1, 1, 1 };
			auto& g = reg.emplace<GeometricDesign>(e); g.shapeType = shape; g.color = col; g.isWireframe = true;
			state.effects.push_back({ e, life, life });

			// ★行列更新
			UpdateWorldMatrix(t);
		}

		void UpdateEffects(Registry& reg, ActionState& state, float dt) {
			for (auto it = state.effects.begin(); it != state.effects.end(); ) {
				it->life -= dt;
				if (it->life <= 0) { if (reg.valid(it->e)) reg.destroy(it->e); it = state.effects.erase(it); }
				else {
					if (reg.valid(it->e)) {
						auto& t = reg.get<Transform>(it->e);
						float p = 1.0f - (it->life / it->maxLife);
						float s = 1.0f + p * 20.0f; t.scale = { s, s, s }; t.rotation.y += dt * 5.0f;
						reg.get<GeometricDesign>(it->e).color.w = it->life / it->maxLife;
					}
					++it;
				}
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerActionSystem, "PlayerActionSystem")