#include "std_include.hpp"
#include "game_memory.hpp"

namespace comp::game
{
	bool read_memory(uintptr_t address, void* out, size_t size)
	{
		__try
		{
			memcpy(out, reinterpret_cast<const void*>(address), size);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
	}
}
