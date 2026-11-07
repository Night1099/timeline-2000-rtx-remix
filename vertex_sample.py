"""Sample caesarim's D3DTLVERTEX arrays at its draw call sites to learn the screen-space mapping.

Spawns the game under Frida (attaching to a running Timeline.exe makes it quit), hooks the
IDirect3DDevice2 DrawPrimitive / DrawIndexedPrimitive call sites, and at most once per second per
site logs the first vertices plus a least-squares fit of sz = a + b * rhw over the whole draw.

Usage: python patches/Timeline/vertex_sample.py [seconds]
Log:   patches/Timeline/vertex_sample.log
"""
import argparse
import time
from pathlib import Path

import frida

GAME_EXE = Path(r"F:/Vibe-Reverse-Engineering/RemixPlus/Timeline/Timeline.exe")
LOG_PATH = Path(__file__).with_name("vertex_sample.log")

AGENT = r"""
// site -> [is_indexed]; stack at the call: this, primType, vtType, verts, vcount, (indices, icount,) flags
const SITES = { '0x6010392c': false, '0x601039fd': false, '0x60103ad7': true };
const STRIDE = 32;
const lastSent = {};

function describe(site, indexed, sp) {
  const prim = sp.add(4).readU32(), vt = sp.add(8).readU32();
  const verts = sp.add(12).readPointer(), n = sp.add(16).readU32();
  const icount = indexed ? sp.add(24).readU32() : 0;
  let sx = 0, sy = 0, sxx = 0, sxy = 0, zmin = 1e9, zmax = -1e9, wmin = 1e9, wmax = -1e9;
  const first = [];
  for (let i = 0; i < n; i++) {
    const v = verts.add(i * STRIDE);
    const x = v.readFloat(), y = v.add(4).readFloat(), z = v.add(8).readFloat(), w = v.add(12).readFloat();
    sx += w; sy += z; sxx += w * w; sxy += w * z;
    zmin = Math.min(zmin, z); zmax = Math.max(zmax, z); wmin = Math.min(wmin, w); wmax = Math.max(wmax, w);
    if (i < 4) first.push([x, y, z, w].map(f => f.toFixed(5)).join(',') + ' c=' + v.add(16).readU32().toString(16) +
                          ' uv=' + v.add(24).readFloat().toFixed(3) + ',' + v.add(28).readFloat().toFixed(3));
  }
  const d = n * sxx - sx * sx;
  const b = d !== 0 ? (n * sxy - sx * sy) / d : NaN, a = (sy - b * sx) / n;
  let res = 0;
  for (let i = 0; i < n; i++) {
    const v = verts.add(i * STRIDE);
    res = Math.max(res, Math.abs(v.add(8).readFloat() - (a + b * v.add(12).readFloat())));
  }
  return site + ' prim=' + prim + ' vt=' + vt + ' n=' + n + ' icount=' + icount +
         ' sz[' + zmin.toFixed(6) + '..' + zmax.toFixed(6) + '] rhw[' + wmin.toFixed(6) + '..' + wmax.toFixed(6) + ']' +
         ' fit sz=' + a.toFixed(6) + '+' + b.toFixed(6) + '*rhw maxres=' + res.toExponential(2) +
         '\n    ' + first.join('\n    ');
}

function instrument(m) {
  send('caesarim.dll base=' + m.base);
  for (const [addr, indexed] of Object.entries(SITES)) {
    Interceptor.attach(ptr(addr), { onEnter() {
      const now = Date.now();
      if (lastSent[addr] && now - lastSent[addr] < 1000) return;
      lastSent[addr] = now;
      try { send(describe(addr, indexed, this.context.esp)); } catch (e) { send(addr + ' read error ' + e); }
    }});
  }
}
const existing = Process.findModuleByName('caesarim.dll');
if (existing) instrument(existing);
else Process.attachModuleObserver({ onAdded(m) { if (m.name.toLowerCase() === 'caesarim.dll') instrument(m); } });
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("seconds", nargs="?", type=float, default=600.0)
    args = parser.parse_args()

    device = frida.get_local_device()
    pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_EXE.parent))
    session = device.attach(pid)
    with LOG_PATH.open("w", encoding="utf-8") as log:
        def on_message(message, _data):
            line = message.get("payload") if message["type"] == "send" else str(message)
            log.write(f"{time.perf_counter():.3f} {line}\n")
            log.flush()

        script = session.create_script(AGENT)
        script.on("message", on_message)
        script.load()
        device.resume(pid)
        deadline = time.time() + args.seconds
        while time.time() < deadline and not session.is_detached:
            time.sleep(0.2)
    print(f"done; log at {LOG_PATH}")


if __name__ == "__main__":
    main()
