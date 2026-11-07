#include "std_include.hpp"
#include "code_patch.hpp"

namespace comp::game::code_patch
{
	namespace
	{
		void write_code(uintptr_t address, const void* bytes, size_t size)
		{
			DWORD old_protect = 0;
			VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old_protect);
			memcpy(reinterpret_cast<void*>(address), bytes, size);
			VirtualProtect(reinterpret_cast<void*>(address), size, old_protect, &old_protect);
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
		}

		bool matches(const char* name, uintptr_t address, const uint8_t* original, size_t size)
		{
			if (memcmp(reinterpret_cast<const void*>(address), original, size) == 0) return true;
			shared::common::log("EnginePatch", std::format("{} @ 0x{:08X}: original bytes differ, skipped", name, address),
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}
	}

	bool apply(const byte_patch& patch)
	{
		if (!matches(patch.name, patch.address, patch.original.data(), patch.original.size())) return false;
		write_code(patch.address, patch.replacement.data(), patch.replacement.size());
		shared::common::log("EnginePatch", std::format("{} @ 0x{:08X}", patch.name, patch.address));
		return true;
	}

	bool redirect_call(uintptr_t site, uintptr_t expected_target, uintptr_t new_target)
	{
		const auto* code = reinterpret_cast<const uint8_t*>(site);
		const uintptr_t current_target = site + 5 + *reinterpret_cast<const int32_t*>(code + 1);
		if (code[0] != 0xE8 || current_target != expected_target)
		{
			shared::common::log("EnginePatch", std::format("call @ 0x{:08X}: not a call to 0x{:08X}, skipped", site, expected_target),
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}
		const int32_t rel = static_cast<int32_t>(new_target - (site + 5));
		write_code(site + 1, &rel, sizeof(rel));
		shared::common::log("EnginePatch", std::format("call @ 0x{:08X} -> 0x{:08X}", site, new_target));
		return true;
	}

	bool write_jump(const char* name, uintptr_t address, const std::array<uint8_t, 5>& original, uintptr_t new_target)
	{
		if (!matches(name, address, original.data(), original.size())) return false;
		uint8_t jump[5] = { 0xE9 };
		const int32_t rel = static_cast<int32_t>(new_target - (address + 5));
		memcpy(jump + 1, &rel, sizeof(rel));
		write_code(address, jump, sizeof(jump));
		shared::common::log("EnginePatch", std::format("{} @ 0x{:08X} -> 0x{:08X}", name, address, new_target));
		return true;
	}

	uintptr_t allocate_code(const std::vector<uint8_t>& code)
	{
		void* memory = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		memcpy(memory, code.data(), code.size());
		FlushInstructionCache(GetCurrentProcess(), memory, code.size());
		return reinterpret_cast<uintptr_t>(memory);
	}
}
