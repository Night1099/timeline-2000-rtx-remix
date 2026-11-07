#include "std_include.hpp"
#include "texture_addressing.hpp"

namespace comp
{
	texture_addressing& texture_addressing::get()
	{
		static texture_addressing instance;
		return instance;
	}

	void texture_addressing::on_set_sampler_state(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value)
	{
		if (!in_scene_ || sampler != 0) return;
		if (type == D3DSAMP_ADDRESSU) address_u_ = value;
		else if (type == D3DSAMP_ADDRESSV) address_v_ = value;
	}

	void texture_addressing::on_begin_scene(IDirect3DDevice9* dev)
	{
		in_scene_ = true;
		dev->SetSamplerState(0, D3DSAMP_ADDRESSU, address_u_);
		dev->SetSamplerState(0, D3DSAMP_ADDRESSV, address_v_);
	}

	void texture_addressing::on_end_scene()
	{
		in_scene_ = false;
	}
}
