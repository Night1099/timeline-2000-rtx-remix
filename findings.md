# Timeline (Eidos 2000) under dxwrapper (Dd7to9) + RTX Remix Plus

Chain: Timeline.exe -> caesarrm.dll -> caesarim.dll (DX5 D3D IM, DirectDrawCreate) -> ddraw.dll stub
-> dxwrapper.dll (Dd7to9) -> Remix bridge d3d9.dll -> .trex runtime.

## Issue 1 (SOLVED in-memory): no D3D driver selected — silent init failure

- caesarim `0x60102B80` detects MMX via CPUID; `0x60102F0B` then sets driver flag 0x40.
- Picker `0x601078F0`: with flag 0x40 the `jne` at `0x601078FC` makes it require a driver whose
  GUID is `IID_IDirect3DMMXDevice` {881949A1-D6F3-11D0-89AB-00A0C9054129}.
- dxwrapper enumerates only Ramp / RGB Emulation / Direct3D HAL, so creation fails with
  "Cannot find a D3D device driver which is compatible with the current display depth or any
  supported fullscreen mode." (buffer `0x60120560`, never shown to the user).
- App_Init returns false and WinMain deletes g_app; TexMgr_dtor `0x4308C0` then calls
  RMRender::SetTextureReloadCallback on a NULL RMRender -> AV at caesarrm `0x60338A24`.
- Verified live (driver_check.py): unpatched -> error string above; patched -> D3D create returns 1,
  dxwrapper creates a 1024x768 X8R8G8B8 D3D9 device, Remix compiles its pipelines.
- **Fix:** caesarim.dll file offset for VA `0x601078FC`: `75 05` -> `90 90`.

## Issue 2 (SOLVED): stale COM registration -> AV at Timeline.exe `0x4AA3A5`

Root cause: all 14 TLS.* components were registered (WOW6432Node CLSID InprocServer32) to
`F:\Timeline\...`, which no longer existed. CoGetClassObject at `0x4AFCE6` failed silently, the
render loop at `0x412824` got NULL from CompMgr_QueryComponent and called `0x4AA3A0` on it.
Affected every copy of the game (native, Z:, wrapped). Fixed by running
`Timeline_Component_Registration_Patch.bat` as admin from the current folder. After that the game
runs with dxwrapper + Remix + the caesarim patch, with no AVs. Re-register if the folder moves.

Original notes on the symptom:

- Preceded in dxwrapper log by `Utils::kernel_HeapSize Error: Invalid heap handle!` and
  `SetMaterial Warning: RampSize Not Implemented`.
- The same AV also occurs with native DirectDraw (no dxwrapper), so it may be a game/OS issue
  rather than a wrapper issue.
- After it, the app tears down (DeleteViewport warning) and the process exits.

## Issue 3 (OPEN): Remix sees textures but ray traces nothing — geometry is pre-transformed

caesarim (DX5 IDirect3DDevice2) submits all geometry as D3DVT_TLVERTEX (vertex type 3 hardcoded):
- `0x6010392C` DrawPrimitive(TRIANGLEFAN, TLVERTEX)
- `0x601039FD` DrawPrimitive(TRIANGLELIST, TLVERTEX)
- `0x60103AD7` DrawIndexedPrimitive(TRIANGLELIST, TLVERTEX)
- `0x60105FDE`, `0x60106039` DrawPrimitive(LINELIST, TLVERTEX)
No D3DVT_VERTEX/LVERTEX path exists; transform, clipping and lighting are done on the CPU.
dxwrapper forwards these as XYZRHW, which Remix treats as screen-space UI (not ray traced).
Natively Win11 also offers no HAL here (Ramp/RGB/MMX emulation only).

## dxwrapper D3D9 output (session 2, live trace)

Captured with the remix-comp-proxy tracer (`RemixPlus/Timeline/captures/dxtrace_20261104_005227.jsonl`,
2 frames, in a 3D scene, chain dxwrapper -> proxy d3d9.dll -> d3d9_remix.dll).

- dxwrapper (`Dd7to9`, `DdrawFixedFunctionVertex = 0`) submits every caesarim TLVERTEX draw as
  `DrawIndexedPrimitiveUP` (126 / 2 frames) or `DrawPrimitiveUP` (53), stride 32 = raw D3DTLVERTEX
  (x, y, z, rhw, diffuse, specular, u, v), with its own vertex declaration and a vertex shader.
- Before each draw: `SetVertexShaderConstantF c0 = (-W/2, -H/2, 0, 0)`, `c1 = (2/W, -2/H, 1, 1)`
  (W=1024, H=768): a pixel->clip conversion shader. `SetVertexShaderConstantI i2` also set.
  `SetTransform(PROJECTION=3)` = diag(1/512, 1/384, 1, 1), a `SetViewport` per draw.
- Backtrace of the draw-time `SetVertexShader` includes the caesarim call site (e.g. `0x60103ADA`
  for DrawIndexedPrimitive at `0x60103AD7`), so the proxy can classify draws by caesarim return address.
- 7 BeginScene/EndScene pairs per frame.
- Implication for Path A: the proxy un-projects in `DrawIndexedPrimitiveUP` / `DrawPrimitiveUP` using the
  `pVertexStreamZeroData` TLVERTEX array, nulls dxwrapper's shader, and draws FFP `D3DFVF_XYZ|DIFFUSE|TEX1`.

## caesarim transform pipeline (session 2)

### Summary
**caesarim does not transform or project anything.** Its `_imvertex` is a plain D3DTLVERTEX (stride 0x20).
It receives vertices that are already in screen space and does only screen-space work before drawing:
a 2D signed-area backface test (`(v2.y-v0.y)*(v0.x-v1.x) - (v0.y-v1.y)*(v2.x-v0.x)` against `0.0f` at
`0x601172E0`), clipping against the portal clip-polygon stack (`PushClip`/`PopClip`, stack at
`IMRender+0x4E00`), then `DrawPrimitive` with flags `0xC` (DONOTCLIP|DONOTUPDATEEXTENTS).
**The software projection is in caesarrm.dll, class `RMCamera`.** caesarrm builds a row-vector
4x4 `mvp = [modelWorld] * view * proj` at `RMCamera+0x224`. Function `0x603025E0` (or `0x603026B0` with fog)
then fills the TLVERTEX arrays from caesarrm `_vertex` (stride 0x34). The projection matrix works directly in
pixel units, so `sx/sy` come out as screen pixels after the divide.

### Key Addresses
| Address | Description |
|---|---|
| caesarim `0x60103820` / `0x60103940` / `0x60103A10` | IMDevice vtbl +0x0C/+0x10/+0x14: Fan / TriList / IndexedTriList wrappers around DrawPrimitive/DrawIndexedPrimitive (TLVERTEX). Only render states SHADEMODE(9) and SPECULARENABLE(0x1D) plus the texture are set |
| caesarim `0x6010CBB0` etc. | IMRender::DrawPoly / DrawPolysNoClip / DrawIndexedPolysNoClip / ExternalDraw*: exported entry points called from caesarrm with `_imvertex*` |
| caesarrm `0x603025E0` | **RM_ProjectVerts**: the TLVERTEX fill loop (no fog) |
| caesarrm `0x603026B0` | RM_ProjectVertsFog: same math, plus fog written to specular alpha (byte +0x17) |
| caesarrm `0x60304670` | RMCamera::ViewportTransformFace: VolumeClipFace (`0x60304810`, view-frustum clip *before* projection) -> `[this+0x1D0+fog*4](this+0x224, ...)` -> optional RenderFX callback `[this+0x1C8]` per output vertex |
| caesarrm `0x60304AF0` | RMCamera_BuildProjection: writes `proj` at `+0x1E0` |
| caesarrm `0x60303930` | RMCamera::UpdateCameraMatrix: `view = inverse(cameraWorld@+0xBC)` -> `+0x31C`; `+0x224 = view*proj`; world-space frustum planes |
| caesarrm `0x60307440` | RMCamera_PushModelMatrix: saves VP to `+0x268`, sets `+0x224 = modelWorld(4x3 by value) * VP`, moves clip planes to model space. Restore copies `+0x268` back (code at `0x603079F9`) |
| caesarrm `0x60303380` | SetFieldAngle: `field(+0x2EC) = tan(angle*0.5) * frontClip(+0x2F0)` (angle in radians) |
| caesarrm `0x603037D0` | SetViewWindow(x,y,w,h,setAspect): rect at `+0x30C`, `aspect(+0xDA0) = w/h` |
| caesarrm `0x60337A30` | RMRender::BeginRenderScene(RMCamera*): `RMRender+0x2C` = current camera, `+0xE4[10]` = cameras this frame |
| caesarrm `0x60308030` | RMCameraSprite quad builder: `sz = 0.5`, `rhw = 1.0` |
| caesarim `0x60105F20` | Debug grid (IMRender::DrawGrid): the two LINELIST draws, `rhw = 1.0` |

Constants (caesarrm): `0x603421C8` = 0.5, `0x60342274` = -0.5, `0x60342278` = **500000.0 (K, the fixed "far" used for sz)**,
`0x603421AC` = 1.0, `0x6034226C` = 5000.0 (default back clip), `0x603421F0` = 255.0 (fog).

### Item 1: who fills the TLVERTEX buffers (confidence: high)
Fill loop: caesarrm `RM_ProjectVerts` 0x603025E0 / `RM_ProjectVertsFog` 0x603026B0 (pointers stored in
RMCamera+0x1D0/+0x1D4 by the ctor at 0x60303128/0x60303132), called only from
`RMCamera::ViewportTransformFace` 0x60304670 (and the bounds check at 0x60305E12/0x603062E3).
ViewportTransformFace callers: RMModel render 0x60321F30 (call 0x603220E4), other RMModel variants
(0x60323124, 0x603245ED, 0x6032908A), RMDeformationModel render 0x60313750 (0x603138DC, 0x60314452, 0x60315349),
RMPortal (0x60330901), GetViewportRect (0x60304568). The `_imvertex` output then goes to IMRender::Draw* in caesarim.
Per vertex the loop copies color (+0x18), specular (+0x1C), tu (+0x20) and tv (+0x24) unchanged. Lighting is baked
earlier (SnapLight) into `_vertex.color`.

### Item 2: exact formulas (confidence: high for the structure; sample values not yet confirmed live)
Per vertex (`M = RMCamera+0x224`, row-vector, `M[r][c]` at `r*0x10 + c*4`):
```
w   = x*M[0][3] + y*M[1][3] + z*M[2][3] + M[3][3]
rhw = 1.0f / w
sx  = (x*M[0][0] + y*M[1][0] + z*M[2][0] + M[3][0]) * rhw
sy  = (x*M[0][1] + y*M[1][1] + z*M[2][1] + M[3][1]) * rhw
sz  = (x*M[0][2] + y*M[1][2] + z*M[2][2] + M[3][2]) * rhw
```
Projection `P` (RMCamera+0x1E0), from BuildProjection 0x60304AF0, perspective (`projType(+0xD94) != 1`):
```
L,T,R,B = ints at +0x30C,+0x310,+0x314,+0x318      W = R-L, H = B-T
cx = (L+R)/2, cy = (T+B)/2
n = frontClip(+0x2F0), f = field(+0x2EC) = tan(fieldAngle/2)*n, t = f/n = tan(fieldAngle/2)
K = 500000.0, a = aspect(+0xDA0)
if a > 1:  sxs = W/2,       sys = H*a/2      (with a = W/H: both = W/2, so horizontal FOV = fieldAngle)
else:      sxs = W/(2a),    sys = H/2        (vertical FOV = fieldAngle)
P = | sxs            0                0                 0 |
    | 0             -sys              0                 0 |
    | cx*t + offX    cy*t + offY      t*K/(K-n)         t |
    | 0              0               -t*n*K/(K-n)       0 |
(offX/offY = int camera offsets +0xD98/+0xD9C, normally 0.
 For n == 1.0 a separate branch gives numerically identical values.)
```
With view-space `(xv, yv, zv)` (`+y` up, `+z` forward, left-handed, D3D-style):
```
rhw = 1 / (zv * t)                                  <-- NOT 1/zv: scaled by 1/tan(fov/2)
sx  = cx + offX/t + sxs * xv / (zv*t)
sy  = cy + offY/t - sys * yv / (zv*t)
sz  = K/(K-n) * (1 - n/zv)   ~ 1 - n/zv              (standard D3D z with far = 500000, not the back clip)
     = K/(K-n) - (K/(K-n))*n*t * rhw   (linear in rhw: matches the live fit sz = 1.000001 - 3.394*rhw,
                                         so slope 3.394 = field*K/(K-n) ~= field(+0x2EC))
```
**Un-projection** (exact inverse):
```
zv = 1 / (rhw * t)
xv = (sx - cx - offX/t) * zv * t / sxs   = (sx - cx - offX/t) / (rhw * sxs)
yv = -(sy - cy - offY/t) / (rhw * sys)
world = [xv yv zv 1] * cameraWorld (RMMatrix43 at RMCamera+0xBC) = inverse of RMCamera+0x31C
```
Parallel projection (`projType == 1`): `P00 = sxs*zoom(+0xDC0)`, `P11 = -sys*zoom`, `P22 = 1/(K-n)`,
`P30 = cx+offX`, `P31 = cy+offY`, `P32 = -n/(K-n)`, `P33 = 1`, `P23 = 0`. This gives `rhw = 1`, `sz = (zv-n)/(K-n)`.
Live-fit reconciliation: the fit `sz = 1.000001 - 3.394*rhw` pins only the **slope**, `B = field*K/(K-n) ~= field(+0x2EC) ~= 3.394`.
The intercept `A = K/(K-n)` is extrapolated from sz values near 1.0 printed to 5 digits, so its uncertainty is about +-1e-5,
which allows n anywhere in roughly [-5, 5]. Two readings fit: (n ~ 0.5, t ~ 6.8, an implausible 163 deg FOV) or
**(n ~ 4.85, t ~ 0.700 = tan(35 deg), i.e. horizontal FOV 70 deg)**. The second matches `config.cfg FOV 70.000000`
and is the expected one. A live read of `+0x2EC/+0x2F0/+0x2F8` settles it.

### Item 3: where the camera / view matrix lives (confidence: high)
Not in caesarim. It is a heap `RMCamera` object in caesarrm. There are no camera globals in either DLL.
- `RMCamera+0xBC`: camera world matrix (`_matrix43`, 4 rows x 3 floats, row-vector: rows 0-2 = right/up/forward axes,
  row 3 (+0xE0) = position). Confirmed RMFrame world transform: `RMFrame::GetWorldTrans` 0x603168F0 copies 12 dwords from `[this+0xBC]`
  (`GetModelTrans` 0x60316830 reads the local transform at `+0x8C`). RMCamera is an RMFrame at offset 0.
- `RMCamera+0x31C`: `view` = 4x4 affine inverse of the above (0x60301F00 then 0x603020B0). Row-vector, same layout
  as `D3DMATRIX`, so it can be passed straight to `SetTransform(D3DTS_VIEW)`.
- `RMCamera+0x1E0`: `proj` (pixel space, above). `RMCamera+0x224`: `view*proj`, or `model*view*proj` during a model draw.
- How to find the current camera: `RMRender+0x2C` (set by `RMRender::BeginRenderScene(RMCamera*)` 0x60337A30). Up to
  10 cameras per frame are in `RMRender+0xE4[]`, count at `+0xE0`. RMRender* = Timeline.exe `RenderMgr+0x10C`
  (RenderMgr = App+0xBAC, `g_app` = `0x4CED70`). `RMCamera+0x670` = its IMRender*.
- `RMCamera::UpdateCameraMatrix` 0x60303930 runs when dirty (`+0x184`). Hooking its exit gives `view`/`proj` per camera.
- **Several cameras per frame.** The live trace shows 7 BeginScene/EndScene pairs per frame. P and view differ per scene
  (viewport rect, FOV, camera), so the proxy must snapshot camera state per scene, not per frame. Options: read `RMRender+0x2C`
  at D3D BeginScene; hook `RMCamera_BeginScene` 0x60304D30 (runs UpdateCameraMatrix and the IMRender view-window setup before
  the D3D BeginScene); or hook `0x603025E0` per batch (`camera = arg0 - 0x224`).

### Item 4: model space vs world space (confidence: medium-high)
Both, depending on the object:
- **Rigid RMModel** (0x60321F30): mesh `_vertex` array (`mesh+0xD8`, mesh = model+0x20C) is in **model space**. If
  `model+0x188 == 0`, `RMCamera_PushModelMatrix` (0x60307440) temporarily sets `+0x224 = modelWorld(model+0xEC, 4x3) * view * proj`.
  If `model+0x188 != 0`, no model matrix is applied (vertices are already world space).
