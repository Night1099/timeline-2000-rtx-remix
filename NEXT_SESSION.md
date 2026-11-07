# Timeline + RTX Remix: next session

Last updated 2026-11-04 ~14:00. Game: *Timeline* (Eidos, 2000, "Caesar" engine).
Goal: get Timeline ray traced by RTX Remix Plus v2.0.0, with stable meshes for asset replacement.

## START HERE

- **Status at last check (13:55, all user-confirmed in game):** Timeline is path traced by Remix with real geometry.
  - **Rigid models and room geometry:** Path B. Drawn from the engine's own model-space meshes with their world matrix.
  - **Characters:** Path B. Drawn from the bind-pose mesh with FFP indexed vertex blending (one joint per vertex). Stable at
    every distance.
  - **Particles, sprites, anything else 3D:** Path A. Un-projected from the screen-space vertices.
  - **HUD and subtitles:** passed through as 2D.
  - **Culling:** all of it is removed with runtime engine patches: object, room quadtree, backface, portal traversal,
    distance LOD and projected shadows. Mouse hover and click still work, and the frame rate is fine.
  - **Sky:** original sky (5 SkyBox textures tagged in `rtx.conf`, `rtx.skyReprojectToMainCameraSpace = True`,
    `rtx.skyBrightness = 1.01`).
  - **Lights:** the engine's point, spot and directional lights are sent to Remix as D3D9 FFP lights.
  - **Tiled textures:** fixed (dxwrapper was leaking CLAMP addressing between scenes).
- **Next actions, in order:**
  1. **Texture UV scrolling on Path B meshes.** Water and other scrolling textures are static now. The engine adds
     `RMTexture+0x180/+0x184` to the projected UVs each frame. Apply it as a `D3DTS_TEXTURE0` translation
     (`D3DTSS_TEXTURETRANSFORMFLAGS = D3DTTFF_COUNT2`) in `screen_space::draw_rigid_mesh` / `draw_character_mesh`.
     The RMTexture is `[model+0x1D0 + 4*group]`.
  2. **Overlay texture passes.** The character overlay pass and RMModel vtbl+0x80 (`0x603236A0`, flag 0x8) redraw a group
     with a second texture and uv2 (`RMVertex+0x28/+0x2C`). For characters the per-scene (model, group) dedupe drops them.
     For rigid models, whether that path reaches the hook was never checked. First find out what they look like (decals?),
     then submit them as a second Path B draw with uv2, or tag them as decals in Remix.
  3. **Texture-mapping-mode groups** (`model+0x338` per group: 1 cylindrical `0x60328060`, 2 `0x603281E0`,
     4 `0x60328410`). These write generated UVs; check whether they still look right on Path B.
  4. **Light tuning:** `[Lights] IntensityScale`. Ambient lights (type 3) are skipped. Emissive surfaces (windows, fires)
     can be tagged by texture in Remix.
  5. **Robustness pass:** more levels; mirror and teleport portals, which keep the engine's facing rule in
     `portal_traversal`; level transitions.
- **Read first:** `patches/Timeline/findings.md` (newest sections at the end) and `patches/Timeline/kb.h`.

## Decision 2026-11-04 (12:00): Path B implemented as draw substitution (supersedes the decision below)

The user asked for Path B in the same session. It is implemented as **substitution inside the existing draw**,
not as a separate renderer. A MinHook detour on `RMModel_DrawGroupNoClip` (`0x603221F0`) records which model and
texture group the engine is drawing. While that record is set, the proxy's `DrawIndexedPrimitiveUP` draws the engine
mesh instead of the screen-space batch. The texture and blend states the engine has already bound are kept, so no
texture lookup table is needed and nothing is drawn twice. This was verified live before switching: over 45,477
samples, indices were 1:1 with the mesh, `NumVertices == mesh+0xF4`, and the world-position error was at most
0.013 units.

## Decision 2026-11-04 (00:45): Path A first, Path B in a future session (SUPERSEDED by the decision above)

The user chose Path A (un-project screen-space vertices in a D3D9 proxy) for the first session and Path B for the
future. Path A is still the fallback for everything the Path B hook does not cover.

## Folder layout

