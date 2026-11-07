#pragma once

namespace comp::game::code_patch
{
	struct byte_patch
	{
		const char* name;
		uintptr_t address;
		std::vector<uint8_t> original;
		std::vector<uint8_t> replacement;
	};

	// Writes `replacement` if the bytes at `address` equal `original`; logs and skips otherwise.
	bool apply(const byte_patch& patch);

	// Points the rel32 of the E8 call at `site` (currently calling `expected_target`) to `new_target`.
	bool redirect_call(uintptr_t site, uintptr_t expected_target, uintptr_t new_target);

	// Overwrites the first 5 bytes at `address` (which must equal `original`) with `jmp new_target`.
	bool write_jump(const char* name, uintptr_t address, const std::array<uint8_t, 5>& original, uintptr_t new_target);

	// Copies `code` into fresh executable memory and returns its address.
	uintptr_t allocate_code(const std::vector<uint8_t>& code);
}
