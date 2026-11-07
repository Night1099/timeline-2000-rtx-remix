#pragma once

namespace comp
{
	/*
	 * Converts the game's screen-space geometry back into world space for RTX Remix.
	 *
	 * caesarrm.dll projects every vertex on the CPU with the active RMCamera and caesarim.dll draws the
	 * result as D3DTLVERTEX. dxwrapper forwards those batches as DrawPrimitiveUP / DrawIndexedPrimitiveUP
	 * (stride 32) with its own pixel->clip shader, which Remix treats as UI. For perspective batches this
	 * class inverts the camera's projection, lifts the vertices into world space and draws them through
	 * the fixed-function pipeline with WORLD = identity, VIEW = the camera's view matrix and a projection
	 * equivalent to the camera's, so the raster is unchanged and Remix receives real world geometry.
	 *
	 * Every batch is re-emitted as a compact indexed triangle list containing only referenced vertices:
	 * caesarrm reuses its vertex pools and leaves stale slots for faces it did not project this time, and
	 * triangles touching a vertex that is not a valid projection by the current camera are dropped.
	 * Screen-space UI (rhw == 1) and draws made without a perspective camera pass through untouched.
	 */
	class screen_space
	{
	public:
		static screen_space& get();

		void on_set_viewport(const D3DVIEWPORT9& viewport);
		void on_set_transform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX& matrix);

		HRESULT draw_primitive_up(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT prim_count,
			const void* vertices, UINT stride);

		HRESULT draw_indexed_primitive_up(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT min_vertex,
			UINT num_vertices, UINT prim_count, const void* indices, D3DFORMAT index_format,
			const void* vertices, UINT stride);

	private:
		struct tl_vertex
		{
			float sx, sy, sz, rhw;
			D3DCOLOR diffuse, specular;
			float tu, tv;
		};

		struct world_vertex
		{
			float x, y, z;
			D3DCOLOR diffuse;
			float tu, tv;
		};

		// State of the RMCamera that projected the batch being drawn, copied from game memory.
		struct camera_snapshot
		{
			D3DXMATRIX view;          // world -> camera (RMCamera+0x31C)
			D3DXMATRIX camera_world;  // inverse of view
			float scale_x, scale_y;   // projection P[0][0], P[1][1] (pixels; P[1][1] < 0)
			float center_x, center_y; // projection centre in pixels: P[2][0] / t, P[2][1] / t
			float tan_half_fov;       // t = P[2][3]
			float near_plane;
		};

		// A TLVERTEX batch decoded into triangles over source vertex indices.
		struct batch
		{
			D3DPRIMITIVETYPE type;
			UINT prim_count;
			const tl_vertex* vertices;
			const void* indices;      // nullptr for non-indexed draws
			D3DFORMAT index_format;
			UINT num_vertices;        // DrawIndexedPrimitiveUP vertex count (0 for non-indexed draws)
		};

		static bool read_camera(camera_snapshot& cam);
		static D3DXVECTOR3 to_world(const camera_snapshot& cam, const tl_vertex& v);
		bool collect_triangles(const batch& b);
		bool is_screen_ui(const batch& b) const;
		bool is_valid_projection(const camera_snapshot& cam, const tl_vertex& v) const;
		UINT build_world_triangles(const camera_snapshot& cam, const batch& b);
		void probe(const batch& b, const camera_snapshot& cam, UINT kept_triangles);
		HRESULT draw(IDirect3DDevice9* dev, const batch& b, const std::function<HRESULT()>& passthrough);
		void begin_ffp(IDirect3DDevice9* dev, const camera_snapshot& cam, const D3DMATRIX& world);
		HRESULT draw_rigid_mesh(IDirect3DDevice9* dev, const camera_snapshot& cam);
		HRESULT draw_character_mesh(IDirect3DDevice9* dev, const camera_snapshot& cam, IDirect3DVertexDeclaration9* declaration);
		void end_ffp(IDirect3DDevice9* dev);

		D3DVIEWPORT9 viewport_ = { 0, 0, 1024, 768, 0.0f, 1.0f };

		std::vector<uint32_t> source_triangles_;  // 3 source vertex indices per triangle
		std::vector<int32_t> remap_;              // source vertex -> output vertex (-1 = not emitted)
		std::vector<world_vertex> world_vertices_;
		std::vector<uint16_t> world_indices_;

		std::unordered_map<uintptr_t, DWORD> last_probe_tick_;
		// Last VIEW / PROJECTION / WORLD the caller (dxwrapper) set, restored after each converted draw.
		D3DMATRIX caller_view_ = shared::globals::IDENTITY;
		D3DMATRIX caller_projection_ = shared::globals::IDENTITY;
		D3DMATRIX caller_world_ = shared::globals::IDENTITY;
	};
}
