// Timeline (Eidos 2000) -- Caesar engine. Timeline.exe base 0x400000, caesarrm.dll 0x60300000, caesarim.dll 0x60100000 (internal name caesarim_dxonly.dll)
// NOTE: addresses span three modules (non-overlapping ranges). Session-1 caesarrm/caesarim notes are // comments; bootstrap and session-2 entries for all three modules are live @/$ lines.

// ---- Timeline.exe ----
@ 0x004014C0 void __cdecl WinMain_InitApp_part(void);            // calls App::Init at 0x4014E3; on false deletes g_app (0x4014F8)
$ 0x004CED70 void* g_app
@ 0x00408720 bool __thiscall App_Init(void* this, int a1, int a2, int a3, int a4, int a5, int a6, int a7); // returns AND of all subsystem inits
@ 0x00407BA0 void __thiscall App_dtor(void* this);               // vtable 0x4b8c10, jmps to 0x4082F0
@ 0x004082F0 void __thiscall App_dtor_body(void* this);
@ 0x00410120 bool __thiscall RenderMgr_Init(void* this, char a, char b, int c, int d, void* hwnd, int fsFlag, int e, int f, int g); // this=app+0xBAC; returns 0 if RMRender null
@ 0x00412000 void __thiscall RenderMgr_CreateRMRender_A(void* this);
@ 0x00412160 void __thiscall RenderMgr_CreateRMRender_B(void* this, void* guid1, void* guid2);
@ 0x004122B0 void __thiscall RenderMgr_CreateRMRender_C(void* this, void* mode, void* guid1, void* guid2);
@ 0x00412420 void __thiscall RenderMgr_CreateRMRender_D(void* this, void* mode);
@ 0x00411FE0 void* __thiscall RenderMgr_GetRMRender(void* this);  // return [this+0x10C]
@ 0x00430960 bool __thiscall TexMgr_Init(void* this);             // guards null RMRender
@ 0x004308C0 void __thiscall TexMgr_dtor(void* this);             // vtable 0x4b90b8; UNGUARDED SetTextureReloadCallback(0) on RMRender -> crash
@ 0x0042FFA0 void __thiscall Cfg_SaveVars(void* this, int a);     // reads [this+0x66C] table; crash #2 when null
@ 0x00417AA0 void __thiscall ObjList_Cleanup(void* this);         // crash #3 [this+0x120] null

// ---- caesarrm.dll (base 0x60300000) ----
// 0x60335DF0 RMRender::RMRender(HWND,int,int,HWND,_fullscreenmode*,GUID*,GUID*)  -- 2 IMRender attempts
// 0x60336130 RMRender::IsConstructed -> [this+8] && [this+0x14](IMRender*) && IMRender::IsConstructed
// 0x60338A20 RMRender::SetTextureReloadCallback(fn) -> [this+0x84]=fn
// 0x60338A10 RMRender::SetCreateCallback(fn)        -> [this+0x80]=fn

// ---- caesarim.dll (base 0x60100000) -- DX5 SDK d3dapp.c derived ----
// 0x6010C4C0 IMRender::IMRender ; device wrapper at [this+0x5ACC]
// 0x6010D0D0 IMRender::GetWindowSize (0 if [this+0x5ACC]==0)
// 0x60102BE0 IMDevice::IMDevice (size 0xADC, vtable 0x60117130, IsConstructed=[this+4])
// 0x60102B80 CpuHasMMX (CPUID.1:EDX bit23)
// 0x60102EF0 IMDevice::InitD3D -> sets flag 0x40 if MMX, calls D3DAppCreate
// 0x60101000 D3DAppCreateFromHWND(flags,...,driverFlags(arg9),w,h,bpp,ddGuid(arg13),d3dGuid(arg14 unused))
// 0x60106290 D3DAppICreateDD(flags, ddGuid)  ; bIsPrimary=$0x6012CBF8
// 0x60102490 D3DAppICreateD3D (QI IID_IDirect3D2)
// 0x601024D0 D3DAppIEnumDevices ; callback 0x60102520 ; driver count $0x60120884, records @0x6012088C stride 0x16C
// 0x60107B40 D3DAppIVerifyDriverAndMode(&drv,&mode,driverFlags)
// 0x601078F0 D3DAppIPickDriver(&drv, depthMask, driverFlags) -- with flag 0x40 REQUIRES a driver with GUID IID_IDirect3DMMXDevice (const @0x60117378)
// 0x60107A80 D3DAppIPickDisplayMode
// 0x60107F10 D3DAppISetErrorString -> buffer $0x60120560

// ---- Component (COM) system: crash at 0x4AA3A5 ----
// IID {5C20B341-9E68-11D2-AF16-444553540000} @0x4BB828 = per-component render interface (in every Components\*.dll)
@ 0x00412580 void __thiscall RenderMgr_RenderFrame(void* this, char skip); // loop 0x412824..0x41284F: QI each component, calls 0x4AA3A0 on result UNCHECKED
@ 0x0047A730 int __thiscall CompMgr_Count(void* this);                    // this=App+4; non-null slots in vec [this+0x144..0x148]
@ 0x0047A770 int __thiscall CompMgr_GetSlot(void* this, int n, int a, int b); // slot index of n-th live component
@ 0x0047B310 void* __thiscall CompMgr_QueryComponent(void* this, int slot, GUID* iid); // vec[slot]->host->QI; NULL if not exported
@ 0x0047ED80 bool __thiscall Component_QI(void* this, GUID* iid, void** out); // [this+0x14]=ComponentHost
@ 0x0047AA60 int __thiscall CompMgr_CreateByProgID(void* this, char* progid); // CLSIDFromProgID; continues even if DLL load fails later
@ 0x0047E880 void* __thiscall Component_ctor(void* this, int a, int b);  // size 0x4C, vtable 0x4BAD10
@ 0x0047EA30 bool __thiscall Component_CreateHost(void* this, GUID clsid); // [this+0x14]=host(0x4AFA10), [this+0x18]=loader(0x4B2580)
@ 0x004AFA10 void* __thiscall ComponentHost_ctor(void* this, GUID* clsid, void* owner); // size 0x60, vtable 0x4BCC8C, CoInitialize
@ 0x004AFB40 int __stdcall ComponentHost_QueryInterface(void* this, GUID* iid, void** out); // IUnknown->self, else table lookup 0x4AFC60, else E_NOINTERFACE + *out=0
@ 0x004AFC60 void* __thiscall ComponentHost_FindInterface(void* this, GUID* iid); // list at [this+0x20] of {IID, ptr@+0x10}, filled by the loaded component
@ 0x004B2580 void* __thiscall ComponentLoader_ctor(void* this, void* host); // vtable 0x4BCCF0, calls 0x4AFCD0
@ 0x004AFCD0 void __thiscall ComponentHost_LoadClass(void* this);       // CoGetClassObject(clsid, CLSCTX 5, IID@0x4BB8F8) @0x4AFCE6; fail -> jl 0x4AFD3A, silent
@ 0x004AA3A0 int __thiscall CompRenderIface_Render(void* this);        // FAULT 0x4AA3A5: mov ecx,[esi+0xC] with this==NULL
// [sigdb: byte, 0.75] crt
@ 0x6010F0A2 crt_xmatch_00EECE8D;

// [sigdb: byte, 0.85] crt
@ 0x601117F8 crt_xmatch_00EF0468;

// [sigdb: byte, 0.85] crt
@ 0x60111806 crt_xmatch_00EF0476;

// [sigdb: byte, 0.85] crt
@ 0x6011180B crt_xmatch_00EF047B;

// [sigdb: byte, 0.85] crt
@ 0x601118D0 crt_xmatch_00EF2FE0;

// [sigdb: byte, 0.85] crt
@ 0x601118E7 crt_xmatch_00EF2FF7;

// [sigdb: byte, 0.70] crt
@ 0x60111930 crt_xmatch_00EF3040;

// [sigdb: byte, 0.70] crt
@ 0x60111945 crt_xmatch_00EF3055;

// [sigdb: byte, 0.70] crt
@ 0x60111975 crt_xmatch_00EF3085;

// [sigdb: byte, 0.70] crt
@ 0x601119CE crt_xmatch_00EF30DE;

// [sigdb: byte, 0.70] crt
@ 0x60111A19 crt_xmatch_00EF3129;

// [sigdb: byte, 0.95] crt
@ 0x60113840 crt_xmatch_00EF0540;

// [sigdb: byte, 0.95] crt
@ 0x60113848 crt_xmatch_00EF0548;

// [sigdb: byte, 0.85] crt
@ 0x60113DF6 crt_xmatch_00EF0AF6;

// [sigdb: byte, 0.85] crt
@ 0x60113E09 crt_xmatch_00EF0B09;

// [sigdb: byte, 0.70] crt
@ 0x60114031 crt_xmatch_00EF0D31;

// [sigdb: byte, 0.70] crt
@ 0x6011424C crt_xmatch_00EF0F4C;

// [sigdb: byte, 0.70] crt
@ 0x60114504 crt_xmatch_00EF1204;

// [propagated: _thunk_sub_4E703075, 0.80]
@ 0x60101C20 _thunk_sub_4E703075;

// [propagated: _thunk_sub_6010EE20, 0.80]
@ 0x60103C00 _thunk_sub_6010EE20;

// [propagated: _thunk_sub_6010EF5E, 0.80]
@ 0x60106080 _thunk_sub_6010EF5E;

// [propagated: _thunk_sub_6010F010, 0.80]
@ 0x60106100 _thunk_sub_6010F010;

// [propagated: _thunk_sub_601071E0, 0.80]
@ 0x601068F3 _thunk_sub_601071E0;

// [propagated: _thunk_sub_60107F10, 0.80]
@ 0x60106ADB _thunk_sub_60107F10;

// [propagated: _thunk_sub_60107F10, 0.80]
@ 0x60106CF0 _thunk_sub_60107F10_60106CF0;

// [propagated: _thunk_sub_60107520, 0.80]
@ 0x60107410 _thunk_sub_60107520;

// [propagated: _thunk_sub_60107630, 0.80]
@ 0x60107630 _thunk_sub_60107630;

// [propagated: _thunk_sub_60107630, 0.80]
@ 0x60107730 _thunk_sub_60107630_60107730;

// [propagated: _thunk_sub_601084D0, 0.80]
@ 0x6010838F _thunk_sub_601084D0;

// [propagated: _thunk_sub_601084D0, 0.80]
@ 0x60108410 _thunk_sub_601084D0_60108410;

// [propagated: _thunk_sub_601084D0, 0.80]
@ 0x60108530 _thunk_sub_601084D0_60108530;

// [propagated: _thunk_sub_6010CAA0, 0.80]
// bootstrap mislabel superseded below: 0x6010DD90 _thunk_sub_6010CAA0

// [propagated: _thunk_sub_6010F010, 0.80]
@ 0x6010E570 _thunk_sub_6010F010_6010E570;

// [propagated: _thunk_sub_6010EF70, 0.80]
@ 0x6010EE20 _thunk_sub_6010EF70;

// [propagated: _thunk_sub_6010EF70, 0.80]
@ 0x6010EE47 _thunk_sub_6010EF70_6010EE47;

// [propagated: _thunk_sub_6010F43F, 0.80]
@ 0x6010F3F1 _thunk_sub_6010F43F;

// [propagated: _thunk_sub_6010EA1A, 0.80]
@ 0x6010FD78 _thunk_sub_6010EA1A;

// [propagated: _thunk_sub_6010EA1A, 0.80]
@ 0x6010FD97 _thunk_sub_6010EA1A_6010FD97;

// [propagated: _thunk_sub_60111ABC, 0.80]
@ 0x6011164E _thunk_sub_60111ABC;

// [propagated: _thunk_sub_60111ABC, 0.80]
@ 0x60111691 _thunk_sub_60111ABC_60111691;

// [propagated: _thunk_sub_60114883, 0.80]
@ 0x60114C47 _thunk_sub_60114883;

// [propagated: _thunk_sub_60116492, 0.80]
@ 0x6011668C _thunk_sub_60116492;

// [propagated: _thunk_sub_60116492, 0.80]
@ 0x601166AD _thunk_sub_60116492_601166AD;

