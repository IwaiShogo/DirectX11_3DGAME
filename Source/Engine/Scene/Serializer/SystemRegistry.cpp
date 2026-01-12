#include "Engine/pch.h"
#include "SystemRegistry.h"

namespace Arche
{
	SystemRegistry& SystemRegistry::Instance()
	{
		static SystemRegistry instance;
		return instance;
	}
}