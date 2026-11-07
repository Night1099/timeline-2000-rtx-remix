"""Spawn Timeline.exe with startup hooks installed before the first instruction runs.

Logs shutdown triggers (DestroyWindow, PostQuitMessage, MessageBoxA, ExitProcess),
COM instantiation results, failed file opens and failed DLL loads, each with a
backtrace, to find what makes the game abort before creating its D3D device.

Usage: python patches/Timeline/spawn_trace.py [seconds]
"""
import sys
import time
from pathlib import Path

import frida

GAME_EXE = Path(r"F:/Vibe-Reverse-Engineering/RemixPlus/Timeline/Timeline.exe")
LOG_PATH = Path(__file__).with_name("spawn_trace.log")

AGENT = r"""
function bt(ctx) {
  return Thread.backtrace(ctx, Backtracer.ACCURATE).slice(0, 8)
    .map(a => { const m = Process.findModuleByAddress(a);
                return m ? m.name + '+0x' + a.sub(m.base).toString(16) : a.toString(); })
    .join(' < ');
}
function guid(p) {
  if (p.isNull()) return 'NULL';
  const d1 = p.readU32(), d2 = p.add(4).readU16(), d3 = p.add(6).readU16();
  const b = []; for (let i = 0; i < 8; i++) b.push(('0' + p.add(8 + i).readU8().toString(16)).slice(-2));
  return '{' + [('0000000' + d1.toString(16)).slice(-8), ('000' + d2.toString(16)).slice(-4),
          ('000' + d3.toString(16)).slice(-4), b.slice(0, 2).join(''), b.slice(2).join('')].join('-').toUpperCase() + '}';
}
const pending = [];
function install(m, h) {
  const a = m.findExportByName(h.name);
  if (!a) { send('nohook ' + h.mod + '!' + h.name); return; }
  Interceptor.attach(a, h.cb(h.name));
  send('hooked ' + h.mod + '!' + h.name);
}
function hook(mod, name, cb) {
  const h = { mod: mod.toLowerCase(), name, cb };
  const m = Process.findModuleByName(mod);
  if (m) install(m, h); else pending.push(h);
}
Process.attachModuleObserver({ onAdded(m) {
  for (let i = pending.length - 1; i >= 0; i--)
    if (pending[i].mod === m.name.toLowerCase()) { install(m, pending[i]); pending.splice(i, 1); }
}});
const stop = name => ({ onEnter(args) {
  let extra = '';
  if (name === 'MessageBoxA') extra = ' text=' + JSON.stringify(args[1].readAnsiString()) + ' caption=' + JSON.stringify(args[2].isNull() ? '' : args[2].readAnsiString());
  if (name === 'PostQuitMessage' || name === 'ExitProcess') extra = ' code=' + args[0].toInt32();
  send('STOP ' + name + extra + '\n    bt: ' + bt(this.context));
}});
['DestroyWindow', 'PostQuitMessage', 'MessageBoxA'].forEach(n => hook('user32.dll', n, stop));
hook('kernel32.dll', 'ExitProcess', stop);

hook('ole32.dll', 'CoCreateInstance', name => ({
  onEnter(args) { this.clsid = guid(args[0]); this.iid = guid(args[3]); this.ctx = this.context; },
  onLeave(rv) { const hr = rv.toUInt32();
    send('COM ' + this.clsid + ' iid=' + this.iid + ' hr=0x' + hr.toString(16) + (hr ? '  FAILED\n    bt: ' + bt(this.ctx) : '')); }
}));
hook('ole32.dll', 'CLSIDFromProgID', name => ({
  onEnter(args) { this.prog = args[0].readUtf16String(); this.out = args[1]; },
  onLeave(rv) { send('PROGID ' + this.prog + ' -> ' + (rv.toUInt32() ? 'FAILED hr=0x' + rv.toUInt32().toString(16) : guid(this.out))); }
}));
hook('kernel32.dll', 'CreateFileA', name => ({
  onEnter(args) { this.path = args[0].readAnsiString(); this.ctx = this.context; },
  onLeave(rv) {
    if (rv.toInt32() === -1) send('FILE-FAIL ' + this.path + '\n    bt: ' + bt(this.ctx)); else send('FILE ' + this.path);
    if (/config\.cfg/i.test(this.path) && ++cfgReads === 2) startStalker(Process.getCurrentThreadId());
  }
}));
hook('kernel32.dll', 'LoadLibraryA', name => ({
  onEnter(args) { this.path = args[0].readAnsiString(); },
  onLeave(rv) { if (rv.isNull()) send('LOADLIB-FAIL ' + this.path); }
}));
const GAME_MODULES = ['timeline.exe', 'caesarrm.dll', 'caesarim.dll'];
const ring = [];
const RING_MAX = 4000;
const FREEZE_AT = ptr('0x408519');
let frozen = false;
function gameAddr(a) { const m = Process.findModuleByAddress(a); return m && GAME_MODULES.indexOf(m.name.toLowerCase()) >= 0; }
let stalking = false;
let cfgReads = 0;
function where(a) {
  const m = Process.findModuleByAddress(a);
  return m ? m.name + '+0x' + a.sub(m.base).toString(16) : a.toString();
}
function startStalker(threadId) {
  if (stalking) return;
  stalking = true;
  Process.enumerateModules().forEach(m => {
    if (GAME_MODULES.indexOf(m.name.toLowerCase()) < 0) Stalker.exclude({ base: m.base, size: m.size });
  });
  Stalker.follow(threadId, {
    transform(iterator) {
      let insn;
      while ((insn = iterator.next()) !== null) {
        if (insn.address.equals(FREEZE_AT)) {
          iterator.putCallout(ctx => { if (!frozen) { frozen = true; ring.push('---- reached teardown 0x408519 ----'); const rets = []; for (let off = 0x2c0; off < 0x700; off += 4) { const v = ctx.esp.add(off).readPointer(); if (Process.findModuleByAddress(v) && !where(v).endsWith('+0x0')) rets.push('+0x' + off.toString(16) + '=' + where(v)); } send('WNDPROC-ARGS +0x2c8..: ' + [0x2c8,0x2cc,0x2d0,0x2d4,0x2d8,0x2dc,0x2e0].map(o => ctx.esp.add(o).readU32().toString(16)).join(' ')); send('TEARDOWN esp=' + ctx.esp + ' ebp=' + ctx.ebp + ' esi=' + ctx.esi + ' stack code ptrs: ' + rets.join('  ')); } });
        }
        if (!gameAddr(insn.address)) { iterator.keep(); continue; }
        if (insn.mnemonic === 'ret') {
          const at = insn.address;
          iterator.putCallout(ctx => {
            if (frozen) return;
            ring.push('ret  ' + where(at) + ' eax=' + ctx.eax);
            if (ring.length > RING_MAX) ring.shift();
          });
        } else if (insn.mnemonic === 'call') {
          const at = insn.address;
          iterator.putCallout(ctx => {
            if (frozen) return;
            ring.push('call ' + where(at));
            if (ring.length > RING_MAX) ring.shift();
          });
        }
        iterator.keep();
      }
    }
  });
  send('stalker started');
}
function dumpRing() { send('RING (' + ring.length + ')\n' + ring.join('\n')); ring.length = 0; }
function thrownTypes(throwInfo) {
  if (throwInfo.isNull()) return '?';
  const cta = throwInfo.add(12).readPointer();
  const names = [];
  for (let i = 0; i < cta.readU32(); i++)
    names.push(cta.add(4 + 4 * i).readPointer().add(4).readPointer().add(8).readAnsiString());
  return names.join(' | ');
}
hook('msvcrt.dll', '_CxxThrowException', name => ({ onEnter(args) {
  const obj = args[0];
  let peek = '';
  try { peek = ' obj=' + obj + ' dwords=' + [0, 4, 8, 12].map(o => obj.add(o).readU32().toString(16)).join(',');
        const s = obj.readPointer().readAnsiString(); if (s && /^[\x20-\x7e]{3,}$/.test(s)) peek += ' str=' + JSON.stringify(s); } catch (e) {}
  send('THROW ' + thrownTypes(args[1]) + peek + '\n    bt: ' + bt(this.context));
}}));
['exit', '_exit', '_cexit', '_c_exit'].forEach(n => hook('msvcrt.dll', n, name => ({ onEnter(args) {
  send('EXITCALL ' + name + ' code=' + args[0].toInt32() + ' esp=' + this.context.esp + '\n    bt: ' + bt(this.context));
}})));
hook('kernelbase.dll', 'RaiseException', name => ({ onEnter(args) {
  const code = args[0].toUInt32();
  send('RAISE ' + name + ' code=0x' + code.toString(16) + '\n    bt: ' + Thread.backtrace(this.context, Backtracer.FUZZY).slice(0, 14).map(where).join(' < '));
}}));
Process.setExceptionHandler(ex => {
  const m = Process.findModuleByAddress(ex.address);
  send('EXCEPTION ' + ex.type + ' at ' + (m ? m.name + '+0x' + ex.address.sub(m.base).toString(16) : ex.address) +
       ' mem=' + (ex.memory ? ex.memory.operation + ' ' + ex.memory.address : '') + '\n    bt: ' + bt(ex.context));
  if (stalking) dumpRing();
  return false;
});
send('hooks installed');
"""


def main() -> None:
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
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
        deadline = time.time() + seconds
        while time.time() < deadline and not session.is_detached:
            time.sleep(0.2)
    print(f"done; log at {LOG_PATH}")


if __name__ == "__main__":
    main()