- **RMDeformationModel** (0x60313750, characters): vertices are skinned on the CPU (0x60315870) into a buffer at `model+0x344`
  and projected with plain `view*proj`, so they are already in **world space**.
- For Path A this makes no difference: un-projecting with P and the camera matrix always gives view/world space,
  whatever per-object matrix was used.
- Frustum clipping (VolumeClipFace) happens in model/world space *before* projection, so every emitted vertex has zv >= n.

### Item 5: the vtable +0x68 calls (confidence: high)
None of them is `IDirect3DDevice2::SetTransform`. caesarim never sets a D3D transform.
| Site | Object | Meaning |
|---|---|---|
| `0x601045B3` | `[[0x6012CFC0]+0xC390]` (IDirectDrawSurface, back buffer) | `ReleaseDC`. Pair of `GetDC` at 0x60104593 (IMDevice vtbl +0x54/+0x58) |
| `0x601056EA` | `[0x6012D040]` (IDirectDrawSurface) | `ReleaseDC`, after `GetDC` at 0x601056B7 (GDI blit path) |
| `0x60105ABD` | `[list+idx*0x3C+0x1C]` overlay entry surface (count at `this+0x270`) | `ReleaseDC` (overlay DC end) |
| `0x60107689`, `0x60107722` | surface in `esi` (GDI `StretchBlt` path, GetDC at +0x44 just before) | `ReleaseDC` (texture/bitmap load through GDI) |
| `0x60107602` | surface after a GDI call | `ReleaseDC` |
| `0x6010D6BD` | `IMRender+0x5ACC` = IMDevice (C++ vtable 0x60117130 slot 26 -> 0x60104D80) | IMDevice::GetRenderStateStruct (copies 21 dwords from 0x6012CFD8) |

The only D3D calls through `[0x6012CFC0]+0xC` (IDirect3DDevice2) are SetRenderState (+0x5C), DrawPrimitive (+0x74) and
DrawIndexedPrimitive (+0x78).

### Item 6: 2D / UI draws and how to tell them apart (confidence: medium)
- **RMCameraSprite** (0x60308030 builds a 4-vertex quad from a RECT, drawn by 0x60308150): every vertex has
  `sz == 0.5f` and `rhw == 1.0f` exactly (uv 0/1, color = alpha<<24 | rgb). This covers HUD/camera-sprite overlays and probably backdrops.
- **Debug grid** (caesarim 0x60105F20, the LINELIST draws 0x60105FDE / 0x60106039): `rhw == 1.0f`. LINELIST is never used for 3D.
- **3D geometry**: `rhw = 1/(zv*t)`, which is far below 1 in practice (live sample 5e-5..8.5e-5), and `sz = K/(K-n)*(1-n/zv)`,
  i.e. tied to rhw by `sz = A - B*rhw` with A, B fixed per camera.
- A parallel-projection camera (`+0xD94 == 1`) also gives `rhw == 1` for all its geometry, so the test below classes it as 2D.
  That is fine for HUD/orthographic cameras, but it is worth knowing.
- Recommended discriminator in the proxy: **2D if every vertex has `rhw == 1.0f` (exact) or the primitive is LINELIST.**
  Otherwise, check that `sz` matches `A - B*rhw` within about 1e-4 (catches anything else drawn with a non-projective rhw).
  Draws can also be classified by caesarim return address. The 3D wrappers are 0x60103820/0x60103940/0x60103A10,
  but sprites use the same wrappers, so the vertex test is the main one.
- Timeline.exe and Components\*.dll do not import any IMRender draw API (Timeline.exe only imports
  `RMRender::GetImmediateDevice`). All TLVERTEX producers are in caesarrm.
- GDI/DirectDraw overlays (`OverlayList`, `BeginOverlayUpdate`/`GetDC`) are blitted surfaces rather than D3D draws (not traced in detail).

### Caveats / open
- Portal clipping in caesarim (screen space, after projection) is **safe for un-projection**. The attribute clippers in the
  `IMRender+0x4D40` table (populated at 0x6010C605: 0x601096A0, 0x60109830, 0x60109A00, 0x60109C40, 0x601093B0, 0x60109EC0,
  0x6010A1F0, 0x6010A8B0, ...) emit new vertices with sx/sy on the edge and **sz (+8) and rhw (+0xC) interpolated linearly in
  screen space** (e.g. 0x601097A4..0x601097CD). Both quantities are affine in 1/zv, so the new vertices lie exactly on the original
  3D triangle. (0x6010BE20 is a 2D-only clip-region helper that writes just sx/sy.)
- Rooms connected by portals may have relative transforms (`RMPortal::GetTransformation` 0x60332E90). If the camera matrix is
  per-room, un-project with the camera that was current for that draw.
- RenderFX callback `RMCamera+0x1C8` can modify every output TLVERTEX. Check it is NULL for normal cameras.

### Suggested Live Verification
1. Hook caesarrm `0x603025E0` entry: read arg0 (mvp ptr). `camera = arg0 - 0x224`. Dump `camera+0x1E0` (P, 16 floats),
   `+0x31C` (view), `+0xBC` (12 floats), `+0x2EC/+0x2F0/+0x2F8/+0xDA0/+0xD94/+0xD98/+0xD9C/+0xDB4`, and `+0x30C..+0x318` (ints).
   Check that `P[2][3] == tan(fieldAngle/2)`, `P[3][2]/P[2][2] == -n`, and that the earlier fit slope 3.394 matches `field*K/(K-n)`.
2. Compare `[RMRender+0x2C]` with that camera, to see whether a single main camera is used per frame.
3. Hook `0x60308030` / `0x60308150` and confirm sprite quads arrive with `rhw == 1.0`, `sz == 0.5`.
4. At draw time in the proxy, un-project one vertex with the formulas above, re-project it with `camera+0x224`, and compare.

## caesarrm camera (session 2)

### Summary
This section agrees with "caesarim transform pipeline (session 2)" above. It adds the pointer chains, the
complete RMCamera/RMFrame layout, the per-frame hook points and the FOV semantics.
- **Active camera from outside:** `[[0x4CED70] + 0xCCC]` (RenderMgr.activeCamera).
- **Camera for the scene being drawn now:** `[[[0x4CED70] + 0xCB8] + 0x2C]` (RMRender.currentSceneCamera).
- No caesarim export takes a matrix. caesarrm projects every vertex itself and passes only `_imvertex` (D3DTLVERTEX) arrays.
- Matrices are final inside `RMCamera_BeginScene` 0x60304D30, after `UpdateCameraMatrix` returns and before
  `IMRender::BeginScene` is called. The exported `RMRender::BeginRenderScene` 0x60337A30 is the matching hook.
- FOV is the full angle in **radians** at camera+0x2F8. Because aspect = W/H > 1, it is the **horizontal** FOV, and both
  screen axes use the same pixel scale (W/2).

### Key Addresses
| Address | Description |
|---|---|
| Timeline `0x4CED70` | `g_app` (App*). RenderMgr is **embedded** at App+0xBAC (`lea ebp,[esi+0xBAC]` at 0x4087B2) |
| App+0xCB8 (=RenderMgr+0x10C) | RMRender* |
| App+0xCCC (=RenderMgr+0x120) | **active RMCamera*** (written only by RenderMgr_SetActiveCamera 0x413950; NULL arg -> default cam at RenderMgr+0x11C) |
| App+0xCC8/+0xCD0 (RenderMgr+0x11C/+0x124) | cameras created by RenderMgr_CreateCamera 0x412910 (0x41066B, 0x410698). RenderMgr+0x114 is the RMContainer/scene they are created in (passed as the CreateCamera container arg), not a camera |
| Timeline `0x412910` | RenderMgr_CreateCamera: CreateCamera(client rect), SetViewWindow(0,0,w,h,1), SetFieldAngle(0x3F9C61AA = 1.2217 rad = 70 deg), SetFrontClip(5.0), SetBackClip(5000), EnableBackClip(0), position (0,0,-300), orientation dir (0,0,1) up (0,1,0) |
| Timeline `0x412580` | RenderMgr_RenderFrame: BeginRenderScene(activeCam) at 0x4126D9, Render, EndRenderScene. Then a loop of extra cameras `[edi]` -> BeginRenderScene at 0x4127D3 |
| caesarrm `0x60337A30` | `?BeginRenderScene@RMRender@@QAEHPAVRMCamera@@@Z` (exported, ecx=RMRender, [esp+4]=RMCamera*): RMRender+0x2C = cam, pushes into +0xE4[10], calls 0x60304D30 |
| caesarrm `0x60304D30` | RMCamera_BeginScene(IMRender*): `_controlfp(0x20300,0x30300)` (24-bit x87), UpdateCameraMatrix, eye -> +0xDD0, world snapshot -> +0x2AC, IMRender::PushClip, UpdateViewWindow(+0x30C rect), **IMRender::BeginScene**. Its only caller is 0x60337A5C |
| caesarrm `0x60337A70` | RMRender::EndRenderScene -> RMCamera_EndScene 0x60304ED0 on RMRender+0x2C (+0x2C is **not** cleared) |
| caesarrm `0x60303930` | UpdateCameraMatrix: `+0x31C = affineInverse(world +0xBC)`, `+0x224 = +0x31C * +0x1E0`, frustum planes |
| caesarrm `0x60304AF0` | BuildProjection -> +0x1E0 |
| caesarrm `0x60307440` / `0x603079F0` | Push/PopModelMatrix: +0x268 = saved VP; +0x224 = modelWorld*VP during an object's draw, restored by Pop |
| caesarrm `0x60317590` | RMFrame_UpdateWorld (world +0xBC, render-world +0xEC with scale, inverse +0x140) |

### Item 1: RMCamera layout (confidence: high)
All matrices are **row-vector, row-major (D3D style): `v' = v * M`, translation in row 3, `M[r][c]` at `r*0x10 + c*4`**.
Evidence: 0x603025E0 computes w from m[3],m[7],m[11],m[15] and x from m[0],m[4],m[8],m[12].
The engine's 4x4 (`RMMatrix44`) is **0x44 bytes**: 16 floats plus a trailing tag dword (0x60301F00 writes
`+0x40 = 0x60301F70`, and every copy moves 0x11 dwords). Copy only 16 floats into a D3DMATRIX.
`RMMatrix43` is 4 rows x 3 floats (0x30). 0x60301F00 expands it with column 3 = (0,0,0,1).
RMCamera size is 0xDEC (ctor 0x60302A30), and it derives from RMFrame.

| Offset | Type | Meaning | Anchor |
|---|---|---|---|
| +0x0BC | RMMatrix43 | camera->world (rows 0..2 = right, up, forward; row 3 = position) | GetWorldTrans 0x603168F0 |
| +0x0E0 | float[3] | camera world position (row 3 of +0xBC) | copied to +0xDD0 in BeginScene |
| +0x184 | int | matrix dirty flag (UpdateCameraMatrix only recomputes when != 0) | 0x60303936 |
| +0x1C8 | fn | RenderFX callback per output vertex | SetRenderFXCallback 0x60305130 |
| +0x1D0 | fn[4] | project funcs 0x603025E0 / 0x603026B0 (fog) / 0x603027F0 / 0x603028D0, index +0xDB4 | ctor 0x60303128 |
| +0x1E0 | RMMatrix44 | **proj** (view -> pixel space). See below | BuildProjection |
| +0x224 | RMMatrix44 | **view*proj** (world -> screen). During a pushed object draw: model*view*proj | UpdateCameraMatrix / 0x60307440 |
| +0x268 | RMMatrix44 | saved view*proj while an object matrix is pushed | 0x60307440 |
| +0x2AC | RMMatrix43 | camera->world snapshot at BeginScene | 0x60304D30 |
| +0x2EC | float | field = tan(fov/2) * near | Get/SetField 0x60303370/0x60303340 |
| +0x2F0 | float | near (front clip). The game sets 5.0 | GetFrontClip 0x603035F0 |
| +0x2F4 | float | back clip, 5000. Used **only** for a cull plane when +0xDA4 != 0 (the game disables it) | GetBackClip 0x60303680, EnableBackClip 0x60303690 |
| +0x2F8 | float | **fov, full angle, radians** | GetFieldAngle 0x60303590 |
| +0x2FC | RECT (int) | clip rect | Get/SetClipRect 0x603037A0/0x60303720 |
| +0x30C | RECT (int) l,t,r,b | **view window / viewport** (SetViewWindow(x,y,w,h): r=x+w, b=y+h) | GetViewWindowRect 0x60303900 |
| +0x31C | RMMatrix44 | **view** (world -> camera), affine inverse of +0xBC. Can go straight to SetTransform(VIEW) | UpdateCameraMatrix |
| +0x360.. | float4[] | frustum planes (count +0xD78) | |
| +0x644..+0x658 | float[6] | side-plane normals from SetFieldAngle | |
| +0x670 | IMRender* | | BeginScene |
| +0x674 | IMVertex[4] | viewport clip quad given to IMRender::PushClip | SetClipRect |
| +0xD94 | int | projection type (0 = perspective, 1 = parallel) | Get/SetProjectionType 0x60303330/0x603032F0 |
| +0xD98/+0xD9C | int | camera offsets X/Y (fiadd into proj), default 0 | Get/SetCameraOffsets |
| +0xDA0 | float | **aspect = W/H** (ctor and SetViewWindow 0x6030386B recompute it; SetAspectRatio overrides it) | GetAspectRatio 0x60303710 |
| +0xDA4 | int | back clip enable | |
| +0xDB4 | int | fog on (index into +0x1D0) | |
| +0xDC0 | float | parallel zoom | |
| +0xDD0 | float[3] | eye position snapshot for this scene | |

Projection (+0x1E0), perspective. Let `W = r-l`, `H = b-t`, `cx = (l+r)/2`, `cy = (t+b)/2`, `n = +0x2F0`, `t = tan(fov/2)`,
`K = 500000` (constant 0x60342278, **not** the back clip):
```
a = +0xDA0;  if a > 1: Sx = W/2, Sy = H*a/2   else: Sx = W/(2a), Sy = H/2
P[0][0]=Sx  P[1][1]=-Sy  P[2][0]=cx*t+offX  P[2][1]=cy*t+offY  P[2][2]=t*K/(K-n)  P[2][3]=t  P[3][2]=-n*t*K/(K-n)  others 0
```
So `rhw = 1/(zv*t)`, `sx = cx + Sx*xv/(zv*t)`, `sy = cy - Sy*yv/(zv*t)`, `sz = K/(K-n)*(1-n/zv)`.
Camera space is left-handed: +x right, +y up, +z forward.

**Un-project** (read P live from +0x1E0 instead of rebuilding it):
- `zv = 1/(rhw*P[2][3])`
- `xv = (sx - P[2][0]/P[2][3]) / (rhw*P[0][0])`
- `yv = (sy - P[2][1]/P[2][3]) / (rhw*P[1][1])`
- `world = [xv yv zv 1] * cameraWorld(+0xBC as 4x4)`
- Equivalent: `world = [sx/rhw, sy/rhw, sz/rhw, 1/rhw] * inverse(+0x31C * +0x1E0)`.

A standard D3D projection for Remix: `xScale = 1/t` and `yScale = a/t` (for a > 1), `zn = n`. Choose `zf` to suit Remix;
the engine uses 500000.

### Item 2: pointer chains (confidence: high)
- `App* = [0x4CED70]`. RenderMgr = App+0xBAC is embedded, not a pointer.
- `RMRender* = [App+0xCB8]`.
- **Active camera:** `RMCamera* = [App+0xCCC]`, i.e. `[[0x4CED70]+0xCCC]`.
- **Camera of the current BeginRenderScene:** `[RMRender+0x2C]`, i.e. `[[[0x4CED70]+0xCB8]+0x2C]`. Prefer this in the proxy.
  RenderFrame issues several BeginRenderScene calls per frame (main camera, then extra cameras), and each one becomes one
  IMRender::BeginScene and so one D3D9 BeginScene. Reading RMRender+0x2C at D3D9 `BeginScene` (or at draw time) always
  gives the camera that produced the vertices. The cameras used this frame are at RMRender+0xE4[0..count-1], count at +0xE0.
- No caesarrm global holds RMRender or a camera (0x6034D504 is a flags word). caesarim's 0x6012CFC0 is its D3DApp
  struct and has no camera data.
- Example reads: view = 16 floats at `cam+0x31C`, proj = 16 floats at `cam+0x1E0`, fov = `cam+0x2F8`,
  near = `cam+0x2F0`, aspect = `cam+0xDA0`, viewport = 4 ints at `cam+0x30C`.

