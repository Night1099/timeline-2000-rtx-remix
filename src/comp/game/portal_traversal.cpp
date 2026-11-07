#include "std_include.hpp"
#include "portal_traversal.hpp"

#include "code_patch.hpp"

namespace comp::game
{
	namespace
	{
		constexpr uintptr_t RENDER_THROUGH_PORTALS = 0x60339F20;
		constexpr std::array<uint8_t, 5> RENDER_THROUGH_PORTALS_PROLOGUE = { 0x51, 0x55, 0x56, 0x8B, 0xF1 };

		// RMRoom fields
		constexpr uintptr_t ROOM_CONTEXT = 0x14;        // render context; +0x2C = per-scene stamp, +0x10 = transform-portal render flag
		constexpr uintptr_t ROOM_PORTALS = 0x15C;       // RMPortal*** (array at [[room+0x15C]])
		constexpr uintptr_t ROOM_PORTAL_COUNT = 0x160;
		constexpr uintptr_t ROOM_IM_RENDER = 0x150;
		constexpr uintptr_t ROOM_CAMERA = 0x154;
		constexpr uintptr_t CONTEXT_TRANSFORM_PORTALS = 0x10;
		constexpr uintptr_t CONTEXT_SCENE_STAMP = 0x2C;

		// RMPortal fields and virtuals
		constexpr uintptr_t PORTAL_IS_TRANSFORM = 0x6D8;  // mirror / teleport: Enter() moves the camera
		constexpr size_t PORTAL_VTBL_GET_OTHER_ROOM = 0x1C / 4;
		constexpr size_t ROOM_VTBL_RENDER = 0x2C / 4;

		constexpr uintptr_t PORTAL_PROJECTION_BUFFER = 0x6034D520;
		constexpr uintptr_t IMRENDER_POP_CLIP_IAT = 0x60342184;

		// Nesting is bounded by IMRender's clip stack (29 entries, one used by BeginScene).
		constexpr int MAX_PORTAL_DEPTH = 24;

		using is_enabled_from_fn = int(__thiscall*)(void* portal, void* room, void* camera);
		using project_fn = void(__thiscall*)(void* portal, void* camera, void* room, void* out_vertices, int* out_count);
		using get_other_room_fn = void*(__thiscall*)(void* portal, void* room);
		using traversed_fn = int(__thiscall*)(void* portal, void* other_room, int stamp);
		using mark_traversed_fn = void(__thiscall*)(void* portal, void* other_room, int stamp);
		using force_viewport_clip_fn = int(__thiscall*)(void* camera);
		using portal_enter_leave_fn = void(__thiscall*)(void* portal, void* camera, void* room);
		using room_render_fn = void(__thiscall*)(void* room, void* im_render, void* camera, int flags);
		using pop_clip_fn = void(__thiscall*)(void* im_render);

		const auto portal_is_enabled_from = reinterpret_cast<is_enabled_from_fn>(0x60333270);
		const auto portal_project = reinterpret_cast<project_fn>(0x60330710);
		const auto portal_already_traversed = reinterpret_cast<traversed_fn>(0x60330410);
		const auto portal_mark_traversed = reinterpret_cast<mark_traversed_fn>(0x603303A0);
		const auto camera_force_viewport_clip = reinterpret_cast<force_viewport_clip_fn>(0x60306E40);
		const auto portal_enter = reinterpret_cast<portal_enter_leave_fn>(0x60330910);
		const auto portal_leave = reinterpret_cast<portal_enter_leave_fn>(0x60330D70);

		template <typename T> T field(void* object, uintptr_t offset)
		{
			return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
		}

		template <typename Fn> Fn virtual_method(void* object, size_t index)
		{
			return reinterpret_cast<Fn>((*static_cast<void***>(object))[index]);
		}

		// Rooms rendered in the current scene. Reset when the engine's per-scene stamp changes.
		int g_scene_stamp = -1;
		int g_depth = 0;
		std::unordered_set<void*> g_rendered_rooms;
		std::vector<void*> g_last_scene_rooms;

		void __fastcall render_through_portals(void* room)
		{
			void* const context = field<void*>(room, ROOM_CONTEXT);
			const int stamp = field<int>(context, CONTEXT_SCENE_STAMP);
			if (stamp != g_scene_stamp)
			{
				g_scene_stamp = stamp;
				g_last_scene_rooms.assign(g_rendered_rooms.begin(), g_rendered_rooms.end());
				g_rendered_rooms.clear();
			}
			if (g_depth == 0) g_rendered_rooms.insert(room);

			auto** const portal_list = field<void***>(room, ROOM_PORTALS);
			if (!portal_list || g_depth >= MAX_PORTAL_DEPTH) return;

			void* const im_render = field<void*>(room, ROOM_IM_RENDER);
			void* const camera = field<void*>(room, ROOM_CAMERA);
			void** const portals = *portal_list;

			for (int i = 0; i < field<int>(room, ROOM_PORTAL_COUNT); i++)
			{
				void* const portal = portals[i];
				if (!portal_is_enabled_from(portal, room, camera)) continue;

				// Projection also refreshes portal state used by Enter() for transform portals.
				int projected = 0;
				portal_project(portal, camera, room, reinterpret_cast<void*>(PORTAL_PROJECTION_BUFFER), &projected);
				const bool is_transform = field<int>(portal, PORTAL_IS_TRANSFORM) != 0;
				if (is_transform && projected == 0) continue;

				void* const other = virtual_method<get_other_room_fn>(portal, PORTAL_VTBL_GET_OTHER_ROOM)(portal, room);
				if (!other || portal_already_traversed(portal, other, stamp)) continue;
				if (!is_transform && g_rendered_rooms.contains(other)) continue;

				if (!camera_force_viewport_clip(camera)) continue;  // clip stack full
				portal_mark_traversed(portal, other, stamp);
				if (!is_transform) g_rendered_rooms.insert(other);

				portal_enter(portal, camera, room);
				if (!is_transform || field<int>(context, CONTEXT_TRANSFORM_PORTALS))
				{
					g_depth++;
					virtual_method<room_render_fn>(other, ROOM_VTBL_RENDER)(other, im_render, camera, 0);
					g_depth--;
				}
				(*reinterpret_cast<pop_clip_fn*>(IMRENDER_POP_CLIP_IAT))(im_render);
				portal_leave(portal, camera, room);
			}
		}
	}

	const std::vector<void*>& rooms_rendered_last_scene()
	{
		return g_last_scene_rooms;
	}

	void install_portal_traversal()
	{
		code_patch::write_jump("RenderThroughPortals -> proxy traversal", RENDER_THROUGH_PORTALS,
			RENDER_THROUGH_PORTALS_PROLOGUE, reinterpret_cast<uintptr_t>(&render_through_portals));
	}
}
