#include "std_include.hpp"
#include "character_meshes.hpp"

#include "../game/game_memory.hpp"
#include "../game/model_draw_hook.hpp"
#include "shared/common/config.hpp"

namespace comp
{
	namespace
	{
		// RMDeformationModel
		constexpr uintptr_t MODEL_SKINNED_VERTICES = 0x344;  // RMVertex[total]: this frame's world positions
		constexpr uintptr_t MODEL_VERTEX_JOINTS = 0x348;     // RMFrame*[unique]: joint of each vertex

		// RMFrame (joint)
		constexpr uintptr_t JOINT_RENDER_WORLD = 0xEC;        // RMMatrix43 with scale: what the CPU skin uses
		constexpr uintptr_t JOINT_INVERSE_RENDER_WORLD = 0x140; // 4x4

		constexpr size_t MAX_JOINTS = 256;
		constexpr float SKIN_TOLERANCE = 0.5f;               // engine units
		constexpr uint64_t MIN_WINDING_SAMPLES = 1000;
		constexpr DWORD LOG_INTERVAL_MS = 5000;

		D3DXVECTOR3 transform(const float* position, const D3DXMATRIX& m)
		{
			D3DXVECTOR3 out;
			const D3DXVECTOR3 in(position);
			D3DXVec3TransformCoord(&out, &in, &m);
			return out;
		}
	}

	character_meshes& character_meshes::get()
	{
		static character_meshes instance;
		return instance;
	}

	void character_meshes::load_settings()
	{
		enabled_ = shared::common::config::get().get_bool("PathB", "Characters", true);
	}

	void character_meshes::on_begin_scene()
	{
		drawn_this_scene_.clear();
	}