// [propagated: _thunk_sub_60116492, 0.80]
@ 0x601166B2 _thunk_sub_60116492_601166B2;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60301010 _thunk_sub_603022D0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x603010B0 _thunk_sub_603022D0_603010B0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60301450 _thunk_sub_603022D0_60301450;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x603022C0 _thunk_sub_603022D0_603022C0;

// [propagated: _thunk_sub_60301070, 0.80]
@ 0x60302310 _thunk_sub_60301070;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x603023C0 _thunk_sub_603022D0_603023C0;

// [propagated: _thunk_sub_60308030, 0.80]
@ 0x603025D0 _thunk_sub_60308030;

// [propagated: _thunk_sub_60304AF0, 0.80]
// bootstrap mislabel superseded below: 0x60303340 _thunk_sub_60304AF0

// [propagated: _thunk_sub_60303380, 0.80]
// bootstrap mislabel superseded below: 0x603035A0 _thunk_sub_60303380

// [propagated: _thunk_sub_60303380, 0.80]
// bootstrap mislabel superseded below: 0x603036C0 _thunk_sub_60303380_603036C0

// [propagated: _thunk_sub_60304810, 0.80]
// (bootstrap mislabel removed; see RMCamera_ViewportTransformFace) 0x60304670

// [propagated: _thunk_sub_60303930, 0.80]
@ 0x60305090 _thunk_sub_60303930;

// [propagated: _thunk_sub_603066F0, 0.80]
// bootstrap mislabel superseded below: 0x60306760 _thunk_sub_603066F0

// [propagated: _thunk_sub_603066F0, 0.80]
// bootstrap mislabel superseded below: 0x603067C0

// [propagated: _thunk_sub_60303930, 0.80]
// bootstrap mislabel superseded below: 0x60307C80

// [propagated: _thunk_sub_60308030, 0.80]
@ 0x60308000 _thunk_sub_60308030_60308000;

// [propagated: _thunk_sub_60308030, 0.80]
@ 0x60308010 _thunk_sub_60308030_60308010;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60308260 _thunk_sub_603022D0_60308260;

// [propagated: _thunk_sub_603088C0, 0.80]
@ 0x603088A0 _thunk_sub_603088C0;

// [propagated: _thunk_sub_60340B86, 0.80]
@ 0x603088C0 _thunk_sub_60340B86;

// [propagated: _thunk_sub_60316C00, 0.80]
@ 0x60308E06 _thunk_sub_60316C00;

// [propagated: _thunk_sub_60316C00, 0.80]
@ 0x60308E20 _thunk_sub_60316C00_60308E20;

// [propagated: _thunk_sub_60309760, 0.80]
@ 0x60308E80 _thunk_sub_60309760;

// [propagated: _thunk_sub_60316C00, 0.80]
@ 0x60309490 _thunk_sub_60316C00_60309490;

// [propagated: _thunk_sub_60309760, 0.80]
@ 0x60309740 _thunk_sub_60309760_60309740;

// [propagated: _thunk_sub_60340BC4, 0.80]
@ 0x603097E0 _thunk_sub_60340BC4;

// [propagated: _thunk_sub_60340BC4, 0.80]
@ 0x60309850 _thunk_sub_60340BC4_60309850;

// [propagated: _thunk_sub_60340BC4, 0.80]
@ 0x603098C0 _thunk_sub_60340BC4_603098C0;

// [propagated: _thunk_sub_60340BC4, 0.80]
@ 0x60309930 _thunk_sub_60340BC4_60309930;

// [propagated: _thunk_sub_603016D0, 0.80]
@ 0x60309C50 _thunk_sub_603016D0;

// [propagated: _thunk_sub_60327CB0, 0.80]
@ 0x6030A3B0 _thunk_sub_60327CB0;

// [propagated: _thunk_sub_603313E0, 0.80]
@ 0x6030C710 _thunk_sub_603313E0;

// [propagated: _thunk_sub_6030CA40, 0.80]
@ 0x6030CBB0 _thunk_sub_6030CA40;

// [propagated: _thunk_sub_6030CA40, 0.80]
@ 0x6030D17C _thunk_sub_6030CA40_6030D17C;

// [propagated: _thunk_sub_6030CA40, 0.80]
@ 0x6030DCE0 _thunk_sub_6030CA40_6030DCE0;

// [propagated: _thunk_sub_6030CA40, 0.80]
@ 0x6030E247 _thunk_sub_6030CA40_6030E247;

// [propagated: _thunk_sub_60311DA0, 0.80]
@ 0x603102B0 _thunk_sub_60311DA0;

// [propagated: _thunk_sub_60311F50, 0.80]
@ 0x603102D0 _thunk_sub_60311F50;

// [propagated: _thunk_sub_603104A0, 0.80]
@ 0x603102F0 _thunk_sub_603104A0;

// [propagated: _thunk_sub_60329C20, 0.80]
@ 0x60312750 _thunk_sub_60329C20;

// [propagated: _thunk_sub_60340350, 0.80]
@ 0x603128A0 _thunk_sub_60340350;

// [propagated: _thunk_sub_60340380, 0.80]
@ 0x603128C0 _thunk_sub_60340380;

// [propagated: _thunk_sub_60312A40, 0.80]
@ 0x60312980 _thunk_sub_60312A40;

// [propagated: _thunk_sub_60312C40, 0.80]
@ 0x603129C0 _thunk_sub_60312C40;

// [propagated: _thunk_sub_60340890, 0.80]
@ 0x60313151 _thunk_sub_60340890;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60313200 _thunk_sub_603022D0_60313200;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x603132E0 _thunk_sub_60340B80;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60313310 _thunk_sub_60340B80_60313310;

// [propagated: _thunk_sub_60315870, 0.80]
@ 0x60313510 _thunk_sub_60315870;

// [propagated: _thunk_sub_60315870, 0.80]
@ 0x60313560 _thunk_sub_60315870_60313560;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x60313660 _thunk_sub_6031C8A0;

// [propagated: _thunk_sub_60325E80, 0.80]
@ 0x603147BE _thunk_sub_60325E80;

// [propagated: _thunk_sub_60307B20, 0.80]
@ 0x60315849 _thunk_sub_60307B20;

// [propagated: _thunk_sub_60317590, 0.80]
// bootstrap mislabel superseded below: 0x60316860 _thunk_sub_60317590

// [propagated: _thunk_sub_60317590, 0.80]
@ 0x603168C0 _thunk_sub_60317590_603168C0;

// [propagated: _thunk_sub_60317590, 0.80]
// bootstrap mislabel superseded below: 0x60316920 _thunk_sub_60317590_60316920

// [propagated: _thunk_sub_60317590, 0.80]
@ 0x60317180 _thunk_sub_60317590_60317180;

// [propagated: _thunk_sub_60317590, 0.80]
@ 0x60317220 _thunk_sub_60317590_60317220;

// [propagated: _thunk_sub_60317590, 0.80]
@ 0x603172C0 _thunk_sub_60317590_603172C0;

// [propagated: _thunk_sub_60317870, 0.80]
@ 0x60317870 _thunk_sub_60317870;

// [propagated: _thunk_sub_603178E0, 0.80]
@ 0x603178E0 _thunk_sub_603178E0;

// [propagated: _thunk_sub_60329D20, 0.80]
@ 0x60317F70 _thunk_sub_60329D20;

// [propagated: _thunk_sub_60317A00, 0.80]
@ 0x60318520 _thunk_sub_60317A00;

// [propagated: _thunk_sub_6031B6A0, 0.80]
@ 0x6031B630 _thunk_sub_6031B6A0;

// [propagated: _thunk_sub_60320340, 0.80]
@ 0x60320320 _thunk_sub_60320340;

// [propagated: _thunk_sub_60329D20, 0.80]
@ 0x60321120 _thunk_sub_60329D20_60321120;

// [propagated: _thunk_sub_60329D20, 0.80]
@ 0x60321350 _thunk_sub_60329D20_60321350;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x603213C0 _thunk_sub_603022D0_603213C0;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60321400 _thunk_sub_60340B80_60321400;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60321430 _thunk_sub_60340B80_60321430;

// [propagated: _thunk_sub_60325860, 0.80]
@ 0x60321E60 _thunk_sub_60325860;

// [propagated: _thunk_sub_60325E80, 0.80]
@ 0x60323670 _thunk_sub_60325E80_60323670;

// [propagated: _thunk_sub_60307B20, 0.80]
@ 0x60325357 _thunk_sub_60307B20_60325357;

// [propagated: _thunk_sub_60317AC0, 0.80]
@ 0x603254B0 _thunk_sub_60317AC0;

// [propagated: _thunk_sub_60317A00, 0.80]
@ 0x603254F0 _thunk_sub_60317A00_603254F0;

// [propagated: _thunk_sub_60325BE0, 0.80]
@ 0x603255D0 _thunk_sub_60325BE0;

// [propagated: _thunk_sub_60317C70, 0.80]
@ 0x60325780 _thunk_sub_60317C70;

// [propagated: _thunk_sub_60317C70, 0.80]
@ 0x603257A0 _thunk_sub_60317C70_603257A0;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x60325860 _thunk_sub_6031C8A0_60325860;

// [propagated: _thunk_sub_603174E0, 0.80]
@ 0x60325AC0 _thunk_sub_603174E0;

// [propagated: _thunk_sub_60325390, 0.80]
// bootstrap mislabel superseded below: 0x60325B40 _thunk_sub_60325390

// [propagated: _thunk_sub_6033C960, 0.80]
@ 0x603274C9 _thunk_sub_6033C960;

// [propagated: _thunk_sub_60325B40, 0.80]
@ 0x60327A30 _thunk_sub_60325B40;

// [propagated: _thunk_sub_60317B60, 0.80]
@ 0x60327F10 _thunk_sub_60317B60;

// [propagated: _thunk_sub_60317BD0, 0.80]
@ 0x60327FA0 _thunk_sub_60317BD0;

// [propagated: _thunk_sub_60317C20, 0.80]
@ 0x60327FE0 _thunk_sub_60317C20;

// [propagated: _thunk_sub_60317B10, 0.80]
@ 0x60328020 _thunk_sub_60317B10;

// [propagated: _thunk_sub_60317F70, 0.80]
@ 0x60328690 _thunk_sub_60317F70;

// [propagated: _thunk_sub_60329C70, 0.80]
@ 0x60329B80 _thunk_sub_60329C70;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60329D40 _thunk_sub_603022D0_60329D40;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60329D60 _thunk_sub_603022D0_60329D60;

// [propagated: _thunk_sub_60317A00, 0.80]
@ 0x6032B050 _thunk_sub_60317A00_6032B050;

// [propagated: _thunk_sub_60317AC0, 0.80]
@ 0x6032B090 _thunk_sub_60317AC0_6032B090;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x6032CF60 _thunk_sub_6031C8A0_6032CF60;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x6032D700 _thunk_sub_6031C8A0_6032D700;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x6032D7A0 _thunk_sub_6031C8A0_6032D7A0;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x6032DD90 _thunk_sub_6031C8A0_6032DD90;

// [propagated: _thunk_sub_6031C8A0, 0.80]
@ 0x6032E130 _thunk_sub_6031C8A0_6032E130;

// [propagated: _thunk_sub_60331400, 0.80]
@ 0x60330137 _thunk_sub_60331400;

// [propagated: _thunk_sub_60303930, 0.80]
@ 0x60330D37 _thunk_sub_60303930_60330D37;

// [propagated: _thunk_sub_60330FB0, 0.80]
@ 0x60330F91 _thunk_sub_60330FB0;

// [propagated: _thunk_sub_60301070, 0.80]
@ 0x60331A20 _thunk_sub_60301070_60331A20;

// [propagated: _thunk_sub_60331400, 0.80]
@ 0x60331C60 _thunk_sub_60331400_60331C60;

// [propagated: _thunk_sub_60302050, 0.80]
@ 0x60332550 _thunk_sub_60302050;

// [propagated: _thunk_sub_60316C00, 0.80]
// bootstrap mislabel superseded below: 0x60333750

// [propagated: _thunk_sub_60334690, 0.80]
@ 0x603345A0 _thunk_sub_60334690;

