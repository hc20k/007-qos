#include "gsc.h"
#include "shared.h"
#include "memory.h"
#include "structs.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

using namespace game;

std::unique_ptr<PLH::x86Detour> Scr_LoadScriptInternal_hook;
// unsigned int __cdecl Scr_LoadScriptInternal(const char *filename, PrecacheEntry *entries, int entriesCount)

uint64_t o_Scr_LoadScriptInternal = 0;
unsigned int Scr_LoadScriptInternal_stub(const char* filename, struct PrecacheEntry* entries, int entriesCount)
{
	spdlog::info("Scr_LoadScriptInternal: {}.gsc", filename);
	auto res =  ((unsigned int(__cdecl*)(const char*, struct PrecacheEntry*, int))o_Scr_LoadScriptInternal)(filename, entries, entriesCount);


	// call original function

	return res;
}

void gsc::init()
{
	spdlog::info("GSC initialized");

	// hook Scr_LoadScriptInternal

	Scr_LoadScriptInternal_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x102D7A60), (uint64_t)Scr_LoadScriptInternal_stub, &o_Scr_LoadScriptInternal);
	Scr_LoadScriptInternal_hook->hook();
}