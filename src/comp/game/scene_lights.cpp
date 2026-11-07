#include "std_include.hpp"
#include "scene_lights.hpp"

#include "portal_traversal.hpp"
#include "shared/common/config.hpp"

namespace comp::game
{
	namespace
	{
		// Scene camera's room: [[[g_app] + 0xCB8] + 0x2C] -> RMCamera, RMCamera+0x19C -> container (room).
		constexpr uintptr_t G_APP = 0x4CED70;
		constexpr uintptr_t APP_RMRENDER = 0xCB8;
		constexpr uintptr_t RMRENDER_SCENE_CAMERA = 0x2C;
		constexpr uintptr_t FRAME_CONTAINER = 0x19C;

		// RMContainer (room) light list
		constexpr uintptr_t ROOM_LIGHTS = 0x70;
		constexpr uintptr_t ROOM_LIGHT_COUNT = 0x138;
		constexpr int MAX_ROOM_LIGHTS = 49;

		// RMLight
		constexpr uintptr_t OBJECT_KIND = 0xC;
		constexpr int OBJECT_KIND_LIGHT = 9;
		constexpr uintptr_t LIGHT_DIRECTION = 0xD4;    // float3, world, light travels along +dir
		constexpr uintptr_t LIGHT_POSITION = 0x110;    // float3, world
		constexpr uintptr_t LIGHT_ENABLED = 0x1C8;
		constexpr uintptr_t LIGHT_FALLOFF_ENABLED = 0x1CC;
		constexpr uintptr_t LIGHT_TYPE = 0x1D0;        // 0 point, 1 spot, 2 directional, 3 ambient
		constexpr uintptr_t LIGHT_COLOR = 0x1D8;       // float3 = colour byte * intensity / 255
		constexpr uintptr_t LIGHT_UMBRA = 0x1E8;       // inner half-angle, radians
		constexpr uintptr_t LIGHT_PENUMBRA = 0x1EC;    // outer half-angle, radians
		constexpr uintptr_t LIGHT_ATTENUATION = 0x1FC; // {a, b, c, range}: linear ramps, 0 beyond range

		constexpr DWORD LOG_INTERVAL_MS = 5000;

		struct raw_light
		{
			int kind, enabled, falloff_enabled, type;
			float direction[3], position[3], color[3];
			float umbra, penumbra;
			float attenuation[4];
		};

		template <typename T> T read(uintptr_t address) { return *reinterpret_cast<const T*>(address); }