// [propagated: _thunk_sub_60334690, 0.80]
@ 0x603345F0 _thunk_sub_60334690_603345F0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60335C80 _thunk_sub_603022D0_60335C80;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60335CC0 _thunk_sub_60340B80_60335CC0;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60335CF0 _thunk_sub_60340B80_60335CF0;

// [propagated: _thunk_sub_60304D30, 0.80]
// bootstrap mislabel superseded below: 0x60337A30 _thunk_sub_60304D30

// [propagated: _thunk_sub_60337C30, 0.80]
@ 0x60337C70 _thunk_sub_60337C30;

// [propagated: _thunk_sub_60306AC0, 0.80]
@ 0x60337DC0 _thunk_sub_60306AC0;

// [propagated: _thunk_sub_603381E0, 0.80]

// [propagated: _thunk_sub_603381E0, 0.80]
@ 0x60337F90 _thunk_sub_603381E0_60337F90;

// [propagated: _thunk_sub_603381E0, 0.80]
@ 0x603388B0 _thunk_sub_603381E0_603388B0;

// [propagated: _thunk_sub_60340380, 0.80]
@ 0x60338B40 _thunk_sub_60340380_60338B40;

// [propagated: _thunk_sub_60338F20, 0.80]
@ 0x60338F50 _thunk_sub_60338F20;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60339000 _thunk_sub_603022D0_60339000;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60339040 _thunk_sub_60340B80_60339040;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x60339070 _thunk_sub_60340B80_60339070;

// [propagated: _thunk_sub_60316C00, 0.80]
@ 0x603392B0 _thunk_sub_60316C00_603392B0;

// [propagated: _thunk_sub_60316490, 0.80]
@ 0x6033AC40 _thunk_sub_60316490;

// [propagated: _thunk_sub_6033B7A0, 0.80]
@ 0x6033B1C0 _thunk_sub_6033B7A0;

// [propagated: _thunk_sub_60317A00, 0.80]
@ 0x6033B740 _thunk_sub_60317A00_6033B740;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x6033B920 _thunk_sub_603022D0_6033B920;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x6033B960 _thunk_sub_60340B80_6033B960;

// [propagated: _thunk_sub_60340B80, 0.80]
@ 0x6033B990 _thunk_sub_60340B80_6033B990;

// [propagated: _thunk_sub_60329CE0, 0.80]
@ 0x6033B9B0 _thunk_sub_60329CE0;

// [propagated: _thunk_sub_60338190, 0.80]
@ 0x6033B9F0 _thunk_sub_60338190;

// [propagated: _thunk_sub_6033BC50, 0.80]
@ 0x6033BC20 _thunk_sub_6033BC50;

// [propagated: _thunk_sub_6033C9F0, 0.80]
@ 0x6033C8C0 _thunk_sub_6033C9F0;

// [propagated: _thunk_sub_6033C9F0, 0.80]
@ 0x6033C8F0 _thunk_sub_6033C9F0_6033C8F0;

// [propagated: _thunk_sub_60340B8C, 0.80]
@ 0x6033D2C0 _thunk_sub_60340B8C;

// [propagated: _thunk_sub_60340B8C, 0.80]
@ 0x6033D7E0 _thunk_sub_60340B8C_6033D7E0;

// [propagated: _thunk_sub_6033DAF0, 0.80]
@ 0x6033DA90 _thunk_sub_6033DAF0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x6033DED0 _thunk_sub_603022D0_6033DED0;

// [propagated: _thunk_sub_603403C0, 0.80]
@ 0x6033E8A0 _thunk_sub_603403C0;

// [propagated: _thunk_sub_603402D0, 0.80]
@ 0x6033E990 _thunk_sub_603402D0;

// [propagated: _thunk_sub_6033E990, 0.80]
@ 0x6033EC90 _thunk_sub_6033E990;

// [propagated: _thunk_sub_6033E640, 0.80]
@ 0x6033F830 _thunk_sub_6033E640;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x603400B0 _thunk_sub_603022D0_603400B0;

// [propagated: _thunk_sub_60340B86, 0.80]
@ 0x60340110 _thunk_sub_60340B86_60340110;

// [propagated: _thunk_sub_60340420, 0.80]
@ 0x60340280 _thunk_sub_60340420;

// [propagated: _thunk_sub_60340530, 0.80]
@ 0x60340310 _thunk_sub_60340530;

// [propagated: _thunk_sub_60340420, 0.80]
@ 0x603403A0 _thunk_sub_60340420_603403A0;

// [propagated: _thunk_sub_60340420, 0.80]
@ 0x603403C0 _thunk_sub_60340420_603403C0;

// [propagated: _thunk_sub_60340B86, 0.80]
@ 0x60340830 _thunk_sub_60340B86_60340830;

// [propagated: _thunk_sub_60340980, 0.80]
@ 0x60340890 _thunk_sub_60340980;

// [propagated: _thunk_sub_60340980, 0.80]
@ 0x60340910 _thunk_sub_60340980_60340910;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60340AD0 _thunk_sub_603022D0_60340AD0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60340AF0 _thunk_sub_603022D0_60340AF0;

// [propagated: _thunk_sub_603022D0, 0.80]
@ 0x60340B10 _thunk_sub_603022D0_60340B10;

// [propagated: _thunk_sub_60340B98, 0.80]
@ 0x60340BC4 _thunk_sub_60340B98;

// [propagated: _thunk_sub_60340CED, 0.80]
@ 0x60340C8F _thunk_sub_60340CED;


// ======================================================================
// Session 2: transform pipeline. caesarrm projects (RMCamera), caesarim only rasterizes.
// ======================================================================

// D3DTLVERTEX layout = caesarim _imvertex (stride 0x20)
struct IMVertex { float sx; float sy; float sz; float rhw; unsigned int color; unsigned int specular; float tu; float tv; };
// caesarrm _vertex (stride 0x34): input to the projection (model space or world space, see findings)
struct RMVertex { float x; float y; float z; float nx; float ny; float nz; unsigned int color; unsigned int specular; float tu; float tv; int pad28; int pad2C; int materialIndex; };
// Row-vector 4x4 (v' = v * M), m[row][col]; trailing dword written by 0x60301F00
struct RMMatrix44 { float m[4][4]; int tag; };
struct RMMatrix43 { float m[4][3]; };

// ---- caesarim.dll: IMRender (exported) / IMDevice (vtable 0x60117130) ----
@ 0x6010CBB0 int __thiscall IMRender_DrawPoly(void* this, IMVertex* v, unsigned int n, int texture, int flags, int a5);
@ 0x6010CC80 int __thiscall IMRender_DrawPolyNoClip(void* this, IMVertex* v, unsigned int n, int texture, int flags, int a5);
@ 0x6010CCE0 int __thiscall IMRender_DrawPolysNoClip(void* this, IMVertex* v, unsigned int n, int texture, int flags, int a5);
@ 0x6010CD10 int __thiscall IMRender_DrawIndexedPolysNoClip(void* this, IMVertex* v, unsigned int n, unsigned short* idx, int ni, int texture, int flags, int a7);
@ 0x6010CD50 int __thiscall IMRender_ExternalDrawTriangles(void* this, IMVertex* v, unsigned int n, int texture, int flags, int clip, int a6);
@ 0x6010CEE0 int __thiscall IMRender_ExternalDrawIndexedTriangles(void* this, IMVertex* v, unsigned int n, unsigned short* idx, int ni, int texture, int flags, int clip, int a8);
@ 0x6010D440 void __thiscall IMRender_UpdateViewWindow(void* this, int left, int top, int right, int bottom, int flag);
@ 0x6010D480 int __thiscall IMRender_PushClip(void* this, IMVertex* poly, int n, int a3, int a4);
@ 0x6010D560 int __thiscall IMRender_PopClip(void* this);
@ 0x6010E060 void __thiscall IMRender_ClipAndAppend(void* this, IMVertex* v, int n, int a3, IMVertex* out, int* outN, int a6, int a7);
@ 0x6010D6B0 void __thiscall IMRender_GetRenderStateStruct(void* this, void* out);
@ 0x6010E4A0 void __thiscall IMRender_DrawGrid(void* this, int a, int b, int c, int d, int rgb);
@ 0x6010C990 int __thiscall IMRender_ClipPolyToStack(void* this, IMVertex* v, int n, IMVertex* out, int* outN);
@ 0x60103820 int __thiscall IMDevice_DrawFan(void* this, IMVertex* v, unsigned int n, int flags, int texture, int a5);
@ 0x60103940 int __thiscall IMDevice_DrawTriList(void* this, IMVertex* v, unsigned int n, int flags, int texture, int a5);
@ 0x60103A10 int __thiscall IMDevice_DrawIndexedTriList(void* this, IMVertex* v, unsigned int n, unsigned short* idx, int ni, int flags, int texture);
@ 0x60103720 int __thiscall IMDevice_SetTexture(void* this, int texture, int a2);
@ 0x60104580 int __thiscall IMDevice_GetBackbufferDC(void* this, void* outHdc);
@ 0x601045A0 int __thiscall IMDevice_ReleaseBackbufferDC(void* this, void* hdc);
@ 0x60104D80 void __thiscall IMDevice_GetRenderStateStruct(void* this, void* out);
@ 0x60105F20 void __thiscall IMDevice_DrawGridLines(void* this, int a, int b, int c);
$ 0x6012CFC0 void* g_d3dapp
$ 0x6012CFD8 int g_imRenderStateCache[21]
$ 0x6012D040 void* g_gdiSurface
$ 0x601172E0 float g_zero_f

// ---- caesarrm.dll (base 0x60300000): RMCamera ----
// RMCamera field offsets:
//  +0x0BC RMMatrix43 cameraWorld (rows 0-2 axes, row 3 at +0xE0 = position)
//  +0x184 int matrixDirty
//  +0x1C8 fn  renderFXCallback(IMVertex*) applied to each output vertex
//  +0x1D0 fn  projectFn[2] (0x603025E0 plain, 0x603026B0 fog), index = +0xDB4
//  +0x1E0 RMMatrix44 proj (pixel-space projection, row-vector)
//  +0x224 RMMatrix44 mvp = [modelWorld *] view * proj (model part pushed by 0x60307440)
//  +0x268 RMMatrix44 savedVP
//  +0x2EC float field = tan(fieldAngle/2) * frontClip
//  +0x2F0 float frontClip   +0x2F4 float backClip (clip planes only)   +0x2F8 float fieldAngle (rad)
//  +0x30C int viewLeft +0x310 viewTop +0x314 viewRight +0x318 viewBottom
//  +0x31C RMMatrix44 view = inverse(cameraWorld), row-vector world->view (D3D convention)
//  +0x670 IMRender*  +0xD94 projectionType (1 = parallel)  +0xD98/+0xD9C int camOffsetX/Y
//  +0xDA0 float aspect  +0xDB4 int fogEnabled  +0xDC0 float parallelZoom  +0xDD0 float camPos[3]
@ 0x603032F0 int __thiscall RMCamera_SetProjectionType(void* this, int type);
@ 0x60303340 void __thiscall RMCamera_SetField(void* this, float field);
@ 0x60303380 void __thiscall RMCamera_SetFieldAngle(void* this, float radians);
@ 0x603035A0 void __thiscall RMCamera_SetFrontClip(void* this, float n);
@ 0x60303600 void __thiscall RMCamera_SetBackClip(void* this, float f);
@ 0x603036C0 void __thiscall RMCamera_SetAspectRatio(void* this, float a);
@ 0x603037D0 void __thiscall RMCamera_SetViewWindow(void* this, int x, int y, int w, int h, int setAspect);
@ 0x60303930 void __thiscall RMCamera_UpdateCameraMatrix(void* this);
@ 0x60304AF0 void __thiscall RMCamera_BuildProjection(void* this);
@ 0x60304670 void __thiscall RMCamera_ViewportTransformFace(void* this, RMVertex* v, unsigned short* idx, int n, IMVertex* out, int* outN, int a6, int a7);
@ 0x60304810 int __thiscall RMCamera_VolumeClipFace(void* this, RMVertex* v, unsigned short* idx, int n, int* outN, int a5);
@ 0x60306850 int __thiscall RMCamera_ViewportTransformPoint(void* this, void* inPt, void* outPt);
@ 0x60306A60 void __thiscall RMCamera_SetCameraOffsets(void* this, int x, int y);
@ 0x60306FE0 void __thiscall RMCamera_SetParallelZoom(void* this, float z);
@ 0x60304D30 int __thiscall RMCamera_BeginScene(void* this, void* imRender);
@ 0x60307440 void __thiscall RMCamera_PushModelMatrix(void* this, RMMatrix43 modelWorld, void* a2, void* a3, void* a4);
@ 0x603025E0 void __cdecl RM_ProjectVerts(RMMatrix44* mvp, RMVertex* in, IMVertex* out, int n, int a5, float* camPos, void* fog);
@ 0x603026B0 void __cdecl RM_ProjectVertsFog(RMMatrix44* mvp, RMVertex* in, IMVertex* out, int n, int a5, float* camPos, void* fog);
@ 0x60301F00 RMMatrix44* __thiscall RMMatrix44_FromMatrix43(RMMatrix44* this, RMMatrix43* src);
@ 0x603020B0 void __thiscall RMMatrix44_InvertAffine(RMMatrix44* this);
@ 0x60321F30 void __thiscall RMModel_Pick(void* this, void* camera, void* a2, void* hitRecord); // vtbl+0x74 (0x60342564); pick path, not render (was RMModel_RenderTris)
@ 0x60325B40 void* __thiscall RMModel_GetTransformedBoundingBox(void* this);
@ 0x60308030 void __thiscall RMCameraSprite_BuildQuad(void* this, void* rect);
@ 0x60308150 void __thiscall RMCameraSprite_Draw(void* this, void* imRender);
@ 0x60337A30 int __thiscall RMRender_BeginRenderScene(void* this, void* camera);
@ 0x60337740 int __thiscall RMRender_Render(void* this, void* camera, void* container, int a3);

