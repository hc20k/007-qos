#pragma once
#include "structs.hpp"
#include "memory.h"

namespace game {
	inline dvar_s* Dvar_FindVar(const char* dvarName)
	{
		auto func = memory::game_offset(0x1024B590);
		return ((dvar_s* (__cdecl*)(const char*))func)(dvarName);
	}
}