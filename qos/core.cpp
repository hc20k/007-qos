#include "core.h"
#include "memory.h"
#include "filesystem.h"
#include "shared.h"
#include "misc_patches.h"
#include "gsc.h"
#include "structs.hpp"
#include <polyhook2/Detour/x86Detour.hpp>

std::unique_ptr<PLH::x86Detour> open_dev_console_hook;
std::unique_ptr<PLH::x86Detour> Com_Printf_hook;
std::unique_ptr<PLH::x86Detour> DB_FindXAssetHeader_hook;
std::unique_ptr<PLH::x86Detour> R_EndFrame_hook;
std::unique_ptr<PLH::x86Detour> Dvar_AddCommands_hook;

typedef void(__cdecl* Cbuf_AddText_t)(const char*, int);
Cbuf_AddText_t Cbuf_AddText_f = 0;

typedef void(__cdecl* CL_ForwardCommandToServer_t)(const char* cmd);
CL_ForwardCommandToServer_t CL_ForwardCommandToServer = 0;

typedef int(__cdecl* DB_FindXAssetHeader_t)(int type, const char* name, bool returnDefault);
DB_FindXAssetHeader_t DB_FindXAssetHeader = 0;

void Cmd_AddCommandInternal(const char* name, void(__cdecl* function)()) {
	auto last_cmd_addr = (uint64_t)memory::game_offset(0x11FA7638);
	auto cmd = new game::cmd_function_s{};

	cmd->name = name;
	cmd->function = function;
	cmd->next = *(game::cmd_function_s**)last_cmd_addr;
	*(game::cmd_function_s**)last_cmd_addr = cmd;
}

bool process_command(const char* buffer) {
	// parse i2m [addr]
	// i2m = ida to memory

	if (strncmp(buffer, "i2m ", 4) == 0) {
		auto addr = std::stoull(buffer + 4, 0, 16);
		spdlog::info("ida: 0x{:X}, memory: 0x{:X}", addr, memory::game_offset(addr));
		return true;
	}

	return false;
}

void Cbuf_AddText(const char* text, int localClientNum) {
	__asm {
		mov eax, text
		mov ecx, localClientNum
		call Cbuf_AddText_f
	}
}

void read_console_thread() {
	while (1) {
		char buffer[1024];
		fgets(buffer, 1024, stdin);

		// first process commands
		if (process_command(buffer)) {
			memset(buffer, 0, 1024);
			continue;
		}

		if (!Cbuf_AddText) {
			spdlog::error("Cbuf_AddText is null");
			continue;
		}

		//CL_ForwardCommandToServer(buffer);
		Cbuf_AddText(buffer, 0);
	}
}

void show_console()
{
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONIN$", "r", stdin);
	
	SetConsoleTitle("qos console");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)read_console_thread, 0, 0, 0);
}

uint64_t o_wndproc = 0;
LRESULT dev_console_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	spdlog::info("wndproc: {}", uMsg);

	// call original wndproc
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void open_dev_console() {
	spdlog::info("Opening dev console");
	auto open_console_addr = memory::game_offset(0x104AD1C0);
	auto wndproc_addr = memory::game_offset(0x104ACC30);

	open_dev_console_hook = std::make_unique<PLH::x86Detour>((uint64_t)wndproc_addr, (uint64_t)dev_console_wndproc, &o_wndproc);
	open_dev_console_hook->hook();

	__asm {
		// takes no arguments
		call open_console_addr
	}
}

void wait_for_debugger() {
	while (1) {
		if (IsDebuggerPresent()) {
			break;
		}
		Sleep(500);
	}
}

int Com_Printf(int channel, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	vsnprintf(buffer, 1024, fmt, args);
	va_end(args);

	// remove trailing newline
	if (strlen(buffer) > 0 &&
		buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = 0;	
	}

	spdlog::info(buffer);
	return 0;
}

uint64_t DB_FindXAssetHeader_o = 0;
int DB_FindXAssetHeader_stub(int type, const char* name, bool returnDefault) {
	//spdlog::info("DB_FindXAssetHeader: type: {}, name: {}", type, name);
	
	return DB_FindXAssetHeader(type, name, type);
}

bool setup_ui = false;
uint64_t R_EndFrame_o = 0;
void R_EndFrame_stub() {
		
	// call Con_DrawConsole (0x101CBF90)
	static auto Con_DrawConsole = (void(*)())memory::game_offset(0x101CBF90);
	Con_DrawConsole();
	
	reinterpret_cast<void(*)()>(R_EndFrame_o)();
}

void list_assets() {
	spdlog::info("Listing assets");

	// 0x100E96A0 - DB_EnumXAssets
	// 0x100E9480 - DB_PrintAssetName

	//DB_EnumXAssets(XAssetType type, void (__cdecl *func)(XAssetHeader *__struct_ptr, void *), void *inData, _BOOL1 includeOverride)
	auto DB_EnumXAssets = (void(__cdecl*)(int, void(__cdecl*)(game::XAssetHeader*, void*), void*, bool))memory::game_offset(0x100E96A0);
	auto DB_PrintAssetName = (void(__cdecl*)(game::XAssetHeader*, void*))memory::game_offset(0x100E9480);

	for (int i = 0; i < 20; i++) {
		spdlog::info("--------- Type: {} ----------", i);
		int type = i;
		DB_EnumXAssets(type, DB_PrintAssetName, &type, 0);
	}
}

uint64_t Dvar_AddCommands_o = 0;
void Dvar_AddCommands_stub() {
	reinterpret_cast<void(*)()>(Dvar_AddCommands_o)();

	// add custom commands
	Cmd_AddCommandInternal("listassets", list_assets);
}

void core::entrypoint(HMODULE mod)
{
	show_console();

	spdlog::info("Waiting for jb_sp_s.dll");

	auto jb = GetModuleHandle("jb_sp_s.dll");
	while (jb == NULL)
	{
		Sleep(1);
	}
	
	assert (jb != NULL);
	memory::game = jb;

	spdlog::info("jb_sp_s.dll found!");

	filesystem::init();
	misc_patches::init();
	gsc::init();

	Cbuf_AddText_f = (Cbuf_AddText_t)memory::game_offset(0x103336B0);
	CL_ForwardCommandToServer = (CL_ForwardCommandToServer_t)memory::game_offset(0x101BA7B0);

	Dvar_AddCommands_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1033E5D0), (uint64_t)Dvar_AddCommands_stub, &Dvar_AddCommands_o);
	Dvar_AddCommands_hook->hook();

	uint64_t temp = 0;
	Com_Printf_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1032F620), (uint64_t)Com_Printf, &temp);
	Com_Printf_hook->hook();

	DB_FindXAssetHeader_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x100E9A90), (uint64_t)DB_FindXAssetHeader_stub, &DB_FindXAssetHeader_o);
	DB_FindXAssetHeader_hook->hook();
	DB_FindXAssetHeader = (DB_FindXAssetHeader_t)DB_FindXAssetHeader_o;

	R_EndFrame_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1010CFA0), (uint64_t)R_EndFrame_stub, &R_EndFrame_o);
	//R_EndFrame_hook->hook();

	Sleep(3000);
}