// ======================================================================
// caesarrm camera (session 2, camera agent): RMFrame layout, camera getters, Timeline.exe camera plumbing
// ======================================================================
// RMFrame field offsets (base of RMCamera / RMModel; all matrices row-vector, translation in row 3):
//  +0x074 float scale[3] (applied into +0xEC when +0x18C > 0)
//  +0x080 float pivotOffset[3] (added to world translation)
//  +0x08C RMMatrix43 localTrans  (Get/SetModelTrans)
//  +0x0BC RMMatrix43 worldTrans  (Get/SetWorldTrans; local * parent world), position at +0xE0
//  +0x0EC RMMatrix43 renderWorld = worldTrans with scale; this is the model->world matrix used at draw time
//  +0x11C float orient33[9]
//  +0x140 RMMatrix44 invWorld (inverse of +0xEC)   +0x184 int dirty   +0x188 int isStatic (verts already world space)
//  +0x18C int scaleEnabled   +0x190 RMFrame* parent
// RMCamera extra: +0x2AC RMMatrix43 cameraWorld snapshot taken in BeginScene; +0x2FC RECT clipRect;
//  +0xDA4 int backClipEnable; +0xDE4 saved x87 control word (BeginScene sets 24-bit precision)
// RMRender: +0x14 IMRender*, +0x2C RMCamera* currentSceneCamera, +0xE0 int sceneCamCount, +0xE4 RMCamera* sceneCams[10]
@ 0x60302A30 void* __thiscall RMCamera_ctor(void* this, void* rmRender, void* container, int a3, int l, int t, int r, int b, int a8); // size 0xDEC (args from call site 0x60336799; r2 dropped leading args); default fieldAngle 70deg, near 1, back 5000
@ 0x60336730 void* __thiscall RMRender_CreateCamera(void* this, int tag, void* container, int l, int t, int r, int b, int a);
@ 0x60337A70 int __thiscall RMRender_EndRenderScene(void* this);     // -> RMCamera EndScene 0x60304ED0 on [this+0x2C]
@ 0x60304ED0 int __thiscall RMCamera_EndScene(void* this);
@ 0x603079F0 void __thiscall RMCamera_PopModelMatrix(void* this);   // +0x224 = +0x268 (restores view*proj), clip planes restored
@ 0x60303330 int __thiscall RMCamera_GetProjectionType(void* this);
@ 0x60303370 float __thiscall RMCamera_GetField(void* this);
@ 0x60303590 float __thiscall RMCamera_GetFieldAngle(void* this);
@ 0x603035F0 float __thiscall RMCamera_GetFrontClip(void* this);
@ 0x60303680 float __thiscall RMCamera_GetBackClip(void* this);
@ 0x60303690 int __thiscall RMCamera_EnableBackClip(void* this, int enable);
@ 0x60303710 float __thiscall RMCamera_GetAspectRatio(void* this);
@ 0x60303720 void __thiscall RMCamera_SetClipRect(void* this, int l, int t, int r, int b);
@ 0x603037A0 void __thiscall RMCamera_GetClipRect(void* this, void* outRect);
@ 0x60303900 void __thiscall RMCamera_GetViewWindowRect(void* this, void* outRect);
@ 0x60316830 int __thiscall RMFrame_GetModelTrans(void* this, RMMatrix43* out);
@ 0x60316860 int __thiscall RMFrame_SetModelTrans(void* this, RMMatrix43* m);
@ 0x603168F0 int __thiscall RMFrame_GetWorldTrans(void* this, RMMatrix43* out);
@ 0x60316920 int __thiscall RMFrame_SetWorldTrans(void* this, RMMatrix43* m);
@ 0x60317910 int __thiscall RMFrame_GetInverseWorldMatrix(void* this, RMMatrix43* out);
@ 0x60316B20 int __thiscall RMFrame_SetPosition(void* this, float* pos, void* relativeTo);
@ 0x60316C00 int __thiscall RMFrame_GetPosition(void* this, float* outPos, void* relativeTo);
@ 0x60317590 void __thiscall RMFrame_UpdateWorld(void* this);       // recompute +0xBC/+0xEC/+0x140, set dirty +0x184
@ 0x60321450 void* __thiscall RMModel_ctor(void* this);               // vtable 0x603424F0
// ---- Timeline.exe camera plumbing (RenderMgr = App+0xBAC embedded; g_app = 0x4CED70) ----
// RenderMgr: +0x10C RMRender*, +0x114 RMContainer* (scene for cameras), +0x11C/+0x124 RMCamera* created by RenderMgr_CreateCamera, +0x120 RMCamera* activeCamera
@ 0x00412910 void* __thiscall RenderMgr_CreateCamera(void* this, void* container);    // CreateCamera(clientRect), SetViewWindow, FOV 70deg (0x3F9C61AA rad), near 5, back 5000
@ 0x00413950 void __thiscall RenderMgr_SetActiveCamera(void* this, void* camera, int mode); // NULL -> [this+0x11C]; writes [this+0x120]
@ 0x0043B8F0 void __stdcall CamIface_SetFieldAngle(void* unused, void* camera, float radians); // component-facing wrapper
@ 0x0043B920 void __stdcall CamIface_SetFrontClip(void* unused, void* camera, float n);

