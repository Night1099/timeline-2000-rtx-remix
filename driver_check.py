"""Verify caesarim.dll's D3D driver selection under dxwrapper, optionally with the MMX-driver patch.

caesarim's driver picker (0x601078F0) requires an IID_IDirect3DMMXDevice driver whenever the CPU
reports MMX. dxwrapper never enumerates one, so selection fails and the renderer is never built.
The patch NOPs the `jne` at 0x601078FC so any driver supporting the colour depth qualifies.

Usage: python patches/Timeline/driver_check.py [--patch] [seconds]
"""
import argparse
import time
from pathlib import Path

import frida

GAME_EXE = Path(r"F:/Vibe-Reverse-Engineering/RemixPlus/Timeline/Timeline.exe")
LOG_PATH = Path(__file__).with_name("driver_check.log")

AGENT = r"""
const PATCH = %PATCH%;
const D3D_CREATE = ptr('0x60101000');
const PICKER = ptr('0x601078F0');
const MMX_JNE = ptr('0x601078FC');
const ERROR_TEXT = ptr('0x60120560');
const DRIVER_COUNT = ptr('0x60120884');
const DRIVER_NAME = ptr('0x6012088C');
const DRIVER_GUID = ptr('0x601209D8');
const DRIVER_STRIDE = 0x16C;

function guid(p) {
  const h = (v, n) => ('0000000' + v.toString(16)).slice(-n);
  const b = []; for (let i = 0; i < 8; i++) b.push(h(p.add(8 + i).readU8(), 2));
  return ('{' + h(p.readU32(), 8) + '-' + h(p.add(4).readU16(), 4) + '-' + h(p.add(6).readU16(), 4) + '-' +
          b.slice(0, 2).join('') + '-' + b.slice(2).join('') + '}').toUpperCase();
}
function drivers() {
  const n = DRIVER_COUNT.readS32();
  const out = [];
  for (let i = 0; i < n && i < 8; i++)
    out.push(i + ': ' + JSON.stringify(DRIVER_NAME.add(i * DRIVER_STRIDE).readAnsiString()) + ' ' + guid(DRIVER_GUID.add(i * DRIVER_STRIDE)));
  return 'count=' + n + (out.length ? '\n    ' + out.join('\n    ') : '');
}
function instrument(m) {
  send('caesarim.dll base=' + m.base + (m.base.equals(ptr('0x60100000')) ? '' : '  (NOT preferred base; addresses invalid)'));
  if (PATCH) {
    const before = MMX_JNE.readByteArray(2);
    Memory.patchCode(MMX_JNE, 2, code => { code.writeU8(0x90); code.add(1).writeU8(0x90); });
    send('patched 0x601078FC ' + Array.from(new Uint8Array(before)).map(b => b.toString(16)).join(' ') + ' -> 90 90');
  }
  Interceptor.attach(PICKER, { onLeave(rv) { send('picker returned eax=' + rv.toInt32() + ' drivers ' + drivers()); } });
  Interceptor.attach(D3D_CREATE, { onLeave(rv) {
    send('D3D create returned eax=' + rv.toInt32() + ' error=' + JSON.stringify(ERROR_TEXT.readAnsiString()));
  }});
}
const existing = Process.findModuleByName('caesarim.dll');
if (existing) instrument(existing);
else Process.attachModuleObserver({ onAdded(m) { if (m.name.toLowerCase() === 'caesarim.dll') instrument(m); } });
Process.setExceptionHandler(ex => {
  const m = Process.findModuleByAddress(ex.address);
  send('EXCEPTION ' + ex.type + ' at ' + (m ? m.name + '+0x' + ex.address.sub(m.base).toString(16) : ex.address));
  return false;
});
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--patch", action="store_true", help="NOP the MMX-driver requirement in memory")
    parser.add_argument("seconds", nargs="?", type=float, default=20.0)
    args = parser.parse_args()

    device = frida.get_local_device()
    pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_EXE.parent))
    session = device.attach(pid)
    with LOG_PATH.open("w", encoding="utf-8") as log:
        def on_message(message, _data):
            line = message.get("payload") if message["type"] == "send" else str(message)
            log.write(f"{time.perf_counter():.3f} {line}\n")
            log.flush()

        script = session.create_script(AGENT.replace("%PATCH%", "true" if args.patch else "false"))
        script.on("message", on_message)
        script.load()
        device.resume(pid)
        deadline = time.time() + args.seconds
        while time.time() < deadline and not session.is_detached:
            time.sleep(0.2)
        alive = not session.is_detached
    print(f"process alive at end: {alive}; log at {LOG_PATH}")


if __name__ == "__main__":
    main()
