# Verified reverse-engineered addresses — lookup table per feature

*[Tiếng Việt (original)](06-dia-chi.md) · English*

This file collects **every address the plugin actually uses**, together with **how it was
verified** and **how to re-derive it** if Valve updates the game. The point is that someone
else can reuse and re-check this work without reading 2600 lines of source.

> **Project rule:** anything that could not be verified is written as **NOT DETERMINED**,
> never guessed. Three evidence tiers are used throughout:
> **🟡 READ** (docs/SDK — hypothesis only) ·
> **🟠 BINARY-VERIFIED** (disassembled, with an address) ·
> **🟢 MEASURED** (observed on a live server).

---

## 0. Reference binaries

**Verified game version: `2.2.4.3` build `10097`** (Left 4 Dead 2 Dedicated Server).

All RVAs below are relative to **ImageBase `0x10000000`**. Add the real runtime base.

| file | size | md5 |
|---|---|---|
| `server.dll` | 9,130,288 | `533888fbb4e5ed534b172470613a3017` |
| `engine.dll` | 4,817,712 | `a16cd381409bab749909d5000c2302d8` |
| `client.dll` | 8,305,664 | `21565d29a23caeabe3d0ffc6156c3e5c` |

Taken from the **dedicated server** tree:
```
<Steam>/Left 4 Dead 2 Dedicated Server/left4dead2/bin/server.dll
<Steam>/Left 4 Dead 2 Dedicated Server/bin/engine.dll
<Steam>/Left 4 Dead 2 Dedicated Server/left4dead2/bin/client.dll
```

🔑 The dedicated server's `client.dll` is **byte-identical** to the retail game's — either
copy answers client-side questions the same way.

> **If your md5 differs:** do not use the RVA table. Read section 6 — re-derive by content
> anchors.

---

## 1. `noedict` — deny edicts to classes that do not use networking

Mechanism: set **bit 9 of `m_iEFlags`** (`EFL_SERVER_ONLY`) **before**
`CBaseEntity::PostConstructor` runs. The engine then routes to
`AddNonNetworkableEntity` (range 2049–4095) instead of `AddNetworkableEntity` (0–2047).

| item | value | kind | tier |
|---|---|---|---|
| `m_iEFlags` | `[this + 0x138]` | field offset | 🟠 |
| `EFL_SERVER_ONLY` | `1 << 9` | constant | 🟡 SDK |
| `CBaseEntity::PostConstructor` | **vtable slot 29** · RVA `0x055620` | virtual | 🟠 (majority vote over **539 classes**) |
| `CBaseEntity::GetServerClass` | **vtable slot 9** | virtual | 🟠 |
| `DT_BaseEntity` (ServerClass) | RVA `0x7D78A8` | data | 🟠 (**229 classes** return it) |
| `CEntityFactoryDictionary::FindFactory` | **vtable slot 3** | virtual | 🟠 |
| `IEntityFactory::Create` | **vtable slot 0** | virtual | 🟠 |
| `UpdateTransmitState` | **vtable slot 21** | virtual | 🟠 |
| `ObjectCaps` | **vtable slot 40** · bit `0x2` = `FCAP_ACROSS_TRANSITION` | virtual | 🟠 |

**How the plugin finds a class vtable** (`ResolveClassVtable`, `sample_mm.cpp:1585`):
`GetEntityFactoryDictionary` (RVA `0x20CA70`) → `FindFactory(classname)` → `Create` → scan
the body for the instruction that stores the vtable pointer. If `Create` is only a wrapper,
follow its `call rel32` and scan the callee.

> ⚠️ **Patching is by VTABLE, not by CLASSNAME.** **20 groups** of classnames share a
> vtable. Enabling one name can drag in the whole group. Known example:
> `info_teleport_destination` shares a vtable with `info_player_start` and `info_landmark`.
>
> ⚠️ **8 classes resolve to the wrong vtable** in the current build:
> `point_commentary_viewpoint`, `env_soundscape_proxy`, `env_soundscape_triggerable`,
> `prop_vehicle_driveable`, `player`, `weapon_first_aid_kit`, `weapon_defibrillator`,
> `env_fire_trail`. **Do not add them to `noedict.txt`.**

### Per-class evidence