// ======================================================================
// Session 2: culling / clipping (see findings "Culling and clipping patch points (session 2)")
// ======================================================================
// RMCamera plane sets (float4 n.xyz,d; inside when n.p + d >= 0):
//  +0x360 clipPlanes[]  count +0xD78. [0] = near (always clipped by VolumeClipFace, even if count==0).
//         If +0xDDC (sideClip) != 0: [1..4] = left/right/top/bottom (+0x370..+0x3A0), [5] = back (+0x3B0, when +0xDA4).
//         Else count = 1 (+1 when a user plane is active: index +0xD7C, set by mirror/transform portals).
//  +0x5E0 cullPlanes[]  count +0x640 (side planes, + back at +0x620 when +0xDA4 and sideClip==0). Only BoundsCheck uses them.
//  +0x65C -> {RMVertex* buf; int cap} clip output (cap 32 RMVertex), +0x660 -> {IMVertex* buf; int cap} projected verts (grows)
//  +0x674 IMVertex[4] viewport clip quad, +0x6F4 int clip flag for PushClip, +0x6F8 RMVertex[32] clip scratch
//  +0xD7C int userPlaneIndex (-1 none), +0xDDC int sideClip (default 0; set to 1 only by RMModel shadow-receiver render 0x60328A30)
// BoundsCheck return codes: 0 = fully visible (whole mesh, no clip), 1 = inside 3D volume but needs 2D clip test,
//  2 = culled, 3 = crosses a 3D clip plane (per-face clip path). *outArea = screen bbox area (1000000 when not measured).
// Render visibility hint (last arg of RMModel vtbl+0x70/+0x84): 0 = skip BoundsCheck (status 0), 2 = culled, 4 = force status 3, else BoundsCheck.
@ 0x60305880 int __thiscall RMCamera_BoundsCheck(void* this, int nVerts, void* box, float cx, float cy, float cz, float radius, long* outArea); // exported; box is _boundingbox by value (0x60 bytes); ret 0x78
@ 0x60305190 int __thiscall RMCamera_ClipFaceToPlane0Gather(void* this, float nx, float ny, float nz, float d, RMVertex* v, unsigned short* idx, int n, RMVertex* out, int a, int* outFlags);
@ 0x60305520 int __thiscall RMCamera_ClipPolyToPlane(void* this, float nx, float ny, float nz, float d, RMVertex* in, int n, RMVertex* out, int a, int* outFlags);
@ 0x60304780 RMVertex* __thiscall RMCamera_ViewportTransformFaceNoCopy(void* this, RMVertex* v, unsigned short* idx, int n, IMVertex* out, int* outN, int a6);
@ 0x60304980 void __thiscall RMCamera_TransformVerts(void* this, RMVertex* v, int n, IMVertex** outBuf, void* a4, int a5, int a6); // no clipping; projects n verts into the cam+0x660 buffer
@ 0x60306370 int __thiscall RMCamera_VolumeTestPoints(void* this, RMVertex* v, int n, int* inside, int* a4, int a5);
@ 0x60306430 int __thiscall RMCamera_VolumeOutcodes(void* this, RMVertex* v, int n, int* outcodes, int* a4, int a5);
@ 0x60306500 void __thiscall RMCamera_TransformVisibleVerts(void* this, RMVertex* v, int n, IMVertex** outBuf, int* flags, int a5, int a6, int a7);
@ 0x603066F0 int __thiscall RMCamera_AddClipPlane(void* this, float* plane);
@ 0x603067C0 int __thiscall RMCamera_SetUserClipPlane(void* this, float* plane, float* oldPlane);
@ 0x60306760 void __thiscall RMCamera_RemoveUserClipPlane(void* this);
@ 0x60306E40 int __thiscall RMCamera_ForceViewportClip(void* this);   // exported; IMRender::PushClip(cam+0x674, 4, [cam+0x6F4], 1 = replace)
@ 0x60306E60 int __thiscall RMCamera_PopForcedClip(void* this);
@ 0x60307C80 void __thiscall RMCamera_SetSideClip(void* this, int enable); // writes +0xDDC, dirties and rebuilds planes
@ 0x603221F0 int __thiscall RMModel_BuildFrontFaceIndices(void* this, void* imr, IMVertex* v, unsigned short* faces, int nFaces, int* io, int tex, int flags, int a8); // 2D backface cull (sign test at 0x6032227E) then DrawIndexedPolysNoClip; shared by RMModel and RMDeformationModel no-clip paths
@ 0x60322330 int __thiscall RMModel_Render(void* this, void* imRender, void* camera, void* a3, int hint); // vtbl+0x70 dispatcher -> +0x84 or +0x80
@ 0x603223E0 int __thiscall RMModel_RenderMesh(void* this, void* imRender, void* camera, void* a3, int pass, int hint); // vtbl+0x84 (0x60342574); BoundsCheck at 0x6032255C, status dispatch at 0x60322884
@ 0x603236A0 int __thiscall RMModel_RenderMesh2(void* this, void* imRender, void* camera, void* a3, int hint); // vtbl+0x80; BoundsCheck at 0x603238E6
@ 0x60328720 int __thiscall RMModel_RenderShadowCaster(void* this, void* imRender, void* shadowCam, void* tex, int a4); // vtbl+0x7C
@ 0x60328A30 int __thiscall RMModel_RenderShadowReceiver(void* this, void* imRender, void* shadowCam, void* tex, int a4); // SetSideClip(1) around it
@ 0x603294C0 void __thiscall RMModel_AddShadowGeometryA(void* this, void* shadowCam, void* a2, void* a3);
@ 0x60329620 void __thiscall RMModel_AddShadowGeometryB(void* this, void* shadowCam, void* a2, void* a3);
@ 0x60313750 void __thiscall RMDeformationModel_Pick(void* this, void* camera, void* a2, void* hitRecord); // vtbl slot 0x6034237C; per-face pick, not the render path
// superseded below (02:14): 0x60313A80 int __thiscall RMDeformationModel_RenderA(void* this, void* imRender, void* camera, void* a3, int hint); // BoundsCheck 0x60313C44
// superseded below (02:14): 0x603147E0 int __thiscall RMDeformationModel_RenderB(void* this, void* imRender, void* camera, void* a3, int hint); // BoundsCheck 0x60314A0D; CPU skin 0x60315870 only when not culled
// superseded below (02:14): 0x603161A0 int __thiscall RMDeformationModel_RenderC(void* this, void* imRender, void* camera, void* a3, int hint); // BoundsCheck 0x6031632B
@ 0x6032B440 void __thiscall RMParticleSystem_Draw(void* this, void* imRender, void* camera, void* a3); // draws each particle model with hint 4 (forced clip path)
@ 0x6032F850 void __thiscall RMProgressiveMesh_SelectLODByArea(void* this, int pixelArea); // area <= 10 -> coarsest; table +0x14C {area, lod}, count +0x150
@ 0x6032F150 void __thiscall RMProgressiveMesh_SetLOD(void* this, int lod);
@ 0x60339330 int __thiscall RMRoom_Render(void* this, void* imRender, void* camera, int renderGlobalRoom); // vtbl+0x2C (0x60342744)
@ 0x60339B60 int __thiscall RMRoom_Pick(void* this, void* imRender, void* camera, int a3);   // vtbl+0x30
@ 0x60339F20 void __thiscall RMRoom_RenderThroughPortals(void* this); // portal list +0x15C/+0x160; recursion via other room vtbl+0x2C
@ 0x6033A060 void __thiscall RMRoom_PickThroughPortals(void* this, int a1);
@ 0x60330710 void __thiscall RMPortal_Project(void* this, void* camera, void* fromRoom, IMVertex* out, int* outN); // facing test then ViewportTransformFace(+0x6CC verts, +0x6D4 count)
@ 0x60330410 int __thiscall RMPortal_AlreadyTraversed(void* this, void* toRoom, int frameStamp); // per side; transform portals count recursion vs +0x6C8
@ 0x603303A0 void __thiscall RMPortal_MarkTraversed(void* this, void* toRoom, int frameStamp);
@ 0x60330910 void __thiscall RMPortal_Enter(void* this, void* camera, void* fromRoom); // transform portals only (+0x6D8): moves camera, sets user clip plane
@ 0x60330D70 void __thiscall RMPortal_Leave(void* this, void* camera, void* fromRoom);
@ 0x60333270 int __thiscall RMPortal_IsEnabledFrom(void* this, void* fromRoom, void* camera); // enable flag (+0x618/+0x6B4) and optional range (+0x620/+0x6BC) via 0x60333750
@ 0x60333750 int __thiscall RMPortal_InEnableRange(void* this, int side, void* camera);
@ 0x6033FF60 int __stdcall RMExt_BoundsCheck(void* ctx, int n, void* box, float cx, float cy, float cz, float r); // in callback table 0x603428C8, forwards to BoundsCheck with ctx+0x28 camera
@ 0x60338E40 void* __thiscall RMRender_CreateShadowGeometry(void* this, void* model, void* shadowCam, void* tex); // exported, imported by Timeline.exe
@ 0x60338D90 void* __thiscall RMRender_GetShadowCamera(void* this);
$ 0x6034D520 IMVertex g_portalProjBuf[16]
$ 0x6034C0F4 unsigned short g_bboxFaceIdx[24]
// ---- caesarim clipping ----
// IMRender: +0x8 clipStack[29] (stride 0x26C: int nEdges; edges[40] stride 0xC at +4; edgeFns at +0x1E4), +0x48B0 int clipTop (-1 none),
//  +0x4D24 clipState, +0x4D40 attribute clipper table, +0x4E00 IMVertex clipOut[~101]
// 2D backface tests: DrawPoly 0x6010CBE2 (cull if area >= 0), DrawPolyNoClip 0x6010CCAC (cull if area > 0), IMDevice_DrawFan 0x60103858 (cull if area > 0).
// DrawPolysNoClip / DrawIndexedPolysNoClip (IMDevice +0x10/+0x14) do no culling and no clipping.
@ 0x6010D580 int __thiscall IMRender_PushClipIntersect(void* this, void* entry, IMVertex* poly, int n);
@ 0x6010D700 void __thiscall IMRender_BuildClipEntry(void* this, void* entry, IMVertex* poly, int n);
@ 0x6010DC30 int __thiscall IMRender_PointsAreVisible(void* this, IMVertex* v, int n, int* flags, unsigned long* a4); // -1 = all outside one edge
@ 0x6010DD90 int __thiscall IMRender_ClipTest(void* this, IMVertex* v, unsigned long n, int* needsClip);
@ 0x6010DD20 int __thiscall IMRender_SetClipState(void* this, int state);
@ 0x6010DDE0 void __thiscall IMRender_ClipAndAppendIndexed(void* this, IMVertex* v, int n, unsigned short* idx, int ni, IMVertex* out, int* outN, int a7, int a8);
@ 0x6010E230 void __thiscall IMRender_ClipAndAppendReverse(void* this, IMVertex* v, int n, int a3, IMVertex* out, int* outN, int a6, int a7);
// ---- sky / background / fog (session 2b) ----
// No engine sky class. The sky is a normal static RMModel named "SkyBox" (scene .oxf object, textures SkyBox<Lvl>_{UP,LF,FR,RT,BK}.bmp),
// world-fixed, drawn in room list order with the z-buffer on. Cloud layers are separate large models (Clds*/CldLyr*/Cloud*).
// RMObject: +0x10 RMRender* (frames/models), +0x24 char name[64] (Get/SetObjectName 0x60329C60/0x60329C70)
// RMModel: +0x1CC int translucent (1 if any texture has RMTexture+0x88 set; such models are depth-sorted and drawn after opaque ones),
//          +0x1D0 RMTexture** textures, +0x22C int textureCount, +0x33C int zwriteOffMode (1/2 -> IM ZWRITEENABLE 0 around draw)
// RMTexture: +0x24 name, +0x70 IMRender*, +0x74 int imTextureHandle (GetCacheID 0x6033C8C0), +0x88 int decal/translucent (IsDecal 0x6033C9B0)
// RMCamera background/backdrop: +0xD80 float fogStart^2, +0xD84 float 255/fogEnd^2, +0xD88 int 255-fogMin, +0xD8C int 255-fogMax,
//   +0xD90 RMTexture* backdrop, +0xDB0/+0xDB1/+0xDB2 byte bgR/G/B, +0xDB4 int fogOn, +0xDB8 int clearZOnly, +0xDBC float
//   backdrop quad = clip quad +0x674 (4 IMVertex, sz = 1.0, rhw = 1.0), UVs in +0x68C/+0x690/+0x6AC/+0x6B0/+0x6CC/+0x6D0/+0x6EC/+0x6F0
// RMRender: +0x48 userSpecular (RM state 0xF), +0x4C fogEnable (state 0x10), +0x50 (state 0xA), +0x58 (0x12), +0x5C (0x17), +0x64 _foginfo
struct _foginfo { float fogStart; float fogEnd; int fogMin; int fogMax; }; // fogMax == 255 -> RenderFrame sets background = fog colour, back clip = fogEnd
// RenderMgr (App+0xBAC): +0x18C/+0x18D/+0x18E byte background B/G/R (used when fog is off or fogMax != 255), +0x130 extra-camera list
enum IMRenderState { // IMRender::SetRenderState(state, value) -> IMDevice vtbl+0x5C 0x601045C0 -> IDirect3DDevice2::SetRenderState
  IMRS_ANTIALIAS=1, IMRS_TEXTUREPERSPECTIVE=2, IMRS_FILLMODE=3, IMRS_ALPHATESTENABLE=4, IMRS_TEXTUREMAG=5, IMRS_TEXTUREMIN=6,
  IMRS_ZFUNC=7, IMRS_ALPHAFUNC=8, IMRS_DITHERENABLE=9, IMRS_ALPHABLENDENABLE=10, IMRS_TEXTUREHANDLE=11, IMRS_INTERNAL12=12,
  IMRS_CULLMODE=13, IMRS_ZWRITEENABLE=14, IMRS_SPECULARENABLE=15, IMRS_FOGENABLE=16, IMRS_FOGCOLOR=17, IMRS_INTERNAL20=20,
  IMRS_SRCBLEND=21, IMRS_DESTBLEND=22 };
@ 0x601045C0 int __thiscall IMDevice_SetRenderState(void* this, int imState, int value); // IMDevice vtbl+0x5C; cached in 0x6012CFD8..0x6012D024
@ 0x6010D650 int __thiscall IMRender_SetRenderState(void* this, int imState, int value); // exported
@ 0x6010E460 void __thiscall IMRender_SetBackgroundColor(void* this, int r, int g, int b); // -> IMDevice vtbl+0xA4 0x60105E50 (viewport background material)
@ 0x60105E50 void __thiscall IMDevice_SetBackgroundColor(void* this, int r, int g, int b);
// IMDevice_SetTexture 0x60103720 (vtbl+0x34): flags bit2 -> TEXTUREMAPBLEND DECAL(1) else MODULATE(2); bit0 -> ADDRESSU MIRROR; bit1 -> ADDRESSV MIRROR
$ 0x6012CFC4 int g_imCurTexture       // IM texture handle currently bound (D3D texture = [[0x6012CFC0]+0x8380+h*4])
$ 0x6012CFDC int g_imTextureEnable     // IM state 11 cache
$ 0x6012D010 int g_imZWriteEnable      // IM state 14 cache (D3DRS_ZWRITEENABLE)
$ 0x6012CFFC int g_imZFunc             // IM state 7 cache
$ 0x6012D018 int g_imFogEnable         // IM state 16 cache (D3DRS_FOGENABLE)
$ 0x6012D01C unsigned int g_imFogColor // IM state 17 cache (D3DRS_FOGCOLOR, 0x00RRGGBB)
@ 0x60306ED0 void __thiscall RMCamera_SetBackgroundColor(void* this, int r, int g, int b); // exported; bytes +0xDB0..+0xDB2
@ 0x60306F00 void __thiscall RMCamera_GetBackgroundColor(void* this, int* r, int* g, int* b);
@ 0x60306F30 void __thiscall RMCamera_SetBackdropTexture(void* this, void* rmTexture); // +0xD90
@ 0x60302470 void* __thiscall RMCamera_GetBackdropTexture(void* this);
@ 0x60306F40 void __thiscall RMCamera_SetBackdropTextureCoordinates(void* this, float* uv8);
@ 0x60302490 void __thiscall RMCamera_SetClearZOnly(void* this, int on); // +0xDB8
@ 0x60337E90 int __thiscall RMRender_SetRenderState(void* this, int state, int value); // exported; 0xF/0x10/0xA/0x12/0x17/0x13 special, rest -> IMRender
@ 0x60337F50 int __thiscall RMRender_GetRenderState(void* this, int state);
@ 0x60338100 void __thiscall RMRender_SetFogInfo(void* this, _foginfo* fi); // copies 16 bytes to +0x64
@ 0x60338120 void __thiscall RMRender_GetFogInfo(void* this, _foginfo* fi);
@ 0x603392B0 float __cdecl RMRoom_ModelDistSq(void* model, float* camPos); // translucent sort key
@ 0x60339310 int __cdecl RMRoom_CompareDistDesc(float* a, float* b);       // qsort comparator: far to near
@ 0x00413BA0 int __thiscall RenderMgr_AddExtraCamera(void* this, ...);       // calls SetClearZOnly(1) at 0x413BEC; called from 0x436AC2
// Timeline 0x477200 area: scene camera object applies back clip, background colour (data+0x15..+0x17) and an optional backdrop texture by name (data+0x18)

