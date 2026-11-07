#pragma once

#include "../game/engine_mesh.hpp"

namespace comp
{
	/*
	 * Path B for characters (RMDeformationModel): supplies the bind-pose mesh and joint matrices so the proxy
	 * draws characters with fixed-function indexed vertex blending instead of the un-projected batch.
	 *
	 * caesarrm skins on the CPU with one joint per vertex and no weights: the mesh's positions are stored in
	 * the local space of their joint (`frames[v]`, model+0x348) and the engine computes
	 * `world = local * joint+0xEC`. This class reproduces that on the GPU: one palette slot per joint,
	 * WORLDMATRIX(slot) = joint+0xEC, blend weight 1. Remix then gets an unchanging bind-pose mesh plus real
	 * bone matrices. Normals (the stored ones are not joint-local) are rebuilt once per mesh from the engine's
	 * skinned positions (model+0x344) and moved into joint space. Every draw self-checks a few vertices against
	 * the engine's skinned output and falls back to Path A on a mismatch.
	 */
	class character_meshes
	{
	public:
		struct blend_vertex
		{
			float x, y, z;
			float weights[3];          // (1, 0, 0): the single joint carries the whole vertex
			uint8_t joint_slot[4];     // all four = the vertex's palette slot
			float nx, ny, nz;
			D3DCOLOR diffuse;
			float tu, tv;
		};

		enum class result { not_character, skip, ready };

		static character_meshes& get();

		void load_settings();
		void on_begin_scene();

		result build(UINT batch_vertex_count, const std::function<D3DCOLOR(UINT)>& batch_diffuse);

		const std::vector<blend_vertex>& vertices() const { return vertices_; }
		const std::vector<uint16_t>& indices() const { return mesh_.group_indices; }
		const std::vector<D3DXMATRIX>& joint_matrices() const { return joint_matrices_; }
		IDirect3DVertexDeclaration9* declaration(IDirect3DDevice9* dev);

	private:
		bool read_joints(uintptr_t model);
		bool skin_matches_engine(uintptr_t model);
		const std::vector<D3DXVECTOR3>* joint_local_normals(uintptr_t model);
		void log_rate_limited(const std::string& message);

		bool enabled_ = true;
		game::engine_mesh mesh_;
		std::vector<uintptr_t> vertex_joints_;           // frames[v] for the unique vertices
		std::unordered_map<uintptr_t, uint8_t> slot_of_joint_;
		std::vector<D3DXMATRIX> joint_matrices_;         // by palette slot
		std::vector<blend_vertex> vertices_;
		std::unordered_map<uintptr_t, std::vector<D3DXVECTOR3>> normals_by_mesh_;
		std::set<std::pair<uintptr_t, int>> drawn_this_scene_;  // (model, group)
		IDirect3DVertexDeclaration9* declaration_ = nullptr;
		DWORD last_log_tick_ = 0;
		UINT drawn_since_log_ = 0;
	};
}
