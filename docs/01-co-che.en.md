# The four live mechanisms

*English translation of [01-co-che.md](01-co-che.md), which is extracted from
`src/sample_mm.cpp`. Function addresses are kept exactly as in the source so they can be
cross-checked. If the two versions disagree, trust the Vietnamese.*

## Two switches for handling non-networked entities

```
TWO SWITCHES FOR HANDLING NON-NETWORKED ENTITIES

The problem: entities that never need sending to a client STILL occupy an edict
in range 0-2047 - the range the 11-bit protocol reserves for things that must be
sent. That is pure waste.
An edict cannot be stripped from a living entity (DetachEdict() is private; only
the destructor can call it). So only two routes remain:

  nonetkill = 1   DELETE them outright once the map has loaded.
                  Walk gEntList, UTIL_Remove every class matching
                  serveronly.txt, then CleanupDeleteList() to return the edicts
                  immediately.
                  Gain: slots returned to range 0-2047 permanently.
                  Loss: the entity's functionality is gone too.

  nonethigh = 1   PUSH them into range 2048-4095 instead of deleting.
                  Reuses the existing Hook_CreateEdict path (allocating downward
                  from 4095) - see the comment block at that function.
                  !!  REQUIRES bigarray=1 AND snapshot=1, otherwise
                  g_ExtReady=false and it does nothing at all.
                  !!  Section 0-AAA: this direction HAS caused a crash in the
                  cleanest A/B test this project ran. Re-enabling it is a
                  deliberate acceptance of that risk in order to re-measure.

Both read their class list from serveronly.txt (matching rule: a line ending in
'_' = prefix match, anything else = exact match).

!!  DO NOT enable both at once - they contradict each other. If both are on,
nonetkill wins and nonethigh is ignored (with a warning in the log).
```

## `noedict` — make certain classes NEVER receive an edict

```
NOEDICT - make certain classes NEVER receive an edict
==========================================================================

XXX THIS IS NOT THE 4096 DIRECTION. It does not use bigarray/snapshot/pinmax/
   pinglobals/markfree. Not a single byte of engine.dll is patched. If anyone
   editing this function finds themselves needing one of those five switches
   => THEY HAVE TAKEN A WRONG TURN, stop.

MECHANISM (verified against the binary):
  CBaseEntity::PostConstructor @ 0x10055620  (RVA 0x55620) is the DECIDING point:
      mov  eax, [esi+0x138]      ; m_iEFlags
      shr  edx, 9
      test dl, 1                 ; bit 9 = EFL_SERVER_ONLY
      je   <TAKE-AN-EDICT path>  ; = 0 -> AddNetworkableEntity, range 0-2047
      mov  ecx, gEntList
      call AddNonNetworkableEntity   ; = 1 -> range 2049-4095, NO EDICT
  All we have to do is set bit 9 BEFORE the original function runs.

HOOK POINT: PostConstructor is a VIRTUAL function at vtable slot 29 (+0x74). Each
class has its OWN vtable, so replacing slot 29 in the target class's vtable
affects ONLY that class. No byte detour, nothing else is touched.

Every class's factory Create looks like:
    push <sizeof>              ; operator new
    call operator new
    push 0                     ; <- bServerOnly = FALSE (Valve's own flag)
    call <ctor>
    mov  dword ptr [esi], <VTABLE>   ; <- this is the value we look for
    ...
    call [vtable+0x74]         ; PostConstructor

XXX FORBIDDEN IN THE LIST:
  - SOLID classes or classes that MOVE: IVEngineServer::SolidMoved /
    TriggerMoved both take an edict_t*. No edict => spatial partition never
    updates.
  - every trigger_* class (same reason)
  - classes with their OWN ServerClass (their own DT_) - the client needs to
    rebuild them.
VERIFIED SAFE: infodecal (StaticDecal uses the SURFACE index, not its own) and
the light family (LightStyle carries no entity index at all).
```

## `noedict`: safety gate 3 — condition 1 (own SendTable)

