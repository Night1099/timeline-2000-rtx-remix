#pragma once

namespace comp::game
{
	/*
	 * A copy of an RMModel's RMMesh (caesarrm) and the texture group currently being drawn.
	 *
	 * Vertices are RMVertex (0x34 bytes: position +0, normal +0xC, colour +0x18, specular +0x1C, uv +0x20).
	 * The first `unique` vertices have their own positions; alias j (vertex unique + j) splits a UV seam
	 * and takes its position from vertex `aliases[j]`. Group triangle lists index all `total` vertices.
	 */
	struct engine_mesh
	{
		uintptr_t mesh = 0;
		int unique = 0;
		int total = 0;
		std::vector<uint8_t> raw_vertices;   // total * VERTEX_STRIDE bytes
		std::vector<int> aliases;            // total - unique entries
		std::vector<uint16_t> group_indices; // forward triangle list of the drawn group
		int group = -1;
		bool reversed_pass = false;          // the engine passed the group's reversed-winding list

		static constexpr size_t VERTEX_STRIDE = 0x34;

		// Index of the vertex whose position vertex i uses.
		int position_source(int i) const { return i < unique ? i : aliases[i - unique]; }
		const float* position(int i) const { return field(position_source(i), 0x0); }
		const float* normal(int i) const { return field(position_source(i), 0xC); }
		const float* uv(int i) const { return field(i, 0x20); }

	private:
		const float* field(int vertex, size_t offset) const
		{
			return reinterpret_cast<const float*>(raw_vertices.data() + vertex * VERTEX_STRIDE + offset);
		}
	};

	// Reads `model`'s mesh and finds the group whose forward or reversed list is `draw_indices`.
	// False if the model, mesh or group cannot be read or matched, or the mesh has more than 0xFFFF vertices.
	bool read_engine_mesh(uintptr_t model, const uint16_t* draw_indices, engine_mesh& out);

	// Appends the forward triangle lists of every texture group of `mesh` to `out`.
	bool read_all_triangles(uintptr_t mesh, std::vector<uint16_t>& out);

	/*
	 * Which way caesarrm winds triangles relative to their normals, measured on rigid meshes (whose stored
	 * normals are correct): +1 if cross(b - a, c - a) points along the stored normals, -1 otherwise.
	 */
	class winding_estimate
	{
	public:
		static winding_estimate& get();
		void add(const engine_mesh& mesh);
		float sign() const { return agree_ >= disagree_ ? 1.0f : -1.0f; }
		uint64_t samples() const { return agree_ + disagree_; }
		bool settled() const { return samples() >= SETTLED_SAMPLES; }
		static constexpr uint64_t SETTLED_SAMPLES = 100000;

	private:
		uint64_t agree_ = 0;
		uint64_t disagree_ = 0;
	};
}