// ===== Path B: characters / RMDeformationModel skinning (static, 2026-11-04 02:14) =====
// Characters = RMDeformationModel (vtable 0x60342308, derives RMModel). ONE RMFrame ("joint") per mesh vertex, no weights.
// Mesh vertices hold positions in their joint's local space (EndFramesUpdate bakes world->joint via joint+0x140).
// Skin: out[i].xyz = mesh[i].xyz * frames[i]->+0xEC (4x3 row-vector, with scale). Normals copied UNROTATED.
struct RMTextureGroup { unsigned short* tris; unsigned short* trisAlt; int nTris; }; // 0xC; tris = nTris*3 u16 indices into the (numVerts+numAliased) vertex range; trisAlt used when camera+0x6F4 != 0
// RMVertex refinement: +0x28/+0x2C = tu2/tv2 (overlay texture UVs, used by the mesh+0xE4 overlay pass); +0x30 unused by skin/render
struct RMMesh_Layout { // offsets only (not full size)
  char _00[0x74]; char bbox[0x60];      // +0x74 _boundingbox (GetBoundingBox 0x60308380)
  char _D4[0x4]; RMVertex* verts;       // +0xD8 numVerts+numAliased entries (stride 0x34)
  void* faces;                          // +0xDC GetFaces 0x603083E0
  char _E0[0x4]; void* overlayTexture;  // +0xE4 RMTexture* (GetOverlayTexture 0x60308370)
  char _E8[0x4]; int* aliasList;        // +0xEC alias j -> source vertex index (GetAliasList 0x603083C0)
  int numVerts;                         // +0xF0 unique (skinned) vertices = frames[] length (GetVertexPointer return)
  int numTotalVerts;                    // +0xF4 numVerts+numAliased (alloc size of model+0x344)
  int numFaces;                         // +0xF8 GetNumFaces 0x603083D0
  char _FC[0x8]; int numTexGroupsCap;   // +0x104 bound for GetTextureGroup
  int numAliased;                       // +0x108 GetNumAliased 0x603083B0
  int numGroups;                        // +0x10C GetNumGroups 0x603083A0
  char _110[0x4]; RMTextureGroup* groups; // +0x114
};
struct RMDeformationModel_Layout { // RMModel/RMFrame base + deformation fields (offsets only)
  // +0x14 RMRender-ish ctx ([+0x14]+0x30 = frame stamp), +0xEC model render-world (RMFrame), +0x1C8 render callback
  // +0x1D0 RMTexture** per texture group (GetAllTextures 0x60308320, count +0x22C), +0x1F8 u16* front-face index scratch
  // +0x1FC flags (bit3 -> RenderB if RMRender state 0xB; bit 0x400 -> RenderA 2nd pass pass=1; bit 0x100 -> 0x60307B20 call)
  // +0x200/+0x204 must be non-null to render, +0x20C RMMesh*, +0x2D0 RMProgressiveMesh* (LOD; active vtx count [+0x2D0]+0x144)
  // +0x344 RMVertex* skinned world-space buffer (numTotalVerts), +0x348 RMFrame** frames[numVerts], +0x34C last skin stamp
  int _dummy;
};
// RMFrame additions: +0x11C float[9] normal matrix of world (+0xBC) (identity if degenerate), +0x140 RMMatrix44 inverse(+0xEC),
//   +0x188 static flag, +0x18C scale enable (scale +0x74/+0x78/+0x7C), +0x190 parent, +0x194 next sibling, +0x198 first child,
//   +0x1AC world-update suspended flag, +0x1B4 listener obj, +0x70 world-changed callback fn(frame, stamp)
@ 0x603139D0 int __thiscall RMDeformationModel_Render(void* this, void* imRender, void* camera, void* a3, void* a4); // vtbl+0x70; dispatches RenderB (+0x80) or RenderA (+0x84, pass 0, and pass 1 if flags&0x400); ret 0x10
@ 0x60313A80 int __thiscall RMDeformationModel_RenderA(void* this, void* imRender, void* camera, void* a3, void* a4, int pass); // vtbl+0x84, ret 0x14 (5 args; supersedes 4-arg line above). skin call 0x60313E56
@ 0x603147E0 int __thiscall RMDeformationModel_RenderB(void* this, void* imRender, void* camera, void* a3, void* a4); // vtbl+0x80, ret 0x10. skin 0x60314B23, no-clip xform 0x60314B7E, per-group 0x603221F0 at 0x60314C19, overlay pass 0x60314C35
@ 0x603161A0 int __thiscall RMDeformationModel_RenderShadowCaster(void* this, void* a0, void* camera, void* a2, int a3); // vtbl+0x7C (was RenderC); only caller 0x6033991A = projected-shadow texture pass (culling patch #4 disables)
@ 0x60315AC0 int __thiscall RMDeformationModel_RayTest(void* this, void* a0, void* a1, void* a2); // vtbl+0x78; collision/ray test on GetTransformedVertices (callers 0x60312ADC..)
@ 0x60315870 void __thiscall RMDeformationModel_Skin(void* this, int* aliasList, int numAliased); // CPU skin into +0x344; also called by Pick, Begin/GetTransformed exports
@ 0x603136A0 void __thiscall RMDeformationModel_InitFrames(void* this); // alloc frames[numVerts] (all = this) and +0x344 (numTotalVerts*0x34); from 0x60321DFC
@ 0x60313560 void** __thiscall RMDeformationModel_BeginFramesUpdate(void* this); // exported: skin with current frames, return frames array (+0x348)
@ 0x60313590 void __thiscall RMDeformationModel_EndFramesUpdate(void* this); // exported: mesh[i].xyz = skinned[i].xyz * frames[i]->+0x140 (bake joint-local bind positions; normals untouched)
@ 0x60313660 void** __thiscall RMDeformationModel_GetFramesArray(void* this, int* count); // exported
@ 0x60313510 int __thiscall RMDeformationModel_GetTransformedVertices(void* this, int* outCount, RMVertex** outVerts); // exported: skins, returns numVerts, outCount=numVerts+numAliased
@ 0x6031C8A0 int __thiscall RMMesh_GetVertexPointer(void* this, int* outTotal, RMVertex** outVerts); // exported: *outTotal=+0xF4, *outVerts=+0xD8, returns +0xF0
@ 0x6031D340 RMTextureGroup* __thiscall RMMesh_GetTextureGroup(void* this, int g); // exported
@ 0x60325BE0 void __thiscall RMModel_UpdateLighting(void* this, void* camera, int n, int* aliasList, int numAliased); // CPU vertex lighting into mesh colors, before skin
@ 0x60304980 void __thiscall RMCamera_TransformNoClip(void* this, RMVertex* in, int n, IMVertex* out, int* outN, int* a4, int a5); // 1:1 RMVertex[i] -> IMVertex[i] (index preserving)
@ 0x60307A70 void* __thiscall RMCamera_AllocIMVerts(void* this, int n); // projected IMVertex pool
@ 0x6033C8C0 int __thiscall RMTexture_GetIMHandle(void* this); // returns +0x74 (IM texture handle), reloads via 0x6033C9F0 if lost; +0x70 IMRender*
@ 0x6033C9A0 int __thiscall RMTexture_GetFlags(void* this); // +0x170
@ 0x60317570 void __thiscall RMFrame_UpdateChildren(void* this); // UpdateWorld on each child (+0x198, sibling +0x194): world matrices propagate eagerly
// Timeline.exe component service vtable 0x4B9654 (ctor ref 0x43A83B): +0x540 -> 0x43CF50 BeginFramesUpdate, +0x544 -> 0x43CF60 EndFramesUpdate,
//   +0x548 -> 0x43CF70 GetTransformedVertices, +0x2F0 GetVertexPointer-style, +0x340 get mesh, +0x64 create frame, +0x14 set name, +0x15C set parent, +0x164 SetPosition, +0x168 GetPosition
@ 0x0043CF50 void** __stdcall Svc_BeginFramesUpdate(void* svc, void* defModel);
@ 0x0043CF60 void __stdcall Svc_EndFramesUpdate(void* svc, void* defModel);
@ 0x0043CF70 int __stdcall Svc_GetTransformedVertices(void* svc, void* defModel, int* outCount, RMVertex** outVerts);
// Human.dll (Components, preferred base 0x61500000, size 0x80000, has .reloc; no overlap with other Components -> expected at preferred base, confirm live).
// NOT bootstrapped: entries below are hand-derived from a capstone dump (scratch/human.asm).
// Human object: +0x368 numJoints, +0x370 HumanJoint[numJoints], +0x3D4 service iface (Timeline vtable 0x4B9654), +0xE98 RMDeformationModel*, +0xE9C parent frame
struct HumanJoint { void* frame; char name[0x100]; float pos[3]; char valid; char _pad[7]; }; // 0x118 stride; frame = RMFrame* (bone)
// Human.dll 0x6150DFF0: text skeleton loader ("NumJoints:", "Parent:", "NONE"): creates one RMFrame per joint, parents, positions; Begin/EndFramesUpdate.
// Human.dll 0x6150EAF0: per-vertex joint index file -> frames[v] = joints[idx].frame (BeginFramesUpdate 0x6150EC50 ... EndFramesUpdate 0x6150EDA1)

