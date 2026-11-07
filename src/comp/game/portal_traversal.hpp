#pragma once

namespace comp::game
{
	/*
	 * Replaces caesarrm's RMRoom::RenderThroughPortals (0x60339F20) so every room connected to the
	 * camera's room is rendered once per scene, including rooms whose portal faces away or lies
	 * behind the camera. Rooms are entered through the full viewport clip instead of the projected
	 * portal outline. Mirror/teleport ("transform") portals keep the engine's facing rejection,
	 * because entering one moves the camera.
	 */
	void install_portal_traversal();

	// Rooms the replacement traversal rendered during the last completed scene (empty if not installed).
	const std::vector<void*>& rooms_rendered_last_scene();
}