| What | Where |
|---|---|
| Game (working copy, the one being modded) | `F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\` |
| Original install (untouched game files) | `Z:\Ben\Timeline\` |
| Game content actually loaded | `Z:\Ben\Timeline\Content` |
| Remix Plus release | `F:\Vibe-Reverse-Engineering\RemixPlus\` |
| Project: notes, proxy source, scripts, backups (git-ignored) | `F:\Vibe-Reverse-Engineering\patches\Timeline\` |

## Render chain

```
Timeline.exe -> caesarrm.dll (scene graph, CPU transform/projection/lighting via RMCamera)
             -> caesarim.dll (DX5 IDirect3DDevice2, draws D3DTLVERTEX)
             -> ddraw.dll + dxwrapper.dll (Dd7to9 = 1, EnableWindowMode = 1, WindowModeBorder = 1)
             -> d3d9.dll        = OUR PROXY (built from patches/Timeline)
             -> d3d9_remix.dll  = Remix Plus bridge (renamed from d3d9.dll) -> .trex\NvRemixBridge.exe
```

Bases: Timeline.exe `0x400000`, caesarrm `0x60300000`, caesarim `0x60100000` (all at their preferred base).
The game folder also needs `rtx.conf` (Remix settings, including the sky tags) and `remix-comp-proxy.ini`.

## What was done

| # | Problem | Fix | Where |
|---|---|---|---|
| 1 | Remix d3d9 never loaded (DX5 game) | dxwrapper dx7 pack, `Dd7to9 = 1` | game folder (session 1) |
| 2 | Startup crash: caesarim requires an MMX driver | caesarim.dll on disk, file offset `0x78FC` `75 05` -> `90 90` | game folder (session 1) |
| 3 | Crash ~5 s in (COM components) | re-ran `Timeline_Component_Registration_Patch.bat` as admin | registry (session 1) |
| 4 | Fullscreen on every launch | `dxwrapper.ini` `EnableWindowMode = 1`, `WindowModeBorder = 1` | game folder |
| 5 | Nothing ray traced (screen-space TLVERTEX) | Path A un-projection with the live camera | `modules/screen_space` |
| 6 | Vertex explosions | compact indexed triangle list of referenced vertices only (stale pool slots) | `modules/screen_space` |
| 7 | Baked lighting / black surfaces | vertex RGB forced to white, alpha kept | `screen_space`, `rigid_meshes`, `character_meshes` |
| 8 | Engine culling | 16 runtime patches + C++ portal traversal, INI `[Culling]` | `game/engine_patches`, `game/portal_traversal` |
| 9 | Large skyboxes clipped | proxy far plane 500000 | `modules/screen_space` |
| 10 | Dark scene / grey sky | SkyBox textures tagged as sky, reproject to main camera on | `rtx.conf` (user, Alt+X) |
| 11 | No real light sources | RMLights -> `SetLight` / `LightEnable` each BeginScene | `game/scene_lights` |
| 12 | Stretched tiled walls (castle hall) | re-apply in-scene `ADDRESSU/V` at BeginScene | `modules/texture_addressing` |
| 13 | Unstable meshes (Remix hashes) | Path B rigid models: engine mesh + world matrix | `game/model_draw_hook`, `modules/rigid_meshes` |
| 14 | Unstable characters | Path B characters: bind pose + FFP indexed vertex blending | `modules/character_meshes` |
| 15 | Characters unstable at distance | progressive-mesh distance LOD forced to LOD 0 (finest) | `game/engine_patches` |

## How the proxy works (files, all under `patches/Timeline/src/comp/`)

- `modules/d3d9ex.cpp` (device wrapper): routes `DrawPrimitiveUP` and `DrawIndexedPrimitiveUP` to `screen_space`.
  `SetViewport` and `SetTransform` are tracked by `screen_space`. `BeginScene` runs, in order: `texture_addressing`,
  `character_meshes::on_begin_scene` (dedupe reset), then `scene_lights::submit`. `SetSamplerState` and `EndScene` feed
  `texture_addressing`.
- `modules/screen_space.cpp`: every caesarim draw arrives as a stride-32 D3DTLVERTEX batch.
  - It reads the scene camera `[[[0x4CED70]+0xCB8]+0x2C]` (projection at `+0x1E0`, view at `+0x31C`, near plane at
    `+0x2F0`).
  - For indexed batches it tries `rigid_meshes`, then `character_meshes`. If either returns `skip`, nothing is drawn;
    if either returns `ready`, that mesh is drawn instead of the batch.
  - Otherwise the batch goes through Path A: un-project, filter out invalid vertices, compact, then draw with FFP
    `XYZ|DIFFUSE|TEX1`.
  - UI (every vertex rhw == 1) is passed through. `begin_ffp` and `end_ffp` set the transforms and restore dxwrapper's
    state.
- `game/model_draw_hook.cpp`: a MinHook detour on `0x603221F0` (`__fastcall(model, edx, 9 args)`). It records the
  `{kind, model, indices, ...}` of exact RMModel objects (vtable `0x603424F0`) and RMDeformationModel objects
  (vtable `0x60342308`) while the original runs.
- `game/engine_mesh.cpp`: reads the RMMesh (vertices `+0xD8`, stride 0x34; aliases `+0xEC`; unique count `+0xF0`; total
  `+0xF4`; groups `+0x114`) and finds the drawn group from the index pointer. It also holds `winding_estimate`: whether
  triangle winding agrees with the stored normals, measured on rigid meshes (result: +1).
- `modules/rigid_meshes.cpp`: copies positions (an alias takes its position from its source vertex), normals, UVs and
  batch alpha. WORLD = `model+0xEC`, or identity when `model+0x188 != 0`. It skips the reversed pass of two-sided models
  (`model+0x1FC & 0x400`).
- `modules/character_meshes.cpp`:
  - **Palette:** one slot per joint (`frames[v]` at `model+0x348`), and `WORLDMATRIX(slot)` = joint `+0xEC`. Vertices
    use weight 1 with all four indices set to the slot.
  - **Normals:** rebuilt once per mesh from the engine's skinned positions (`model+0x344`), then moved into joint space
    with joint `+0x140`.
  - **Dedupe and self-check:** (model, group) pairs are deduped per scene. Each draw self-checks 3 vertices against
    `model+0x344` and falls back to Path A on a mismatch.
- `game/scene_lights.cpp`: lights from the scene camera's room (`camera+0x19C`) plus the rooms the portal traversal
  rendered in the last scene. Light list at `room+0x70`, count `+0x138`. RMLight layout is in findings "Lights (session 2b)".
- `game/engine_patches.cpp` + `game/code_patch.cpp`: each byte patch checks the original bytes first and logs
  `[EnginePatch]`. `game/portal_traversal.cpp` replaces `RenderThroughPortals` (`0x60339F20`): every connected room
  (up to 24 hops) is rendered once per scene, and mirror/teleport portals keep the engine's facing test.
- `modules/texture_addressing.cpp`: re-applies sampler 0 `ADDRESSU/V` from inside scenes at every BeginScene.
- `game/game_memory.cpp`: an SEH-guarded `read_memory` plus `expand_matrix43`.

## Runtime config (`remix-comp-proxy.ini` in the game folder; source `patches/Timeline/assets/`)

```
[FFP]      Enabled=1                  ; 0 = plain passthrough (no 3D conversion at all)
[Culling]  DisableObjectCulling=1     ; BoundsCheck -> whole mesh (5 call sites), shadows off, quadtree off,
                                      ; particles no-clip, progressive-mesh LOD 0
           DisableBackfaceCulling=1   ; 4 backface patches
           DisablePortalCulling=1     ; C++ portal traversal + portal distance cutoffs off
