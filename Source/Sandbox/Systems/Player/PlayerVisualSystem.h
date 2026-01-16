#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Sandbox/Components/Visual/GeometricDesign.h"
#include "Sandbox/Components/Player/PlayerController.h"
#include <cmath>
#include <map>

namespace Arche
{
	struct PlayerVisualParts {
		Entity core;
		Entity ring;
		Entity wings[2];
		Entity boosters[2];
	};

	class PlayerVisualSystem : public ISystem
	{
		std::map<Entity, PlayerVisualParts> m_parts;

	public:
		PlayerVisualSystem() { m_systemName = "PlayerVisualSystem"; m_group = SystemGroup::PlayOnly; }

		void Update(Registry& reg) override
		{
			Entity player = NullEntity;
			for (auto e : reg.view<PlayerController>()) { player = e; break; }
			if (!reg.valid(player)) return;

			auto& pTrans = reg.get<Transform>(player);

			if (m_parts.find(player) == m_parts.end()) {
				PlayerVisualParts p;

				// Core
				p.core = reg.create(); reg.emplace<Transform>(p.core);
				auto& gCore = reg.emplace<GeometricDesign>(p.core);
				gCore.shapeType = GeoShape::Diamond; gCore.color = { 0, 1, 1, 1 }; gCore.isWireframe = true;

				// Ring
				p.ring = reg.create(); reg.emplace<Transform>(p.ring);
				auto& gRing = reg.emplace<GeometricDesign>(p.ring);
				gRing.shapeType = GeoShape::Torus; gRing.color = { 0, 0.5f, 1, 0.5f }; gRing.isWireframe = true;

				// Wings & Boosters
				for (int i = 0; i < 2; ++i) {
					p.wings[i] = reg.create(); reg.emplace<Transform>(p.wings[i]);
					auto& gW = reg.emplace<GeometricDesign>(p.wings[i]);
					gW.shapeType = GeoShape::Pyramid; gW.color = { 0, 0.8f, 1, 0.8f }; gW.isWireframe = true;

					p.boosters[i] = reg.create(); reg.emplace<Transform>(p.boosters[i]);
					auto& gB = reg.emplace<GeometricDesign>(p.boosters[i]);
					gB.shapeType = GeoShape::Cube; gB.color = { 1, 0.5f, 0, 1 }; gB.isWireframe = false;
				}
				m_parts[player] = p;

				if (reg.has<GeometricDesign>(player)) reg.get<GeometricDesign>(player).color = { 0,0,0,0 };
			}

			auto& p = m_parts[player];
			float time = Time::TotalTime();
			float dt = Time::DeltaTime();

			// Core
			auto& tCore = reg.get<Transform>(p.core);
			tCore.position = pTrans.position; tCore.position.y += 1.0f + sinf(time * 2.0f) * 0.2f;
			tCore.rotation.y += dt * 90.0f; tCore.scale = { 0.8f, 1.2f, 0.8f };

			// Ring
			auto& tRing = reg.get<Transform>(p.ring);
			XMMATRIX pRot = XMMatrixRotationRollPitchYaw(pTrans.rotation.x, pTrans.rotation.y, pTrans.rotation.z);
			XMVECTOR ringOffset = XMVector3Transform(XMVectorSet(0, 1.2f, -0.5f, 0), pRot);
			XMStoreFloat3(&tRing.position, XMLoadFloat3(&pTrans.position) + ringOffset);
			tRing.rotation = pTrans.rotation; tRing.scale = { 1.2f, 1.2f, 0.1f };

			// Wings & Boosters
			float speed = 0.0f;
			if (reg.has<Rigidbody>(player)) speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&reg.get<Rigidbody>(player).velocity)));

			for (int i = 0; i < 2; ++i) {
				float side = (i == 0) ? -1.0f : 1.0f;
				auto& tWing = reg.get<Transform>(p.wings[i]);
				XMVECTOR wOffset = XMVector3Transform(XMVectorSet(side * 0.8f, 1.0f, -0.2f, 0), pRot);
				XMStoreFloat3(&tWing.position, XMLoadFloat3(&pTrans.position) + wOffset);
				tWing.rotation = pTrans.rotation; tWing.rotation.z = side * 45.0f; tWing.scale = { 0.3f, 1.5f, 0.8f };

				auto& tBoot = reg.get<Transform>(p.boosters[i]);
				XMVECTOR bOffset = XMVector3Transform(XMVectorSet(side, 0.5f, -0.5f, 0), pRot);
				XMStoreFloat3(&tBoot.position, XMLoadFloat3(&pTrans.position) + bOffset);
				tBoot.rotation = pTrans.rotation;
				float boostLen = 0.5f + speed * 0.1f;
				tBoot.scale = { 0.2f, 0.2f, boostLen };
				reg.get<GeometricDesign>(p.boosters[i]).color.w = 0.5f + 0.5f * sinf(time * 20.0f);
			}
		}
	};
}
#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::PlayerVisualSystem, "PlayerVisualSystem")