### Item 3: caesarrm -> caesarim boundary and per-frame hook (confidence: high)
- caesarim imports used per scene: `UpdateViewWindow(RECT,int)`, `PushClip(_imvertex*,..)`, `BeginScene(long)`,
  `SetBackgroundColor`, then the Draw*/ClipAndAppend*/External* families, all of which take `_imvertex*` (32-byte TLVERTEX).
  **No matrix crosses the boundary.**
- Transform: `RMCamera::ViewportTransformFace` 0x60304670 -> `[cam+0x1D0+fog*4](cam+0x224, in, out, n, ...)`.
- Hook options, best first:
  1. No code hook: in the D3D9 proxy, read `[[[0x4CED70]+0xCB8]+0x2C]` at BeginScene or draw time and use
     view `+0x31C` and proj `+0x1E0`. These two are **not** touched by Push/PopModelMatrix, so they stay stable for the
     whole scene. `+0x224` is not stable: it holds model*VP while a rigid model is being drawn.
  2. Exported `?BeginRenderScene@RMRender@@QAEHPAVRMCamera@@@Z` (GetProcAddress on caesarrm). Matrices are final when
     it returns.
  3. Internal `RMCamera_BeginScene` 0x60304D30 (thiscall, ecx=cam), after the UpdateCameraMatrix call (0x60304D5x).
- Caveats:
  - `RMRender::Render` 0x60337740 can temporarily move the camera with SetPosition/SetOrientation (portal-frame case)
    and restore it at the end. Reading at draw time is safest.
  - BeginScene sets x87 to 24-bit precision for the scene.

### Item 4: FOV 70 (confidence: angle semantics high; config plumbing medium)
- Engine angle unit is radians. SetFieldAngle does `fptan(angle*0.5)`. The game hardcodes `0x3F9C61AA` = 1.22164 rad = 70 deg
  in RenderMgr_CreateCamera, and the caesarrm ctor default is the same.
- Projection scale: `t = tan(fov/2)` = 0.7002 for 70 deg. It is stored as `proj[2][3]` (+0x20C) and as
  `field = t*near` (+0x2EC). Pixel scale is `Sx = W/2` (and `Sy = H*aspect/2 = W/2` when aspect = W/H > 1), so
  **fov = horizontal FOV** for a landscape view window. Vertical FOV = 2*atan(t*H/W) = 55.4 deg at 4:3.
  This holds while +0xDA0 = W/H. SetViewWindow recomputes it, but `SetAspectRatio` is reachable from components through
  the wrapper at Timeline 0x43B9B9, so **verify +0xDA0 live**.
- Config: the key "FOV" is a UTF-16 resource string in Timeline.exe (file offset 0xDB902). Config table field `cfg+0xD4`
  gets a default of 70.0f at 0x43033C (table at `[obj+0x66C]`). Player.dll uses 70.0 as the default for setting index 0x11
  at 0x61D2237D. **The degrees -> radians -> SetFieldAngle site was not found.** Component-facing wrappers
  `0x43B8F0` (SetFieldAngle) and `0x43B920` (SetFrontClip) exist. Inference: the config value is degrees and is converted to radians.
  Live test: set `FOV 90` in config.cfg, then read `cam+0x2F8`. Expect 1.5708. If it stays 1.2216, the config does not
  drive the render camera.

### Item 5: per-object world matrices (confidence: medium-high)
- Every renderable derives from RMFrame. `RMFrame+0xEC` (RMMatrix43) is the **model->world matrix used at draw time**
  (`worldTrans +0xBC` with per-axis scale +0x74..+0x7C applied when +0x18C > 0, built by RMFrame_UpdateWorld 0x60317590).
  `+0xBC` = unscaled world, `+0x8C` = local, `+0x140` = inverse(+0xEC) as 4x4, `+0x190` = parent frame.
- Rigid RMModel draw (0x60321F30/0x60322720): `if (model+0x188 == 0) cam->PushModelMatrix(copy of model+0xEC, ...)`
  (0x60322750). After that, cam+0x224 = modelWorld*view*proj and cam+0x268 = view*proj until PopModelMatrix 0x603079F0.
  `model+0x188 != 0` ("static") means the vertices are already in world space and no matrix is pushed (rooms/level
  geometry). For Remix, use identity world for these.
- For hooking per object: break at 0x60307440 (thiscall, ecx=cam). The 0x30-byte matrix is passed **by value** on the stack
  at [esp+4..+0x30]. Alternatively read `model+0xEC` from the RMModel* in ebp at 0x6032272A. RMModel vtable is 0x603424F0.
  Mesh is at model+0x20C.
- Deformation models (characters) are skinned on the CPU into world space (see the section above). They have no
  per-object matrix.

### Suggested Live Verification
1. `mem read [[0x4CED70]+0xCCC]` -> cam. Read cam+0x2F8 (expect 1.2216), +0x2F0 (5.0), +0xDA0 (1.3333 at 1024x768),
   +0x30C (0,0,1024,768), +0x20C (0.7002), +0x1E0 (P00 = 512, P11 = -512).
2. Check `[[[0x4CED70]+0xCB8]+0x2C] == [[0x4CED70]+0xCCC]` during the main scene, and trace 0x60337A30 to count scenes per frame
   (the dxwrapper trace showed 7 BeginScene per frame).
3. Un-project one live TLVERTEX with the formulas above, re-project it with +0x224 (outside a model push), and compare.
4. Config FOV test (Item 4).

## Path A milestone 1: view-space un-projection (session 2, 01:05)

Proxy module `src/comp/modules/screen_space.{hpp,cpp}` (wired in `d3d9ex.cpp` DrawPrimitiveUP /
DrawIndexedPrimitiveUP / SetViewport / SetTransform) rebuilds view-space XYZ from TLVERTEX and draws FFP
`XYZ|DIFFUSE|TEX1` with WORLD = VIEW = identity. Result: **Remix ray traces Timeline** (shadows visible),
but camera-at-origin makes the world move every frame (artifacts); real VIEW matrix is the next step.

Live probe results (`rtx_comp/console.log`, `[ScreenSpace]` lines), caesarim return addresses:
| Site | Call | Content |
|---|---|---|
| `0x6010392F` | DrawPrimitive TRIANGLEFAN | HUD/2D: rhw = 1, sz = 0.5 exactly |
| `0x60103A00` | DrawPrimitive TRIANGLELIST | distant geometry, diffuse = FF000000 (fogged), z ~ 10k-20k |
| `0x60103ADA` | DrawIndexedPrimitive TRIANGLELIST | main world + characters, n up to ~950 |

- Depth mapping: sz = 1.00001 - 3.508 * rhw across many draws (residual ~1e-6) => rhw = 1/z_view,
  near ~= 3.5, far ~= 350000 (effectively infinite). A few indexed batches contain vertices off the fit
  (sz < 0, residual 0.17): near-plane-clipped / generated vertices.
- Game renders at the window size (1680x1050 in windowed mode); viewport tracked from SetViewport.
- Vertex colour carries baked CPU lighting + fog; RGB forced to white, alpha kept.
- Frida (livetools attach, or spawn + Interceptor on caesarim draw sites) makes the game shut down
  cleanly within seconds (bridge logs show an orderly D3D9 destroy, not a watchdog). Cause unknown.
  Use the proxy for runtime data instead.

## Path A milestone 2: world space with the real camera (session 2, 01:20) — WORKING

`screen_space` now reads the scene camera at draw time from `[[[0x4CED70]+0xCB8]+0x2C]` (live values:
t = 0.70021, near = 5, P00 = 840 = W/2, P11 = -840, centre = viewport centre, at 1680x1050), inverts
the projection exactly, lifts vertices to world space with inverse(view +0x31C) and draws FFP with
WORLD = I, VIEW = camera view, PROJECTION rebuilt from P. Vertex RGB forced to white (baked lighting).

Vertex explosions: caused by stale/zeroed slots in caesarrm's reused vertex pools (rhw = 0, sz = -0.001
at screen (0,0); or off-curve sz from older projections) lying inside the DrawIndexedPrimitiveUP
[MinVertexIndex, NumVertices) range. Indices never referenced them (no out-of-range indices), yet Remix
used the whole vertex range. Fix: every batch is re-emitted as a compact indexed TRIANGLELIST containing
only referenced vertices; triangles with a vertex off the camera's depth curve
(sz = Q(1 - near*t*rhw), Q = 500000/(500000-near), tolerance 1e-3) are dropped (0 dropped in practice).
Confirmed fixed by user. The TRIANGLEFAN site 0x6010392F also carries 3D geometry, not only HUD.

Remaining: engine culling (object/portal visibility, frustum clip, portal 2D clip, backface) means
Remix only gets what the camera sees; clipped fragments change every frame. Agent mapping patch points.

## Culling and clipping patch points (session 2, from static agent; bytes verified offline, not yet live)

All object culling goes through `RMCamera::BoundsCheck` caesarrm 0x60305880. Returns 0 = whole mesh, no 3D/2D clip
(no-clip transform 0x60304980 + DrawIndexedPolysNoClip), 1 = 2D clip test, 2 = culled, 3 = per-face clip. Forcing 0
sends every model/character through the whole-mesh path; the only remaining cull there is the 2D backface filter
0x603221F0. 3D clipping is already near-plane only in normal rendering. Real renderers: RMModel 0x603223E0 / 0x603236A0,
characters 0x60313A80 / 0x603147E0 / 0x603161A0 (0x60321F30 / 0x60313750 are mouse pick).

| # | Item | Address | Original | Patch | Effect / risk |
|---|---|---|---|---|---|
| 1 | Object cull + LOD | rm 0x60305880 (22 B) | 81 EC 90 05 00 00 53 55 56 57 8B D9 33 FF 89 7C 24 34 8D 44 24 54 | 8B 44 24 78 85 C0 74 06 C7 00 40 42 0F 00 33 C0 C2 78 00 90 90 90 | area=1000000 (finest LOD), return 0. Affects pick too. Needs #4 |
| 1b | Same, render paths only | call sites 0x6032255C 0x603238E6 0x60313C44 0x60314A0D 0x6031632B | E8 rel32 -> 0x60305880 | E8 rel32 -> cave with #1 stub | keeps pick/shadow original |
| 2 | rm backface | 0x6032227E | 74 31 | 90 90 | both sides in no-clip filter |
| 3 | 3D clip near only | 0x6030489E | 7E 7A | EB 7A | optional (already default) |
| 4 | Projected shadows off | 0x6033941B | 7D 33 | EB 33 | required with #1 (else receivers redrawn per caster) |
| 5 | Portal 2D narrowing | 0x60339FB2 (37 B) | 8B 86 54 01 00 00 8B 4C 24 10 6A 00 8B 80 F4 06 00 00 50 51 8B 8E 50 01 00 00 68 20 D5 34 60 FF 15 0C 21 34 60 | 8B 8E 54 01 00 00 E8 83 CE FC FF EB 18 + 24x90 | ForceViewportClip instead of portal polygon; rooms reachable twice drawn twice; max 28 hops |
| 6 | Portal distance cutoff | 0x6033328F, 0x603332CC | 75 19, 75 19 | EB 19, EB 19 | optional |
| 7 | "known culled" hint | 0x603225E2 / 0x60323894 / 0x60313BE4 / 0x603149C5 | 74 D2 / 0F 84 9E 00 00 00 / 74 72 / 74 56 | NOPs | only if objects still pop |
| 8 | Particles no-clip | 0x6032B540 | 6A 04 | 6A 00 | optional |
| 9 | im backface DrawPoly | im 0x6010CBE2 | 75 09 | EB 09 | |
| 10 | im backface DrawPolyNoClip | im 0x6010CCAC | 75 05 | EB 05 | |
| 11 | im backface DrawFan | im 0x60103858 | 75 06 | EB 06 | |

Notes: projection rhw = 1/w (0x6030261C) is unguarded; w = 0 -> inf/NaN (proxy drops non-finite), w < 0 -> negative rhw
(proxy inverts exactly). Don't disable near clip globally (pick/portals rely on it). Don't NOP 0x60339F83 count==0 check.
Rooms behind the camera still skipped after #5 (would need a cave around call 0x60339F78). Duplicate rooms: detour
RMRoom_Render 0x60339330. No buffer overflows with this set. Verification script: scratch/verify_patches.py.

## Sky and background (session 2b)

### Summary
The engine has **no sky renderer**. In every outdoor level the sky is an ordinary static RMModel named **`SkyBox`**
(some levels also have a `SkyBox<Lvl>` object). It is a closed box textured with five images, `SkyBox<Lvl>_{UP,LF,FR,RT,BK}.bmp`.
Cloud layers are separate models (`Clds01/02`, `CldLyr*`, `Cloud*`), and so are the lightning cards (`OneFlsh`..`ForFlsh`).
The box is **fixed in world space** (it does not follow the camera). It is depth-tested, writes z and is drawn in normal room
order: not first, with no special render state and no flag. It spans 37,800 to 200,000 units and stays visible only because the
projection far plane is K = 500000.
The engine's only background features are:
- the per-camera clear colour,
- fog (computed per vertex on the CPU),
- an optional camera backdrop texture (a full-screen quad with `rhw = 1`, `sz = 1`).

Recommendation: the proxy flags SkyBox draws itself. It hooks `RMModel_Render` through the vtable and tests the name at
`model+0x24`. It then submits those batches with viewport `MinZ = MaxZ = 1.0`, which the existing `rtx.skyMinZThreshold = 1`
should pick up. The proxy's projection must use far >= 500000.

### Key Addresses
| Address | Description |
|---|---|
| caesarrm `0x60342560` | Slot +0x70 of the RMModel vtable (0x603424F0) = `RMModel_Render` 0x60322330. This slot is the **only reference** to the function, so patching it catches every rigid-model draw. thiscall, 4 stack args (`imRender, camera, a3, hint`), `ret 0x10` |
| `RMObject+0x24` | `char name[64]` (GetObjectName 0x60329C60 = `lea eax,[ecx+0x24]`). Timeline.exe sets names with SetObjectName during scene load (about 25 call sites, e.g. 0x44F011). The model name should be `"SkyBox"` |
| `RMModel+0x1D0 / +0x22C` | RMTexture* array and its count (GetTexture 0x603256D0). `RMTexture+0x24` = name (`"SkyBoxOPP_UP.bmp"`), `RMTexture+0x74` = IM texture handle |
| caesarim `0x6012CFC4` | IM texture handle currently bound (written by IMDevice_SetTexture 0x60103720). Its D3D texture = `[[0x6012CFC0]+0x8380+h*4]` |
| caesarrm `0x60339330` | RMRoom_Render draw order: (1) rooms seen through portals; (2) the room's opaque models in list order (`[model+0x1CC]==0`, vtbl+0x70 call at 0x60339536); (3) translucent models sorted far to near (qsort 0x603395A1, key 0x603392B0, comparator 0x60339310) with IM ALPHATEST on |
| caesarrm `0x60304D30` | RMCamera_BeginScene, in order: fog setup from `[cam+0x14]`; `IMRender::SetBackgroundColor(cam+0xDB0..B2)` at 0x60304E1F; PushClip; UpdateViewWindow; `IMRender::BeginScene(clearColour)` at 0x60304E91 (arg 0 when ClearZOnly or an opaque backdrop is set); the backdrop quad via DrawIndexedPolysNoClip at 0x60304EBB |
| Timeline `0x412635..0x412664` | Background choice in RenderMgr_RenderFrame. If fog is on and `foginfo.fogMax == 255`: background = fog colour (RM state 0x11), `EnableBackClip(1)`, `SetBackClip(fogEnd)`. Otherwise: `EnableBackClip(0)`, background = RenderMgr+0x18E/+0x18D/+0x18C (R,G,B) = `[0x4CED70]+0xD3A/+0xD39/+0xD38` |
| caesarim `0x601045C0` | IMDevice_SetRenderState: maps IM states to D3D states (enum IMRenderState in kb.h). 14 -> ZWRITEENABLE, 7 -> ZFUNC, 16 -> FOGENABLE, 17 -> FOGCOLOR |
| caesarim `0x6012D018 / 0x6012D01C` | Current FOGENABLE / FOGCOLOR (0x00RRGGBB). The same values reach D3D9 as SetRenderState(FOGENABLE / FOGCOLOR) |
| caesarrm `0x60338100 / 0x60338120` | RMRender Set/GetFogInfo: 16-byte `_foginfo` at RMRender+0x64 |

