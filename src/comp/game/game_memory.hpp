#pragma once

namespace comp::game
{
	// Copies `size` bytes of game memory; false if any byte is unreadable (objects can be mid-teardown).
	bool read_memory(uintptr_t address, void* out, size_t size);

	template <typename T> bool read(uintptr_t address, T& out)
	{
		return read_memory(address, &out, sizeof(T));
	}

	// Expands an engine RMMatrix43 (rows 0..2 axes, row 3 translation; row-vector convention) to a D3D matrix.
	inline D3DXMATRIX expand_matrix43(const float (&m)[12])
	{
		return D3DXMATRIX(
			m[0], m[1], m[2], 0.0f,
			m[3], m[4], m[5], 0.0f,
			m[6], m[7], m[8], 0.0f,
			m[9], m[10], m[11], 1.0f);
	}
}