[Lights]   Enabled=1  IntensityScale=1.0  UnattenuatedRange=5000
[PathB]    Enabled=1                  ; installs the draw hook; rigid models on Path B
           Characters=1               ; characters on Path B (0 = characters back to Path A)
```
Startup log `rtx_comp/console.log`: 16 `[EnginePatch]` lines with no errors, plus `[PathB] hooked RMModel_DrawGroupNoClip`.
Periodic log lines: `[Lights]` (count by type), `[PathB]` (rigid and character draw counts; any line containing
`self-check failed` means a character fell back to Path A), and `[ScreenSpace]` (Path A batches).

## Build and deploy

1. Build with VS 2022 Build Tools (installed), from PowerShell:
   `cmd /c "F:\Vibe-Reverse-Engineering\patches\Timeline\build.bat release --name Timeline"`
   - The DirectX SDK x86 libs were copied into `patches/Timeline/deps/dxsdk/Lib/x86/` (the template ships headers only).
   - After renaming or deleting a `.cpp`, delete its stale `.obj` in `patches/Timeline/build/obj/release/comp/`, or the
     link fails with C1382.
2. Close the game first; the DLL is locked while it runs.
3. Back up the deployed `d3d9.dll` and `remix-comp-proxy.ini` into `patches/Timeline/backups/<YYYY-MM-DD_HHMM>_<slug>/`.
4. Copy `patches/Timeline/build/bin/release/d3d9.dll` and `patches/Timeline/assets/remix-comp-proxy.ini` into the game
   folder. They differ only in CRLF/LF line endings plus whatever new sections the source adds; diff them first if the
   user may have tuned values.
5. Launch from PowerShell:
   `Start-Process "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\Timeline.exe" -WorkingDirectory "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline"`

## Testing gotchas

- **Frida makes the game exit cleanly within seconds**, both with `livetools attach` and with spawn plus an Interceptor
  on caesarim draw sites. Cause unknown. Get runtime data through the proxy instead (log lines, or the tracer below).
- **D3D9 frame capture:** create `RemixPlus/Timeline/dxtrace_capture.trigger` containing `frames=2`. The output goes to
  `RemixPlus/Timeline/captures/dxtrace_*.jsonl`. It records 0 calls if the game window is not rendering (focus); retry.
- The proxy's ImGui overlay (F4) appears but gets no mouse or keyboard input in this game.
- Screenshots: the game window must be in front. A PowerShell screen grab captures the whole screen.
- Kill leftovers: `taskkill /F /IM Timeline.exe` and `taskkill /F /IM NvRemixBridge.exe`.
- `RemixApi` init fails with code 11 (logged at startup). FFP lights don't need it.
- `decompiler.py --types patches/Timeline/kb.h` crashes r2ghidra (stack overflow, with older kb.h backups too).
  Decompile without `--types`, or set up Ghidra (`python verify_install.py --setup`).
- Remix capture textures: Timeline's 16-bit textures are saved as DDS with FourCC `GLI1` (GLI library), which
  NVTT and most tools reject; their header masks are also R/B swapped. Convert with
  `python -I patches/Timeline/tools/convert_gli_dds.py <rtx-remix/captures/textures> <output dir>` (standard RGB 5:6:5,
  red in the high bits; verified visually). RGBA8 textures are already standard DX10 DDS and are copied as-is.
  To PNG (after conversion): for each `.dds`, run
  `"C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe" <file.dds> -o <file.png>`
  (done 2026-11-04 for 111 textures into `RemixPlus/Timeline/rtx-remix/captures/textures_png/`; alpha preserved).
- dxwrapper can drop redundant state calls. If the game ever wants CLAMP inside a scene while dxwrapper's cache already
  holds CLAMP, `texture_addressing` would leave WRAP in place (not seen so far).

## Backups (restore by copying back into the game folder)

- `backups/2026-11-04_0030_caesarim-mmx-driver/caesarim.dll`: original caesarim.dll
- `backups/2026-11-04_0050_proxy-deploy-windowed/`: original Remix `d3d9.dll` (now `d3d9_remix.dll`) and `dxwrapper.ini`
  from before windowed mode
- `backups/2026-11-04_*_deployed-*`: each deployed proxy build before it was replaced. The newest,
  `2026-11-04_1350_deployed-characters-v1`, is the characters build without the LOD patch.
- `backups/2026-11-04_*` (other slugs): proxy source, kb.h and findings before each change
- `backups/2026-11-04_1400_next-session/`: this file and findings.md before this rewrite

## Open questions

| Question | Who answers |
|---|---|
| Goal for assets: is Remix asset/texture replacement wanted next, or lighting polish? Sets the priority of the next actions | User |
| What do the overlay texture passes show (decals, detail, damage)? | Next session (capture or toggle), then user |
| Are scrolling textures (water, effects) noticeably wrong now? | User (observe) |
| Why does Frida make the game exit? Only matters if live tools are needed again | Next session |
