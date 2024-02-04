#include "misc_patches.h"
#include "shared.h"
#include "memory.h"
#include "structs.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

using namespace game;

std::shared_ptr<PLH::x86Detour> R_CreateWindow_hook;
std::shared_ptr<PLH::x86Detour> Dvar_RegisterNew_hook;

uint64_t o_R_CreateWindow = 0;
int R_CreateWindow_stub()
{
	__asm {
		// first arg is windowInfo (esi)
		// arg1 + 8 = fullscreen mode

		mov byte ptr[esi + 8], 0 // force windowed mode
		call o_R_CreateWindow
	}
}

// dvar_s *__cdecl Dvar_RegisterNew(const char *dvarName, char type, unsigned __int16 flags, DvarValue *value, DvarLimits *domain, const char *description)
uint64_t o_Dvar_RegisterNew = 0;
dvar_s* __cdecl Dvar_RegisterNew_stub(const char* dvarName, dvar_type type, dvar_flags flags, DvarValue* value, DvarLimits* domain, const char* description, int a6, int a7, int a8, int16_t a9)
{
	auto dvar = ((dvar_s* (__cdecl*)(const char*, dvar_type, dvar_flags, DvarValue*, DvarLimits*, const char*, int, int, int, int16_t))o_Dvar_RegisterNew)(dvarName, type, flags, value, domain, description, a6, a7, a8, a9);
	dvar->flags = (dvar_flags)(dvar->flags & ~dvar_flags::cheat_protected); // remove cheat_protected flag
	dvar->flags = (dvar_flags)(dvar->flags & ~dvar_flags::read_only); // remove read_only flag
	dvar->flags = (dvar_flags)(dvar->flags & ~dvar_flags::write_protected); // remove write_protected flag

	// this doesn't work rn because of struct problems...

	if (!dvar)
	{
		spdlog::error("Failed to register dvar: {}", dvarName);
		return dvar;
	}

	if (!strcmp(dvarName, "monkeytoy")) {
		// set monkeytoy to 0
		*(int*)((uintptr_t)dvar + 0x10) = 0;
	}

	spdlog::info("Registered dvar: {} @ {}", dvarName, (void*)dvar);

	// call original function
	return dvar;
}

void misc_patches::init()
{
	R_CreateWindow_hook = std::make_shared<PLH::x86Detour>((uint64_t)memory::game_offset(0x101046A0), (uint64_t)R_CreateWindow_stub, &o_R_CreateWindow);
	R_CreateWindow_hook->hook();

	Dvar_RegisterNew_hook = std::make_shared<PLH::x86Detour>((uint64_t)memory::game_offset(0x1024BFA0), (uint64_t)Dvar_RegisterNew_stub, &o_Dvar_RegisterNew);
	//Dvar_RegisterNew_hook->hook();
}