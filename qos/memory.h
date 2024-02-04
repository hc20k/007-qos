#pragma once
#include "shared.h"
#include <assert.h>

namespace memory
{
	static inline HMODULE game = NULL;
	bool nop(void* address, size_t size);
	bool write(void* address, const void* data, size_t size);

	inline uintptr_t game_offset(uintptr_t ida_address)
	{
		assert (game != NULL);
		return ida_address - 0x10000000 + (uintptr_t)game;
	}
};