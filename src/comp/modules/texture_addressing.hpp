#pragma once

namespace comp
{
	/*
	 * Keeps the game's texture addressing mode across scenes.
	 *
	 * Between scenes, dxwrapper sets sampler 0 to CLAMP (its own surface blits) and never restores it.
	 * caesarim caches its texture-address state and does not re-send it, so a scene that does not change
	 * addressing inherits CLAMP and tiled textures (UVs beyond 0..1) smear their edge texels. This class
	 * remembers the addressing set inside scenes (the game's intent; WRAP is the D3D5 default) and
	 * re-applies it at every BeginScene.
	 */
	class texture_addressing
	{
	public:
		static texture_addressing& get();

		void on_set_sampler_state(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value);
		void on_begin_scene(IDirect3DDevice9* dev);
		void on_end_scene();

	private:
		bool in_scene_ = false;
		DWORD address_u_ = D3DTADDRESS_WRAP;
		DWORD address_v_ = D3DTADDRESS_WRAP;
	};
}
