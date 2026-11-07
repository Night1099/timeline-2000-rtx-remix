#include "std_include.hpp"
#include "screen_space.hpp"

#include "shared/common/ffp_state.hpp"
#include "rigid_meshes.hpp"
#include "character_meshes.hpp"

namespace comp
{
	namespace
	{
		// Pointer chain to the camera of the scene being drawn: [[[g_app] + 0xCB8] + 0x2C].
		// g_app is Timeline.exe's App*, App+0xCB8 its RMRender*, RMRender+0x2C the camera passed to
		// the current RMRender::BeginRenderScene (not cleared at EndRenderScene).
		constexpr uintptr_t G_APP = 0x4CED70;
		constexpr uintptr_t APP_RMRENDER = 0xCB8;
		constexpr uintptr_t RMRENDER_SCENE_CAMERA = 0x2C;

		// RMCamera fields. Engine matrices are row-vector, row-major (D3D layout), 16 floats + a tag dword.
		constexpr uintptr_t CAM_PROJECTION = 0x1E0;  // view -> pixel space
		constexpr uintptr_t CAM_NEAR = 0x2F0;
		constexpr uintptr_t CAM_VIEW = 0x31C;        // world -> camera
		constexpr uintptr_t CAM_PROJECTION_TYPE = 0xD94;  // 0 = perspective, 1 = parallel

		// Far plane for the projection handed to Remix. The engine has no far clip; its z mapping uses
		// 500000, and the level SkyBox models reach about 100000 units from the camera.
		constexpr float REMIX_FAR_PLANE = 500000.0f;

		// caesarrm bakes its CPU lighting and fog into the vertex colour (distant geometry arrives
		// black, fog supplies the colour). Remix lights the scene itself, so only alpha is kept.
		constexpr D3DCOLOR UNLIT_RGB = 0x00FFFFFF;

		// caesarrm's depth mapping is sz = Q * (1 - near / zv) with Q = K / (K - near), K = 500000, and
		// rhw = 1 / (zv * t). Genuine projections satisfy it to float precision (also after caesarim's
		// screen-space clipping, which interpolates sz and rhw linearly); stale pool slots do not.
		constexpr float ENGINE_Z_CONSTANT = 500000.0f;
		constexpr float DEPTH_TOLERANCE = 1e-3f;

		constexpr UINT TLVERTEX_STRIDE = 32;
		constexpr size_t MAX_OUTPUT_VERTICES = 0xFFFF;
		constexpr DWORD WORLD_VERTEX_FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
		constexpr DWORD PROBE_INTERVAL_MS = 2000;

		constexpr uintptr_t CAESARIM_BEGIN = 0x60100000;
		constexpr uintptr_t CAESARIM_END = 0x60140000;

		// Return address inside caesarim.dll that issued this draw (through dxwrapper), or 0.
		uintptr_t caesarim_call_site()
		{
			void* frames[16] = {};
			const USHORT n = CaptureStackBackTrace(1, 16, frames, nullptr);
			for (USHORT i = 0; i < n; i++)
			{
				const auto addr = reinterpret_cast<uintptr_t>(frames[i]);
				if (addr >= CAESARIM_BEGIN && addr < CAESARIM_END) return addr;
			}
			return 0;
		}

		struct raw_camera
		{
			float projection[16];
			float view[16];
			float near_plane;
			int projection_type;
		};

		// Copies the scene camera out of game memory. Returns false when the chain is not set up yet
		// (menus, loading) or points at unmapped memory.
		bool copy_scene_camera(raw_camera& out)
		{
			__try
			{
				const auto app = *reinterpret_cast<const uintptr_t*>(G_APP);
				if (!app) return false;
				const auto rm_render = *reinterpret_cast<const uintptr_t*>(app + APP_RMRENDER);
				if (!rm_render) return false;
				const auto cam = *reinterpret_cast<const uintptr_t*>(rm_render + RMRENDER_SCENE_CAMERA);
				if (!cam) return false;

				memcpy(out.projection, reinterpret_cast<const void*>(cam + CAM_PROJECTION), sizeof(out.projection));
				memcpy(out.view, reinterpret_cast<const void*>(cam + CAM_VIEW), sizeof(out.view));
				out.near_plane = *reinterpret_cast<const float*>(cam + CAM_NEAR);
				out.projection_type = *reinterpret_cast<const int*>(cam + CAM_PROJECTION_TYPE);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}
	}

