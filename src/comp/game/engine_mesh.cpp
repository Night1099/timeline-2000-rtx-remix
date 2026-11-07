#include "std_include.hpp"
#include "engine_mesh.hpp"

#include "game_memory.hpp"

namespace comp::game
{
	namespace
	{
		constexpr uintptr_t MODEL_MESH = 0x20C;

		constexpr uintptr_t MESH_VERTICES = 0xD8;
		constexpr uintptr_t MESH_ALIASES = 0xEC;
		constexpr uintptr_t MESH_UNIQUE_VERTICES = 0xF0;
		constexpr uintptr_t MESH_TOTAL_VERTICES = 0xF4;
		constexpr uintptr_t MESH_GROUP_COUNT = 0x10C;
		constexpr uintptr_t MESH_GROUPS = 0x114;      // {u16* tris; u16* trisReversed; int nTris}, stride 0xC

		constexpr int MAX_GROUPS = 256;

		struct texture_group
		{
			const uint16_t* tris;
			const uint16_t* tris_reversed;
			int triangle_count;
		};
	}

	bool read_all_triangles(uintptr_t mesh, std::vector<uint16_t>& out)
	{
		uintptr_t groups = 0;
		int group_count = 0;
		if (!read(mesh + MESH_GROUP_COUNT, group_count) || !read(mesh + MESH_GROUPS, groups)) return false;
		for (int g = 0; g < group_count && g < MAX_GROUPS; g++)
		{
			texture_group group = {};
			if (!read(groups + g * sizeof(texture_group), group)) return false;
			const size_t first = out.size();
			out.resize(first + static_cast<size_t>(group.triangle_count) * 3);
			if (!read_memory(reinterpret_cast<uintptr_t>(group.tris), out.data() + first, (out.size() - first) * sizeof(uint16_t)))
				return false;
		}
		return true;
	}

	winding_estimate& winding_estimate::get()
	{
		static winding_estimate instance;
		return instance;
	}

	void winding_estimate::add(const engine_mesh& mesh)
	{
		for (size_t t = 0; t + 2 < mesh.group_indices.size(); t += 3)
		{
			const int a = mesh.group_indices[t], b = mesh.group_indices[t + 1], c = mesh.group_indices[t + 2];
			if (a >= mesh.total || b >= mesh.total || c >= mesh.total) continue;
			const D3DXVECTOR3 pa(mesh.position(a)), pb(mesh.position(b)), pc(mesh.position(c));
			const D3DXVECTOR3 ab = pb - pa, ac = pc - pa;
			D3DXVECTOR3 cross;
			D3DXVec3Cross(&cross, &ab, &ac);
			const D3DXVECTOR3 stored(mesh.normal(a));
			const float d = D3DXVec3Dot(&cross, &stored);
			if (d > 0.0f) agree_++;
			else if (d < 0.0f) disagree_++;
		}
	}

	bool read_engine_mesh(uintptr_t model, const uint16_t* draw_indices, engine_mesh& out)
	{
		uintptr_t vertex_array = 0, alias_array = 0, groups = 0;
		int group_count = 0;
		if (!read(model + MODEL_MESH, out.mesh) || !read(out.mesh + MESH_VERTICES, vertex_array) ||
			!read(out.mesh + MESH_ALIASES, alias_array) || !read(out.mesh + MESH_UNIQUE_VERTICES, out.unique) ||
			!read(out.mesh + MESH_TOTAL_VERTICES, out.total) || !read(out.mesh + MESH_GROUP_COUNT, group_count) ||
			!read(out.mesh + MESH_GROUPS, groups))
			return false;
		if (out.total <= 0 || out.total > 0xFFFF || out.unique < 0 || out.unique > out.total) return false;

		texture_group group = {};
		out.group = -1;
		for (int g = 0; g < group_count && g < MAX_GROUPS; g++)
		{
			if (!read(groups + g * sizeof(texture_group), group)) return false;
			if (group.tris == draw_indices || group.tris_reversed == draw_indices)
			{
				out.group = g;
				out.reversed_pass = group.tris_reversed == draw_indices;
				break;
			}
		}
		if (out.group < 0) return false;

		out.raw_vertices.resize(static_cast<size_t>(out.total) * engine_mesh::VERTEX_STRIDE);
		out.aliases.resize(out.total - out.unique);
		out.group_indices.resize(static_cast<size_t>(group.triangle_count) * 3);
		if (!read_memory(vertex_array, out.raw_vertices.data(), out.raw_vertices.size()) ||
			(!out.aliases.empty() && !read_memory(alias_array, out.aliases.data(), out.aliases.size() * sizeof(int))) ||
			!read_memory(reinterpret_cast<uintptr_t>(group.tris), out.group_indices.data(), out.group_indices.size() * sizeof(uint16_t)))
			return false;

		for (const int source : out.aliases)
			if (source < 0 || source >= out.total) return false;
		return true;
	}
}