```
Safety gate 3: CONDITION 1 - the class must NOT have its own SendTable.

This is the strongest filter of the six conditions, and the ONLY one a machine
can check on a human's behalf. It used to live only in the comments of
noedict.txt, so anyone who did not read them could add a class blindly and break
things at runtime.

GetServerClass() = vtable slot 9. The body is `mov eax, imm32 ; ret`:
    B8 <imm32> C3
  imm32 == 0x107D78A8  -> ServerClass CBaseEntity / DT_BaseEntity -> ALLOW
  imm32 != 0x107D78A8  -> the class has its own SendTable         -> REJECT

24 classes were scanned this way, calibrated against 8 known values, matching
100%. The four classes currently enabled (infodecal/light/light_spot/path_track)
all return 0x107D78A8.
Examples that get rejected: spotlight_end (CSpotlightEnd), beam (CBeam),
                            env_sprite (CSprite), light_dynamic (DT_DynamicLight),
                            the whole trigger_* family (CBaseTrigger).

NOTE: imm32 is a STATIC image address. At runtime the module loads at a different
base and the pointer read from the vtable is ALREADY REBASED, so the comparison
MUST be against base + DT_BASEENTITY_RVA, NEVER against the static value.
Comparing directly silently REJECTED all four working classes and disabled
noedict entirely - the log read "patched 0 vtables / 4 classes requested", while
all four were correctly returning 0x540378A8 = 0x53860000 (the real base) +
0x7D78A8. Same class of bug as the mapclear signature being rejected because its
prologue had no mask.
```

## `freegate` — drop the 1-second wait

```
Ask the engine to DROP the one-second cooldown before a just-freed edict may be
handed out again.

ED_Alloc refuses to reuse an edict until 1 second after it was freed. But a wipe
frees hundreds of entities and recreates them IN THE SAME FRAME, so none of them
qualify, and the engine is forced to append fresh edicts - which is exactly what
exhausts a map sitting at 2012/2047.

IVEngineServer::AllowImmediateEdictReuse() is Valve's own answer to this ("Tells
the engine we can immdiately re-use all edict indices even though we may not have
waited enough time", eiface.h:345). Its paired convar sv_useexplicitdelete - ON by
default - makes the engine tell clients the old entity is gone BEFORE its index is
recycled, which is precisely what the cooldown was protecting against.

This attacks the real failure mode. Segregation only ever added headroom; this
removes the NEED for headroom.
```

## `freegate`: the ED_Alloc loop in detail

```
Drop the 1-second wait before an edict may be reused
==========================================================================

ED_Alloc only accepts a freed edict when:
    comiss  2.0f, freetime[i]      ; freetime < 2.0 (freed at map start)
    ja      take_it
    fsub    freetime[i]            ; curtime - freetime
    fcompi  1.0
    jae     take_it                ; or more than 1 SECOND has passed  <-- the byte

So during a wipe (deleting and recreating hundreds of entities in the SAME frame)
no edict qualifies, the engine is forced to allocate new ones, and num_edicts
climbs to the ceiling.
Measured in practice: num_edicts=2012 with 906-918 edicts ACTUALLY FREE while the
engine still reported "ED_Alloc: no free edicts". This is exactly the Source 2009
engine bug the CEF author described: "running out of edicts when you have 1000
free".

Changing one byte 73 -> EB (jae -> jmp) makes every free edict immediately
reusable. The jump target is unchanged, the instruction length is unchanged, no
trampoline is involved.

Safety: the engine already ships sv_useexplicitdelete (ON by default) - when an
index is recycled early it sends an explicit delete to clients first. That is the
mechanism Valve designed as a replacement for this wait.
```

## 🛑 `freegate`: the unconditional byte patch BREAKS item transfer (30 Aug 2026)

The "safe because of `sv_useexplicitdelete`" argument is **right at snapshot level and
wrong at frame level**. Two events inside one frame have **no snapshot boundary between
them** for an explicit delete to travel through.

The L4D2 engine only lets players hand over `weapon_pain_pills` and `weapon_adrenaline`.
Plugins such as **Gear Transfer** extend this to seven more classes by **destroying and
recreating** the item:

```sourcepawn
RemoveEdict(item);                      // -> ED_Free, freetime = GetTime()
item = CreateAndEquip(target, type);    // -> ED_Alloc, SAME FRAME
```

The unconditional patch hands back **the index just freed**, so the client never observes
a delete/create boundary ⇒ **ghost weapon**. That plugin's own changelog records both
symptoms: v2.16 *"ghost weapon attached between players legs"*, v2.19 *"'Invalid edict'
error when creating items to give"*.

