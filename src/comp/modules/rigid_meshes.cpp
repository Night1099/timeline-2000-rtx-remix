#include "std_include.hpp"
#include "rigid_meshes.hpp"

#include "../game/engine_mesh.hpp"
#include "../game/game_memory.hpp"
#include "../game/model_draw_hook.hpp"
#include "shared/common/config.hpp"

namespace comp
{
	namespace
	{
		constexpr uintptr_t MODEL_WORLD = 0xEC;        // RMMatrix43 (with scale)
		constexpr uintptr_t MODEL_IS_STATIC = 0x188;   // mesh already baked into world space (MakeStatic)
		constexpr uintptr_t MODEL_SHADE_FLAGS = 0x1FC;
		constexpr int SHADE_TWO_SIDED = 0x400;         // drawn twice: reversed winding, then forward

		constexpr DWORD LOG_INTERVAL_MS = 5000;
	}

	rigid_meshes& rigid_meshes::get()
	{
		static rigid_meshes instance;
		return instance;
	}

	void rigid_meshes::load_settings()
	{
		enabled_ = shared::common::config::get().get_bool("PathB", "Enabled", true);
	}

	rigid_meshes::result rigid_meshes::build(UINT batch_vertex_count, const std::function<D3DCOLOR(UINT)>& batch_diffuse)
	{
		const auto* draw = game::current_model_draw();
		if (!enabled_ || !draw || draw->kind != game::model_kind::rigid) return result::not_rigid;

		int is_static = 0, shade_flags = 0;
		float world43[12];
		if (!game::read(draw->model + MODEL_IS_STATIC, is_static) || !game::read(draw->model + MODEL_SHADE_FLAGS, shade_flags) ||
			!game::read_memory(draw->model + MODEL_WORLD, world43, sizeof(world43)) ||
			!game::read_engine_mesh(draw->model, draw->indices, mesh_) ||
			static_cast<UINT>(mesh_.total) != batch_vertex_count)
			return result::not_rigid;
		if (mesh_.reversed_pass && (shade_flags & SHADE_TWO_SIDED)) return result::skip;
		if (auto& winding = game::winding_estimate::get(); !winding.settled()) winding.add(mesh_);

		vertices_.resize(mesh_.total);
		for (int i = 0; i < mesh_.total; i++)
		{
			const float* position = mesh_.position(i);
			const float* normal = mesh_.normal(i);
			const float* uv = mesh_.uv(i);
			vertices_[i] = {
				position[0], position[1], position[2],
				normal[0], normal[1], normal[2],
				(batch_diffuse(i) & 0xFF000000) | 0x00FFFFFF,
				uv[0], uv[1]
			};
		}

		if (is_static) D3DXMatrixIdentity(&world_);
		else world_ = game::expand_matrix43(world43);

		drawn_since_log_++;
		if (const DWORD now = GetTickCount(); now - last_log_tick_ >= LOG_INTERVAL_MS)
		{
			shared::common::log("PathB", std::format("{} rigid mesh draws in the last {} ms", drawn_since_log_, now - last_log_tick_));
			last_log_tick_ = now;
			drawn_since_log_ = 0;
		}
		return result::ready;
	}
}
