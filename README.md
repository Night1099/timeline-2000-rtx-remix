# Timeline (2000) — RTX Remix

Path-traced *Timeline* (Eidos Interactive, 2000, "Caesar" engine) with [NVIDIA RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix).

Timeline transforms, clips and lights everything on the CPU, then draws pre-transformed screen-space vertices
(DX5 `D3DTLVERTEX`), which Remix cannot ray trace. This project is a `d3d9.dll` proxy that sits between
[dxwrapper](https://github.com/elishacloud/dxwrapper) and Remix. It hands Remix real 3D geometry with real
transforms, and it patches the engine at runtime so nothing is culled.

## What works

- **Rigid models and level geometry:** drawn from the engine's own model-space meshes with their world matrix. Mesh
  data stays the same between frames, so Remix asset replacement works.
- **Characters:** drawn from their bind-pose mesh with fixed-function indexed vertex blending, one joint matrix per
  vertex, so meshes stay the same while characters animate.
- **Everything else in 3D** (particles, sprites): un-projected from screen space back to world space using the game's
  camera.
- **HUD and subtitles:** passed through as 2D.
- **No engine culling:** object, room quadtree, backface, portal traversal (rooms behind the camera included), distance
  LOD and projected shadows are all switched off. Mouse picking is unaffected.
- **Game lights:** point, spot and directional lights are sent to Remix as D3D9 lights.
- **dxwrapper texture-addressing leak fixed:** this leak made tiled textures look smeared.

Known gaps:
- texture UV scrolling on Path B meshes;
- second-texture (overlay) passes are dropped on characters;
- ambient engine lights are not sent.

See `NEXT_SESSION.md`.

## Render chain

```
Timeline.exe -> caesarrm.dll (scene graph, CPU transform/projection/lighting)
             -> caesarim.dll (DX5 IDirect3DDevice2, draws D3DTLVERTEX)
             -> ddraw.dll + dxwrapper.dll (dxwrapper, Dd7to9 = 1)
             -> d3d9.dll        = this proxy
             -> d3d9_remix.dll  = RTX Remix bridge (renamed from Remix's d3d9.dll)
```

## Installing

You need your own copy of Timeline. No game files are included here.

1. **dxwrapper:** from the dxwrapper release, take the dx7 games package. Copy `ddraw.dll`, `dxwrapper.dll` and
   `dxwrapper.ini` into the game folder. In `dxwrapper.ini` set:
   ```
   Dd7to9           = 1
   EnableWindowMode = 1   ; optional, windowed
   WindowModeBorder = 1   ; optional
   ```
2. **caesarim.dll MMX driver patch:** Timeline insists on an "MMX" Direct3D driver that dxwrapper does not offer, so
   startup crashes. Patch `caesarim.dll` in a hex editor: at file offset `0x78FC`, change bytes `75 05` to `90 90`.
   Keep a backup of the original.
3. **COM components:** the game's components must be registered to the folder the game is in. Run the game's
   `Timeline_Component_Registration_Patch.bat` **as administrator** from the game folder. Do this again whenever
   you move the folder.
4. **RTX Remix:** install the Remix runtime into the game folder as usual, then rename Remix's `d3d9.dll` to
   `d3d9_remix.dll`.
5. **This proxy:** copy the built `d3d9.dll` and `assets/remix-comp-proxy.ini` into the game folder.
6. **Remix sky:** in the Remix menu (Alt+X → Game Setup → Categorize Textures), tag each level's SkyBox textures as
   **Sky Texture**. Then enable **Reproject sky to main camera** (`rtx.skyReprojectToMainCameraSpace = True`).

## Building

Requirements:
- Visual Studio 2022 with the C++ x86 toolset (Build Tools are enough);
- the [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812).

1. Copy the SDK's x86 import libraries into this repo. They are not redistributed here:
   ```
   copy "%DXSDK_DIR%Lib\x86\*.lib" deps\dxsdk\Lib\x86\
   ```
2. Build:
   ```
   build.bat release --name Timeline
   ```
   The output is `build\bin\release\d3d9.dll`.

## Configuration (`remix-comp-proxy.ini`)

| Section | Key | Meaning |
|---|---|---|
| `[FFP]` | `Enabled` | `0` = plain passthrough (no 3D conversion) |
| `[Culling]` | `DisableObjectCulling` | whole meshes at the finest LOD; also turns off the quadtree, projected shadows and particle clipping |
| `[Culling]` | `DisableBackfaceCulling` | submit both faces |
| `[Culling]` | `DisablePortalCulling` | render every connected room once per scene |
| `[Lights]` | `Enabled`, `IntensityScale`, `UnattenuatedRange` | engine lights → Remix lights, with a brightness scale |
| `[PathB]` | `Enabled` | draw rigid models from engine meshes (needs `DisableObjectCulling=1`) |
| `[PathB]` | `Characters` | draw characters with vertex blending |

The log is `rtx_comp\console.log` in the game folder. At startup it should show 16 `[EnginePatch]` lines with no
errors.

## Repository layout

| Path | What |
|---|---|
| `src/comp/modules/screen_space.*` | draw routing, Path A un-projection, Path B draws |
| `src/comp/modules/rigid_meshes.*`, `character_meshes.*` | Path B mesh extraction (rigid, skinned) |
| `src/comp/modules/texture_addressing.*` | fix for dxwrapper's CLAMP leak |
| `src/comp/game/engine_patches.*`, `portal_traversal.*`, `code_patch.*` | runtime engine patches |
| `src/comp/game/model_draw_hook.*`, `engine_mesh.*`, `scene_lights.*`, `game_memory.*` | engine hooks and data readers |
| `src/shared/` | remix-comp-proxy framework (game-agnostic) |
| `kb.h`, `findings.md` | reverse-engineering knowledge base: addresses, structs, analysis notes |
| `NEXT_SESSION.md` | current state, architecture, next steps |
| `TEXTURE_CONVERSION.md`, `tools/convert_gli_dds.py` | converting Remix capture textures (GLI1 DDS) to standard DDS and PNG |
| `driver_check.py`, `spawn_trace.py`, `vertex_sample.py` | Frida scripts used during analysis |

## Credits

- **Proxy framework:** remix-comp-proxy, from the
  [Vibe Reverse Engineering](https://github.com/Ekozmaster/Vibe-Reverse-Engineering) toolkit. It is based on
  [xoxor4d/remix-comp-base](https://github.com/xoxor4d/remix-comp-base) and adapted by Kim2091 (MIT, see `LICENSE`).
- **Bundled libraries:** [MinHook](https://github.com/TsudaKageyu/minhook) and
  [Dear ImGui](https://github.com/ocornut/imgui), under their own licenses.
- **DirectX 5 to Direct3D 9 translation:** [dxwrapper](https://github.com/elishacloud/dxwrapper).
- **Rendering:** [RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix).

*Timeline* is © Eidos Interactive / Timeline Computer Entertainment. This project contains no game code or assets.