### Three modes

| `freegate` | what it does |
|---|---|
| `0` | off — the engine's 1-second gate stays intact |
| **`1`** | **denylist mode (default).** Hooks `IVEngineServer::RemoveEdict` (**vtable slot 23**). After the original runs: a class **not** in `freekeep.txt` gets `freetime[i] = 0.0` ⇒ `ED_Alloc`'s **first branch** takes it immediately. A class **in** the list keeps `GetTime()` ⇒ the 1-second quarantine holds |
| `2` | the unconditional byte patch (legacy, kept for comparison) |

`ED_Free` has **exactly one entry point** (1 `call rel32`, 0 data references), so a single
hook at slot 23 covers **100%** of edict frees.

> 🔑 **It does not narrow the wipe headroom:** `wipeclear` calls
> `AllowImmediateEdictReuse()` (**vtable slot 95**) right after `CleanupDeleteList()`. That
> function sets `freetime = 0.0` for **every** currently-free edict — including the ones
> just quarantined. The denylist only bites **during normal play**.

Full dossier: `tools/freegate-hong-gear-transfer.md` (development repo).

> ⚠️ **`freegate` is the LEAST-VALIDATED of the four mechanisms — it has NOT been fully
> tested or signed off.** It has never been exercised on a busy server over a long uptime,
> and the original A/B measurement that justified it predates `wipeclear` in its current
> form. The item-transfer bug of mode `2`, by contrast, **is verified** — traced end to end
> through `RemoveEdict` → `ED_Free` → `ED_Alloc` by disassembly.

## `wipeclear` — clean at the start of RestartRound

```
WIPECLEAR: clean entities at the start of CTerrorGameRules::RestartRound (vtable
slot 178), before the player respawn loop. See the full explanation block near
InstallWipeClear().

THREE STATES, not two - so each test step changes only ONE thing:
  0 = FULLY OFF. No vtable hook, no event listener. A genuine no-op, useful as a
      baseline for comparison.
  1 = OBSERVE ONLY. Hooks the vtable and listens for events, logs full timestamps
      and slot counts, but deletes NOT ONE entity. Near-zero risk, and it answers
      the open question: does the wipe signal fire BEFORE or AFTER RestartRound,
      and at what point does num_edicts hit 2048.
  2 = CLEAN FOR REAL. Performs the "clean" half of CleanUpMap at the very start
      of RestartRound.

Default 0. Change it in patches.txt; no rebuild needed.
```

## `wipeclear`: the full mechanism

```
WIPECLEAR - clean entities at the START of the restart chain, BEFORE the player
respawn loop.

CTerrorGameRules::CleanUpMap() (RVA 0x2DDB10) already does exactly this job:
    UTIL_Remove(everything outside the preserve list)
      -> CleanupDeleteList() -> AllowImmediateEdictReuse()
      -> MapEntity_ParseAllEntities()
The problem is that it runs TOO LATE. The real order (verified with capstone
against this very server's server.dll, 9,130,288 bytes, ImageBase 0x10000000):

  CDirector::Restart          0x2700D0
    m_bRestarting = 1         0x27045F
    RestartRound()            0x2704C4   <- vtable slot 178
      PLAYER RESPAWN LOOP     0x2E0794..0x2E08A3   <== edicts consumed HERE
      FIRE round_start_pre_entity        0x2E08CE
      CleanUpMap()            0x2E08DF   <== the game only cleans HERE
    m_bRestarting = 0         0x2705DF

Everything before 0x2E08DF runs while the map still holds all 2012 entities with
35 free slots. This block performs the "clean" half of CleanUpMap at the start of
RestartRound and then lets the game continue normally - CleanUpMap will find
almost nothing left to delete, and MapEntity_ParseAllEntities still rebuilds
everything from the entity lump.

IMPORTANT - this is both a PATCH and a MEASUREMENT:
  the "free slots before -> after" log answers the open question directly:
    +~1100 slots and no more crash  => the leak was BEFORE CleanUpMap, patch correct
    +~1100 slots but still crashing => the leak is AFTER the rebuild; at that point
                                       the problem returns to section 0-CONCLUSION
                                       (the map genuinely needs 2012/2047, there is
                                       no waste to reclaim)

It keeps the GAME'S OWN preserve set (read at runtime from RVA 0x7ACE40) so the
semantics are identical to CleanUpMap - only the TIMING differs. That is a
deliberate choice: change exactly one variable.
```

