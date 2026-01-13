#include "Engine/pch.h"
#include "SystemRegistry.h"

namespace Arche
{
	SystemRegistry& SystemRegistry::Instance()
	{
		return *GetInstancePtr();
	}

	SystemRegistry*& SystemRegistry::GetInstancePtr()
	{
		static SystemRegistry* instance = nullptr;

		if (instance == nullptr)
		{
			instance = new SystemRegistry();
		}

		return instance;
	}

	void SystemRegistry::Destroy()
	{
		SystemRegistry*& ptr = GetInstancePtr();
		if (ptr)
		{
			delete ptr;
			ptr = nullptr;
		}
	}
}