| class | key evidence | tier |
|---|---|---|
| `infodecal` | `StaticDecal()` = `IVEngineServer + 0xA4`; the argument is the index of the **surface** being decalled, not the entity's own | 🟢 |
| `light`, `light_spot` | `LightStyle(style, pattern)` = `IVEngineServer + 0xA0`, carries **no entity index at all** (6 call sites) | 🟢 |
| `path_track` | `SetSolid(SOLID_NONE)`, no model, nothing references it through a SendProp | 🟢 |
| `func_areaportal` | `UpdateTransmitState` slot 21 = RVA `0x0DA8F0`, **3 instructions**: `push 0x10` (`FL_EDICT_DONTSEND`) ; `call SetTransmitState` ; `ret` → **unconditional**. `Spawn` `0x0DA8A0` never calls `SetSolid`. The engine calls `SetAreaPortalState` (vtable `+0xF4`) with `m_portalNumber [+0x42C]` and `m_state [+0x438]` = **two integers** | 🟢 |
| `info_zombie_spawn` | 0/86 carry `model` ⇒ `GetModelIndex()` returns 0 ⇒ `je 0x056A84` = DONTSEND. The Director locates it via `FindEntityByClassname` RVA `0x0B47F0`, which walks `CEntInfo` (`[esi+0x10004]`), compares `[esi+0x74]` = `m_iClassname`, and **never reads `[this+0x28]`** ⇒ an edict-less entity **is still findable** | 🟢 |
| `func_nav_blocker` | **CURRENTLY DISABLED.** `Spawn` `0x48C80E`: `push 0x20` (`EF_NODRAW`) ; `call AddEffects`. The consumer at `0x48BD58` walks its **own table** `0x7C31A4` by direct pointer, reading `[esi+0x42C]/[+0x438]/[+0x444]`, **never using the spatial partition** | 🟠 |

> ⚠️ **False positive actually hit:** `func_nav_blocker` reads `[esi+0x28]` **six times**,
> but `esi` is a **nav grid struct**, not `this`. `[reg+0x28]` is **not** evidence of edict
> access.

---

## 2. `swap` — retype an entity to a cheaper class at creation

| item | value | tier |
|---|---|---|
| `GetEntityFactoryDictionary` | RVA `0x20CA70` | 🟠 |
| `CEntityFactoryDictionary::Create` | **vtable slot 1** | 🟢 |

Hook slot 1 and rewrite the classname at creation time. This sits **beneath every entity
creation path** — lump parsing, runtime `CreateEntityByName`, and VScript.

**The only viable pair:** `point_spotlight` → `beam_spotlight`.

Reason: `CPointSpotlight::SpotlightCreate` (RVA `0x18E5xx`) spawns an extra `spotlight_end`
+ `beam` ⇒ **1 lump line = 3 edicts**. `beam_spotlight` draws entirely client-side ⇒
**1 edict**.

```
🟢 measured: live 1954 -> 1330   exactly -624   headroom 93 -> 718 slots
```

Cost: `client.dll` hardcodes `HaloScale = 60.0`, so the halo is 6× larger.

> A sweep of **557 classes** in `server.dll` against **16 maps** across three campaigns
> produced **exactly one** such pair. See [07-het-huong.en.md](07-het-huong.en.md).

---

## 3. `freegate` — drop the 1-second edict reuse delay

| item | value | tier |
|---|---|---|
| patch site (mode 2) | `engine.dll` RVA `0x1E022A` — `jae` → `jmp` (**one byte**) | 🟢 |
| **`IVEngineServer` vtable** | `0x1037D9BC` | 🟠 |
| **slot 22** `CreateEdict` | → `ED_Alloc` `0x101E0170` | 🟠 |
| **slot 23** `RemoveEdict` | `0x10130B50` → `ED_Free` `0x101DFF60` — **the ONLY door** (1 `call rel32`, 0 data references) | 🟠 |
| **slot 95** `AllowImmediateEdictReuse` | thunk `0x101311C0` → `0x101DFF10` — sets `freetime = 0.0` for **every currently-free edict** | 🟠 |
| what `ED_Free` does | `or [edict],2` (`FL_EDICT_FREE`) · `freetime[i] = sv.GetTime()` · `serial++` | 🟠 |
| `freetime` table | `engine + 0x6B3A58` — the plugin **derives it from the code** (`D9 1C B5 <imm32>` inside `ED_Free`), never hardcoded | 🟠 |
| edict array | `[engine + 0x645774]` — derived from `A1 <imm32>` inside `ED_Free` | 🟠 |
| clear the whole `freetime` table | `0x101DFEF0` — `memset(table, 0, 0x2000)`, runs at level load | 🟠 |
| anchor signature | `D9 E8 D9 C9 DF F1 DD D8 73` (`fld1 ; fxch st(1) ; fcompi st(1) ; fstp st(0) ; jae`) | 🟠 |
| "just freed" table | `sv + 0x104` (`sv + 0x180` exists but is **unused**) | 🟠 |
| table clear size | constant `0x2000` hardcoded in the clearing function | 🟠 |