	screen_space& screen_space::get()
	{
		static screen_space instance;
		return instance;
	}

	void screen_space::on_set_viewport(const D3DVIEWPORT9& viewport)
	{
		viewport_ = viewport;
	}

	void screen_space::on_set_transform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX& matrix)
	{
		if (state == D3DTS_VIEW) caller_view_ = matrix;
		else if (state == D3DTS_PROJECTION) caller_projection_ = matrix;
		else if (state == D3DTS_WORLD) caller_world_ = matrix;
	}

	bool screen_space::read_camera(camera_snapshot& cam)
	{
		raw_camera raw;
		if (!copy_scene_camera(raw) || raw.projection_type != 0) return false;

		const float t = raw.projection[11];  // P[2][3]
		if (!(t > 0.0f) || raw.projection[0] == 0.0f || raw.projection[5] == 0.0f) return false;

		cam.view = D3DXMATRIX(raw.view);
		cam.view._14 = cam.view._24 = cam.view._34 = 0.0f;  // affine; D3D requires the w column explicit
		cam.view._44 = 1.0f;
		if (!D3DXMatrixInverse(&cam.camera_world, nullptr, &cam.view)) return false;

		cam.scale_x = raw.projection[0];   // P[0][0]
		cam.scale_y = raw.projection[5];   // P[1][1]
		cam.center_x = raw.projection[8] / t;  // P[2][0]
		cam.center_y = raw.projection[9] / t;  // P[2][1]
		cam.tan_half_fov = t;
		cam.near_plane = raw.near_plane;
		return true;
	}

	D3DXVECTOR3 screen_space::to_world(const camera_snapshot& cam, const tl_vertex& v)
	{
		// Inverse of caesarrm's projection: sx = cx + P00 * xv / (zv * t), rhw = 1 / (zv * t).
		const D3DXVECTOR3 view_pos(
			(v.sx - cam.center_x) / (v.rhw * cam.scale_x),
			(v.sy - cam.center_y) / (v.rhw * cam.scale_y),
			1.0f / (v.rhw * cam.tan_half_fov));
		D3DXVECTOR3 world_pos;
		D3DXVec3TransformCoord(&world_pos, &view_pos, &cam.camera_world);
		return world_pos;
	}

	bool screen_space::collect_triangles(const batch& b)
	{
		const auto index_at = [&](UINT k) -> uint32_t {
			if (!b.indices) return k;
			return b.index_format == D3DFMT_INDEX16 ? static_cast<const uint16_t*>(b.indices)[k]
			                                        : static_cast<const uint32_t*>(b.indices)[k];
		};

		source_triangles_.clear();
		for (UINT p = 0; p < b.prim_count; p++)
		{
			switch (b.type)
			{
			case D3DPT_TRIANGLELIST:
				source_triangles_.insert(source_triangles_.end(), { index_at(p * 3), index_at(p * 3 + 1), index_at(p * 3 + 2) });
				break;
			case D3DPT_TRIANGLESTRIP:  // odd triangles swap their first two vertices to keep the winding
				if (p & 1) source_triangles_.insert(source_triangles_.end(), { index_at(p + 1), index_at(p), index_at(p + 2) });
				else       source_triangles_.insert(source_triangles_.end(), { index_at(p), index_at(p + 1), index_at(p + 2) });
				break;
			case D3DPT_TRIANGLEFAN:
				source_triangles_.insert(source_triangles_.end(), { index_at(0), index_at(p + 1), index_at(p + 2) });
				break;
			default:
				return false;
			}
		}
		return true;
	}

	bool screen_space::is_screen_ui(const batch& b) const
	{
		for (const uint32_t i : source_triangles_)
		{
			const float rhw = b.vertices[i].rhw;
			if (rhw < 0.999f || rhw > 1.001f) return false;
		}
		return true;
	}

	bool screen_space::is_valid_projection(const camera_snapshot& cam, const tl_vertex& v) const
	{
		if (!std::isfinite(v.sx) || !std::isfinite(v.sy) || !std::isfinite(v.sz) || !std::isfinite(v.rhw) || v.rhw == 0.0f)
			return false;
		const float q = ENGINE_Z_CONSTANT / (ENGINE_Z_CONSTANT - cam.near_plane);
		const float expected_sz = q * (1.0f - cam.near_plane * cam.tan_half_fov * v.rhw);
		return std::abs(v.sz - expected_sz) <= DEPTH_TOLERANCE * std::max(1.0f, std::abs(expected_sz));
	}