### Item 1: how the sky is rendered (confidence: high)
- No binary contains the string "sky" (Timeline.exe, caesarrm, caesarim, Components\*.dll), so the sky is pure content.
  Every outdoor `.oxf` has a `SkyBox` object (01, 03, 04, 05, 06, 07, 08, 09, 10, 11, 12, 13, 14, 16, 17, 20).
- Scene data, decoded with scratch/oxfobj.py and scratch/oxfbig.py, for `SkyBox` in 01ObservationPointPresent:
  - frame position (-1.7, -9306.9, -270.9), identity orientation;
  - bbox x[-18899,18892] y[-2160,13791] z[-19167,18624], i.e. a fixed world box of about 37,800 x 16,000 x 37,800;
  - flag chunks (TSMH `..0101`, EDMH `01`) are the same as on ordinary static level geometry (919 of about 1,200 objects have them),
    so the data has **no sky flag**.
- SkyBox size in other levels: 05 = 169k, 12 = 203k, 16 = 133k, 20 = 96k, 13 = 37.8k. Cloud layers are 20k to 190k and sit inside the box.
  Some large objects are not sky (level 20: `TrrnOutr` 22k, `Mask` 96k), so size alone does not separate the sky cleanly.
- Draw path: RMRoom_Render -> RMModel vtbl+0x70 (0x60322330) -> RenderMesh 0x603223E0 / 0x603236A0, with the z-buffer on.
  ZWRITE is not a sky signal: the RMModel paths change IM ZWRITEENABLE (0x60322812 / 0x60323678) only for transparency modes
  (`model+0x33C` = 1 or 2). FILLMODE (IM 3) changes only for the wireframe flag `+0x1FC & 0x20`.
- **The FF000000 batches at 0x60103A00 (view depth 10k-20k) are almost certainly the SkyBox**, possibly with cloud cards
  (confidence medium-high):
  - The camera is always inside the box's bounds. Before the culling patch, BoundsCheck therefore returned "intersecting"
    (status 3). That sends the box down the per-face clip path, which emits non-indexed lists through DrawPolysNoClip
    (0x6010CCE0 -> IMDevice +0x10 -> site 0x60103A00).
  - The faces sit about 19k from the camera, which matches the depth seen at that site.
  - The vertices are black because nothing lights the box. It is still visible because IMDevice_SetTexture sets
    TEXTUREMAPBLEND = DECAL (texture only) when bit 2 of the IM texture flags is set, instead of MODULATE.
  - **With culling patch #1 (BoundsCheck -> 0), the sky goes through the indexed whole-mesh path, site 0x60103ADA**, so the
    call site no longer identifies the sky.
- Camera backdrop: the game can set one with `RMCamera_SetBackdropTexture`, called from the scene camera object (Timeline
  0x47728E) or the component wrapper (0x43BB69). It is drawn at BeginScene as an indexed 4-vertex quad with `sz = 1.0` and
  `rhw = 1.0` (clip quad `cam+0x674`). The proxy's existing `rhw == 1` test already sends it to 2D passthrough.
  It has not appeared in the live data from the levels seen so far.

### Item 2: recognising sky draws at draw time (confidence: high for the mechanism, medium that the name is exactly "SkyBox")
Options, best first:
1. **RMModel_Render vtable hook (recommended).** In the proxy, once caesarrm is loaded:
   - `VirtualProtect(0x60342560, 4)` and replace `0x60322330` with a thunk:
     `__fastcall Hook(RMModel* m, void*, void* imr, void* cam, void* a3, int hint) { prev = g_drawModel; g_drawModel = m; r = orig(m, imr, cam, a3, hint); g_drawModel = prev; return r; }`
   - In DrawPrimitiveUP / DrawIndexedPrimitiveUP: `isSky = g_drawModel && strncmp((char*)g_drawModel + 0x24, "SkyBox", 6) == 0`.
   - Optionally also match cloud layers by name (`Cld`, `Clds`, `Cloud`), but not "Dust Cloud".
   - The vertex fill and the IMRender draw run synchronously inside this call, so the flag is exact for each batch. Saving and
     restoring `prev` keeps nested calls correct.
   - The proxy must apply the patch itself, because Frida kills the game.
   - Characters (RMDeformationModel) and particles have their own vtables, so the flag stays clear for them.
   - Fallback if the model names turn out to be generic: treat the model as sky if any texture in `model+0x1D0[0..model+0x22C)`
     has an `RMTexture+0x24` name starting with `"SkyBox"`.
2. Texture handle: at draw time, compare `[0x6012CFC4]` (current IM texture handle) with the handles of the SkyBox RMTextures
   (`+0x74`). Building that handle set needs the same model or texture walk as option 1, so this adds nothing.
3. Geometry heuristic, no hook: after un-projection, treat a batch as sky if its world AABB is larger than about 30k, or if all its
   vertices are farther than 15k from the camera. This catches the sky and cloud layers, but also `TrrnOutr` and `Mask` in level 20.
   Use it only as a fallback.
- The engine has **no** flag, global or render state that marks the sky. ZWRITE, ZFUNC and FOG do not change around it.

### Item 3: scale and camera-relativity (confidence: high)
- The box is fixed in world space and does not follow the camera. Nothing moves it: no code does, and the only sky reference in
  the scene scripts is the `LightUpSky` lightning toggle (`Geometry.UseObject "OneFlsh"`).
- It sits about 19k (level 01) to about 100k (levels 05/12) from the playable area. That area is only a few thousand units across,
  so parallax exists but is tiny.
- Un-projected sky vertices therefore come out at stable world positions, not as a mesh that moves with the camera.
  Consequences for Remix:
  - The proxy's rebuilt PROJECTION needs `zf >= 500000`. Box corners reach about 150k in level 12, so a smaller far plane clips the sky.
  - Drawn as opaque RT geometry, the box encloses the whole level and blocks all sky and dome light. That is the dark scene
    observed so far.

### Item 4: background colour, fog colour and range (confidence: high for storage and flow, medium for `_foginfo` field names)
- Fog is computed per vertex on the CPU: `RM_ProjectVertsFog` (0x603026B0) writes the fog factor into specular alpha.
  At BeginScene the camera copies the fog parameters from `S = [cam+0x14]`:

  | Source | Camera field |
  |---|---|
  | `S+4` (enable) | `cam+0xDB4` |
  | `S+0x1C` (start) | `cam+0xD80 = start^2` |
  | `S+0x20` (end) | `cam+0xD84 = 255/end^2` |
  | `S+0x24` (min) | `cam+0xD88 = 255-min` |
  | `S+0x28` (max) | `cam+0xD8C = 255-max` |

  These offsets line up exactly with RMRender if `S` = RMRender+0x48: `+0x4C` is fog enable (RM state 0x10) and `+0x64` is
  `_foginfo {start, end, min, max}`. This is inferred, not traced.
- Fog colour is IM state 17, cached in caesarim at `0x6012D01C`. It reaches D3D9 through dxwrapper as
  `SetRenderState(D3DRS_FOGCOLOR)`, and fog enable reaches it as `D3DRS_FOGENABLE` (cached at `0x6012D018`). The proxy can capture
  both in its own SetRenderState hook.
- Background colour: RenderMgr_RenderFrame (Timeline 0x412580) chooses it every frame.
  - Fog on and `fogMax == 255` (distance fully fogged): background = fog colour, back clip = fogEnd.
  - Otherwise: background = RenderMgr bytes `+0x18E/+0x18D/+0x18C` (R,G,B) = `[0x4CED70]+0xD3A/+0xD39/+0xD38`, and back clip is off.
  - The chosen colour is stored per camera at `cam+0xDB0/1/2` (R,G,B). It goes to IMRender::SetBackgroundColor -> IMDevice
    vtbl+0xA4 (0x60105E50, the DX5 viewport background material). The proxy sees it as the colour of the D3D9 `Clear(D3DCLEAR_TARGET)`.
- Fog and background are scene data. RenderMgr serialises them (load around 0x411380..0x4114D1 -> SetFogInfo, save around
  0x410FB0), so they change when a level or room loads, not every frame.
- Extra cameras added by RenderMgr 0x413BA0 have `ClearZOnly = 1`, so their BeginScene does not clear colour.

### Item 5: recommendation for Remix (confidence: high for the engine facts; Remix behaviour not verified here)
The current rtx.conf already sets `rtx.skyMinZThreshold = 1`, `rtx.skyAutoDetect = 0`, `rtx.skyDrawcallIdThreshold = 0` and
`rtx.skyReprojectToMainCameraSpace = False`, and lists 5 hashes in `rtx.skyBoxTextures` (probably one level's SkyBox faces).
- (a) Texture tagging:
  - It needs no code, but each sky set must be tagged once by hand: about 9 sets x 5 faces (OPP, OPST, CLV, VLNT, CTG, CCYGH, CA,
    ROV, LRE).
  - Remix hashes texture content, so un-projecting the geometry does not change the hashes.
  - If the sky still goes dark in the level those 5 hashes belong to, the hash list is not the problem. Remix's sky path is
    rejecting the draws for another reason, probably draw order (see (b)).
- (b) Distinctive state (recommended):
  - The engine draws the sky **mid-frame in room order with depth testing on**, not first and not with z disabled. So
    `skyAutoDetect` (camera-position and depth-flag heuristics) and `skyDrawcallIdThreshold` will not find it as drawn.
    The proxy has to create the signal itself.
  - Flag the batch with the vtable hook from Item 2. Draw it with the same world-space vertices, VIEW and PROJECTION, but with
    `SetViewport(MinZ = 1.0, MaxZ = 1.0)`, then restore the viewport. Remix's `skyMinZThreshold = 1` should then classify it as
    sky and rasterise it into the sky background instead of ray tracing it. Optionally also set ZWRITEENABLE = FALSE for these draws.
  - If Remix also requires sky draws to come before the first world draw: the sky is static world geometry, so the proxy can cache
    its world-space batches once per level and re-emit them right after each frame's first BeginScene. Key the cache on the model
    pointer plus vertex count, and invalidate it when the set of RMModel* changes.
- (c) Screen-space passthrough of sky batches (keep the original TLVERTEX):
  - This is the simplest option. Remix treats the batches as UI/overlay, so the sky neither blocks nor gives light, and its
    colour does not light the scene.
  - Because the engine draws the sky after some world geometry, the overlay would also cover that geometry.
  - Not recommended, except as a quick test to confirm that removing the box fixes the darkness.
- Fog and background for Remix:
  - Per draw: read `FOGCOLOR` / `FOGENABLE` from D3D9 SetRenderState, and the `Clear` colour.
  - Per level: read `[0x4CED70]+0xD38..0xD3A` and `_foginfo` at `RMRender+0x64` (`RMRender = [[0x4CED70]+0xCB8]`).

### Suggested Live Verification
1. In the proxy, after patching vtable slot 0x60342560, log `(char*)model+0x24` and the batch vertex count once per unique model
   per frame. Confirm the name is "SkyBox" (or "SkyBox<Lvl>") and that these are the batches at view depth 10k-20k.
2. For the flagged batches, log `[0x6012CFC4]` (IM texture handle). Check that it maps to the D3D9 texture whose Remix hash is in
   the current `rtx.skyBoxTextures`.
3. In level 01, read `[[[0x4CED70]+0xCB8]+0x4C]` (fog enable), the 16 bytes at `[[0x4CED70]+0xCB8]+0x64` (`_foginfo`), and the
   bytes at `[0x4CED70]+0xD38..+0xD3A` (background B,G,R). Compare them with the FOGCOLOR and Clear colour the proxy sees.
4. Draw the flagged batches with viewport MinZ = MaxZ = 1. In the Remix debug view, check that the sky becomes background and
   the scene is lit.

## Path B: characters and skinning (research) — 2026-11-04 02:14

### Summary
Characters are `RMDeformationModel` objects (caesarrm, vtable `0x60342308`, derived from RMModel). The skinning is **rigid
per-vertex**: every unique mesh vertex is bound to exactly **one** `RMFrame` (a skeleton joint), and there are **no weights**.
At load time Human.dll creates one RMFrame per joint, assigns `frames[v] = joint` for every vertex, and calls
`EndFramesUpdate`. That call rewrites the mesh's own vertex positions into **joint-local space**
(`mesh[v].xyz = world.xyz * joint->inverseWorld(+0x140)`), so no separate inverse-bind matrix exists. The inverse bind is
already baked into the vertices.
Each frame, `RMDeformationModel_Skin` 0x60315870 (called from RenderA/RenderB) computes
`out[v].xyz = mesh[v].xyz * frames[v]->renderWorld(+0xEC)` into `model+0x344` (world space). Normals are copied **unrotated**.
The out buffer then goes through the same projection and draw path as rigid models.
For Remix this maps directly to D3D9 FFP indexed vertex blending with one matrix per vertex:
- bone matrix n = `joint+0xEC` (4x3 expanded to 4x4)
- vertex position = `mesh[v].xyz`
- BLENDINDICES = palette slot of `frames[v]`

Recommended design: substitute the character's existing `DrawIndexedPrimitiveUP` in the proxy (index-stable with culling
patches #1/#1b and #2), rather than drawing characters a second time.
Confidence: high for the data layout and skin math; medium for the proxy design (not yet verified live).

### Key Addresses
| Address | Description |
|---|---|
| rm `0x603139D0` | RMDeformationModel::Render (vtbl +0x70). If `flags(+0x1FC)&8` and RMRender state 0xB: RenderB (+0x80). Otherwise RenderA (+0x84) with pass=0, and again with **pass=1 if flags&0x400** (second pass of the same geometry) |
| rm `0x60313A80` | RenderA (vtbl +0x84), thiscall, **5 args, ret 0x14**: (IMRender*, RMCamera*, a3, a4, pass). Skin call at `0x60313E56` |
| rm `0x603147E0` | RenderB (vtbl +0x80), 4 args, ret 0x10: (IMRender*, RMCamera*, a3, a4). BoundsCheck `0x60314A0D`, light `0x60314B12`, skin `0x60314B23`, alloc IMVerts `0x60314B34`, no-clip transform `0x60314B7E`, per-texture-group `0x603221F0` at `0x60314C19`, overlay-texture pass from `0x60314C35` |
| rm `0x603161A0` | (was "RenderC") vtbl +0x7C, the **projected-shadow caster** pass. Its only caller is `0x6033991A` (shadow texture render), and culling patch #4 disables it |
| rm `0x60315AC0` | vtbl +0x78: ray/collision test on GetTransformedVertices (callers `0x60312ADC`...). Not rendering |
| rm `0x60313750` | vtbl +0x74: pick. Also calls Skin |
| rm `0x60315870` | **RMDeformationModel_Skin(aliasList, numAliased)**. CPU skin into `+0x344` |
| rm `0x603136A0` | InitFrames: `frames[numVerts]` (all = the model itself) and the `+0x344` buffer (`numTotalVerts*0x34`, xyz and normal zeroed) |
| rm `0x60313560` / `0x60313590` | exported Begin/EndFramesUpdate. End bakes joint-local positions (xyz only) |
| rm `0x60313660` / `0x60313510` | exported GetFramesArray(int&) / GetTransformedVertices(int&, _vertex*&) |
| rm `0x60325BE0` | RMModel lighting: CPU lighting into mesh vertex colours, runs before Skin |
| rm `0x60304980` | RMCamera no-clip transform. Maps `RMVertex[i]` to `IMVertex[i]` 1:1, so it **preserves indices** |
| rm `0x603221F0` | RMModel_BuildFrontFaceIndices: copies a group's u16 triangles to `model+0x1F8` (2D backface test at `0x6032227E`), then `DrawIndexedPolysNoClip` (`0x603222F6`). Shared with RMModel |
| rm `0x6033C8C0` | RMTexture -> IM texture handle (`+0x74`; -2 = lost, then reloaded) |
| rm `0x60317590` / `0x60317570` | RMFrame_UpdateWorld recomputes `+0xBC/+0xEC/+0x11C/+0x140`, then recurses to its children: **eager propagation** |
| Timeline `0x43CF50/60/70` | Component service wrappers (vtable `0x4B9654` slots +0x540/+0x544/+0x548) -> Begin/EndFramesUpdate/GetTransformedVertices |
| Human `0x6150DFF0` | text skeleton loader ("NumJoints:", "Parent:", "NONE"). One RMFrame per joint via service +0x64, parent via +0x15C, position via +0x164 |
| Human `0x6150EAF0` | per-vertex joint-index loader: reads one int per vertex and sets `frames[v] = joints[idx].frame` (`0x6150ED6B..0x6150ED88`), then EndFramesUpdate `0x6150EDA1` |

### 1. Mesh format (confidence: high)
`RMVertex` (`_vertex`, stride 0x34): `+0 xyz`, `+0xC normal xyz`, `+0x18 color`, `+0x1C specular`, `+0x20 tu`, `+0x24 tv`,
`+0x28 tu2`, `+0x2C tv2` (overlay UVs), `+0x30` unused.

RMMesh (`model+0x20C`):
| Offset | Field |
|---|---|
| `+0xD8` | verts (`numVerts + numAliased` entries) |
| `+0xDC` | faces |
| `+0xE4` | overlay RMTexture* |
| `+0xEC` | aliasList (int per alias) |
| `+0xF0` | numVerts (skinned, unique) |
| `+0xF4` | numTotal |
| `+0xF8` | numFaces |
| `+0x108` | numAliased |
| `+0x10C` | numGroups |
| `+0x114` | `RMTextureGroup[]` (0xC each: `u16* tris; u16* trisAlt; int nTris`) |

- Triangles are u16 triples that index the full `numVerts + numAliased` range. `trisAlt` is used when `camera+0x6F4 != 0`.
- Aliased vertex `numVerts + j` (UV seams) takes its **position** from `aliasList[j]` and its own UV/colour.
  Skin does not write its normal, so it stays 0.
- Bind pose: `mesh.verts[v].xyz` is in the local space of joint `frames[v]`, already inverse-bound by EndFramesUpdate.
- Normals: `mesh.verts[v].nxyz` are still in the mesh/model space they were loaded in. Neither EndFramesUpdate nor Skin
  rotates them, so they are **not** joint-local. **Open:** whether they are meaningful at all. Check them live against
  recomputed normals.
- Bone assignment: `model+0x348` = `RMFrame* frames[numVerts]`, one per unique vertex. There are no weights and no per-vertex
  index field; the frame pointer *is* the assignment. Initially every entry = the model itself.
- Skeleton: one RMFrame per joint in a parent/child RMFrame hierarchy (`+0x190` parent, `+0x198` first child, `+0x194` sibling).
  Human object: `+0x368` numJoints, `+0x370` `HumanJoint[ ]` stride 0x118 (`+0` RMFrame*, `+4` name[256], `+0x104` pos,
  `+0x110` valid), `+0xE98` RMDeformationModel*, `+0x3D4` Timeline service interface.
- Bone matrices per joint frame:
  - local `+0x8C` (RMMatrix43)
  - world `+0xBC` (43, unscaled)
  - **render world `+0xEC` (43, with scale), which is what the skin uses**
  - normal matrix `+0x11C` (3x3)
  - inverse render world `+0x140` (4x4)

  All are row-vector, D3D-style. There is no separate inverse-bind matrix.
- Asset files: characters are not loose files under `Content\Models` (only `.ASC` props and two `.hrm`). The joint files are
  binary ('o'-prefixed extension) or text. The format was not reversed; the runtime structures are enough.

### 2. Skinning function (confidence: high)
`0x60315870 __thiscall Skin(int* aliasList, int numAliased)`, `ecx` = RMDeformationModel:
```
model+0x34C = [[model+0x14]+0x30]                      // frame stamp (never checked here: skins on every call)
n = mesh+0xF0; if (model+0x2D0) n = [[model+0x2D0]+0x144]   // progressive-mesh LOD vertex count
for v < n: J = frames[v]; M = J+0xEC (4x3)
   out[v].xyz = in[v].x*M[0] + in[v].y*M[1] + in[v].z*M[2] + M[3]  (rows of 3 floats)
   out[v].{normal,color,spec,uv,uv2} = in[v].{...}       // normal NOT rotated
for j < numAliased: s = aliasList[j]; out[N+j].xyz = out[s].xyz; out[N+j].{uv,uv2,color,spec} = in[N+j]   // N = mesh+0xF0
```
- Output buffer: `model+0x344`, **one per model**. It is allocated once in InitFrames and reused every frame, with a stable
  index layout (`numVerts` skinned plus `numAliased` aliases).
- Callers:
  - RenderA `0x60313E56`
  - RenderB `0x60314B23`
  - RenderShadowCaster `0x603163E6`
  - Pick `0x60313875`
  - exports Begin `0x6031357F` and GetTransformed `0x60313539`. Human.dll calls these outside rendering (load time, and
    `0x6150E903`, likely a ground/foot offset from a min-y scan (inferred)).

  So Skin is **not** a clean per-draw context hook.
- After Skin, in the whole-mesh path (BoundsCheck == 0):
  1. `RMCamera_AllocIMVerts(numTotal)`
  2. `0x60304980` maps `+0x344` to IMVertex 1:1
  3. per texture group g: texture = `model+0x1D0[g]` (RMTexture*), then `0x603221F0` emits the group's triangles (original
     vertex indices) and calls `DrawIndexedPolysNoClip` (one caesarim draw per group)
  4. if `mesh+0xE4` (overlay texture): a second pass with uv2 (`+0x28/+0x2C`) copied into tu/tv and colour = alpha only

  With BoundsCheck 1/3, faces go through `ViewportTransformFace`/`ClipAndAppendIndexed` and new vertices are generated, so
  indices no longer equal mesh indices.

