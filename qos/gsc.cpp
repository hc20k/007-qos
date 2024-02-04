#include "gsc.h"
#include "shared.h"
#include "memory.h"
#include "structs.hpp"
#include "functions.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

using namespace game;

typedef unsigned int(__cdecl* Scr_LoadScriptInternal_t)(const char* filename, struct PrecacheEntry* entries, int entriesCount);
Scr_LoadScriptInternal_t Scr_LoadScriptInternal = 0;

typedef int(__cdecl* DB_FindXAssetHeader_t)(int type, const char* name, bool returnDefault);
DB_FindXAssetHeader_t DB_FindXAssetHeader = 0;

std::unique_ptr<PLH::x86Detour> Scr_LoadScriptInternal_hook;
std::unique_ptr<PLH::x86Detour> DB_LinkXAssetEntry_hook;
// unsigned int __cdecl Scr_LoadScriptInternal(const char *filename, PrecacheEntry *entries, int entriesCount)

// XAssetEntry *__cdecl DB_LinkXAssetEntry(XAssetEntry *newEntry, int allowOverride)
typedef XAssetEntry* (__cdecl* DB_LinkXAssetEntry_t)(XAssetEntry* newEntry, int allowOverride);
DB_LinkXAssetEntry_t DB_LinkXAssetEntry = 0;

XAssetEntry* DB_CreateDefaultEntry(int type, const char* name) {
	// int __usercall sub_100E98E0@<eax>(int a1@<eax>, char *a2)
	int t = type;
	auto func = memory::game_offset(0x100E98E0);
	XAssetEntry *entry = nullptr;

	__asm {
		mov eax, t
		push name
		call func
		mov entry, eax
	}

	return entry;
}

void load_user_scripts() {
	std::filesystem::path path = "scripts";

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_regular_file()) {
			auto filename = entry.path().filename().string();
			auto ext = entry.path().extension().string();

			if (ext == ".gsc") {
				auto script = entry.path().string();
				spdlog::info("Loading user script: {}", script);
				

				// use utility as a base, make a copy
				

				//if (!header) {
				//	spdlog::error("Failed to create default entry for: {}", script);
				//	continue;
				//}

				//// read file
				//std::ifstream file(script, std::ios::binary);
				//if (!file.is_open()) {
				//	spdlog::error("Failed to open file: {}", script);
				//	continue;
				//}

				//file.seekg(0, std::ios::end);
				//auto size = file.tellg();

				//header->asset.header.rawfile->len = size;
				//
				//file.seekg(0, std::ios::beg);
				//file.read((char*)header->asset.header.rawfile->buffer, size);
				//file.close();

				//spdlog::info("Loaded user script: {}", script);
			}
		}
	}
}

uint64_t o_Scr_LoadScriptInternal = 0;
unsigned int Scr_LoadScriptInternal_stub(const char* filename, struct PrecacheEntry* entries, int entriesCount)
{
	auto res =  ((unsigned int(__cdecl*)(const char*, struct PrecacheEntry*, int))o_Scr_LoadScriptInternal)(filename, entries, entriesCount);

	spdlog::info("Scr_LoadScriptInternal: {}.gsc", filename);
	auto mapname = Dvar_FindVar("mapname");

	if (!mapname)
		return res;
	
	auto mapname_str = (const char*)((uintptr_t)mapname + 0x14); // yeah whatever
	
	std::string script_override_name = "maps/" + std::string(mapname_str);

	if (strcmp(filename, script_override_name.data()))
		return res;

	// load user scripts
	load_user_scripts();
	return res;
}

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
	// hook Scr_LoadScriptInternal
	Scr_LoadScriptInternal_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x102D7A60), (uint64_t)Scr_LoadScriptInternal_stub, &o_Scr_LoadScriptInternal);
	Scr_LoadScriptInternal_hook->hook();
	Scr_LoadScriptInternal = (Scr_LoadScriptInternal_t)o_Scr_LoadScriptInternal;

	DB_FindXAssetHeader = (DB_FindXAssetHeader_t)memory::game_offset(0x100E9A90);
	DB_LinkXAssetEntry = (DB_LinkXAssetEntry_t)memory::game_offset(0x100E9FA0);

	DB_LinkXAssetEntry_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x100E9FA0), (uint64_t)DB_LinkXAssetEntry_stub, (uint64_t*)&DB_LinkXAssetEntry);
	DB_LinkXAssetEntry_hook->hook();
}