	UINT screen_space::build_world_triangles(const camera_snapshot& cam, const batch& b)
	{
		const uint32_t max_source = *std::max_element(source_triangles_.begin(), source_triangles_.end());
		remap_.assign(max_source + 1, -1);
		world_vertices_.clear();
		world_indices_.clear();

		const auto emit = [&](uint32_t source) {
			if (remap_[source] >= 0) return remap_[source];
			const tl_vertex& v = b.vertices[source];
			const D3DXVECTOR3 world_pos = to_world(cam, v);
			world_vertices_.push_back({ world_pos.x, world_pos.y, world_pos.z, (v.diffuse & 0xFF000000) | UNLIT_RGB, v.tu, v.tv });
			return remap_[source] = static_cast<int32_t>(world_vertices_.size() - 1);
		};

		UINT kept = 0;
		for (size_t t = 0; t + 2 < source_triangles_.size(); t += 3)
		{
			const uint32_t* tri = &source_triangles_[t];
			if (!is_valid_projection(cam, b.vertices[tri[0]]) || !is_valid_projection(cam, b.vertices[tri[1]]) ||
				!is_valid_projection(cam, b.vertices[tri[2]]))
				continue;
			if (world_vertices_.size() + 3 > MAX_OUTPUT_VERTICES) break;
			for (int k = 0; k < 3; k++) world_indices_.push_back(static_cast<uint16_t>(emit(tri[k])));
			kept++;
		}
		return kept;
	}

	void screen_space::probe(const batch& b, const camera_snapshot& cam, UINT kept_triangles)
	{
		const uintptr_t site = caesarim_call_site();
		const DWORD now = GetTickCount();
		auto& last = last_probe_tick_[site];
		if (last && now - last < PROBE_INTERVAL_MS) return;
		last = now;

		const UINT total = static_cast<UINT>(source_triangles_.size() / 3);
		shared::common::log("ScreenSpace", std::format(
			"site=0x{:08X} {} prim={} tris={} kept={} dropped={} verts_out={} cam: t={:.5g} near={:.5g} P00={:.6g} P11={:.6g} c=({:.1f},{:.1f}) eye=({:.1f},{:.1f},{:.1f}) vp={}x{}",
			site, b.indices ? "DIPUP" : "DPUP", static_cast<int>(b.type), total, kept_triangles, total - kept_triangles,
			world_vertices_.size(), cam.tan_half_fov, cam.near_plane, cam.scale_x, cam.scale_y, cam.center_x, cam.center_y,
			cam.camera_world._41, cam.camera_world._42, cam.camera_world._43, viewport_.Width, viewport_.Height));
	}

	void screen_space::begin_ffp(IDirect3DDevice9* dev, const camera_snapshot& cam, const D3DMATRIX& world)
	{
		// Projection reproducing the camera's raster on the D3D9 viewport:
		// ndc_x = (sx - vp_cx) / half_w with sx = cam.cx + P00 * xv / (zv * t), likewise for y (flipped).
		const float half_w = viewport_.Width * 0.5f;
		const float half_h = viewport_.Height * 0.5f;
		const float vp_cx = viewport_.X + half_w;
		const float vp_cy = viewport_.Y + half_h;
		const float t = cam.tan_half_fov;
		const float q = REMIX_FAR_PLANE / (REMIX_FAR_PLANE - cam.near_plane);

		const D3DMATRIX projection = {
			cam.scale_x / (t * half_w),         0.0f,                                0.0f,                  0.0f,
			0.0f,                               -cam.scale_y / (t * half_h),         0.0f,                  0.0f,
			(cam.center_x - vp_cx) / half_w,    -(cam.center_y - vp_cy) / half_h,    q,                     1.0f,
			0.0f,                               0.0f,                                -q * cam.near_plane,   0.0f
		};

		dev->SetVertexShader(nullptr);
		dev->SetTransform(D3DTS_WORLD, &world);
		dev->SetTransform(D3DTS_VIEW, &cam.view);
		dev->SetTransform(D3DTS_PROJECTION, &projection);
	}

	void screen_space::end_ffp(IDirect3DDevice9* dev)
	{
		const auto& ffp = shared::common::ffp_state::get();
		dev->SetTransform(D3DTS_WORLD, &caller_world_);
		dev->SetTransform(D3DTS_VIEW, &caller_view_);
		dev->SetTransform(D3DTS_PROJECTION, &caller_projection_);
		dev->SetVertexDeclaration(ffp.last_decl());
		dev->SetVertexShader(ffp.last_vs());
	}