## `wipeclear`: event listening (diagnostic only)

```
--- Event listening: DIAGNOSTIC ONLY, no longer a gate ---------------------

The original plan used 'mission_lost' as the gate. THAT WAS DROPPED (option A).
The reason, verified in the binary rather than guessed:
  mission_lost is fired at exactly ONE site, 0x10269096, inside function
  0x10268CA0. That function only pushes four strings: 'trigger_finale',
  'finale_trigger', 'FinaleLost', 'mission_lost' => this is the FINALE LOSS path.
  The other 11 push sites for mission_lost are all AddListener(+0x0C) or string
  comparisons.
  c6m1_riverbank is not a finale => the gate would never open.

The listener is kept because it answers a still-open question: does mission_lost
actually fire, and does it fire before or after RestartRound. The log will tell us.
A ONE-SHOT FLAG, NOT a time window.

The original used a 5.0s window. WRONG: real measurement showed mission_lost firing
at t=63.47 while RestartRound ran at t=70.50 - 7.03s apart, OUTSIDE the 5s window
=> the gate would have missed a genuine wipe.
That interval is decided by the Director (loss screen, countdown...) and no value
is safe to guess. A one-shot flag removes the guessing:
  mission_lost  -> raise the flag
  RestartRound  -> if the flag is up, clean, then LOWER IT IMMEDIATELY
  new map load  -> lower the flag (so no stale flag survives)
```

## `wipeclear`: the extra keep list — `wipekeep.txt`

```
EXTRA KEEP LIST - wipekeep.txt

The game's preserve list (0x7ACE40) is what the game itself uses. But there are
classes the game is willing to delete which, if deleted EARLY, cause client-side
problems. The first case encountered:
  keeping the player's own entities caused a LOST SHADOW bug.

So an ADDITIONAL keep list is needed, editable by file rather than by rebuild -
in the same style as serveronly.txt:
  a line ending in '_'  -> PREFIX match for the whole family (e.g. "weapon_")
  anything else         -> EXACT classname match

Location: left4dead2/addons/edictbudget/wipekeep.txt
A missing file means nothing extra is kept (the game's preserve list alone).
```

## `wipeclear`: the gate

```
--- THE GATE (RESTORED Aug 7 after real measurement) ---

This gate was once removed, based on an inference from the disassembly that
mission_lost "only fires when a finale is lost" (function 0x10268CA0 pushes
trigger_finale / FinaleLost). THAT INFERENCE WAS WRONG - real measurement on
c6m1_riverbank (NOT a finale) shows mission_lost DOES fire, at t=63.47.
This walked straight into the trap described in section 0-LESSONS: inferring from
strings that happen to sit near each other.

What happens without the gate (log from Aug 7, wipeclear=2):
  RestartRound is called at t=1.00 THE INSTANT THE MAP LOADS (the first round,
  not a wipe). The patch deleted 1155 of the map's entities right there, and free
  slots AFTER RestartRound were still 1462 (baseline 474) => the map was NOT
  rebuilt. The map was destroyed.

=> Only clean when a mission_lost is PENDING. A one-shot flag, not a time window.
```

### 🛑 Warning: SourceMod plugins and `mission_lost`

`wipeclear` destroys entities **earlier** than normal — `CleanupDeleteList()` runs inside the
hook body, before the original `RestartRound`. The game preserve list holds only **38
classes**, so most of the map entities are deleted at this step.

Any plugin still holding a reference to an entity that was just deleted now holds a
**dangling** reference ⇒ the server can **crash**. The most dangerous references are those
that **do not check the serial** — raw pointers, or references held by the engine itself.

**The fix belongs on the plugin side:** clear your own references on **`mission_lost`** — it
fires a few frames **before** `RestartRound`, and that is the window to clean up. Cleaning up
afterwards is too late.

