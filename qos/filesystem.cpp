#include "filesystem.h"
#include <filesystem>
#include "shared.h"
#include "memory.h"
#include "structs.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

using namespace game;
namespace fs = std::filesystem;

std::unique_ptr<PLH::x86Detour> FS_InitCvars_hook;

uint64_t o_FS_InitCvars = 0;
void FS_InitCvars_stub() {
	// fs_game = 0x11A67BA8
	dvar_s* fs_game = (dvar_s*)memory::game_offset(0x11A67BA8);

	__asm {
		// call original function
		call o_FS_InitCvars
	}
}

void filesystem::init()
{
	fs::create_directories("filesystem");

	// hook FS_InitCvars
	FS_InitCvars_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x140D3A0A0), (uint64_t)FS_InitCvars_stub, &o_FS_InitCvars);
	FS_InitCvars_hook->hook();

	spdlog::info("Filesystem initialized");
}