### 3. Animation: where final bone matrices live and when (confidence: high for location; medium for exact timing)
- Final per-frame bone matrix = `joint RMFrame +0xEC` (render world with scale). `+0x140` is its inverse and `+0x11C` its
  normal matrix.
- Human.dll animates the joints through Timeline service calls that set frame transforms. RMFrame_UpdateWorld 0x60317590
  recomputes `+0xBC/+0xEC/+0x11C/+0x140` at once and recurses through the children (0x60317570), unless the frame's
  `+0x1AC` (update suspended) flag is set (begin/end helpers 0x60317870 / 0x603178E0; callers not mapped, medium).
  So matrices are already final when rendering starts.
- **Hook point where they are certainly final:** the Skin call inside RenderA (`0x60313E56`) or RenderB (`0x60314B23`). For a
  proxy, simply read `frames[v]->+0xEC` at draw time (inside RenderA/RenderB), which is the same data the engine uses.

### 4. Segmented? (confidence: high)
Not separate meshes: each character is **one mesh**. Every vertex is rigidly bound to one joint, so it behaves like rigid
segments, but triangles whose vertices sit in different joints stretch across the joint. Drawing per-joint rigid sub-meshes
would tear or drop those triangles. **FFP indexed blending with a single matrix per vertex is exact.**
Rigid segments are a fallback only. Count the cross-joint faces live to size that problem.

### 5. Recommended Path B design for characters
Approach: **replace the character's own D3D9 draw** inside the proxy. Do not add a second draw.
1. **Context.** Patch the RMDeformationModel vtable slots (data writes, VirtualProtect): `0x6034238C` (+0x84 RenderA, 5 args,
   ret 0x14) and `0x60342388` (+0x80 RenderB, 4 args, ret 0x10). Point them at proxy thunks that set
   `g_curDefModel = this` and call the original.
   - Alternative: detour `0x603221F0` (`ecx` = model, called once per texture group, shared with RMModel). This also gives
     rigid models the same per-draw context.
   - Leave `0x60342384` (shadow caster) and `0x60342380` (ray test) alone.
2. **Required engine patches** so indices are stable:
   - #1 or #1b on `0x60313C44` / `0x60314A0D` (BoundsCheck forced to 0: whole-mesh no-clip path, and finest LOD via
     area=1e6, so `[+0x2D0]+0x144` stays at full count)
   - **#2** `0x6032227E` (no 2D backface removal, so each draw is the full group triangle list every frame)
   - #4 (projected shadows off)
3. **In `DrawIndexedPrimitiveUP`** while `g_curDefModel` is set (the draw comes from `DrawIndexedPolysNoClip` inside
   `0x603221F0`), skip Path A un-projection and build:
   - `mesh = [m+0x20C]`, `V = [mesh+0xD8]`, `N = [mesh+0xF0]`, `alias = [mesh+0xEC]`, `frames = [m+0x348]`
   - palette: unique `frames[]` pointers -> slot (cache per model; rebuild if the pointer set changes). D3D9 allows 256
     WORLDMATRIX slots.
   - per referenced index `i`: `src = i < N ? i : alias[i-N]`; `pos = V[src].xyz` (joint-local);
     `idx = palette[frames[src]]`; `uv = V[i].tu,tv` (or the TLVERTEX's tu,tv, which also covers the overlay pass);
     normal = cached joint-local normal (below)
   - `SetTransform(D3DTS_WORLDMATRIX(k), expand43(frame_k+0xEC))`, `VIEW = cam+0x31C`, `PROJECTION` as in Path A
   - `D3DRS_INDEXEDVERTEXBLENDENABLE = TRUE`, `D3DRS_VERTEXBLEND = D3DVBF_1WEIGHTS` with weight 1.0 and all 4 indices = k.
     This matches the repo's tested skinning path: `src/comp/modules/skinning.cpp:89-99` expanded decl (FLOAT3 pos, FLOAT3
     weights, UBYTE4 idx, FLOAT3 normal, FLOAT2 uv), `:286` WORLDMATRIX upload, `:297-298` blend states. The dx9-ffp-port
     skill says FFP indexed blending is the supported Remix route.
   - Texture: keep what dxwrapper already bound for this draw. It is the same mechanism as rigid RMModel (`model+0x1D0[g]`
     RMTexture -> IM handle `+0x74` -> caesarim -> dxwrapper SetTexture), so no texture mapping is needed in the proxy.
   - Indices: pass the engine's u16 indices through unchanged. They are mesh indices, and the vertex order is stable per
     mesh, so Remix gets stable bind-pose vertex/index data and real bone matrices.
4. **Normals.** Stored normals are not joint-local (see 1). Option (a) is recommended:
   - (a) On first sight of each mesh, compute area-weighted vertex normals from the **world-space** `+0x344` positions
     (aliases share their source), then convert each into joint-local space with the rotation of `frames[v]+0x140`
     (normalize). Cache per mesh.
   - (b) Alternative: hook EndFramesUpdate (Timeline IAT `0x4B84D0`, all calls go through `0x43CF60`) and compute
     `n_local = n_mesh * R(model+0xEC) * R(frame+0x140)` there. This needs the proxy loaded before any character loads.
5. **Double-draw / extra passes:**
   - RenderA pass=1 (flags `0x400`) and the mesh overlay-texture pass draw the same geometry again with another texture.
     Substituting them would give coplanar duplicates. Drop them, or mark them as decals, after checking live what they
     look like.
   - The shadow-caster pass is removed by #4.
   - Path A's un-projection must ignore draws flagged as characters.
6. **Fallback** (if Remix FFP blending misbehaves): use the same substitution, but CPU-transform positions in the proxy
   (`pos * frame+0xEC`, `normal * frame+0x11C`) and submit world-space vertices with `WORLD = I` and the same stable
   indices. Ray tracing is fine; asset-replacement hashes then change every frame unless positions are excluded from
   `rtx.geometryAssetHashRuleString`.

### Human.dll base
- Preferred base `0x61500000`, size `0x80000`, relocation directory present (0x5130 bytes).
- No overlap with the other 11 Components DLLs, caesarrm (`0x60300000`) or caesarim (`0x60100000`). Only
  `ActiveScriptor.ocx`/`binkw32.dll` collide at `0x10000000`, and one of them relocates.
- Expected to load at its preferred base; confirm live with `livetools modules`.
- Human.dll was **not bootstrapped**. Its kb.h entries are hand-derived from a capstone dump (`scratch/human.asm`).
- Human.dll does not import caesarrm. It reaches the engine only through Timeline's service interface (vtable `0x4B9654`).

### Suggested Live Verification
1. Find a character: break or trace RenderA/RenderB (`0x60313A80` / `0x603147E0`) and record which one runs. That is
   `flags(+0x1FC)&8` together with RMRender state 0xB (`0x60337F50`). Also record `flags&0x400` and `mesh+0xE4`.
2. Dump `mesh+0xF0/+0xF4/+0x108/+0x10C` and `[m+0x2D0]`. Count the unique pointers in `frames[m+0x348]` (the joint count).
3. Count cross-joint faces (triangles whose three `frames[]` differ) across all groups.
4. Check `mesh[v].xyz * frames[v]->+0xEC == (m+0x344)[v].xyz` for a few v.
5. Compare stored normals `mesh[v]+0xC` with normals recomputed from `+0x344` positions.
6. In the proxy, confirm that the `DrawIndexedPrimitiveUP` issued inside RenderA/RenderB has indices < `numTotal` and
   `NumVertices == numTotal` once culling patches #1/#2 are active.

Addendum (session 2b):
- There are no direct `call 0x60322330` sites in caesarrm; the vtable slot 0x60342560 is the only way in.
- The SkyBox may be drawn in the opaque pass (list order) or the sorted translucent pass (`model+0x1CC != 0`, set when any of its textures has `RMTexture+0x88`; ALPHATEST on). Either way it is drawn mid-frame with depth testing, so the recommendation does not change. Log `[model+0x1CC]` in verification step 1 to find out which.
- kb.h has two lines for 0x60337E90: the bootstrap stub `_thunk_sub_603381E0` (line 530) and `RMRender_SetRenderState` (appended). The second is correct; it is the exported state setter.

## Lights (session 2b)

Static analysis only, not verified live. kb.h updated (backup `backups/2026-11-04_0213_kb-lights/`).

### Summary
- One light class in caesarrm: `RMLight` (an RMFrame, size 0x214, vtable 0x60342418, `+0xC == 9` identifies a light).
  No subclasses: `+0x1D0` = type (0 point, 1 spot, 2 directional, 3 ambient).
- All lighting is CPU per-vertex, summed into `RMVertex.color` (+0x18).
- Lights belong to a container (rooms are containers): array `+0x70[]`, count `+0x138` (max 49).
  A model is lit only by its own container's lights, `[model+0x19C]`.
- Room being rendered = active camera's container: `[[[0x4CED70]+0xCCC]+0x19C]`.
- Formula: standard FFP diffuse N·L with linear distance falloff; per-light result clamped to the material diffuse colour.
- Baked lighting also exists: prelit vertex colours, snapshot (cached) lighting, per-room radiosity.

### RMLight layout (high confidence, anchored on getters/setters and ctor 0x60318110)
| Offset | Type | Meaning |
|---|---|---|
| +0x110..0x118 | float3 | world position |
| +0xD4..0xDC | float3 | world direction (row 2 of world matrix, local +Z); light travels along +dir |
| +0xC8..0xD0 | float3 | up (row 1) |
| +0x19C | RMContainer* | the light's room |
| +0x1C8 | int | enabled (default 1) |
| +0x1CC | int | falloffEnable (default 0) |
| +0x1D0 | int | type 0 point / 1 spot / 2 directional / 3 ambient |
| +0x1D4 | u8[3] | colour r,g,b |
| +0x1D8/1DC/1E0 | float | effective R,G,B = byte * intensity / 255 |
| +0x1E4 | float | intensity (default 1) |
| +0x1E8 / +0x1F0 | float | umbra (inner half-angle, rad, default 15°) / cos |
| +0x1EC / +0x1F4 | float | penumbra (outer half-angle, default 25°) / cos |
| +0x1FC..0x208 | float[4] | attenuation {a, b, c, range}, default {0,0,0,1000}; b >= a, range >= c |
| +0x20C / +0x210 | float | 1/(b-a), 1/(range-c) |

No shadow flag (shadows are separate RMShadow objects). Level-data light object (Timeline 0x476160 create,
0x476240 apply, 0x476330 read back): `obj+0x20` -> `{u16 type; s8 enable; s8 falloffEnable; float intensity, umbra,
penumbra, falloffRange; u8 rgb[3]; pad; float atten[4]}`, `obj+0x14` = RMLight. Creator 0x489900 forces falloff on, range 20000.

### Enumeration (high confidence for the chain)
```
cam  = [[0x4CED70]+0xCCC]     // active camera
room = [cam+0x19C]            // RenderMgr_GetRenderRoom 0x413A20 passes this to RMRender::Render
n    = [room+0x138];  light_i = [room+0x70 + 4*i]
```
- Portals: array `[[room+0x15C]]`, count `[room+0x160]`; other room of portal P = `[P+0x594]` if `[P+0x590]==room`
  else `[P+0x630]` if `[P+0x62C]==room` (RMPortal::GetOtherRoom 0x60330670). Global room `[RMRender+0x38]`.
- Most robust: detour RMRoom_Render 0x60339330 (vtbl+0x2C), collect rooms drawn this frame, union their lights.
- All objects: `[RMRender+0x18]` list, data `[v+4]`, count `[v+0xC]`; lights have `[obj+0xC]==9`.
- Scene ambient `[[room+0x158]+0x1C8]` (3 bytes): set by Timeline, never read by caesarrm (medium confidence).