	IDirect3DVertexDeclaration9* character_meshes::declaration(IDirect3DDevice9* dev)
	{
		if (declaration_) return declaration_;
		const D3DVERTEXELEMENT9 elements[] = {
			{ 0, offsetof(blend_vertex, x), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, offsetof(blend_vertex, weights), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
			{ 0, offsetof(blend_vertex, joint_slot), D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
			{ 0, offsetof(blend_vertex, nx), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, offsetof(blend_vertex, diffuse), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, offsetof(blend_vertex, tu), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};
		if (FAILED(dev->CreateVertexDeclaration(elements, &declaration_)))
		{
			shared::common::log("PathB", "failed to create the character vertex declaration", shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			declaration_ = nullptr;
		}
		return declaration_;
	}

	bool character_meshes::read_joints(uintptr_t model)
	{
		uintptr_t joint_array = 0;
		vertex_joints_.resize(mesh_.unique);
		if (!game::read(model + MODEL_VERTEX_JOINTS, joint_array) ||
			!game::read_memory(joint_array, vertex_joints_.data(), vertex_joints_.size() * sizeof(uintptr_t)))
			return false;

		slot_of_joint_.clear();
		joint_matrices_.clear();
		for (const uintptr_t joint : vertex_joints_)
		{
			if (slot_of_joint_.contains(joint)) continue;
			if (joint_matrices_.size() >= MAX_JOINTS) return false;
			float render_world[12];
			if (!game::read_memory(joint + JOINT_RENDER_WORLD, render_world, sizeof(render_world))) return false;
			slot_of_joint_[joint] = static_cast<uint8_t>(joint_matrices_.size());
			joint_matrices_.push_back(game::expand_matrix43(render_world));
		}
		return true;
	}

	bool character_meshes::skin_matches_engine(uintptr_t model)
	{
		uintptr_t skinned = 0;
		if (!game::read(model + MODEL_SKINNED_VERTICES, skinned) || mesh_.unique == 0) return false;
		for (const int v : { 0, mesh_.unique / 2, mesh_.unique - 1 })
		{
			D3DXVECTOR3 engine;
			if (!game::read_memory(skinned + v * game::engine_mesh::VERTEX_STRIDE, &engine, sizeof(engine))) return false;
			const D3DXVECTOR3 ours = transform(mesh_.position(v), joint_matrices_[slot_of_joint_.at(vertex_joints_[v])]);
			const D3DXVECTOR3 delta = ours - engine;
			if (D3DXVec3Length(&delta) > SKIN_TOLERANCE) return false;
		}
		return true;
	}

	const std::vector<D3DXVECTOR3>* character_meshes::joint_local_normals(uintptr_t model)
	{
		if (const auto it = normals_by_mesh_.find(mesh_.mesh); it != normals_by_mesh_.end()) return &it->second;

		const auto& winding = game::winding_estimate::get();
		if (winding.samples() < MIN_WINDING_SAMPLES) return nullptr;

		uintptr_t skinned = 0;
		std::vector<uint8_t> skinned_raw(static_cast<size_t>(mesh_.total) * game::engine_mesh::VERTEX_STRIDE);
		std::vector<uint16_t> triangles;
		if (!game::read(model + MODEL_SKINNED_VERTICES, skinned) ||
			!game::read_memory(skinned, skinned_raw.data(), skinned_raw.size()) || !game::read_all_triangles(mesh_.mesh, triangles))
			return nullptr;
		const auto world_position = [&](int i) {
			return D3DXVECTOR3(reinterpret_cast<const float*>(skinned_raw.data() + mesh_.position_source(i) * game::engine_mesh::VERTEX_STRIDE));
		};

		// Area-weighted normals in world space, accumulated on the position-owning vertex.
		std::vector<D3DXVECTOR3> world_normals(mesh_.unique, D3DXVECTOR3(0, 0, 0));
		for (size_t t = 0; t + 2 < triangles.size(); t += 3)
		{
			const int a = triangles[t], b = triangles[t + 1], c = triangles[t + 2];
			if (a >= mesh_.total || b >= mesh_.total || c >= mesh_.total) continue;
			const D3DXVECTOR3 pa = world_position(a), ab = world_position(b) - pa, ac = world_position(c) - pa;
			D3DXVECTOR3 face;
			D3DXVec3Cross(&face, &ab, &ac);
			face *= winding.sign();
			for (const int corner : { a, b, c }) world_normals[mesh_.position_source(corner)] += face;
		}

		// Into each vertex's joint space, so the blend matrix rotates them back.
		std::vector<D3DXVECTOR3> local(mesh_.unique);
		for (int v = 0; v < mesh_.unique; v++)
		{
			float inverse[16];
			if (!game::read_memory(vertex_joints_[v] + JOINT_INVERSE_RENDER_WORLD, inverse, sizeof(inverse))) return nullptr;
			D3DXVec3TransformNormal(&local[v], &world_normals[v], reinterpret_cast<const D3DXMATRIX*>(inverse));
			D3DXVec3Normalize(&local[v], &local[v]);
		}

		std::vector<D3DXVECTOR3> by_vertex(mesh_.total);
		for (int i = 0; i < mesh_.total; i++) by_vertex[i] = local[mesh_.position_source(i)];
		return &(normals_by_mesh_[mesh_.mesh] = std::move(by_vertex));
	}

	void character_meshes::log_rate_limited(const std::string& message)
	{
		if (const DWORD now = GetTickCount(); now - last_log_tick_ >= LOG_INTERVAL_MS)
		{
			shared::common::log("PathB", std::format("{} ({} character draws in the last {} ms)", message, drawn_since_log_, now - last_log_tick_));
			last_log_tick_ = now;
			drawn_since_log_ = 0;
		}
	}

	character_meshes::result character_meshes::build(UINT batch_vertex_count, const std::function<D3DCOLOR(UINT)>& batch_diffuse)
	{
		const auto* draw = game::current_model_draw();
		if (!enabled_ || !draw || draw->kind != game::model_kind::character) return result::not_character;

		const uintptr_t model = draw->model;
		if (!game::read_engine_mesh(model, draw->indices, mesh_) || static_cast<UINT>(mesh_.total) != batch_vertex_count)
			return result::not_character;
		// RenderA's second pass (two-sided) and the overlay pass redraw a group already submitted this scene.
		if (!drawn_this_scene_.insert({ model, mesh_.group }).second) return result::skip;

		if (!read_joints(model)) return result::not_character;
		if (!skin_matches_engine(model))
		{
			log_rate_limited("character skin self-check failed; drawing it screen-space");
			return result::not_character;
		}
		const auto* normals = joint_local_normals(model);
		if (!normals) return result::not_character;

		vertices_.resize(mesh_.total);
		for (int i = 0; i < mesh_.total; i++)
		{
			const float* position = mesh_.position(i);
			const float* uv = mesh_.uv(i);
			const uint8_t slot = slot_of_joint_.at(vertex_joints_[mesh_.position_source(i)]);
			const D3DXVECTOR3& n = (*normals)[i];
			vertices_[i] = {
				position[0], position[1], position[2],
				{ 1.0f, 0.0f, 0.0f },
				{ slot, slot, slot, slot },
				n.x, n.y, n.z,
				(batch_diffuse(i) & 0xFF000000) | 0x00FFFFFF,
				uv[0], uv[1]
			};
		}

		drawn_since_log_++;
		log_rate_limited(std::format("characters: {} joints in last mesh, winding sign {:+.0f} from {} samples",
			joint_matrices_.size(), game::winding_estimate::get().sign(), game::winding_estimate::get().samples()));
		return result::ready;
	}
}
