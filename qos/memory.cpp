#include "memory.h"

bool memory::nop(void* address, size_t size)
{
	DWORD oldProtect;
	if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;

	memset(address, 0x90, size);

	return VirtualProtect(address, size, oldProtect, &oldProtect);
}

bool memory::write(void* address, const void* data, size_t size)
{
	DWORD oldProtect;
	if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;

	memcpy(address, data, size);

	return VirtualProtect(address, size, oldProtect, &oldProtect);
}