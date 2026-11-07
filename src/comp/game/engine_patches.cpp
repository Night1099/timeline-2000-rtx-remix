#include "std_include.hpp"
#include "engine_patches.hpp"

#include "code_patch.hpp"
#include "portal_traversal.hpp"
#include "shared/common/config.hpp"

namespace comp::game
{
	namespace
	{
		using code_patch::byte_patch;

		constexpr uintptr_t CAESARRM_BASE = 0x60300000;
		constexpr uintptr_t CAESARIM_BASE = 0x60100000;

		// --- Object culling: RMCamera::BoundsCheck (caesarrm 0x60305880) decides per object whether it is
		// drawn whole (0), 2D-clipped (1), culled (2) or clipped per face (3), and writes the screen area
		// that selects the LOD. The render paths call a replacement that reports "whole mesh" with a huge
		// area (finest LOD); mouse picking and the shadow pass keep calling the original.
		constexpr uintptr_t BOUNDS_CHECK = 0x60305880;
		const std::vector<uint8_t> WHOLE_MESH_BOUNDS_CHECK = {
			0x8B, 0x44, 0x24, 0x78,              // mov eax, [esp+0x78]   ; out: screen area (may be null)
			0x85, 0xC0,                          // test eax, eax
			0x74, 0x06,                          // jz +6
			0xC7, 0x00, 0x40, 0x42, 0x0F, 0x00,  // mov dword [eax], 1000000
			0x33, 0xC0,                          // xor eax, eax          ; 0 = draw whole mesh, no clipping
			0xC2, 0x78, 0x00,                    // ret 0x78
		};
		constexpr uintptr_t BOUNDS_CHECK_RENDER_CALLS[] = {
			0x6032255C, 0x603238E6,              // RMModel renderers
			0x60313C44, 0x60314A0D, 0x6031632B,  // character renderers
		};

		const std::vector<byte_patch> OBJECT_CULLING_PATCHES = {
			// Whole meshes would otherwise be redrawn once per projected-shadow caster on every receiver.
			{ "projected shadows off", 0x6033941B, { 0x7D, 0x33 }, { 0xEB, 0x33 } },
			// Room quadtrees (RMRender::SetGregEnable) test node boxes with the original BoundsCheck and
			// leave culled subtrees out of the room's draw list; skip the node test.
			{ "room quadtree node test", 0x6033F2B7, { 0x74, 0x41 }, { 0xEB, 0x41 } },
			// Particles are forced onto the per-face clip path; give them the whole-mesh path.
			{ "particles no-clip", 0x6032B540, { 0x6A, 0x04 }, { 0x6A, 0x00 } },
			// Progressive meshes without an LOD table coarsen with distance (beyond 500 units:
			// lod = coarsest * (1 - 250000 / d^2)); always take the near branch, which selects LOD 0 (finest).
			{ "progressive mesh distance LOD", 0x6032F8DA, { 0x75, 0x0E }, { 0xEB, 0x0E } },
		};

		const std::vector<byte_patch> BACKFACE_PATCHES = {
			{ "rm no-clip backface filter", 0x6032227E, { 0x74, 0x31 }, { 0x90, 0x90 } },
			{ "im DrawPoly backface",       0x6010CBE2, { 0x75, 0x09 }, { 0xEB, 0x09 } },
			{ "im DrawPolyNoClip backface", 0x6010CCAC, { 0x75, 0x05 }, { 0xEB, 0x05 } },
			{ "im DrawFan backface",        0x60103858, { 0x75, 0x06 }, { 0xEB, 0x06 } },
		};

		const std::vector<byte_patch> PORTAL_RANGE_PATCHES = {
			{ "portal distance cutoff A", 0x6033328F, { 0x75, 0x19 }, { 0xEB, 0x19 } },
			{ "portal distance cutoff B", 0x603332CC, { 0x75, 0x19 }, { 0xEB, 0x19 } },
		};

		void disable_object_culling()
		{
			const uintptr_t whole_mesh = code_patch::allocate_code(WHOLE_MESH_BOUNDS_CHECK);
			for (const uintptr_t site : BOUNDS_CHECK_RENDER_CALLS)
				code_patch::redirect_call(site, BOUNDS_CHECK, whole_mesh);
			for (const auto& p : OBJECT_CULLING_PATCHES)
				code_patch::apply(p);
		}

		void disable_portal_culling()
		{
			install_portal_traversal();
			for (const auto& p : PORTAL_RANGE_PATCHES)
				code_patch::apply(p);
		}

		bool module_at(const char* name, uintptr_t base)
		{
			if (reinterpret_cast<uintptr_t>(GetModuleHandleA(name)) == base) return true;
			shared::common::log("EnginePatch", std::format("{} not loaded at 0x{:08X}; culling patches skipped", name, base),
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}
	}

	void apply_engine_patches()
	{
		if (!module_at("caesarrm.dll", CAESARRM_BASE) || !module_at("caesarim.dll", CAESARIM_BASE))
			return;

		const auto& cfg = shared::common::config::get();
		if (cfg.get_bool("Culling", "DisableObjectCulling", true))
			disable_object_culling();
		if (cfg.get_bool("Culling", "DisableBackfaceCulling", true))
			for (const auto& p : BACKFACE_PATCHES) code_patch::apply(p);
		if (cfg.get_bool("Culling", "DisablePortalCulling", true))
			disable_portal_culling();
	}
}