// ======================================================================
// Session 2b: lights (see findings "## Lights (session 2b)")
// ======================================================================
// RMVertex +0x30 (pad30) is the material index (lighting code switches material when it changes).
// _color = 3 bytes {r,g,b}. _lightAttenuation = 4 floats {fadeInStart a, fullStart b, fullEnd c, range d}, needs b>=a, d>=c.
struct LightAttenuation { float fadeInStart; float fullStart; float fullEnd; float range; };
enum RMLightType { RMLIGHT_POINT = 0, RMLIGHT_SPOT = 1, RMLIGHT_DIRECTIONAL = 2, RMLIGHT_AMBIENT = 3 };
// RMFrame extra fields: +0x08 int id, +0x0C int objectKind (9 = RMLight), +0x10 RMRender*, +0x194 RMFrame* nextSibling,
//  +0x198 RMFrame* firstChild, +0x19C RMContainer* container (room the frame lives in), +0x1B0 int containerLocked
//  World position used by lighting = +0x110..+0x118 (row 3 of +0xEC renderWorld; static frames use +0x80).
//  GetOrientation(world): dir = +0xD4..+0xDC (worldTrans row 2 = local +Z), up = +0xC8..+0xD0 (row 1).
// RMLight (size 0x214, vtable 0x60342418) = RMFrame + :
//  +0x1C8 int enabled (default 1)        +0x1CC int falloffEnable (default 0)   +0x1D0 int type (RMLightType)
//  +0x1D4 uchar rgb[3] (SetColor)        +0x1D8 float colR, +0x1DC colG, +0x1E0 colB = rgb * intensity / 255
//  +0x1E4 float intensity (default 1)    +0x1E8 float umbra (rad, inner half-angle, default 15 deg) +0x1F0 cos(umbra)
//  +0x1EC float penumbra (rad, outer half-angle, default 25 deg) +0x1F4 cos(penumbra)
//  +0x1FC LightAttenuation atten (default {0,0,0,1000}; +0x208 = range = SetFalloffRange)
//  +0x20C float 1/(fullStart-fadeInStart) (1.0 if 0)   +0x210 float 1/(range-fullEnd) (1.0 if 0)
@ 0x60318110 void* __thiscall RMLight_ctor(void* this, void* rmRender, void* container, int type);
@ 0x60336820 void* __thiscall RMRender_CreateLight(void* this, int tag, void* container, int type); // new 0x214, registers in [RMRender+0x18]
@ 0x603182B0 void __thiscall RMLight_SetContainer(void* this, void* container); // vtbl+0x10: RemoveLight(old), AddLight(new)
@ 0x603182F0 void __thiscall RMLight_SetType(void* this, int type);
@ 0x60318300 int __thiscall RMLight_GetType(void* this);
@ 0x60318310 void __thiscall RMLight_SetColorRGB(void* this, int r, int g, int b);
@ 0x603183A0 void __thiscall RMLight_SetColor(void* this, unsigned int rgb3);
@ 0x60318420 void __thiscall RMLight_GetColor(void* this, unsigned char* out3);
@ 0x60318440 void __thiscall RMLight_SetUmbra(void* this, float rad);
@ 0x60318460 float __thiscall RMLight_GetUmbra(void* this);
@ 0x60318470 void __thiscall RMLight_SetPenumbra(void* this, float rad);
@ 0x60318490 float __thiscall RMLight_GetPenumbra(void* this);
@ 0x603184A0 void __thiscall RMLight_SetIntensity(void* this, float i);
@ 0x60318510 float __thiscall RMLight_GetIntensity(void* this);
@ 0x60318520 int __thiscall RMLight_SetEnable(void* this, int enable, int recurse); // vtbl+0x28
@ 0x60318550 int __thiscall RMLight_GetEnable(void* this);                      // vtbl+0x2C
@ 0x6031B630 void __thiscall RMLight_SetFalloffRange(void* this, float range);   // atten.range, recompute inverses (ignored if <= 0)
@ 0x6031B660 float __thiscall RMLight_GetFalloffRange(void* this);
@ 0x6031B670 int __thiscall RMLight_SetFalloffEnable(void* this, int e);
@ 0x6031B690 int __thiscall RMLight_GetFalloffEnable(void* this);
@ 0x6031B6A0 void __thiscall RMLight_SetAttenuation(void* this, LightAttenuation* a);
@ 0x6031B760 void __thiscall RMLight_GetAttenuation(void* this, LightAttenuation* out);
@ 0x6031B7A0 float __thiscall RMLight_Attenuate(void* this, float dist); // trapezoid: 0 below a, ramp a..b, 1 on [b,c), 1-(dist-c)*inv on [c,range], 0 beyond
@ 0x60318560 int __thiscall RMLight_LightModel(void* this, void* camera, void* model, int nVerts); // enabled check, dispatch on type
@ 0x603193E0 int __thiscall RMLight_LightPoint(void* this, void* camera, void* model, int nVerts); // ret 0xC;
@ 0x60319F90 int __thiscall RMLight_LightSpot(void* this, void* camera, void* model, int nVerts); // ret 0xC;
@ 0x6031AC50 int __thiscall RMLight_LightDirectional(void* this, void* camera, void* model, int nVerts); // ret 0xC;
@ 0x6031B4C0 int __thiscall RMLight_LightAmbient(void* this, void* camera, void* model, int nVerts); // ret 0xC; adds col*255 to every vertex
@ 0x60318600 int __thiscall RMLight_RadiosityShoot(void* this);  // called from radiosity build 0x60334C30 (not analyzed)
@ 0x60317050 int __thiscall RMFrame_GetOrientation(void* this, float* dir, float* up, void* relativeTo);
@ 0x603174E0 void __thiscall RMFrame_SetContainer(void* this, void* container); // writes +0x19C
@ 0x603023F0 void* __thiscall RMFrame_GetContainer(void* this);                 // vtbl+0x14, returns +0x19C
@ 0x60317D10 int __thiscall RMFrame_SnapLight(void* this, void* camera, int recurse, int a3);
// RMContainer (base of RMRoom/RMScene): +0x70 RMLight* lights[49], +0x138 int lightCount, +0x13C ObjVec* objects, +0x16C RMRadiositySolution*
// ObjVec (generic): +0 vtbl, +4 void** data, +8 cap, +0xC count, +0x10 end ptr. RMRender+0x18 = ObjVec* of all created objects.
@ 0x603128E0 void __thiscall RMContainer_AddLight(void* this, void* light);
@ 0x60312910 void __thiscall RMContainer_RemoveLight(void* this, void* light);
@ 0x60312960 void** __thiscall RMContainer_GetLightList(void* this, int* outCount); // returns this+0x70, count +0x138
@ 0x603128A0 int __thiscall RMContainer_Contains(void* this, void* obj);
@ 0x603128C0 void* __thiscall RMContainer_GetContainedItem(void* this, int i);
@ 0x60340380 void* __thiscall ObjVec_Get(void* this, int i);
// RMRoom (size 0x1D8): +0x10 RMRender*, +0x158 RMScene*, +0x15C ptr to portal array (data = [[+0x15C]]), +0x160 int portalCount
// RMPortal: side0 +0x590 room / +0x594 other room, side1 +0x62C room / +0x630 other room
// RMScene: +0x1C8 _color ambientLight (stored only; no consumer found in caesarrm)
// RMRender: +0x18 ObjVec* allObjects, +0x38 RMRoom* globalRoom. Models relight when model+0x1F0 != [[model+0x14]+0x30] (object at frame+0x14, probably RMRender; +0x30 = frame stamp)
@ 0x60339BD0 void __thiscall RMRoom_SnapshotLight(void* this, void* camera, int a2); // vtbl+0x34 SnapLight on each contained object
@ 0x6033A180 void __thiscall RMRoom_SnapshotLightRad(void* this, void* camera, void* progressCb, float a3, int a4); // builds radiosity into +0x16C
@ 0x6033A150 void** __thiscall RMRoom_GetPortalList(void* this, int* outCount);
@ 0x603392A0 void* __thiscall RMRoom_GetScene(void* this);
@ 0x60330670 void* __thiscall RMPortal_GetOtherRoom(void* this, void* fromRoom);
@ 0x60338D70 void* __thiscall RMRender_GetGlobalRoom(void* this);
@ 0x6033ACD0 void __thiscall RMScene_SetAmbientLight(void* this, unsigned int rgb3);
@ 0x6033ACF0 void __thiscall RMScene_GetAmbientLight(void* this, unsigned char* out3);
// RMModel lighting fields: +0x1D4 RMMaterial** materials, +0x1D8/+0x1DC uint* cached colour/specular (snapshot mode), +0x1E0 int relightDirty,
//  +0x1F0 int lastLitFrame, +0x1FC int shadeState flags (0x2 specular, 0x8 lightmap path, 0x10 base = prelit colours +0x228,
//  0x40 snapshot/cached lighting, 0x80 dynamic lights, 0x200 extra pass 0x60329270), +0x20C RMMesh*, +0x228 uint* prelitColours
@ 0x60325BE0 void __thiscall RMModel_UpdateLighting(void* this, void* camera, int a2, int a3, int a4); // reset colours, apply every light of GetContainer()
@ 0x60325860 void __thiscall RMModel_ResetVertexColors(void* this); // colour = material +0xA0 (or +0x228 prelit when flag 0x10), specular = 0
@ 0x60325530 int __thiscall RMModel_SnapLight(void* this, void* camera, int recurse, int a3);
@ 0x60321E60 int __thiscall RMModel_SetShadeState(void* this, int flags, int recurse);
@ 0x60321F20 int __thiscall RMModel_GetShadeState(void* this);
@ 0x60327130 void __thiscall RMModel_LightmapLighting(void* this); // mesh+0x120&0x80 and flag 0x8: vertex colours white, uses container lights (not fully analyzed)
@ 0x6031C8A0 int __thiscall RMMesh_GetVertexPointer(void* this, int* outCount, RMVertex** outVerts); // +0xF4 count, +0xD8 verts, returns +0xF0
// RMMaterial: +0x70 uchar ambient[3], +0x73 diffuse[3], +0x76 specular[3], +0x7C float ambientF[3] (=byte*+0xB0),
//  +0x88 float diffuseF[3] (=byte*+0xB4), +0x94 float specularF[3] (=byte*+0xB8), +0xA0 uint packed ambient ARGB (alpha +0xAC),
//  +0xB0/+0xB4/+0xB8 float ambient/diffuse/specular scalars (SetScalars)
@ 0x6031B970 void __thiscall RMMaterial_SetAmbientRGB(void* this, int r, int g, int b);
@ 0x6031BA20 void __thiscall RMMaterial_SetAmbient(void* this, unsigned int rgb3);
@ 0x6031BAC0 void __thiscall RMMaterial_GetAmbient(void* this, unsigned char* out3);
@ 0x6031BB50 void __thiscall RMMaterial_SetDiffuse(void* this, unsigned int rgb3);
@ 0x6031BBC0 void __thiscall RMMaterial_GetDiffuse(void* this, unsigned char* out3);
@ 0x6031BBE0 void __thiscall RMMaterial_SetSpecularRGB(void* this, int r, int g, int b);
@ 0x6031BF00 void __thiscall RMMaterial_GetDiffuseF(void* this, float* out3);
@ 0x6031BF30 void __thiscall RMMaterial_GetSpecularF(void* this, float* out3);
// ---- Timeline.exe light plumbing ----
// Camera room: RenderMgr_GetRenderRoom 0x413A20 returns activeCamera->GetContainer() (= [cam+0x19C]); 0x412580 passes it to RMRender::Render.
// Level light object (class around 0x476160): data ptr at obj+0x20 (LevelLightData); RMLight* at obj+0x14
struct LevelLightData { unsigned short type; char enable; char falloffEnable; float intensity; float umbra; float penumbra; float falloffRange; unsigned char rgb[3]; unsigned char pad; LightAttenuation atten; };
@ 0x00413A20 void* __thiscall RenderMgr_GetRenderRoom(void* this);
@ 0x00412580 void __thiscall RenderMgr_RenderFrame(void* this, char a1);
@ 0x00476160 char __thiscall LevelLight_Create(void* this);
@ 0x00476240 char __thiscall LevelLight_Apply(void* this, void* light);   // data -> RMLight setters (SetAttenuation last)
@ 0x00476330 void __thiscall LevelLight_Read(void* this, void* light);    // RMLight -> data
@ 0x00489900 void* __thiscall LightDesc_CreateLight(void* this, void* rmRender, void* container); // desc: +4 type, +8 pos, +0x14 up, +0x20 dir, +0x2C rgb, +0x30 umbra, +0x34 penumbra, +0x38 intensity, +0x3C enable; falloff on, range 20000
@ 0x0046DC00 char __thiscall SceneObj_ApplyAmbient(void* this, void* scene); // data +0x20 rgb -> RMScene::SetAmbientLight

