#include "core.h"
#include "memory.h"
#include "shared.h"
#include "misc_patches.h"
#include "gsc.h"
#include "structs.hpp"
#include <polyhook2/Detour/x86Detour.hpp>
#include "functions.hpp"

std::unique_ptr<PLH::x86Detour> open_dev_console_hook;
std::unique_ptr<PLH::x86Detour> Com_Printf_hook;
std::unique_ptr<PLH::x86Detour> R_EndFrame_hook;
std::unique_ptr<PLH::x86Detour> Dvar_AddCommands_hook;

HWND output_textbox = 0;

typedef void(__cdecl* Cbuf_AddText_t)(const char*, int);
Cbuf_AddText_t Cbuf_AddText_f = 0;

typedef void(__cdecl* CL_ForwardCommandToServer_t)(const char* cmd);
CL_ForwardCommandToServer_t CL_ForwardCommandToServer = 0;

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

void open_dev_console() {
	spdlog::info("Opening dev console");
	auto open_console_addr = memory::game_offset(0x104AD1C0);

	__asm { call open_console_addr }

	auto hwnd = *(HWND*)memory::game_offset(0x1208CED0);
	spdlog::info("Console Hwnd: {}", (void*)hwnd);

	output_textbox = *(HWND*)memory::game_offset(0x1208CED4);
	
	MSG msg;
	while (GetMessage(&msg, hwnd, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

int Com_Printf(int channel, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char buffer[1024];
	vsnprintf(buffer, 1024, fmt, args);
	va_end(args);

	if (output_textbox) {
		auto len = GetWindowTextLength(output_textbox);
		SendMessage(output_textbox, EM_SETSEL, len, len);
		SendMessage(output_textbox, EM_REPLACESEL, 0, (LPARAM)buffer);
		SendMessage(output_textbox, EM_REPLACESEL, 0, (LPARAM)"\r\n");

		SendMessage(output_textbox, EM_SCROLL, SB_BOTTOM, 0);
	}

	// remove trailing newline
	//if (strlen(buffer) > 0 &&
	//	buffer[strlen(buffer) - 1] == '\n') {
	//	buffer[strlen(buffer) - 1] = 0;	
	//}

	//spdlog::info(buffer);
	return 0;
}

bool setup_ui = false;
uint64_t R_EndFrame_o = 0;
void R_EndFrame_stub() {
		
	// call Con_DrawConsole (0x101CBF90)
	static auto Con_DrawConsole = (void(*)())memory::game_offset(0x101CBF90);
	Con_DrawConsole();
	
	reinterpret_cast<void(*)()>(R_EndFrame_o)();
}

uint64_t SaveGame_o = 0;
bool SaveGame_stub() {
	return 1;
}

void dump_rawfiles() {
	spdlog::info("Listing rawfiles");

	// 0x100E96A0 - DB_EnumXAssets
	// 0x100E9480 - DB_PrintAssetName

	//DB_EnumXAssets(XAssetType type, void (__cdecl *func)(XAssetHeader *__struct_ptr, void *), void *inData, _BOOL1 includeOverride)
	const auto DB_EnumXAssets = (void(__cdecl*)(int, void(__cdecl*)(game::XAssetHeader, void*), void*, bool))memory::game_offset(0x100E96A0);

	std::filesystem::path path = "dump";
	if (!std::filesystem::exists(path)) {
		std::filesystem::create_directory(path);
	}

	auto iter_rawfile = [](game::XAssetHeader header, void* data) {
		auto rawfile = header.rawfile;
		if (!rawfile) {
			return;
		}

		auto filename = fmt::format("dump/{}", rawfile->name);
		// create directories
		std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
		
		// write to file
		std::ofstream file(filename, std::ios::binary);
		if (!file.is_open()) {
			spdlog::error("Failed to open file: {}", filename);
			return;
		}

		file.write((char*)rawfile->buffer, rawfile->len);
		file.close();

		spdlog::info("Dumped rawfile: {}", filename);
	};


	DB_EnumXAssets(game::ASSET_TYPE_RAWFILE, iter_rawfile, NULL, 0);
}

void loadzone() {
	// this is pretty buggy lol
	auto argc = *(int*)memory::game_offset(0x11FA75F4);
	auto argv = *(int**)memory::game_offset(0x11FA7614);


	if (argc < 2) {
		spdlog::error("Usage: loadzone <zone>");
		return;
	}

	auto zone = (const char*)(argv[argc - 1]);
	spdlog::info("Loading zone: {}", zone);

	const auto DB_LoadXZone = (void(__cdecl*)(game::XZoneInfo*, uint32_t zonecount))memory::game_offset(0x100EA360);
	auto info = game::XZoneInfo{};
	info.name = zone;
	DB_LoadXZone(&info, 1);
}

uint64_t Dvar_AddCommands_o = 0;
void Dvar_AddCommands_stub() {
	reinterpret_cast<void(*)()>(Dvar_AddCommands_o)();

	// add custom commands
	Cmd_AddCommandInternal("dumpraw", dump_rawfiles);
	Cmd_AddCommandInternal("loadzone", loadzone);
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

	spdlog::info("jb_sp_s.dll: {}", (void*)jb);

	misc_patches::init();
	gsc::init();

	Cbuf_AddText_f = (Cbuf_AddText_t)memory::game_offset(0x103336B0);
	CL_ForwardCommandToServer = (CL_ForwardCommandToServer_t)memory::game_offset(0x101BA7B0);

	Dvar_AddCommands_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1033E5D0), (uint64_t)Dvar_AddCommands_stub, &Dvar_AddCommands_o);
	Dvar_AddCommands_hook->hook();

	uint64_t temp = 0;
	Com_Printf_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1032F620), (uint64_t)Com_Printf, &temp);
	Com_Printf_hook->hook();

	R_EndFrame_hook = std::make_unique<PLH::x86Detour>((uint64_t)memory::game_offset(0x1010CFA0), (uint64_t)R_EndFrame_stub, &R_EndFrame_o);
	//R_EndFrame_hook->hook();

	std::thread(open_dev_console).detach();
	Sleep(3000);
}