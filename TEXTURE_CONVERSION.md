# Converting Timeline's RTX Remix capture textures to standard DDS and PNG

Written 2026-11-04. Game: *Timeline* (Eidos, 2000) running through dxwrapper and RTX Remix Plus.

## The problem

When you take a Remix capture, its textures are written to
`F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures\` as `<hash>.dds`. These files come in two
kinds:

| Kind | How to recognise it | Can NVTT / Photoshop open it? |
|---|---|---|
| 16-bit colour (most textures; 78 of 111 in the first capture) | DDS pixel format FourCC `GLI1`, 16 bits per pixel | **No.** NVTT reports `DDS file had FourCC code GLI1, which the DDS reader does not support` |
| 32-bit RGBA8 (textures with transparency; 33 of 111) | DDS FourCC `DX10`, DXGI format 28 (`R8G8B8A8_UNORM`) | Yes, it is standard |

The `GLI1` files are written by the GLI image library that Remix uses. They are ordinary uncompressed images, but:
- **Non-standard header:** the header uses the non-standard FourCC `GLI1` and carries an extra 20-byte GLI extension
  after the normal 128-byte DDS header, so standard readers reject the file.
- **Swapped colour masks:** the header's colour masks are wrong (red and blue swapped). The pixel data is really
  D3D9 R5G6B5: 5 bits red in the high bits, then 6 bits green, then 5 bits blue. Reading it the way the masks claim
  turns brown wood blue. This was verified by decoding samples both ways.
- **Wrong row pitch:** for example 32 for a 64-pixel-wide texture; it should be width x 2.

## Step 1: convert the GLI1 files to standard DDS

The converter, `F:\Vibe-Reverse-Engineering\patches\Timeline\tools\convert_gli_dds.py`, rewrites each `GLI1` file as a
standard DDS:
- **Pixel format:** uncompressed RGB, 16 bits per pixel, masks R = `0xF800`, G = `0x07E0`, B = `0x001F`.
- **Header fixes:** the 20-byte GLI extension is removed and the row pitch is corrected.
- **Unchanged:** the pixel data and all mip levels are kept exactly as they were.
- **Other files:** files that are already standard (the DX10 RGBA8 ones) are copied unchanged, so the output folder
  holds the whole set.
- **Originals:** the original capture folder is never modified.

Requirement: Python 3 (any recent version; no extra packages).

Run this from a terminal (PowerShell or Git Bash):

```
python -I F:\Vibe-Reverse-Engineering\patches\Timeline\tools\convert_gli_dds.py "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures" "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_standard"
```

Expected output (the numbers depend on the capture):

```
converted 78 GLI1 textures, copied 33 standard ones -> F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_standard
```

The resulting `.dds` files in `textures_standard\` open in NVIDIA Texture Tools, Photoshop (with the NVIDIA plugin),
Paint.NET, GIMP and Python Pillow.

If the converter stops with `unsupported GLI1 format: N bits per pixel`, a new capture contains a GLI format other than
16-bit colour. Only the 16-bit kind has been seen in Timeline so far; the converter needs extending for any other kind.

## Step 2: convert the standard DDS files to PNG with NVTT

Tool: NVIDIA Texture Tools Exporter, installed at
`C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe`.
It picks the output format from the file extension, so `-o name.png` writes a PNG.

Run this in **PowerShell** (it converts every file in `textures_standard\` into `textures_png\` and lists any failures):

```powershell
$nv  = "C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"
$src = "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_standard"
$dst = "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_png"
New-Item -ItemType Directory -Force $dst | Out-Null
$fail = @()
foreach ($f in Get-ChildItem "$src\*.dds") {
    $out = "$dst\$($f.BaseName).png"
    & $nv $f.FullName -o $out 2>&1 | Out-Null
    if (-not (Test-Path $out)) { $fail += $f.Name }
}
"png count: $((Get-ChildItem "$dst\*.png").Count)"
"failed: $($fail.Count) $($fail -join ', ')"
```

Expected output for the first capture:

```
png count: 111
failed: 0
```

For a single file:

```
"C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe" "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_standard\<hash>.dds" -o "F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\textures_png\<hash>.png"
```

## Checking the result

- **Colours:** wood should be brown, grass green, and the "Propriete Privee / DEFENSE D'ENTRER" sign red. If wood looks
  blue, the red and blue channels are swapped: an unconverted `GLI1` file was read with its wrong header masks.
- **Transparency:** textures such as barbed wire, plants and fences should keep their transparent areas (alpha is
  preserved through both steps).
- **File names:** they keep the capture's texture hash (`<hash>.dds` -> `<hash>.png`), which is the hash Remix uses
  for texture replacement and categorisation.

## Doing it again after a new capture

1. Take the capture in Remix as usual (new textures appear in `rtx-remix\captures\textures\`).
2. Run step 1 again with the same two folders. It rewrites `textures_standard\` with the full current set.
3. Run step 2 again. It overwrites the PNGs for textures that already exist and adds the new ones.

## Folders

| Folder (under `F:\Vibe-Reverse-Engineering\RemixPlus\Timeline\rtx-remix\captures\`) | Contents |
|---|---|
| `textures\` | original Remix capture output (`GLI1` and `DX10` DDS); never modified |
| `textures_standard\` | standard DDS (step 1 output) |
| `textures_png\` | PNG (step 2 output) |
