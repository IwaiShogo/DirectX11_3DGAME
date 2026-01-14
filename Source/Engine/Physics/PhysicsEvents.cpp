#include "Engine/pch.h"
#include "Engine/Physics/PhysicsEvents.h"

namespace Arche
{
	namespace Physics
	{
		EventManager& EventManager::Instance()
		{
			static EventManager s_instance;
			return s_instance;
		}
	}
}
