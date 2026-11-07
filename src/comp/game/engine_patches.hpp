#pragma once

namespace comp::game
{
	/*
	 * Runtime byte patches to caesarrm.dll / caesarim.dll that remove the engine's culling, so the
	 * D3D9 proxy (and RTX Remix) receives whole meshes, both faces, and rooms outside the view.
	 * Groups are toggled by the [Culling] section of remix-comp-proxy.ini. Every patch verifies the
	 * original bytes first and is skipped (with an error in the log) if they do not match.
	 */
	void apply_engine_patches();
}
