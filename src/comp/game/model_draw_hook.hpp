#pragma once

namespace comp::game
{
	enum class model_kind { rigid, character };

	// The model texture group caesarrm is drawing right now (between entry and exit of
	// RMModel_DrawGroupNoClip 0x603221F0). caesarim's resulting D3D draw happens inside that window.
	struct model_group_draw
	{
		model_kind kind;       // RMModel or RMDeformationModel
		uintptr_t model;
		const uint16_t* indices; // the triangle list passed to the engine (forward or reversed group list)
		int triangle_count;
		int vertex_count;
		int texture_slot;      // caesarim texture slot (-1 none)
	};

	// Detours RMModel_DrawGroupNoClip; exact RMModel and RMDeformationModel objects are recorded.
	void install_model_draw_hook();

	// The group being drawn, or nullptr outside a model group draw.
	const model_group_draw* current_model_draw();
}