		// Game memory can be mid-teardown during level changes; every read is guarded.
		bool copy_light(uintptr_t light, raw_light& out)
		{
			__try
			{
				out.kind = read<int>(light + OBJECT_KIND);
				out.enabled = read<int>(light + LIGHT_ENABLED);
				out.falloff_enabled = read<int>(light + LIGHT_FALLOFF_ENABLED);
				out.type = read<int>(light + LIGHT_TYPE);
				memcpy(out.direction, reinterpret_cast<const void*>(light + LIGHT_DIRECTION), sizeof(out.direction));
				memcpy(out.position, reinterpret_cast<const void*>(light + LIGHT_POSITION), sizeof(out.position));
				memcpy(out.color, reinterpret_cast<const void*>(light + LIGHT_COLOR), sizeof(out.color));
				out.umbra = read<float>(light + LIGHT_UMBRA);
				out.penumbra = read<float>(light + LIGHT_PENUMBRA);
				memcpy(out.attenuation, reinterpret_cast<const void*>(light + LIGHT_ATTENUATION), sizeof(out.attenuation));
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
		}

		// Returns the number of light pointers copied from the room's list (0 on a bad pointer).
		int copy_room_lights(uintptr_t room, uintptr_t (&out)[MAX_ROOM_LIGHTS])
		{
			__try
			{
				const int count = std::clamp(read<int>(room + ROOM_LIGHT_COUNT), 0, MAX_ROOM_LIGHTS);
				memcpy(out, reinterpret_cast<const void*>(room + ROOM_LIGHTS), count * sizeof(uintptr_t));
				return count;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
		}

		uintptr_t scene_camera_room()
		{
			__try
			{
				const auto app = read<uintptr_t>(G_APP);
				if (!app) return 0;
				const auto rm_render = read<uintptr_t>(app + APP_RMRENDER);
				if (!rm_render) return 0;
				const auto camera = read<uintptr_t>(rm_render + RMRENDER_SCENE_CAMERA);
				return camera ? read<uintptr_t>(camera + FRAME_CONTAINER) : 0;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
		}

		std::vector<uintptr_t> rooms_to_light()
		{
			std::vector<uintptr_t> rooms;
			if (const uintptr_t room = scene_camera_room(); room) rooms.push_back(room);
			for (void* room : rooms_rendered_last_scene())
				if (std::find(rooms.begin(), rooms.end(), reinterpret_cast<uintptr_t>(room)) == rooms.end())
					rooms.push_back(reinterpret_cast<uintptr_t>(room));
			return rooms;
		}

		D3DCOLORVALUE scaled_color(const raw_light& l, float scale)
		{
			return { l.color[0] * scale, l.color[1] * scale, l.color[2] * scale, 1.0f };
		}

		// Maps an RMLight to a D3D9 light. Returns false for types Remix should not receive (ambient).
		bool to_d3d_light(const raw_light& l, float intensity_scale, float unattenuated_range, D3DLIGHT9& out)
		{
			out = {};
			switch (l.type)
			{
			case 0: out.Type = D3DLIGHT_POINT; break;
			case 1: out.Type = D3DLIGHT_SPOT; break;
			case 2: out.Type = D3DLIGHT_DIRECTIONAL; break;
			default: return false;
			}

			out.Diffuse = scaled_color(l, intensity_scale);
			out.Specular = out.Diffuse;
			out.Position = { l.position[0], l.position[1], l.position[2] };
			D3DXVECTOR3 direction(l.direction[0], l.direction[1], l.direction[2]);
			D3DXVec3Normalize(&direction, &direction);
			out.Direction = direction;

			// The engine ramps linearly to zero at `range` when falloff is on and does not attenuate at all
			// otherwise. D3D's 1 / (A0 + A1 d) with A1 = 2 / range matches the linear ramp at half range.
			const float range = l.falloff_enabled ? l.attenuation[3] : unattenuated_range;
			out.Range = range;
			out.Attenuation0 = 1.0f;
			out.Attenuation1 = l.falloff_enabled && range > 0.0f ? 2.0f / range : 0.0f;

			// The engine's spot ramp is linear in cos(angle) between umbra and penumbra, i.e. Falloff = 1.
			out.Theta = 2.0f * l.umbra;
			out.Phi = 2.0f * l.penumbra;
			out.Falloff = 1.0f;
			return true;
		}
	}

	scene_lights& scene_lights::get()
	{
		static scene_lights instance;
		return instance;
	}

	void scene_lights::load_settings()
	{
		const auto& cfg = shared::common::config::get();
		enabled_ = cfg.get_bool("Lights", "Enabled", true);
		intensity_scale_ = cfg.get_float("Lights", "IntensityScale", 1.0f);
		unattenuated_range_ = cfg.get_float("Lights", "UnattenuatedRange", 5000.0f);
	}

	void scene_lights::submit(IDirect3DDevice9* dev)
	{
		if (!enabled_) return;

		std::unordered_set<uintptr_t> seen;
		DWORD index = 0;
		UINT by_type[3] = {};
		for (const uintptr_t room : rooms_to_light())
		{
			uintptr_t lights[MAX_ROOM_LIGHTS];
			const int count = copy_room_lights(room, lights);
			for (int i = 0; i < count; i++)
			{
				raw_light l;
				D3DLIGHT9 d3d_light;
				if (!lights[i] || !seen.insert(lights[i]).second || !copy_light(lights[i], l)) continue;
				if (l.kind != OBJECT_KIND_LIGHT || !l.enabled) continue;
				if (!to_d3d_light(l, intensity_scale_, unattenuated_range_, d3d_light)) continue;

				dev->SetLight(index, &d3d_light);
				dev->LightEnable(index, TRUE);
				by_type[l.type]++;
				index++;
			}
		}

		for (DWORD stale = index; stale < lights_enabled_; stale++)
			dev->LightEnable(stale, FALSE);
		lights_enabled_ = index;

		if (const DWORD now = GetTickCount(); now - last_log_tick_ >= LOG_INTERVAL_MS)
		{
			last_log_tick_ = now;
			shared::common::log("Lights", std::format("{} lights ({} point, {} spot, {} directional) from {} rooms",
				index, by_type[0], by_type[1], by_type[2], rooms_to_light().size()));
		}
	}
}
