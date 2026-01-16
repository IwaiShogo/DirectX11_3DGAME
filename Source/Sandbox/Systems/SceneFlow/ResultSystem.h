#pragma once
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Scene/Core/SceneManager.h"
#include "Engine/Scene/Core/SceneTransition.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Core/Window/Input.h"
#include "Sandbox/Core/GameSession.h"
#include "Engine/Renderer/Text/TextRenderer.h" // テキストコンポーネント用

namespace Arche
{
	class ResultSystem : public ISystem
	{
	public:
		ResultSystem() { m_systemName = "ResultSystem"; }

		void Update(Registry& registry) override
		{
			// 初回のみテキスト生成
			if (!registry.valid(m_scoreTextEntity))
			{
				CreateResultUI(registry);
			}

			// スペースキーでタイトルへ戻る
			if (Input::GetButton(Button::A))
			{
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/TitleScene.json", new FadeTransition(1.0f));
			}
			// Rキーで同じステージをリトライ
			else if (Input::GetButton(Button::B))
			{
				GameSession::ResetResult();
				SceneManager::Instance().LoadScene("Resources/Game/Scenes/GameScene.json", new FadeTransition(0.5f));
			}
		}

	private:
		Entity m_scoreTextEntity = NullEntity;

		void CreateResultUI(Registry& reg)
		{
			// 結果文字列作成
			std::string resultStr = GameSession::isGameClear ? "MISSION CLEAR!" : "GAME OVER";
			std::string scoreStr = "SCORE: " + std::to_string(GameSession::lastScore);

			// 1. タイトル表示
			Entity title = reg.create();
			reg.emplace<Transform>(title).position = { 0, 2.0f, 0 };
			auto& txt1 = reg.emplace<TextComponent>(title);
			txt1.text = resultStr;
			txt1.fontSize = 80.0f;
			txt1.color = GameSession::isGameClear ? XMFLOAT4{ 0,1,1,1 } : XMFLOAT4{ 1,0,0,1 };
			txt1.centerAlign = true;
			txt1.isBold = true;

			// 2. スコア表示
			m_scoreTextEntity = reg.create();
			reg.emplace<Transform>(m_scoreTextEntity).position = { 0, 0.0f, 0 };
			auto& txt2 = reg.emplace<TextComponent>(m_scoreTextEntity);
			txt2.text = scoreStr;
			txt2.fontSize = 60.0f;
			txt2.color = { 1,1,1,1 };
			txt2.centerAlign = true;

			// 3. 案内表示
			Entity guide = reg.create();
			reg.emplace<Transform>(guide).position = { 0, -2.0f, 0 };
			auto& txt3 = reg.emplace<TextComponent>(guide);
			txt3.text = "PRESS SPACE TO TITLE\nPRESS R TO RETRY";
			txt3.fontSize = 30.0f;
			txt3.color = { 0.7f, 0.7f, 0.7f, 1.0f };
			txt3.centerAlign = true;
		}
	};
}

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::ResultSystem, "ResultSystem")