### CPU lighting (high confidence)
`RMModel_UpdateLighting` 0x60325BE0. Dynamic models relight when `model+0x1F0 != [[model+0x14]+0x30]` (frame stamp);
snapshot models only when dirty `+0x1E0`. Per relight: colour reset to material packed ambient (+0xA0) or prelit array
`model+0x228` (shade flag 0x10); add each light's term (0-255 int space), clamp each channel to material diffuse byte
(+0x73..+0x75). Md = material diffuse float +0x88..+0x90, Cl = light colour +0x1D8..+0x1E0.
- Point: `Md*Cl*max(N.L,0)*att(d)`. Falloff off: no distance term, L computed once from model origin. Falloff on: model
  culled beyond range + radius (+0x2DC).
- Spot: `cosA = dir . normalize(vtx - pos)`; lit if cosA > cos(penumbra); factor 1 inside umbra else
  `(cosA - cosP)/(cosU - cosP)`; term `Md*Cl*N.L*spot*(falloff ? att(d) : 1)`.
- Directional: `L = -dir`, `Md*Cl*max(N.L,0)`.
- Ambient (type 3): adds `Cl*255` to every vertex. Specular (flag 0x2): Phong into RMVertex.specular.
- att(d) (0x6031B7A0): 1 if b <= d < c; (d-a)/(b-a) if a <= d < b; 1-(d-c)/(range-c) if c <= d <= range; else 0.
  Defaults: `max(0, 1 - d/range)`.

### Recommended D3DLIGHT9 mapping
Risk: point/spot lights with `+0x1CC == 0` have no distance falloff in the engine; physical falloff will look different.
| D3DLIGHT9 | Source |
|---|---|
| Type | 0 POINT, 1 SPOT, 2 DIRECTIONAL, 3 skip (fill-light hint at most) |
| Diffuse | (+0x1D8, +0x1DC, +0x1E0) * K, K global scale, start 1-4, calibrate live |
| Position | +0x110..+0x118 |
| Direction | +0xD4..+0xDC normalised |
| Range | +0x208 if falloff on, else large (e.g. 5000) |
| Attenuation0/1/2 | falloff on: 1, 2/Range, 0 (matches at half range); off: 1, 0, 0 |
| Theta / Phi / Falloff | 2*umbra (+0x1E8) / 2*penumbra (+0x1EC) / 1.0 (engine ramp is linear in cos = D3D Falloff 1) |
Skip lights with `+0x1C8 == 0`. One index per light; LightEnable(i, FALSE) on stale indices. Remix's index limit unverified.

### Ambient and baked lighting (medium confidence)
Shade flags `model+0x1FC` (set by 0x60321E60): 0x80 dynamic lights; 0x10 prelit base colours at +0x228; 0x40 snapshot
(cached +0x1D8/+0x1DC); 0x2 specular; 0x8 with `mesh+0x120 & 0x80` -> 0x60327130 (white colours, probably lightmap; low
confidence). `RMRoom::SnapshotLight` 0x60339BD0 re-bakes lights into snapshot models; `SnapshotLightRad` 0x6033A180
builds a radiosity solution at `room+0x16C`; radiosity also loaded from level data (`CreateRadiositySolution(_raddata)`
called at 0x43ADD6, 0x4699CE) and applied with `UpdateAllColors`. Room geometry is often pre-lit: discarding vertex
colours is still correct for Remix.

### Emissive candidates (low confidence)
No emissive field in RMMaterial. Candidates: materials with ambient ~ diffuse, unlit models (no 0x80) with bright prelit
colours, RMCameraSprite (draw 0x60308150, likely flares), particles (draw 0x6032B440) -> tag textures emissive in Remix.
MultiLight.dll (COM component, uses Timeline light wrappers and `rand`) may animate lights: re-read every frame.

### Hook point (medium confidence)
Lights are permanent container members and models relight lazily, so any point between BeginRenderScene (0x4126D9)
and EndRenderScene (0x4126FB) in RenderMgr_RenderFrame 0x412580 sees the final state. Read `[cam+0x19C]` at D3D
BeginScene / first draw (0x413A20 can move the camera into another room just before Render; returns NULL for a
non-default camera when `+0x12C == 0`). Extra cameras (0x4127D3/0x4127EC) render `cam->GetContainer()`.

### Live checks
1. Read room via the chain; for each light dump +0x1C8..+0x214, +0xD4..+0xE8, +0x110..+0x118; confirm `[light+0xC]==9`.
2. Confirm `[light+0x19C]==room`; watch light count change across portals.
3. Read `model+0x1FC` on room/world models.
4. Trace 0x60318560 calls per frame.

## Path B: rigid meshes and materials (research) — 2026-11-04 02:20

### Summary
Rigid models are `RMModel` objects (vtable 0x603424F0). Each one points to an `RMMesh` (model+0x20C). The mesh holds a
**per-vertex** `RMVertex[]` array (position, normal, colour, UV) plus **one u16 triangle index list per texture group**
(`RMTextureGroup`, one group per texture, triangles only). Room/level geometry uses the same RMModels. `RMRoom::Optimize`
calls `RMModel::MakeStatic`, which bakes `model+0xEC` into the mesh vertices in place and sets `model+0x188 = 1`. LOD is the
progressive mesh only (`model+0x2D0` = mesh when the mesh type is 0xF). It is changed in place and bumps `mesh+0x124`.
A face's texture is `model+0x1D0[group]` (RMTexture*). `RMTexture+0x74` is the caesarim texture slot. caesarim binds it with
`SetRenderState(TEXTUREHANDLE, g_d3dapp+0x8380[slot])`, which dxwrapper turns into `SetTexture(0, IDirect3DTexture9*)`.
**Recommended Path B design (substitution).** Hook caesarrm `0x603221F0` (one call per model per texture group, just before
the engine's D3D draw for that group) and record which model/mesh/group is being drawn. Then, in the proxy's
`DrawIndexedPrimitiveUP`, replace the screen-space batch with a cached model-space VB/IB for that (mesh, group), using
WORLD = model+0xEC (or identity if static), VIEW = cam+0x31C and PROJ from the camera. The texture and blend states the
engine already bound are left as they are. This avoids double drawing and needs no texture mapping table.
Confidence: layouts high; design medium-high (needs the live checks listed at the end).

### Key Addresses
| Address | Description |
|---|---|
| rm 0x603368E0 / 0x603369E0 | `RMRender::CreateMesh` (two exports, both imported by Timeline.exe). new 0x134 -> ctor 0x6031C000 / 0x6031C4E0 |
| rm 0x6031C000 | RMMesh ctor (13-arg variant): fills +0xD4..+0x11C (see the layout below), then 0x6031D000(1) and 0x6031CD90 |
| rm 0x6031C4E0 | RMMesh ctor (point2d-UV variant): flags `&0x43` select builder 0x6031DEB0 or 0x6031F390. Splits vertices per UV (aliases) |
| rm 0x6031D000 | RMMesh_BuildTextureGroups(sortTextures): non-decal textures first, then decal; remaps face+0x3C; one group per texture |
| rm 0x6031CD90 | RMMesh_ComputeBounds: radius -> +0x70, 8-corner box -> +0x74 |
| rm 0x6031C8D0 | `RMMesh::Modified(flags)`: ++mesh+0x124; bit0 rebuilds groups, bit1 rebounds |
| rm 0x6031C810 / 0x6031C860 | `RMMesh::SetVertices` (++mesh+0x124) / `GetVertices` |
| rm 0x60322330 | `RMModel::Render` (vtbl+0x70, slot at 0x60342560): (imRender, scene, camera, hint). Needs +0x20C, +0x200, +0x204. flag 0x8 + RMRender state 0xB -> vtbl+0x80; flag 0x400 -> vtbl+0x84 with pass 1 (reversed winding), then pass 0 |
| rm 0x603223E0 | `RMModel::RenderMesh` (vtbl+0x84, slot 0x60342574): (imRender, scene, camera, hint, reverseWinding), ret 0x14 |
| rm 0x603236A0 | RMModel vtbl+0x80: overlay/second-texture path (mesh+0xE4 overlay texture), used only with flag 0x8 + RMRender state 0xB |
| rm 0x603221F0 | **RMModel_DrawGroupNoClip** (thiscall model; imRender, IMVertex* proj, u16* groupIdx, nTris, nVerts, flags, imTex, texFlags, int* io), ret 0x24. 2D backface filter into model+0x1F8, then `IMRender::DrawIndexedPolysNoClip` only if any triangle survives. Also used by RMDeformationModel (0x60314C19) |
| rm 0x60326E70 | `RMModel::MakeStatic` (vtbl+0x3C): pos/normal of mesh verts (and mesh+0x12C extra verts, face normals +0x48) *= +0xEC; +0x188 = 1; local = identity, pivot = old position |
| rm 0x60339C20 | `RMRoom::Optimize`: MakeStatic on every top-level (no parent +0x190) model in room+0x140 |
| rm 0x60339330 | RMRoom_Render: visible set or model list -> `model->vtbl+0x70`. Translucent (+0x1CC) models are qsorted far->near and drawn after RMRender::SetRenderState(4 = ALPHATEST, 1) |
| rm 0x603257D0 | RMModel_UpdateTranslucency: +0x1CC = drawMode(+0x33C) > 0 OR any material opacity byte != 255 OR any texture IsDecal |
| rm 0x60321A70 | `RMModel::SetMesh`: +0x2D0 = mesh if mesh type (+0xC) == 0xF (RMProgressiveMesh) |
| rm 0x6032F150 / 0x6032F850 | `RMProgressiveMesh::GotoLOD` (applies vsplits, calls Modified -> rebuilds groups, ++mesh+0x124) / SelectLODByArea |
| rm 0x6033C8C0 | `RMTexture::GetCacheID`: returns +0x74 (IM slot) and reloads through AddTexture if `IsTextureLoaded` fails |
| rm 0x6033DD90 | RMTexture_GetShiftOffset(&du,&dv): +0x180/+0x184. RenderMesh adds them to the projected tu/tv (UV scrolling) |
| im 0x6010C970 | `IMRender::SetCurrentTexture(slot, texFlags)` (exported) -> IMDevice vtbl+0x34 = 0x60103720 |
| im 0x60103720 | IMDevice_SetTexture: TEXTUREMAPBLEND = DECAL if texFlags&4 else MODULATE; TEXTUREADDRESSU/V = MIRROR if bit0/bit1 else WRAP; then `SetRenderState(TEXTUREHANDLE, [g_d3dapp+0x8380+slot*4])`; current slot cached at 0x6012CFC4 |
| im 0x601045C0 | IMDevice_SetRenderState (engine enum -> DX5 enum, jump table 0x601049BC). See section 4 |

### 1. Mesh data layout (confidence: high)
`RMMesh` (size 0x134, vtable 0x603424B0, RMObject type 7; progressive mesh is type 0xF):

| Off | Type | Meaning | Anchor |
|---|---|---|---|
| +0x0C | int | RMObject type (7 mesh, 8 model, 0xF progressive mesh) | 0x6031C4A0, 0x60321A92 |
| +0x10 | RMRender* | owner | ctor |
| +0x70 | float | bounding radius (max length over the first numVerts) | 0x6031CEBB |
| +0x74 | _boundingbox (0x60) | 8 corners x float3 | GetBoundingBox 0x60308380 |
| +0xD4 | int | arrays borrowed from caller (no copy) | ctor arg 14 |
| +0xD8 | RMVertex* | vertices, stride 0x34, count +0xF4 | GetVertexPointer 0x6031C8A0 |
| +0xDC | RMFace* | faces, stride 0x58, count +0xF8 | GetFaces |
| +0xE0 | RMTexture** | textures, count +0x104 (sorted: non-decal first) | GetAllTextures |
| +0xE4 | RMTexture* | overlay texture (second pass) | GetOverlayTexture |
| +0xE8 | RMMaterial** | materials, count +0x110 | GetAllMaterials |
| +0xEC | int* | alias list: alias j (vertex numVerts+j) -> source vertex | GetAliasList |
| +0xF0 | int | numVerts (unique positions) | GetVertexPointer return |
| +0xF4 | int | numVerts + numAliased (total array length) | |
| +0xF8 | int | numFaces | |
| +0x104 | int | numTextures (= number of groups) | |
| +0x108 | int | numAliased | |
| +0x10C | int | numGroups | GetNumGroups |
| +0x114 | RMTextureGroup* | groups, stride 0xC: `{u16* tris; u16* trisReversed; int nTris}` | GetTextureGroup 0x6031D340 |
| +0x118/+0x11C | u16* | whole-mesh tri list fwd / reversed (only when numTextures == 1; then group 0 aliases them) | ctor |
| +0x120 | int | flags (0x80: lightmap mode, see 0x60325C57) | |
| +0x124 | int | **modification counter** (SetVertices, Modified, GotoLOD) -> use it as the cache version | 0x6031C846 |
| +0x12C/+0x130 | RMVertex*/int | second vertex array (also baked by MakeStatic); purpose unknown | 0x60326F7C |

`RMVertex` (0x34): `+0 float pos[3]`, `+0xC float normal[3]` (MakeStatic rotates it as a vector), `+0x18 u32 colour`
(ARGB, overwritten every frame by CPU lighting), `+0x1C u32 specular` (fog in alpha), `+0x20 float tu`, `+0x24 float tv`
(RM_ProjectVerts copies +0x18/+0x1C/+0x20/+0x24 to the TLVERTEX, verified at 0x60302605..0x60302686), `+0x28/+0x2C float tu2/tv2`
(second UV pair: overlay pass, and texture-mapping modes 1/2/4 write generated UVs here, e.g. 0x6032817F/0x603281A9),
`+0x30 int` per-vertex material index (zeroed by the ctor when no materials are given).

`RMFace` (0x58): three corners of 0x14 bytes at +0x00/+0x14/+0x28. Corner `+0 u16/int vertexIndex`, `+0xC uvIndex0`,
`+0x10 uvIndex1` (indices into the two `_point2d` arrays of CreateMesh variant 2; +4/+8 unknown). `+0x3C int textureIndex`
(group), `+0x40 int materialIndex`, `+0x48 float normal[3]` (+0x44 probably plane d), `+0x54 int` set to 1 by the ctor.

- **Triangles only.** Every face has exactly 3 corners. Groups are plain u16 triangle lists in the 0..numTotal-1 range.
- **UVs are per vertex.** Per-corner UVs are resolved at build time by duplicating vertices (aliases).
  At draw time an alias takes its **position** from its source (`out[numVerts+j].xyzw = out[alias[j]].xyzw`, TransformVerts
  0x60304A6A) but keeps its own colour/UV. Path B should read the alias position from `verts[alias[j]]`, which is exact.
- **Material** is per face (+0x40) and only feeds CPU lighting. `RMMaterial`: +0x70/+0x73/+0x76 ambient/diffuse/specular RGB
  bytes, +0xA4 specular strength, +0xA8 opacity float, +0xAC opacity byte. Opacity reaches the render only as vertex alpha.
- **LOD:** only `RMProgressiveMesh` (model+0x2D0 = model+0x20C when the type is 0xF). `RenderMesh` calls SelectLODByArea
  (0x6032F850) with the BoundsCheck pixel area. GotoLOD rewrites the vertex/face arrays **in place**, rebuilds the groups and
  bumps +0x124. Active vertex count is PM+0x144. `SetLODMeshes` (model+0x214[4], count +0x2B8) has no reader in the render
  paths (medium confidence: unused). With culling patch #1 (area forced to 1000000), every model sits at the finest LOD,
  which gives stable hashes.
- **Where positions/UVs are read in the no-clip path:** RenderMesh 0x6032281A calls `GetVertexPointer(mesh)` (verts = mesh+0xD8).
  It then calls `TransformVerts` 0x603228BF `(verts, numVerts, countToProject (= PM+0x144 or numVerts), &outIM, aliasList,
  numAliased)`, which calls RM_ProjectVerts (pos -> sx..rhw, +0x18..+0x24 copied) and then the alias copy.
  The per-group loop 0x60322908 does: group = mesh+0x114[g]; idx = `(cam+0x6F4 XOR reverse) ? trisReversed : tris`;
  tex = model+0x1D0[g]; slot = GetCacheID; texFlags = GetState(+0x170); UV shift added to referenced verts.
  Then `0x603221F0(imRender, outIM, idx, nTris, nVerts, flags = (slot>-1)|(model+0x1FC & 6), slot, texFlags, &io)`
  -> `DrawIndexedPolysNoClip`. **Indices are passed through unchanged (1:1 with mesh vertex indices).**

### 2. Static room geometry (confidence: high)
- A room's geometry is ordinary RMModels contained in the room. `RMRoom::Optimize` 0x60339C20 makes every top-level model
  static. MakeStatic **overwrites the mesh's own vertex positions and normals with world-space values**. The mesh is therefore
  world-space and model+0x188 = 1. Do **not** use model+0xEC for these: MakeStatic resets local to identity with pivot =
  old position, so +0xEC keeps a translation. Use WORLD = identity.
- Enumerating a room's models (RMRoom, size 0x1D8): `+0x13C` object list (`{vtbl; T** data @+4; ...; count @+0xC}`,
  get(i) = 0x60340380, count via vtbl+0x14), `+0x140` model list (same type), `+0x144` traversal/visibility object
  (vtbl+0x34 fills `+0x148`), `+0x148` visible set (array: base +4, stride +0xC, count +0x10, get 0x603408F0; entry
  `{RMModel*, hint, ...}`), `+0x150` IMRender*, `+0x154` camera, `+0x158` RMScene*, `+0x15C/+0x160` portals,
  `+0x178/+0x17C/+0x180` translucent sort buffer `{float distSq; RMModel*; hint}`, `+0x184` shadow casters[20].