**Why the cleanup is not simply moved after `RestartRound`:** `RestartRound` creates the new
entities while the old ones are still alive — the edict peak is *old + new*. `wipeclear`
exists to free space **before** that peak. Moving it is a return to the exact `ED_Alloc`
failure it was written to fix.

If you cannot fix the plugin, set `wipeclear=1` (observe only) or `0`.

## `swap` — substitute an entity class for a CHEAPER one

```
 SWAP: substitute one entity class for a CHEAPER one at creation time
=====================================================================

The problem: `point_spotlight` SPAWNS TWO EXTRA entities (`spotlight_end` +
  `beam`) => 3 edicts for every line in the BSP lump.
  `beam_spotlight` does the same job but is drawn ENTIRELY CLIENT-SIDE and
  spawns no children => 1 edict.
  the_hive's own author used both classes in the same campaign
  (m1 has 2 beam_spotlight, m5 has 21, m4 has 312 point_spotlight).
  Substituting: m4 937 -> 313, m3 240 -> 80, m5 41 -> 33. 792 edicts in total.

FUNDAMENTALLY DIFFERENT from noedict: this is NOT un-networking. The client still
  receives the entity and still draws the light shaft. It is simply a cheaper
  class. So none of the six conditions apply.

THE HOOK POINT - why this site is clean:
  CreateEntityByName (0x101196B0) creates nothing itself; it calls through
    EntityFactoryDictionary()->vtable[1]:
      101196E7 call 0x1020CA70 ; mov eax,[edx+4] ; call eax
  Sweeping all of .text: 562 calls to 0x1020CA70, of which
    558 use slot 0 (InstallFactory), 1 uses slot 4 (GetCannonicalName),
    and EXACTLY 3 use slot 1 (Create): CreateEntityByName plus two branches of
    the BSP lump parser.
  => Patching ONE vtable pointer covers both lump parsing and runtime creation.
  0x1020CA70 is an address the plugin ALREADY uses in ResolveClassVtable.
  Uses a SourceHook-style vtable swap, NOT a byte detour.

KEYVALUE MAPPING (read from both classes' datamaps):
  SpotlightLength / SpotlightWidth / HDRColorScale  IDENTICAL NAMES
  inputs LightOn / LightOff, output OnLightOn       identical names
  same baseMap = CBaseEntity                        every inherited key identical
  EXACTLY ONE KEY IS LOST: HaloScale - client.dll hardcodes halo = 60.0 at 1006CC80.
    => any map setting HaloScale 10 (e.g. the_hive_m4) will show a halo 6x LARGER.
    43 of 517 point_spotlight across 50 maps already set 60 = the default, so
    nothing changes for those.

spawnflags: bit 0 (start on) and bit 1 (no dynamic light) are IDENTICAL between
  the two classes.
  Scanning 517 point_spotlight across 45 stock maps + 5 hive maps: the value has
  only ever been 2 or 3; not one sets bit 2/3/6 => zero risk of accidentally
  enabling rotation/nofog.

m_iClassname will be overwritten back to "point_spotlight" from the lump. It has
  been proven that server.dll never looks that string up anywhere except
  InstallFactory => harmless, and it preserves backward compatibility for any
  SourceMod plugin filtering by classname.

NOT DONE YET (deliberately, to allow incremental testing):
  Automatically diffing both classes' datamaps at startup to report which key is
  lost.
  GetDataDescMap() = vtable slot 11 (+0x2C), same `B8 imm32 C3` shape as slot 9.
  datamap_t is 24 bytes {dataDesc, nFields, className, baseMap};
  typedescription_t is 60 bytes, with the keyvalue name at +0x10.
  Not written yet because it reads pointers not yet verified on this build - add
  it later, once the substitution mechanism itself is running reliably.
```

## `swap`: resetting the counter at LevelInit

```
Reset the SWAP counters HERE, not in SwapReport() (ServerActivate).

Aug 15: the log showed a report reading "seen 392" = 312 (m4) + 80 (m3), i.e. ONE
report covering TWO map loads - ServerActivate does not run in step with every
load. Same for "seen 82" (80 + 2). Only the numbers in the log were wrong, never
the substitution itself (`substituted` always equalled `seen`), but reading the log
to work out which map it referred to became misleading.
LevelInit runs exactly once per map and BEFORE the lump is parsed, so resetting
here lines up correctly.
```