	HRESULT screen_space::draw_rigid_mesh(IDirect3DDevice9* dev, const camera_snapshot& cam)
	{
		const auto& mesh = rigid_meshes::get();
		begin_ffp(dev, cam, mesh.world());
		dev->SetFVF(rigid_meshes::MESH_VERTEX_FVF);
		const HRESULT hr = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(mesh.vertices().size()),
			static_cast<UINT>(mesh.indices().size() / 3), mesh.indices().data(), D3DFMT_INDEX16,
			mesh.vertices().data(), sizeof(rigid_meshes::mesh_vertex));
		end_ffp(dev);
		return hr;
	}

	HRESULT screen_space::draw_character_mesh(IDirect3DDevice9* dev, const camera_snapshot& cam, IDirect3DVertexDeclaration9* declaration)
	{
		const auto& character = character_meshes::get();
		begin_ffp(dev, cam, shared::globals::IDENTITY);
		dev->SetVertexDeclaration(declaration);
		const auto& joints = character.joint_matrices();
		for (UINT slot = 0; slot < joints.size(); slot++)
			dev->SetTransform(D3DTS_WORLDMATRIX(slot), &joints[slot]);
		dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_1WEIGHTS);
		dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE);

		const HRESULT hr = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(character.vertices().size()),
			static_cast<UINT>(character.indices().size() / 3), character.indices().data(), D3DFMT_INDEX16,
			character.vertices().data(), sizeof(character_meshes::blend_vertex));

		dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
		dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
		end_ffp(dev);
		return hr;
	}

	HRESULT screen_space::draw(IDirect3DDevice9* dev, const batch& b, const std::function<HRESULT()>& passthrough)
	{
		camera_snapshot cam;
		if (!shared::common::ffp_state::get().is_enabled() || !read_camera(cam))
			return passthrough();

		if (b.indices)
		{
			const auto batch_diffuse = [&](UINT i) { return b.vertices[i].diffuse; };
			switch (rigid_meshes::get().build(b.num_vertices, batch_diffuse))
			{
			case rigid_meshes::result::skip: return S_OK;
			case rigid_meshes::result::ready: return draw_rigid_mesh(dev, cam);
			case rigid_meshes::result::not_rigid: break;
			}
			if (auto* declaration = character_meshes::get().declaration(dev))
			{
				switch (character_meshes::get().build(b.num_vertices, batch_diffuse))
				{
				case character_meshes::result::skip: return S_OK;
				case character_meshes::result::ready: return draw_character_mesh(dev, cam, declaration);
				case character_meshes::result::not_character: break;
				}
			}
		}

		if (!collect_triangles(b) || source_triangles_.empty() || is_screen_ui(b))
			return passthrough();

		const UINT kept = build_world_triangles(cam, b);
		probe(b, cam, kept);
		if (kept == 0) return S_OK;

		begin_ffp(dev, cam, shared::globals::IDENTITY);
		dev->SetFVF(WORLD_VERTEX_FVF);
		const HRESULT hr = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(world_vertices_.size()), kept,
			world_indices_.data(), D3DFMT_INDEX16, world_vertices_.data(), sizeof(world_vertex));
		end_ffp(dev);
		return hr;
	}

	HRESULT screen_space::draw_primitive_up(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT prim_count,
		const void* vertices, UINT stride)
	{
		const auto passthrough = [&] { return dev->DrawPrimitiveUP(type, prim_count, vertices, stride); };
		if (stride != TLVERTEX_STRIDE) return passthrough();
		return draw(dev, { type, prim_count, static_cast<const tl_vertex*>(vertices), nullptr, D3DFMT_UNKNOWN, 0 }, passthrough);
	}

	HRESULT screen_space::draw_indexed_primitive_up(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, UINT min_vertex,
		UINT num_vertices, UINT prim_count, const void* indices, D3DFORMAT index_format,
		const void* vertices, UINT stride)
	{
		const auto passthrough = [&] {
			return dev->DrawIndexedPrimitiveUP(type, min_vertex, num_vertices, prim_count, indices, index_format, vertices, stride);
		};
		if (stride != TLVERTEX_STRIDE || !indices) return passthrough();
		return draw(dev, { type, prim_count, static_cast<const tl_vertex*>(vertices), indices, index_format, num_vertices },
			passthrough);
	}
}
