#include "std_include.hpp"
#include "model_draw_hook.hpp"

namespace comp::game
{
	namespace
	{
		// RMModel_DrawGroupNoClip(this = model; imRender, projected IMVertex*, u16* indices, nTris, nVerts,
		// flags, imTexture slot, texFlags, int* io), thiscall, ret 0x24. Shared with RMDeformationModel.
		constexpr uintptr_t DRAW_GROUP_NO_CLIP = 0x603221F0;
		constexpr uintptr_t RMMODEL_VTABLE = 0x603424F0;
		constexpr uintptr_t RMDEFORMATIONMODEL_VTABLE = 0x60342308;

		using draw_group_fn = int(__fastcall*)(void* model, void* edx, void* im_render, void* projected,
			const uint16_t* indices, int triangle_count, int vertex_count, int flags, int texture_slot, int tex_flags, int* io);
		draw_group_fn original_draw_group = nullptr;

		model_group_draw g_current = {};
		bool g_active = false;

		int __fastcall draw_group_detour(void* model, void* edx, void* im_render, void* projected,
			const uint16_t* indices, int triangle_count, int vertex_count, int flags, int texture_slot, int tex_flags, int* io)
		{
			const uintptr_t vtable = *static_cast<uintptr_t*>(model);
			if (vtable == RMMODEL_VTABLE || vtable == RMDEFORMATIONMODEL_VTABLE)
			{
				const auto kind = vtable == RMMODEL_VTABLE ? model_kind::rigid : model_kind::character;
				g_current = { kind, reinterpret_cast<uintptr_t>(model), indices, triangle_count, vertex_count, texture_slot };
				g_active = true;
			}
			const int result = original_draw_group(model, edx, im_render, projected, indices, triangle_count, vertex_count,
				flags, texture_slot, tex_flags, io);
			g_active = false;
			return result;
		}
	}

	void install_model_draw_hook()
	{
		const auto target = reinterpret_cast<void*>(DRAW_GROUP_NO_CLIP);
		if (MH_CreateHook(target, reinterpret_cast<void*>(&draw_group_detour), reinterpret_cast<void**>(&original_draw_group)) != MH_OK ||
			MH_EnableHook(target) != MH_OK)
		{
			shared::common::log("PathB", "failed to hook RMModel_DrawGroupNoClip 0x603221F0", shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return;
		}
		shared::common::log("PathB", "hooked RMModel_DrawGroupNoClip 0x603221F0");
	}

	const model_group_draw* current_model_draw()
	{
		return g_active ? &g_current : nullptr;
	}
}