// ===== Path B: rigid meshes, rooms, textures (static, 2026-11-04 02:20; see findings "Path B: rigid meshes and materials") =====
// RMObject: +0x0C int type (7 RMMesh, 8 RMModel, 0xF RMProgressiveMesh), +0x10 RMRender*
// RMMesh (size 0x134, vtable 0x603424B0) additions to RMMesh_Layout: +0x70 float radius, +0xD4 int borrowedArrays,
//   +0xE0 RMTexture** textures (count +0x104, non-decal first), +0xE8 RMMaterial** (count +0x110), +0x118/+0x11C u16* whole-mesh
//   tris fwd/reversed (only when numTextures==1), +0x120 flags (0x80 lightmap), +0x124 int modCounter (SetVertices/Modified/GotoLOD),
//   +0x12C RMVertex* extra verts (count +0x130; baked by MakeStatic, purpose unknown). Groups: one per texture.
// RMVertex: +0x18 colour (CPU-lit each frame), +0x1C specular (fog alpha), +0x20/+0x24 tu/tv (copied by RM_ProjectVerts), +0x30 int materialIndex
// RMModel (vtable 0x603424F0): +0x1C8 renderCallback, +0x1CC translucent (sorted), +0x1D0 RMTexture** per group (count +0x22C),
//   +0x1D4 RMMaterial** (count +0x230), +0x1F8 u16* front-face scratch, +0x1FC shadeFlags (2 spec, 4 gouraud, 8 overlay, 0x20 wireframe,
//   0x40 cached light, 0x100 fog toggle, 0x400 two-sided = drawn twice), +0x200 enable, +0x204 drawEnable, +0x20C RMMesh*,
//   +0x214 RMMesh*[4] LOD meshes (SetLODMeshes, count +0x2B8; no render reader found), +0x2D0 RMProgressiveMesh* (= +0x20C when type 0xF),
//   +0x2C0/+0x2C4 drawMode blend pair, +0x2DC boundingRadius, +0x334 default mapping mode, +0x338 int* per-group mapping mode,
//   +0x33C drawMode (1 = ZERO,SRCCOLOR; 2 = SRCCOLOR,ONE; ZWRITE off), +0x340 selectable
// RMModel vtable slots: +0x3C MakeStatic, +0x70 Render (entry 0x60342560), +0x74 Pick, +0x7C ShadowCaster, +0x80 overlay path, +0x84 RenderMesh (entry 0x60342574)
struct RMFaceCorner { int vertexIndex; int unk4; int unk8; int uvIndex0; int uvIndex1; }; // 0x14; vertexIndex read as u16
struct RMFace { RMFaceCorner c[3]; int textureIndex; int materialIndex; int unk44; float normal[3]; int enabled; }; // 0x58; +0x44 possibly plane d (unverified)
// RMTexture (size 0x190): +0x70 IMRender*, +0x74 IM slot (-1 unloaded, -2 none), +0x80/+0x84 width/height, +0x88 isDecal,
//   +0x168 numMips, +0x170 texFlags (bit0 U mirror, bit1 V mirror, bit2 DECAL blend), +0x178/+0x17C scroll speed, +0x180/+0x184 scroll offset
// RMMaterial: +0xA4 specularStrength, +0xA8 float opacity, +0xAC int opacity byte
// RMRoom: +0x13C object list, +0x140 model list (ObjVec: data +4, count +0xC, get 0x60340380), +0x144 traversal obj, +0x148 visible set
//   (base +4, stride +0xC, count +0x10, get 0x603408F0; entry {RMModel*, hint}), +0x150 IMRender*, +0x154 camera, +0x158 RMScene*,
//   +0x178/+0x17C/+0x180 translucent sort buf {float d2; RMModel*; hint}, +0x184 RMModel* shadowCasters[20]
// IM engine render-state enum -> DX5: 1 ANTIALIAS 2 TEXPERSPECTIVE 3 FILLMODE 4 ALPHATEST 5 TEXMAG 6 TEXMIN 7 ZFUNC 8 ALPHAFUNC 9 DITHER
//   10 ALPHABLEND 11 texturing(engine) 13 CULLMODE 14 ZWRITE 15 SPECULAR 16 FOG 17 FOGCOLOR 21 SRCBLEND 22 DESTBLEND (Timeline.exe sets 10=1 globally)
// Corrections: RMModel_Render 0x60322330 args are (imRender, scene, camera, hint); RMModel_RenderMesh 0x603223E0 args are
//   (imRender, scene, camera, hint, reverseWinding), ret 0x14; 0x603236A0 is the overlay pass (flag 0x8 + RMRender state 0xB).
@ 0x603368E0 void* __thiscall RMRender_CreateMesh(void* this, long id, int numVerts, int numTotalVerts, int numFaces, int numTextures, int numMaterials, RMVertex* verts, RMFace* faces, int* aliasList, void** textures, void* overlayTex, void** materials, int borrowArrays);
@ 0x603369E0 void* __thiscall RMRender_CreateMeshUV(void* this, long id, int a1, int a2, int a3, int a4, int a5, int a6, RMVertex* verts, RMFace* faces, void* uv0, void* uv1, void** textures, void* overlayTex, void** materials, int flags, float f);
@ 0x6031C000 void* __thiscall RMMesh_ctor(void* this, void* rmRender, long id, int numVerts, int numTotalVerts, int numFaces, int numTextures, int numMaterials, RMVertex* verts, RMFace* faces, int* aliasList, void** textures, void* overlayTex, void** materials, int borrowArrays);
@ 0x6031C4E0 void* __thiscall RMMesh_ctorUV(void* this, void* rmRender, long id, int a3, int a4, int a5, int a6, int a7, RMVertex* v, RMFace* f, void* uv0, void* uv1, void** tex, void* overlay, void** mats, int flags, float f2);
@ 0x6031D000 void __thiscall RMMesh_BuildTextureGroups(void* this, int sortTextures);
@ 0x6031CD90 void __thiscall RMMesh_ComputeBounds(void* this);
@ 0x6031C8D0 void __thiscall RMMesh_Modified(void* this, int flags);   // ++ +0x124; bit0 rebuild groups, bit1 bounds
@ 0x6031C810 void __thiscall RMMesh_SetVertices(void* this, int first, int count, RMVertex* src);
@ 0x6031C860 void __thiscall RMMesh_GetVertices(void* this, int first, int count, RMVertex* dst);
@ 0x6031C970 void __thiscall RMMesh_SetTexture(void* this, void* tex, int index);
@ 0x6031C9D0 void* __thiscall RMMesh_GetTexture(void* this, int index);
@ 0x603221F0 int __thiscall RMModel_DrawGroupNoClip(void* this, void* imRender, IMVertex* proj, unsigned short* groupTris, int nTris, int nVerts, int flags, int imTex, int texFlags, int* io); // ret 0x24; supersedes the 0x603221F0 arg order above (verified from call site 0x60322A12 and callee). Path B substitution hook; shared with RMDeformationModel
@ 0x60321A70 void __thiscall RMModel_SetMesh(void* this, void* mesh);   // +0x2D0 = mesh if type 0xF
@ 0x60326E70 void __thiscall RMModel_MakeStatic(void* this);           // bakes +0xEC into mesh verts/normals/face normals, +0x188 = 1
@ 0x603257D0 void __thiscall RMModel_UpdateTranslucency(void* this);   // +0x1CC
@ 0x60325E50 void __thiscall RMModel_SetDrawModeBlend(void* this, void* imRender); // SRC/DESTBLEND = +0x2C0/+0x2C4
@ 0x60325E80 void __thiscall RMModel_RestoreDefaultBlend(void* this, void* imRender); // SRCALPHA/INVSRCALPHA
@ 0x60325670 void __thiscall RMModel_SetTexture(void* this, void* tex, int index);
@ 0x603256F0 void __thiscall RMModel_SetMaterial(void* this, void* mat, int index);
@ 0x60325AF0 void __thiscall RMModel_SetLODMeshes(void* this, void** meshes, int count); // max 4 -> +0x214, +0x2B8
@ 0x603270B0 void __thiscall RMModel_SetTextureMappingMode(void* this, int group, int mode); // group -1 -> +0x334 default
@ 0x60329470 void __thiscall RMModel_SetDrawMode(void* this, int mode, int recurse); // +0x33C (0..2)
@ 0x60328060 void __thiscall RMModel_MapCylindrical(void* this, int group); // mapping mode 1; writes RMVertex +0x28/+0x2C
@ 0x603281E0 void __thiscall RMModel_MapMode2(void* this, int group, void* camera);
@ 0x60328410 void __thiscall RMModel_MapMode4(void* this, int group, void* camera);
@ 0x60339C20 void __thiscall RMRoom_Optimize(void* this);              // MakeStatic on top-level models in +0x140
@ 0x6032ED10 int __thiscall RMProgressiveMesh_NextLOD(void* this);
@ 0x6032EF30 int __thiscall RMProgressiveMesh_PrevLOD(void* this);
@ 0x6033C9C0 void __thiscall RMTexture_GetSize(void* this, int* w, int* h);
@ 0x6033DCA0 void __thiscall RMTexture_SetShift(void* this, float du, float dv);
@ 0x6033DD90 int __thiscall RMTexture_GetShiftOffset(void* this, float* du, float* dv); // +0x180/+0x184
@ 0x60336CA0 void* __thiscall RMRender_CreateTexture(void* this, long id, void* imTexInfo);
@ 0x6010C970 int __thiscall IMRender_SetCurrentTexture(void* this, int slot, int texFlags); // exported -> IMDevice vtbl+0x34
@ 0x6010C900 int __thiscall IMRender_AddTexture(void* this, void* imTexInfo, int a2);      // -> IMDevice vtbl+0x28 (0x60103510)
@ 0x6010C940 int __thiscall IMRender_IsTextureLoaded(void* this, int slot);
// 0x6033C8F0 is RMTexture::Use (exported), not a thunk (bootstrap label above is wrong)

// ======================================================================
// Session 2b: remaining culling (Greg quadtree, room traversal, visibility flags)
// ======================================================================
// RMRender+0x48 = render context (RMObject+0x14 points here): ctx+0x2C (RMRender+0x74) = scene stamp (++ in EndRenderScene),
//  ctx+0x34 (RMRender+0x7C) = Greg (quadtree) enable, ctx+0x10 (RMRender+0x58) = render state 0x12 (gates transform-portal recursion)
// RMObject: +0xC type (8 = model), +0x10 RMRender*, +0x14 RMRender+0x48 ctx. RMModel +0x200 enable, +0x204 drawEnable, +0x1C8 renderCallback (veto), +0x2D0 progressive mesh
// RMRoom: +0x13C/+0x140 object lists, +0x144 room manager (vtable 0x6034286C), +0x148 visible list {obj, hint}, +0x150 IMRender*, +0x154 RMCamera*, +0x15C -> RMPortal*[], +0x160 portal count
// RMPortal: +0x590/+0x62C side rooms (project), +0x594/+0x630 side rooms (traversal), +0x614/+0x6B0 traversed stamps, +0x618/+0x6B4 side enable,
//  +0x620/+0x6BC enable range, +0x6C8 recursion depth, +0x6CC verts, +0x6D4 nverts, +0x6D8 transform portal
@ 0x60338810 int __thiscall RMRender_SetGregEnable(void* this, int enable); // exported; RMRender+0x7C; enables per-room quadtree culling
@ 0x60338890 void* __thiscall RMRender_CreateRoomManager(void* this, void* room); // [this+0x34]->vtbl+0xC
@ 0x6033DF90 void* __thiscall RoomMgr_ctor(void* this, int a1, int a2);   // vtable 0x6034286C; +0x28 camera, +0x30 entries, +0x38 quadtree root (0x6033EAE0), +0x3C external override, +0x48 dirty
@ 0x6033E680 void __stdcall RoomMgr_BeginFrame(void* mgr, void* camera);  // vtbl+0x30
@ 0x6033E6D0 void __stdcall RoomMgr_CollectVisible(void* mgr, void* outList); // vtbl+0x34; entries hint 3 + quadtree
@ 0x6033F270 void __thiscall QuadNode_CollectVisible(void* this, void* outList, int parentHint); // vtable 0x60342928 +0x2C; node test 0x6033F2EC, skip-if-0 branch 0x6033F2B7
@ 0x6033EAE0 void* __thiscall QuadNode_ctor(void* this, void* mgr, int a2);
@ 0x6033FE70 void* __stdcall RoomMgr_AcceptObject(void* mgr, void* obj); // vtbl+0x50: only type-8 models with a mesh
@ 0x603306A0 int __thiscall RMPortal_EnableRoom(void* this, int sideMask, int enable); // exported; +0x618 / +0x6B4
@ 0x603306E0 int __thiscall RMPortal_GetRoomEnable(void* this, int sideMask);
@ 0x603336F0 void __thiscall RMPortal_SetEnableRange(void* this, int side, float range);
@ 0x603254B0 int __thiscall RMModel_SetDrawEnable(void* this, int enable, int recurse); // +0x204
@ 0x603254F0 int __thiscall RMModel_SetEnable(void* this, int enable, int recurse);     // +0x200
@ 0x60317AC0 int __thiscall RMFrame_SetDrawEnable(void* this, int enable, int recurse);
@ 0x60317A00 int __thiscall RMFrame_SetEnable(void* this, int enable, int recurse);
@ 0x60325660 void __thiscall RMModel_SetRenderCallback(void* this, void* fn);  // +0x1C8, fn(model, int* draw, int clipped)
@ 0x6010D480 int __thiscall IMRender_PushClip(void* this, IMVertex* poly, int n, int flag, int replace); // fails (0) at depth >= 0x1D
@ 0x6010D560 int __thiscall IMRender_PopClip(void* this);
@ 0x0043BB00 int __stdcall EngineIface_BoundsCheck(void* self, void* camera, int n); // vtable 0x4B9654 +0x25C, forwards to RMCamera::BoundsCheck (box/centre/radius by value follow)
@ 0x0043C760 int __stdcall EngineIface_PortalEnableRoom(void* self, void* portal, int side, char enable);
