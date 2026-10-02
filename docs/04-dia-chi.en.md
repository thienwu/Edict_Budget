# Addresses

*[Tiếng Việt](04-dia-chi.md) · English*

The addresses the plugin uses, for checking them or re-deriving them after a game update.

---

## Reference binaries

Left 4 Dead 2 Dedicated Server `2.2.4.3`, build `10097`. RVAs assume ImageBase `0x10000000`;
at runtime add the module's real base.

| file | size | md5 |
|---|---|---|
| `left4dead2/bin/server.dll` | 9 130 288 | `533888fbb4e5ed534b172470613a3017` |
| `bin/engine.dll` | 4 817 712 | `a16cd381409bab749909d5000c2302d8` |
| `left4dead2/bin/client.dll` | 8 305 664 | `21565d29a23caeabe3d0ffc6156c3e5c` |

If your md5 differs, don't use the RVAs in these tables — see **Re-deriving after an update**.

"vtable slot N" = the N-th virtual function (offset `N × 4`).

---

## noedict (`server.dll`)

| item | address |
|---|---|
| `m_iEFlags` | `[this+0x138]`, `EFL_SERVER_ONLY` = bit 9 |
| `CBaseEntity::PostConstructor` | vtable slot 29, RVA `0x55620` |
| `GetServerClass` | vtable slot 9, body `mov eax, imm32 ; ret` |
| ServerClass of `DT_BaseEntity` | RVA `0x7D78A8` |
| `UpdateTransmitState` | vtable slot 21 (default RVA `0x56A40`) |
| `ObjectCaps` | vtable slot 40, bit `0x2` = `FCAP_ACROSS_TRANSITION` |
| `GetEntityFactoryDictionary` | RVA `0x20CA70` |
| dictionary `FindFactory` / `Create` | vtable slot 3 / slot 1 |
| factory `Create` | vtable slot 0 |
| `CreateEntityByName(name, forceIndex)` | RVA `0x1196B0` |
| entity list (`gEntList`) | RVA `0x7E0760` |
| `CBaseEntityList` constructor | RVA `0xB7890` — 4096 slots; slots 2049–4095 go on the non-networked list |
| `AddNonNetworkableEntity` | RVA `0xB7BD0` — when full: `Warning(...no free slots!)`, returns handle `0xFFFFFFFF` |
| `RemoveEntity` (entity table) | RVA `0xB7970` — slots ≥ 2048 go back to the non-networked list |
| `FindEntityByClassname` | RVA `0xB47F0` — does **not** look at the edict |
| `FindEntityByClassnameNearest` | RVA `0xB5370` — skips entities with `[ent+0x28] == 0` (no edict) |

How the plugin finds a class's vtable: `GetEntityFactoryDictionary` → `FindFactory(name)` →
`Create` → look for `mov dword ptr [reg], imm32` (storing the vtable pointer). If not found,
follow the `call rel32` instructions in the first 0x30 bytes of `Create`. Scanning stops at two
consecutive `0xCC` bytes (`v2.0.1`); every derived pointer must lie inside `server.dll` (`v2.0.2`).

---

## swap (`server.dll`)

| item | address |
|---|---|
| dictionary `Create` | vtable slot 1 of the object returned by `GetEntityFactoryDictionary` |

The whole `.text` has only 3 call sites of slot 1: `CreateEntityByName` and 2 branches of the
entity lump parser.

---

## wipeclear (`server.dll`)

| item | address |
|---|---|
| `CTerrorGameRules::RestartRound` | vtable slot 178, RVA `0x2E0650` |
| `g_pGameRules` | RVA `0x7F7F6C` |
| `UTIL_Remove` | RVA `0x2071E0` |
| `CleanupDeleteList` | RVA `0xB5D10` |
| `NextEnt` | RVA `0xB4270` |
| `g_fInCleanupDelete` | RVA `0x7E0730` |
| the game's preserve list | RVA `0x7ACE40`, 38 entries, `[0]` = `ai_network`, `[33]` = `predicted_viewmodel` |
| `IVEngineServer::AllowImmediateEdictReuse` | vtable slot 95 |

Before hooking, the plugin checks the start of the function and that slot 178 still points at it.

---

## mapclear (`server.dll`)

| item | address |
|---|---|
| `PrepChangelevel` | vtable slot 38, RVA `0x2B8140` |
| anchor string | `"Preparing player entities for changelevel"` (1 reference) |

The start of this function contains an absolute address; matching must skip those 4 bytes.

---

## freegate, trap (`engine.dll`)

| item | address |
|---|---|
| `ED_Alloc` | RVA `0x1E0170` (`IVEngineServer::CreateEdict`, slot 22) |
| `ED_Free` | RVA `0x1DFF60` (`IVEngineServer::RemoveEdict`, slot 23, the only entry point) |
| `freetime` table | `engine + 0x6B3A58` (the plugin derives it from the machine code of `ED_Free`) |
| `freegate=2` patch point | RVA `0x1E022A`, `jae` → `jmp` (located by signature `D9 E8 D9 C9 DF F1 DD D8 73`) |
| `ED_Alloc` error branch (`trap`) | RVA `0x1E0247` `test ebx,ebx` / `0x1E0249` `js` |

---

## Re-deriving after an update

| item | how to find it |
|---|---|
| `PostConstructor` | the function that appears most often in slot 29 of entity vtables |
| `DT_BaseEntity` | the most common `imm32` in slot-9 bodies (`B8 imm32 C3`) |
| preserve list | string `"ai_network"`; the array whose `[33]` is `"predicted_viewmodel"` |
| `PrepChangelevel` | string `"Preparing player entities for changelevel"` |
| `CreateEntityByName` | string `"CreateEntityByName( %s, %d ) - CreateEdict failed."` |
| `ED_Alloc` | string `"ED_Alloc: no free edicts"` — it is in **`engine.dll`** |

`RestartRound` and the `__cdecl` functions without their own strings (`CleanupDeleteList`,
`NextEnt`, `UTIL_Remove`) have to be traced from their call sites in functions already found.

Three common mistakes when searching:

1. Looking for a string in the wrong module (`"ED_Alloc: ..."` is in `engine.dll`, not
   `server.dll`).
2. Data arrays live in `.rdata`; searching for references only in `.text` finds nothing.
3. Virtual calls in this build look like `mov edx, [eax+N]` followed by `call edx`, not
   `call [reg+N]` or `call rel32`. Searching only for `E8` leads to the wrong conclusion that
   "nothing calls this".

---

## Runtime check

The plugin writes to `addons/edictbudget/edictbudget.log` every time it installs a feature, for
example:

```
NOEDICT: server.dll base=67A90000 size=0x982000 (chan bien cho bo giai vtable)
NOEDICT: 'func_areaportal' vtable=680940A4 slot29 -> thunk (OK)
NOEDICT: da sua N vtable / M lop yeu cau
```

Subtract the base from an address in the log and add `0x10000000` to get the static address to
compare with the tables (example above: `0x680940A4 − 0x67A90000 + 0x10000000 = 0x106040A4`, the
vtable of `func_areaportal`). `N` smaller than `M` is normal when classes share a vtable (e.g.
`light` / `light_spot`).