- Draw call: `model->vtbl+0x70(room+0x150 imRender, room+0x158 scene, room+0x154 camera, hint)`. hint = visible-set entry
  hint, or 3 (BoundsCheck) when there is no traversal object.

### 3. Per-object world matrix (confidence: high)
- RenderMesh 0x60322720: `if (model+0x188 == 0) cam->PushModelMatrix(copy of model+0xEC (RMMatrix43, 0x30 bytes by value),
  &model+0x11C, &model+0x140, &model+0x74)`, so mvp = M(+0xEC) * view * proj, applied to `mesh+0xD8` positions as row
  vectors. **model+0xEC maps mesh vertices to world**, including per-axis scale (+0x74..+0x7C when +0x18C > 0).
- To build a D3DMATRIX: `_11.._13 = row0, _21.._23 = row1, _31.._33 = row2, _41.._43 = row3 (translation)`, `_14 = _24 = _34 = 0`,
  `_44 = 1`. No transpose is needed: the row-vector convention matches D3D. View = cam+0x31C (16 floats), as already used by Path A.
- Mirror/transform portals: `cam+0x6F4` flips winding (the reversed list is chosen). Static models: identity.

### 4. Textures, materials, render states (confidence: high for the chain, medium for the dxwrapper side)
- Engine texture -> IM slot: `RMTexture* t = model+0x1D0[g]` (count model+0x22C; per-model copy of the mesh's sorted
  texture list, can be overridden by `RMModel::SetTexture`). `slot = t->GetCacheID()` (= t+0x74; -1 not loaded, -2 none).
  `texFlags = t+0x170` (GetState). `t+0x88` IsDecal = has transparent texels (decal textures are grouped last, and the model
  becomes depth-sorted). `t+0x80/+0x84` size, `t+0x168` mip count, `t+0x178/+0x17C` scroll speed, `t+0x180/+0x184` current
  scroll offset, `t+0x70` IMRender*.
- IM slot -> D3D: `IMDevice_SetTexture` 0x60103720 does `IDirect3DDevice2::SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE (1),
  [g_d3dapp(0x6012CFC0)+0x8380 + slot*4])`. The current slot is kept at `0x6012CFC4`; slot validity is checked at
  `g_d3dapp + 0x2388 + slot*0x18` (record contents not analysed; AddTexture is IMDevice vtbl+0x28 = 0x60103510, not read).
  dxwrapper resolves the DX5 handle to its IDirect3DTexture wrapper and calls `IDirect3DDevice9::SetTexture(0, tex9)`.
- Recognising the texture in the proxy: no table is needed with the substitution design. For other uses, the proxy can learn
  `slot -> tex9` by reading `[0x6012CFC4]` inside DrawIndexedPrimitiveUP and pairing it with its tracked stage-0 texture.
  Invalidate on `IMRender::RemoveTexture` (texture cache eviction, `CacheTextureMemory`).
- Engine render-state enum (IMRender::SetRenderState -> DX5): 1 ANTIALIAS, 2 TEXTUREPERSPECTIVE, 3 FILLMODE, 4 ALPHATESTENABLE,
  5 TEXTUREMAG, 6 TEXTUREMIN, 7 ZFUNC, 8 ALPHAFUNC, 9 DITHER, 10 ALPHABLENDENABLE, 11 texturing on (engine only),
  13 CULLMODE, 14 ZWRITEENABLE, 15 SPECULARENABLE, 16 FOGENABLE, 17 FOGCOLOR, 21 SRCBLEND, 22 DESTBLEND.
- Per-material/per-model states:
  - Alpha blending is **globally on** (Timeline.exe RenderMgr_CreateRMRender_* 0x4120F6/0x412262/0x4123AE/0x412530 set engine
    state 10 = 1). The default blend is SRCALPHA/INVSRCALPHA (restored by 0x60325E80), so opacity is vertex alpha (material
    opacity) times texture alpha.
  - `model+0x33C` drawMode: 1 = (ZERO, SRCCOLOR) multiply, 2 = (SRCCOLOR, ONE) additive; both with ZWRITE off, restored at
    0x60323670.
  - `model+0x1CC` translucent: depth-sorted pass with ALPHATESTENABLE = 1.
  - `model+0x1FC` shade flags: &6 -> IM draw flags (2 specular, 4 gouraud); 0x8 overlay pass; 0x20 wireframe; 0x40 cached
    lighting; 0x100 fog toggle (0x60307B20); **0x400 two-sided** (drawn twice: reversed list, then forward).
  - texFlags bit2 = TEXTUREMAPBLEND DECAL, bits 0/1 = U/V mirror addressing.
  - Colour key: no COLORKEYENABLE call in caesarim's draw path. Transparency comes from texture alpha, plus IsDecal sorting.
  - Back faces are culled in software (0x6032227E); the D3D CULLMODE is left to the IM default.

### 5. Hook point and minimal Path B design (confidence: medium-high)
**Primary: substitution at the per-group draw.**
1. Detour caesarrm `0x603221F0` (9 args, thiscall, ret 0x24). It is shared with RMDeformationModel, so only handle
   `*(void**)model == 0x603424F0` (exact RMModel). Characters (0x60342308), particles (hint 4 -> clip path, never reaches
   here) and shadows stay on Path A.
2. In the detour: mesh = model+0x20C; find g with `groups[g].tris == idx` (fwd) or `.trisReversed == idx` (reversed);
   static = model+0x188. Store a thread-local `PendingRigid{model, mesh, g, reversed, static, world=+0xEC, camera}` (camera
   = `[[[0x4CED70]+0xCB8]+0x2C]`), call the original, then clear the record.
