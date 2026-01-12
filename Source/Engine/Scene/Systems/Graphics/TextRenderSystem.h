/*****************************************************************//**
 * @file	TextRenderSystem.h
 * @brief	
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 * 
 * @date   2025/12/18	初回作成日
 * 			作業内容：	- 追加：
 * 
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 * 
 * @note	（省略可）
 *********************************************************************/

#ifndef ___TEXT_RENDER_SYSTEM_H___
#define ___TEXT_RENDER_SYSTEM_H___

// ===== インクルード =====
#include "Engine/Scene/Core/ECS/ECS.h"
#include "Engine/Renderer/Text/TextRenderer.h"

namespace Arche
{

	class TextRenderSystem
		: public ISystem
	{
	public:
		TextRenderSystem()
		{
			m_systemName = "Text Render System";
		}

		void Update(Registry& registry) override {}

		void Render(Registry& registry, const Context& ctx) override
		{
			TextRenderer::Draw(registry, nullptr);
		}
	};

}	// namespace Arche

#include "Engine/Scene/Serializer/SystemRegistry.h"
ARCHE_REGISTER_SYSTEM(Arche::TextRenderSystem, "Text Render System")

#endif // !___TEXT_RENDER_SYSTEM_H___