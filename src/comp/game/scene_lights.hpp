#pragma once

namespace comp::game
{
	/*
	 * Submits the engine's lights to RTX Remix as D3D9 fixed-function lights.
	 *
	 * caesarrm lights everything on the CPU and bakes the result into vertex colours, which the proxy
	 * discards. Each scene, this reads the RMLight objects of the rooms being drawn (the scene camera's
	 * room plus the rooms the portal traversal rendered last scene) and calls SetLight / LightEnable,
	 * which Remix converts into path-traced lights. Settings: [Lights] in remix-comp-proxy.ini.
	 */
	class scene_lights
	{
	public:
		static scene_lights& get();

		void load_settings();
		void submit(IDirect3DDevice9* dev);

	private:
		bool enabled_ = true;
		float intensity_scale_ = 1.0f;
		float unattenuated_range_ = 5000.0f;  // D3D range for lights the engine does not attenuate

		DWORD lights_enabled_ = 0;            // indices 0..lights_enabled_-1 are currently enabled
		DWORD last_log_tick_ = 0;
	};
}