3. The original calls DrawIndexedPolysNoClip -> IMDevice_DrawIndexedTriList (SetTexture + SHADEMODE/SPECULAR states) ->
   `DrawIndexedPrimitive(TRIANGLELIST, TLVERTEX)` at 0x60103AD7 -> dxwrapper -> proxy `DrawIndexedPrimitiveUP`. When a
   PendingRigid is set there, the proxy:
   - gets or builds a cached VB/IB for key `(mesh, g, mesh+0x124)`. VB = all numTotal verts: pos (alias -> source pos),
     normal, tu/tv, diffuse = white with alpha from the TLVERTEX colour if needed. IB = `groups[g].tris` (always the forward
     list; handle mirroring with CULLMODE NONE). Static meshes are already world space, so build once per level;
   - sets WORLD = identity (static) or +0xEC as a 4x4, VIEW = cam+0x31C, PROJ = the Path A rebuild. Keeps the stage-0
     texture and blend/ztest states dxwrapper has set (they are the engine's own). Turns off dxwrapper's XYZRHW vertex
     shader/decl the same way Path A does;
   - adds the texture scroll: RMTexture (model+0x1D0[g]) +0x180/+0x184 -> D3DTS_TEXTURE0 translation
     (TEXTURETRANSFORMFLAGS = COUNT2), so the VB stays constant;
   - issues `DrawIndexedPrimitive` and **does not** forward the screen-space draw. Path A's unproject must skip this batch.
4. What this replaces / what to skip so nothing is drawn twice:
   - The screen-space DIPUP for that group (skipped in the proxy, replaced 1:1).
   - Two-sided models (flag 0x400): RenderMesh runs twice (pass 1 reversed, pass 0 forward). Draw on the forward call only
     (the idx pointer passed to 0x603221F0 equals `groups[g].tris`, not `.trisReversed`; RenderMesh already folds cam+0x6F4 into that choice at 0x60322916) with CULL_NONE, and drop the reversed one. Otherwise two coplanar copies z-fight in Remix.
   - Empty-after-backface groups: 0x603221F0 issues no draw when all triangles face away. That leaves the group missing for
     that frame, so apply **culling patch #2** (0x6032227E `74 31` -> `90 90`) or draw from the detour when the original
     drew nothing (`*io` unchanged).
   - Clip paths (BoundsCheck status 1/3) do not go through 0x603221F0; they use per-face ClipAndAppend*. Apply **culling patch
     #1** so every model takes the status-0 path. Otherwise those models stay screen-space via Path A, which is acceptable as a
     fallback.
   - Overlay pass (flag 0x8, vtbl+0x80 0x603236A0): not yet checked whether it uses 0x603221F0. Leave it on Path A until it
     has been checked.
   - CPU work still runs (lighting 0x60325BE0, TransformVerts). That is harmless but wasted; a later optimisation is to
     detour RenderMesh itself.
5. Rejected alternative: detour `RMModel::RenderMesh` (vtbl+0x84) and draw everything yourself while skipping the original.
   It needs a slot->tex9 table, and it re-implements SetTexture/blend/zwrite/alpha-test/fog state and the translucent ordering.
   It also desyncs dxwrapper's cached state unless the proxy wraps it in a state block. Only worth doing after substitution
   works.

### Open items
- (Consistent with the 00:52 dxwrapper trace: stride 32 raw TLVERTEX, no out-of-range indices; confirm MinVertexIndex == 0.) Whether dxwrapper passes the caesarim vertex pointer to DrawIndexedPrimitiveUP unchanged (base = IMVertex[0], MinVertexIndex
  0). If it rebases, map indices through MinVertexIndex. Index values must equal mesh vertex indices.
- Texture-mapping modes (model+0x338 per group: 1 cylindrical 0x60328060, 2 0x603281E0, 4 0x60328410) write +0x28/+0x2C.
  The consumer was not traced; treat those groups as dynamic UVs (re-upload per frame, or leave on Path A).
- PM models that share one mesh between instances: with patch #1 all select the finest LOD, so +0x124 stays stable.
  Without it, the LOD thrashes between instances every frame.

### Suggested Live Verification
1. Proxy log inside DIPUP when PendingRigid is set: `(char*)model+0x24` name, g, nTris, NumVertices, MinVertexIndex, the
   first three indices vs `groups[g].tris[0..2]`, and `[0x6012CFC4]` vs `RMTexture(+0x1D0[g])+0x74`. Expect equality.
2. Pick one dynamic RMModel: transform `mesh+0xD8[i].pos` by +0xEC as a 4x4, then by cam+0x31C and the Path A PROJ, and
   compare with the TLVERTEX at index i in the same DIPUP. This confirms the world matrix and the 1:1 indexing.
3. For a room model (+0x188 = 1): verts[i].pos should already equal the Path A unprojected world position.
4. Count reversed-pass calls (flag 0x400) and empty-draw groups per frame before deciding on patch #2.

## Remaining culling (session 2b)

### Summary
After the session-2 patches, the **main remaining cull is the "Greg" quadtree** (RMRender::SetGregEnable, which the game
turns on). In each room the quadtree tests every node box with `RMExt_BoundsCheck` 0x6033FF60. That function calls the
**original** BoundsCheck at 0x6033FFB5, which was **not redirected**. When a node gets result 2, every object in that node
and its subtree is left out of the room's draw list, so those objects never reach the renderer. Objects whose extents
reach outside their node box, and moving objects, pop in and out as the camera turns. That matches "intermittent".
The "known culled" hint branches (0x603225E2 and the others) are effectively dead code: no caller ever passes hint 2.
The second source is room traversal. A room is entered only through a portal whose projection is non-empty, which
needs the right facing and at least part of the portal in front of the near plane. Rooms behind the camera, and
everything reached only through them, are skipped. A design for a safe fix is below.
Smaller sources:
- particles still use the 2D-clip path (hint 4);
- progressive-mesh distance LOD (`dist > 500` coarsens; this is not culling, but the mesh changes);
- game-driven `RMModel::SetEnable` / `SetDrawEnable`;
- per-side portal enables set from level data and scripts (`RMPortal::EnableRoom`). Leave these alone.
The no-clip transform path never rejects w <= 0.

### Key Addresses
| Address | Description |
|---|---|
| rm 0x60338810 | `RMRender::SetGregEnable(int)` -> RMRender+0x7C (= ctx+0x34). Timeline calls it (0 then 1) at 0x40A7ED/0x40C906/0x4101F5/0x411F2F/0x412D8F |
| rm 0x6033E6D0 | RoomMgr_CollectVisible (vtbl+0x34 of per-room manager, vtable 0x6034286C): list {obj, hint=3} for unbounded entries, then quadtree root vtbl+0x2C(list, 3) |
| rm 0x6033F270 | QuadNode_CollectVisible (vtable 0x60342928 +0x2C): if parentHint != 0 -> ctx->vtbl+0x5C = RMExt_BoundsCheck(node box +0x6C, centre +0x5C, radius +0x68). **2 -> whole subtree dropped**, else children + node objects get hint = result (0/1/3) |
| rm 0x6033FF60 | RMExt_BoundsCheck(ctx, ...) -> original BoundsCheck at **0x6033FFB5** (outArea = NULL). Returns 2 if ctx+0x28 camera is NULL |
| rm 0x6033E680 | RoomMgr_BeginFrame(ctx, camera): ctx+0x28 = camera (called from RMRoom_Render 0x603393CF when Greg is on) |
| rm 0x60339330 | RMRoom_Render: (Greg on) draws list room+0x148 {obj,hint}; (Greg off) room+0x140 children with hint 3; type 0x11 objects hint 3. **Portals first** (0x603393D4), then its own objects |
| rm 0x60339F20 | RMRoom_RenderThroughPortals (code walk below) |
| rm 0x60330710 | RMPortal_Project: facing dot(camPos-P0, N) > 0 (side 1, +0x590) / >= 0 (side 2, +0x62C), then ViewportTransformFace (near clip only) -> outN (0 when facing fails or the portal is fully behind the near plane) |
| rm 0x60330410 / 0x603303A0 | AlreadyTraversed / MarkTraversed per **directed** portal side; stamp = `[[room+0x14]+0x2C]` = **RMRender+0x74**, incremented by EndRenderScene 0x60337A70 (once per scene) |
| rm 0x603306A0 | `RMPortal::EnableRoom(sideMask, enable)` (+0x618 / +0x6B4). Called by Timeline level loader 0x46C736 (data bytes) and script wrapper 0x43C760 |
| rm 0x603254B0 / 0x603254F0 | `RMModel::SetDrawEnable` (+0x204) / `SetEnable` (+0x200) (val, recurse). RMModel_Render 0x60322330 requires mesh +0x20C, +0x200 and +0x204 all non-zero. RMFrame versions 0x60317AC0 / 0x60317A00 recurse to children |
| rm 0x60325660 | `RMModel::SetRenderCallback` -> model+0x1C8; the callback can veto a draw (0x603225A2). Not imported by Timeline.exe |
| rm 0x6032F850 | RMProgressiveMesh_SelectLODByArea: LOD-table path uses area (1000000 from the cave = finest); **no-table path uses distance squared from 0x6032267D: dist > 500 -> lod = n*(1-250000/d^2)** |
| Timeline 0x43BB00 | Script/engine-interface wrapper around BoundsCheck (vtable 0x4B9654 slot +0x25C). Has no direct callers (probably dispatched by index) |
| Timeline 0x43B5C0 / 0x43BD40 / 0x43C1C0 / 0x43D360 / 0x43D390 | Engine-interface wrappers calling RM vtbl+0x28 (SetEnable) / +0x20 (SetDrawEnable) |
| im 0x6010D480 | IMRender::PushClip: stack top >= 0x1D -> logs and returns 0 (room skipped). Max nesting is 28 below the BeginScene clip |

### BoundsCheck 0x60305880: all 13 callers
| Call site | Function | Class | Status |
|---|---|---|---|
| 0x6032255C | RMModel_RenderMesh | render | redirected |
| 0x603238E6 | RMModel_RenderMesh2 | render | redirected |
| 0x60313C44 / 0x60314A0D / 0x6031632B | RMDeformationModel_RenderA/B/C | render | redirected |
| **0x6033FFB5** | RMExt_BoundsCheck (quadtree node test via 0x6033F2EC) | **render (pre-cull)** | **NOT redirected** |
| 0x60322027 | RMModel_Pick 0x60321F30 | pick | keep |
| 0x6031385B | RMDeformationModel_Pick 0x60313750 | pick | keep |
| 0x603288A0 | RMModel_RenderShadowCaster 0x60328720 | shadow | dead with the 0x6033941B patch |
| 0x60328BE0 / 0x60328C40 | RMModel_RenderShadowReceiver 0x60328A30 | shadow | dead with the 0x6033941B patch |
| 0x60329619 / 0x60329679 | RMModel_AddShadowGeometryA/B | shadow | keep |

Timeline.exe also imports BoundsCheck (IAT 0x4B8A50, wrapper 0x43BB00). Its use is unknown: trace it live.

### Hint values (answers candidate 1)
Hints passed to vtbl+0x70 come from three places:
- 0x60339536 / 0x603395DD: the quadtree list, hint 0, 1 or 3. Never 2, because a node that returns 2 is dropped.
- 0x60339637 / 0x60339682 / 0x60339B29: always 3.
- Particles at 0x6032B54F: 4.

**No caller passes hint 2**, so the branches 0x603225E2 / 0x60323894 / 0x60313BE4 / 0x603149C5 never run, and NOP-ing
them changes nothing. The hint is not a last-frame cache: it is the quadtree node result for this frame.
Hint 0 skips the per-object BoundsCheck (whole mesh, no clip). Models with a progressive-mesh LOD table
(+0x2D0 -> +0x148) always call BoundsCheck regardless of the hint.

### Room traversal (candidate 3)
RenderThroughPortals 0x60339F20, ecx = room:
```
portals = *(RMPortal***)(room+0x15C), count room+0x160; imr = room+0x150; cam = room+0x154
for p in portals:
  if !RMPortal_IsEnabledFrom(p, room, cam) continue                 // 0x60333270 (+0x618/+0x6B4, range patched off)
  n=0; RMPortal_Project(p, cam, room, g_portalProjBuf, &n)           // 0x60339F78
  if n==0 continue                                                   // 0x60339F83 <- rooms behind camera die here
  other = p->vtbl[0x1C](room); if !other continue                   // GetOtherRoom
  stamp = [[room+0x14]+0x2C]                                         // RMRender+0x74
  if RMPortal_AlreadyTraversed(p, other, stamp) continue             // 0x60330410
  if !PushClip(...) continue        // patched: ForceViewportClip(cam) 0x60306E40
  RMPortal_MarkTraversed(p, other, stamp)                            // 0x60339FE5
  RMPortal_Enter(p, cam, room)                                       // 0x60330910 (transform portals only)
  if (!p+0x6D8 || [[room+0x14]+0x10]) other->vtbl[0x2C](imr, cam, 0) // RMRoom_Render, recursion
  IMRender::PopClip(imr)                                             // [0x60342184]
  RMPortal_Leave(p, cam, room)                                       // 0x60330D70
```
- **No per-room visited flag exists.** Only directed portal sides are marked, once per scene stamp. The facing test is
  the only thing that stops traversal from going back into the camera's room. Since ForceViewportClip, a room
  reachable through two portals is drawn twice.
- Recommended fix: a **C++ replacement of 0x60339F20** (5-byte `jmp` at 0x60339F20 to a `__fastcall(room)` function;
  the original ends in a plain `ret`). Keep the loop above with these changes:
  1. Keep a per-scene visited set of RMRoom* pointers. Reset it when `stamp` changes. Insert the room at depth 0 (the
     root; the global room's own call is a harmless extra root).
  2. Non-transform portals (`[p+0x6D8]==0`): ignore `n` (still call Project, because it refreshes portal state for
     transform/LOD), skip if `other` is already visited, else mark `other` visited before recursing.
  3. Transform portals (`[p+0x6D8]!=0`, mirrors/teleports): keep the original `n==0` rejection. Enter() moves the
     camera, so a back-facing transform portal would draw garbage. Leave them out of the visited set.
  4. Depth cap: a static depth counter, `N <= 24` (the IMRender clip stack allows 28 nested pushes; PushClip returns 0
     past that, which the loop already handles).
  5. Keep IsEnabledFrom. Level data and scripts disable portal sides (EnableRoom); forcing them on could draw
     alternate-time-period geometry.
  Re-entrancy is safe: RMRoom_Render traverses portals **before** filling its own lists (+0x148 / +0x178), and +0x150 /
  +0x154 get the same values within a scene. Cost: every connected room, up to N hops, is drawn once per scene.
- Byte-only alternative (rougher): replace 12 bytes at 0x60339F7D (`8B 44 24 10 85 C0 0F 84 B8 00 00 00`) with
  `E9 <cave>` + 7x`90`. Cave: `mov eax,[esp+0x10]; test eax,eax; jnz 0x60339F89; mov eax,[edi]; cmp dword [eax+0x6D8],0;
  jne 0x6033A041; jmp 0x60339F89`. Also redirect the call at 0x60339FE5 (`E8 B6 63 FF FF` -> MarkTraversed) to a cave
  that marks the reverse side too:
  `push ecx; push [esp+0xC]; push esi; call 0x603303A0; pop ecx; jmp 0x603303A0`. ecx is the portal, esi is the
  from-room, [esp+8] at entry is the stamp; do this only when `[ecx+0x6D8]==0`. This stops immediate back-tracking, but
  rooms in portal cycles can still be drawn twice.

### Candidate 5 (near plane / w <= 0)
RMCamera_TransformVerts 0x60304980 -> RM_ProjectVerts 0x603025E0 projects every vertex with `rhw = 1/w` and **no
w test**. RMModel_BuildFrontFaceIndices 0x603221F0 has no test either, apart from the backface sign (patched).
DrawIndexedPolysNoClip does no culling. The only losses are proxy-side: w = 0 gives inf, which the proxy drops.

### Candidate 6 (frame-coherent / caps)
- No occlusion and no last-frame visibility.
- The quadtree rebuilds when its dirty flag (+0x48) is set.
- Caps: projected shadow casters <= 20 (0x60339416, already bypassed); IMRender clip stack 29; RMRender
  scene-camera list 10 (0x60337A3F; only the bookkeeping list, not drawing).
- The sort buffers (room+0x178) and the cam+0x660 vertex buffer grow on demand.

### Patch points (ordered by likely impact)
| # | Item | Address | Original | Patch | Effect | Risk | Conf |
|---|---|---|---|---|---|---|---|
| A | Quadtree node cull off | rm 0x6033F2B7 | 74 41 | EB 41 | Node test never runs, all objects in each traversed room get hint 3 and go to the (redirected) whole-mesh path | Low: more objects drawn; pick/LocateModels use other queries (+0x34/+0x38) | High |
| A' | (alt) same via the call | rm 0x6033FFB5 | E8 C6 58 FC FF | E8 -> existing whole-mesh cave | Nodes report 0, objects get hint 0 | Low | High |
| B | Rooms behind camera | rm 0x60339F20 | 51 55 56 8B F1 | E9 -> C++ RenderThroughPortals (design above) | All connected rooms within N hops, each once per scene, no duplicates | Medium (performance; transform portals keep original behaviour) | Medium-high |
| C | Particles 2D clip | rm 0x6032B540 | 6A 04 | 6A 00 | Particles use the whole-mesh no-clip path (no cuts at screen edges) | Low | High |
| D | PMesh distance LOD | rm 0x6032F8DA | 75 0E | EB 0E | Progressive meshes without a LOD table always use LOD 0 (finest). Stops mesh changes beyond 500 units | Low-medium (performance) | Medium (LOD 0 = finest inferred from the area <= 10 -> +0x138 coarsest path) |
| E | Portal side enables | rm 0x60339F5C | 85 C0 0F 84 DD 00 00 00 | 85 C0 90 90 90 90 90 90 | Ignores EnableRoom (doors / time variants) | **High**: may draw rooms the game disabled on purpose. Test only | Medium |
| F | Hint-2 branches | 0x603225E2 etc. | - | do not patch | Dead code | - | High |
| G | Game hide flags | RMModel +0x200/+0x204, setters 0x603254F0/0x603254B0 | - | do not patch blindly | Gameplay visibility | - | - |

### Suggested Live Verification
1. Read RMRender+0x7C (`[[[0x4CED70]+0xCB8]+0x7C]`): expect 1 (Greg on). Count hits on 0x6033FFB5 per frame and how
   many return 2. Apply A and confirm the popping stops.
2. Trace 0x60339F83 with eax = `[esp+0x10]` to count portals rejected per scene. After B, count RMRoom_Render
   (0x60339330) calls per scene and check that no room appears twice.
3. Trace the setters 0x603254B0 / 0x603254F0 / 0x60317A00 / 0x60317AC0 (and Timeline 0x43BB4D for script BoundsCheck)
   with return addresses. Any caller that toggles per frame or by distance is game LOD that may need its own patch.
4. Check the 0x60330178 / 0x603306A0 writes for portals with +0x618 == 0 in the current level (candidate E).

## Status 2026-11-04 morning (user-confirmed)

- Path A rendering: world-space geometry, real camera, no vertex explosions.
- Culling: object/quadtree/particles/backface patches + C++ portal traversal (`src/comp/game/portal_traversal.cpp`)
  remove all culling the user could see; hover/click still work; frame rate OK.
- Sky: SkyBox textures tagged as Sky Texture plus `rtx.skyReprojectToMainCameraSpace = True` in `rtx.conf` gives the
  original sky. (Proxy far plane raised to 500000 so large SkyBox models are not clipped.)

## Stretched wall textures (castle hall) — cause and fix (2026-11-04 11:40)

Capture `RemixPlus/Timeline/captures/dxtrace_20261104_113447.jsonl` (castle hall): after every EndScene, dxwrapper sets
sampler 0 `ADDRESSU/V = CLAMP` (its own blits) and never restores it. caesarim caches its texture-address state and does
not re-send WRAP, so a scene that never changes addressing (the hall's main scene, seq 4..8068) draws tiled wall textures
clamped -> edge texels smeared. (Outdoor scenes happened to re-send WRAP at scene start.) Not caused by the culling
patches (user tested DisableObjectCulling=0: same).
Fix: `src/comp/modules/texture_addressing.{hpp,cpp}` records ADDRESSU/V set inside scenes (default WRAP) and re-applies
them at every BeginScene. Known edge: if the game requests CLAMP while dxwrapper's cache already holds CLAMP, dxwrapper
may drop the redundant call and WRAP would persist.
Lights module (`src/comp/game/scene_lights.cpp`) confirmed working by the user: 11 lights (10 point, 1 spot) from 2 rooms.

User confirmed 11:50: castle hall walls tile correctly with the texture_addressing fix.

## Path B stage 1: verification results (2026-11-04 12:20, live, castle + outdoor)

Detour on RMModel_DrawGroupNoClip 0x603221F0 (MinHook, `__fastcall(model, edx, 9 args)`, exact RMModel vtable 0x603424F0 only)
plus a check inside the proxy's DrawIndexedPrimitiveUP. 45,477 samples, 1,515 models:
- Indices 1:1: DIPUP indices == engine group list indices (0 mismatches); MinVertexIndex = 0; NumVertices = mesh+0xF4 always.
- Every draw matched a group (`groups[g].tris` or `.trisReversed` == pointer passed to the engine).
- World positions: `mesh+0xD8[src].pos * model+0xEC` (src via alias list for i >= unique) vs Path A un-projection: max 0.013 units.
- No models had `+0x188 != 0` in these areas (all used +0xEC).
- Reversed passes: 4,768 of 45,477 (two-sided models, flag 0x400) -> skip reversed when two-sided.
- Texture slot: draw slot == [0x6012CFC4] == RMTexture(+0x1D0[g])+0x74, except untextured models (slot -1, no texture).

## Path B stage 2: rigid model substitution — WORKING (user-confirmed 12:45)

`src/comp/game/rigid_draw_hook.cpp` (MinHook on 0x603221F0, records exact RMModel group draws) +
`src/comp/modules/rigid_meshes.cpp` (copies mesh: alias->source positions, normals, UVs, batch alpha, forward group
list; WORLD = model+0xEC or identity if static; skips reversed pass of two-sided models) +
`screen_space::draw_rigid_mesh` (FVF XYZ|NORMAL|DIFFUSE|TEX1, VIEW/PROJ as Path A). Falls back to Path A when no rigid
draw is recorded or the mesh does not match the batch. INI `[PathB] Enabled`.
Known gaps: texture UV scrolling (RMTexture +0x180/+0x184) not applied; characters, particles, sprites still Path A.

## Path B stage 3: characters — WORKING (user-confirmed 13:55)

`src/comp/modules/character_meshes.cpp`: RMDeformationModel group draws (same hook, vtable 0x60342308) are drawn
from the bind-pose (joint-local) mesh with FFP indexed vertex blending: palette slot per joint (`frames[v]`, model+0x348),
WORLDMATRIX(slot) = joint+0xEC, weight 1, D3DVBF_1WEIGHTS. Normals rebuilt once per mesh from the engine's skinned
positions (model+0x344, all groups) and moved into joint space with joint+0x140; sign from `winding_estimate`
(measured on rigid meshes: +1). (model, group) deduped per scene (drops RenderA pass 1 and overlay pass). Per-draw
self-check of 3 vertices vs model+0x344 -> Path A fallback on mismatch.
Instability at distance was progressive-mesh distance LOD (only active vertices re-skinned -> self-check failed ->
Path A). Fixed with patch `0x6032F8DA 75 0E -> EB 0E` (always LOD 0; constant at 0x603421A8 = 0.0 confirms LOD 0 is
finest) in the DisableObjectCulling group. Shared helpers: `game/game_memory`, `game/engine_mesh`, `game/model_draw_hook`.

## Session 2 end state (2026-11-04 14:00) — summary of what is live

Everything below is user-confirmed in game. `NEXT_SESSION.md` has the file-by-file architecture and next actions.
- **Pipeline per caesarim draw (DrawIndexedPrimitiveUP / DrawPrimitiveUP, stride 32):**
  1. Rigid RMModel group: Path B, the engine mesh with WORLD = model+0xEC (identity if static).
  2. RMDeformationModel group: Path B, bind pose with FFP indexed blending (1 joint per vertex).
  3. Anything else 3D: Path A un-projection.
  4. rhw == 1: 2D passthrough.
- **Engine patches (16 `[EnginePatch]` lines):**
  - BoundsCheck redirected to a whole-mesh cave at 0x6032255C, 0x603238E6, 0x60313C44, 0x60314A0D and 0x6031632B.
  - Single byte patches:

    | Address | Bytes | Effect |
    |---|---|---|
    | 0x6033941B | 7D->EB | shadows off |
    | 0x6033F2B7 | 74->EB | quadtree off |
    | 0x6032B540 | 6A 04->6A 00 | particles no-clip |
    | 0x6032F8DA | 75->EB | progressive-mesh LOD 0 |
    | 0x6032227E | 74 31->90 90 | backface |
    | 0x6010CBE2, 0x6010CCAC, 0x60103858 | 75->EB | backface |
    | 0x6033328F, 0x603332CC | 75->EB | portal range |

  - RenderThroughPortals 0x60339F20 -> C++ traversal.
- **Lights:** RMLights from the rendered rooms -> D3D9 lights per BeginScene.
- **Texture addressing:** in-scene ADDRESSU/V re-applied per BeginScene (dxwrapper leaks CLAMP).
- **Remix config (`rtx.conf`):** 5 SkyBox textures in `rtx.skyBoxTextures`, `rtx.skyReprojectToMainCameraSpace = True`,
  `rtx.skyBrightness = 1.01`, `rtx.skyMinZThreshold = 1`.
- **Superseded recommendations from the earlier agent sections in this file:**
  - **Sky:** the "SkyBox vtable thunk + MinZ = 1" option is not needed (texture tagging + reprojection works).
  - **Characters:** the "patch RenderA/RenderB vtable slots" design was replaced by the shared 0x603221F0 hook.
  - **Portals:** the 37-byte portal clip patch (#5) was replaced by the C++ traversal.
- **Known gaps:**
  - No UV scrolling on Path B meshes.
  - Overlay texture passes are dropped for characters; unchecked for rigid models.
  - Texture-mapping-mode groups are unverified on Path B.
  - Ambient lights (type 3) are skipped.
