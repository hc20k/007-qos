#include "gsc.h"
#include "shared.h"
#include "memory.h"
#include "structs.hpp"
#include "functions.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

using namespace game;


std::unique_ptr<PLH::x86Detour> DB_LinkXAssetEntry_hook;
typedef XAssetEntry* (__cdecl* DB_LinkXAssetEntry_t)(XAssetEntry* newEntry, int allowOverride);
DB_LinkXAssetEntry_t DB_LinkXAssetEntry = 0;

XAssetEntry* DB_LinkXAssetEntry_stub(XAssetEntry* newEntry, int allowOverride)
{
	*(bool*)memory::game_offset(0x11F4F706) = true; // so we can see script errors. too lazy to do this somewhere else

	if (newEntry->asset.type == ASSET_TYPE_RAWFILE) {
		auto rawfile = newEntry->asset.header.rawfile;
		if (rawfile) {
			auto relative_path = std::string("filesystem/") + rawfile->name;
			if (!std::filesystem::exists(relative_path)) {
				return DB_LinkXAssetEntry(newEntry, allowOverride);
			}

			spdlog::info("Overriding rawfile: {}", rawfile->name);
			std::ifstream file(relative_path, std::ios::binary);

			if (!file.is_open()) {
				spdlog::error("Failed to open file: {}", relative_path);
				return nullptr;
			}

			file.seekg(0, std::ios::end);
			const auto size = (int)file.tellg();
			rawfile->len = size;
			file.seekg(0, std::ios::beg);

			// allocate buffer
			rawfile->buffer = (const char*)malloc(size);
			if (!rawfile->buffer) {
				spdlog::error("Failed to allocate buffer for: {}", rawfile->name);
				return nullptr;
			}

			file.read((char*)rawfile->buffer, size);
			file.close();

			// sometimes there are garbage characters at the end of the file
			// so we need to null-terminate it
			((char*)rawfile->buffer)[size] = '\0';
		}
	}
	return DB_LinkXAssetEntry(newEntry, allowOverride);
}

void gsc::init()
{
	DB_LinkXAssetEntry_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x100E9FA0), (uint64_t)DB_LinkXAssetEntry_stub, (uint64_t*)&DB_LinkXAssetEntry);
	DB_LinkXAssetEntry_hook->hook();
}