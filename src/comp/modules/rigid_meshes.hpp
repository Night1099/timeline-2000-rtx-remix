#pragma once

#include "../game/engine_mesh.hpp"

namespace comp
{
	/*
	 * Path B for rigid models: supplies caesarrm's own model-space mesh and world matrix for the texture
	 * group being drawn, so the proxy can draw it instead of the un-projected screen-space batch.
	 *
	 * While caesarrm draws an RMModel texture group (recorded by game::model_draw_hook), the batch dxwrapper
	 * submits indexes the mesh's vertex array 1:1 (verified live). This class copies that mesh: positions
	 * (aliases take their source vertex position), normals and UVs from the engine, alpha from the batch
	 * (material opacity), and the group's forward triangle list. Static (MakeStatic) meshes are already in
	 * world space and get an identity world matrix. Remix then sees the same mesh data every frame.
	 */
	class rigid_meshes
	{
	public:
		struct mesh_vertex
		{
			float x, y, z;
			float nx, ny, nz;
			D3DCOLOR diffuse;
			float tu, tv;
		};
		static constexpr DWORD MESH_VERTEX_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

		enum class result
		{
			not_rigid,  // no rigid group draw in progress, or the mesh does not match the batch: use Path A
			skip,       // second (reversed-winding) pass of a two-sided model: draw nothing
			ready,      // vertices(), indices(), world() describe the mesh to draw
		};

		static rigid_meshes& get();

		void load_settings();

		// `batch_diffuse(i)` is the diffuse colour of vertex i in the batch being drawn.
		result build(UINT batch_vertex_count, const std::function<D3DCOLOR(UINT)>& batch_diffuse);

		const std::vector<mesh_vertex>& vertices() const { return vertices_; }
		const std::vector<uint16_t>& indices() const { return mesh_.group_indices; }
		const D3DXMATRIX& world() const { return world_; }

	private:
		bool enabled_ = true;
		game::engine_mesh mesh_;
		std::vector<mesh_vertex> vertices_;
		D3DXMATRIX world_;
		DWORD last_log_tick_ = 0;
		UINT drawn_since_log_ = 0;
	};
}