The plugin does **not** use a hardcoded RVA — it scans for the signature. The RVA is
recorded only for cross-checking.

> ⚠️ **`freegate` is the LEAST-VALIDATED of the four mechanisms — it has NOT been fully
> tested or signed off.** It has never been exercised on a busy server over a long uptime,
> and the original A/B measurement that justified it predates `wipeclear` in its current
> form. The item-transfer bug of mode `2`, by contrast, **is verified** — traced end to end
> through `RemoveEdict` → `ED_Free` → `ED_Alloc` by disassembly.
>
> There is also an unproven suspicion that it causes system congestion over long uptimes and
> skews entity counts on servers with ≥ 4 players. The package ships **`freegate=0`** — if you enable it, use `1`
> (denylist mode); if your server is busy and the tickrate looks wrong, **set it to 0 first**.

---

## 4. `wipeclear` — clean up entities at the start of the respawn round

| item | value | tier |
|---|---|---|
| `CTerrorGameRules::RestartRound` | **vtable slot 178** · RVA `0x2E0650` | 🟢 |
| `g_pGameRules` | RVA `0x7F7F6C` | 🟠 |
| `gEntList` | RVA `0x7E0760` | 🟠 |
| `g_fInCleanupDelete` | RVA `0x7E0730` | 🟠 |
| `CleanupDeleteList` | RVA `0x0B5D10` (`__cdecl`) | 🟠 |
| `NextEnt` | RVA `0x0B4270` (`__cdecl`) | 🟠 |
| `UTIL_Remove` | RVA `0x2071E0` (`__cdecl`) | 🟠 |
| `s_PreserveEnts` | RVA `0x7ACE40` — array of **38 entries**, `[0]` = `ai_network`, `[33]` = `predicted_viewmodel` | 🟠 |

**Two safety gates** before hooking: the prologue must match, **and** slot 178 must currently
point at that exact function. No match ⇒ **skip, do not patch**.

---

## 5. `mapclear` — clean up at level transition

| item | value | tier |
|---|---|---|
| `CServerGameDLL::PrepChangelevel` | **vtable slot 38** · RVA `0x2B8140` | 🟢 |
| string anchor | `"Preparing player entities for changelevel"` — `server.dll` `0x687718`, **1 xref** | 🟠 |
| `ObjectCaps` | vtable slot 40, bit `0x2` = `FCAP_ACROSS_TRANSITION` | 🟠 |

> ⚠️ **The most expensive lesson in the project:** deleting an entity that **carries over**
> at a level transition **crashes the server**. The "delete fewer things" rule that everyone
> reaches first is the **wrong rule**. Read [02-mapclear.en.md](02-mapclear.en.md) before
> enabling this.

---

## 6. Other anchors + how to re-derive after a Valve update

When RVAs die, **content** and **structure** survive. The script `tools/doi-offset.py` (in
the development repo) re-derives the main anchors with **zero hardcoded RVAs**:

| anchor | how | result on this build |
|---|---|---|
| `s_PreserveEnts` | find the string `"ai_network"`, walk every xref, accept the array whose `[33]` is `"predicted_viewmodel"` | `0x107ACE40`, 38 entries |
| `PostConstructor` | **majority vote** on slot 29 across all entity vtables | `0x10055620` (539 classes) |
| `DT_BaseEntity` | slot 9 whose body is `mov eax,imm32 ; ret`; most common `imm` | `0x107D78A8` (229 classes) |
| `PrepChangelevel` | string `"Preparing player entities for changelevel"` | `server.dll 0x10687718`, 1 xref |
| `CreateEntityByName` | string `"CreateEntityByName( %s, %d ) - CreateEdict failed."` | `server.dll 0x10619988`, 1 xref |
| `ED_Alloc` | string `"ED_Alloc: no free edicts"` | **`engine.dll`** `0x10395800`, 1 xref |
| server temp-entity ceiling | string `"sv_multiplayer_maxtempentities"` | `engine.dll 0x10357998` |
| client temp-entity pool (500 slots) | string `"Overflow %d temporary ents!"` | **`client.dll`** `0x1059B01C` |

**Still manual:** `RestartRound` (take the `g_pGameRules` vtable, read slot 178, check the
prologue) and the three `__cdecl` helpers with no strings of their own (`CleanupDeleteList`,
`NextEnt`, `UTIL_Remove` — follow the **call sites** from a function already located).

### ⚠️ Two traps actually hit — don't repeat them

1. **Which module holds the string.** `"ED_Alloc: no free edicts"` lives in **`engine.dll`**,
   not `server.dll`. Searching the wrong module returns zero hits and reads as "it's gone".
2. **Data arrays are not in `.text`.** `s_PreserveEnts` is in `.rdata`. An xref scan limited
   to `.text` will **never** find it.

### ⚠️ Third trap: how virtual calls are encoded

In this binary, engine/virtual calls are emitted as:

```asm
mov  edx, [eax+0x4C]
call edx
```

**not** `call [reg+disp]` and **not** `call rel32`.

⇒ Scanning for `E8` (call rel32) or `FF /2` returns **zero hits** and leads to the false
conclusion *"nothing calls this function"*. Hit **twice**: once on `SetOwnerEntity`, once on
`LightStyle`. A correct scanner tracks `mov reg,[reg+disp]` and then looks for `call reg`
immediately after.

---

## 7. `client.dll` addresses (used to falsify hypotheses)

| item | RVA | what it proves | tier |
|---|---|---|---|
| `C_Sprite` constructor | `0x19D6C0` | **zero** absolute-address stores ⇒ no global registration ⇒ the client does **not** independently retain `env_sprite` | 🟠 |
| `CSprite::Spawn` (server) | `0x1E0140` | only calls `UTIL_Remove`, never touches `g_pEngineServer` | 🟠 |
| `CSprite::Activate` (server) | `0x065CD0` | only `FindEntityByName` + 2 internal calls | 🟠 |
| client temp-entity pool | 500 slots (`"Overflow %d temporary ents!"`) | refutes converting `env_sprite` to temp entities (would need 639–730 slots) | 🟠 |

---

## 8. The three "asset vs. instance" doors

The principle derived from this work: **state survives entity deletion only if it is written
into a store that lives OUTSIDE the entity.** L4D2 has **exactly three** such doors:

| door | `IVEngineServer` offset | call sites | status |
|---|---|---|---|
| `LightStyle` | `+0xA0` | 6 | ✅ exploited (`light`, `light_spot`) |
| `StaticDecal` | `+0xA4` | 1 | ✅ exploited (`infodecal`) |
| `EmitAmbientSound` | `+0x70` | 3 | ❌ **forbidden** — first argument is the entity's own `entindex` (violates condition 3) |

There is no fourth door. This is why `noedict` **has run out of room to grow** — see
[07-het-huong.en.md](07-het-huong.en.md).

---

## 9. How to re-check it yourself

The plugin **self-checks at runtime** and writes to `edictbudget.log`. Reading the log is the
fastest way to tell whether the addresses still hold:

```
[EdictBudget] NOEDICT: 'func_areaportal' vtable=64C48B8C slot29 -> thunk (OK)
[EdictBudget] MAPCLEAR: da moc vtable slot 38 (0x102B8140 @ ...)
[EdictBudget] WIPECLEAR: da moc vtable slot 178 (RestartRound @ ...)
[EdictBudget] SWAP: da moc dictionary slot 1 (Create) @...
```

The line **`slot29 -> thunk (OK)`** must appear **once per patched vtable**, and the total at
the end must match the number of enabled classes.

> ⚠️ **A silent failure that actually happened:** an older DLL that could not resolve a
> vtable logged *"vtable not found, SKIPPING"* and then **carried on as normal**. For **six
> sessions** `info_zombie_spawn` was never enabled and nobody noticed. Fixed in
> `ResolveClassVtable` (`sample_mm.cpp:1585`) — if the `call rel32` path yields nothing,
> **keep trying** instead of returning `NULL` immediately. **Always cross-check the total at
> the end of the log.**
