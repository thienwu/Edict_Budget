# edictbudget

<https://github.com/thienwu/Edict_Budget>

*English version. The Vietnamese original is [README.md](README.md) and is the one kept
most current — if the two disagree, trust the Vietnamese.*

A Metamod:Source plugin for **Left 4 Dead 2 dedicated servers** that stops the server
from dying with:

```
Engine Error
ED_Alloc: no free edicts
```

No SourceMod required. It does **not** raise the engine's entity limit.

---

## 📌 Status — 23 August 2026: **limit reached**

This project declares the search for further savings closed. The four live mechanisms have
taken **everything that can be taken without paying for it in something players can see or
touch**.

Three fronts are closed, with evidence:

| direction | result |
|---|---|
| `noedict` | state survives outside an entity through **exactly three doors** (`LightStyle`, `StaticDecal`, `EmitAmbientSound`). The first two are **already used**; the third is **forbidden** because the entity puts its own `entindex` in the packet. **There is no fourth door.** |
| `swap` | a **108 × 549** class sweep produced **exactly one** viable pair |
| `nonetkill` / `killent` | **provably the empty set**, and `killent` is **rejected** because its condition set never asked whether an entity has **collision** — see below |

The largest remaining visible quantity — `phys_bone_follower` ≈ **587 edicts** — is
**permanently off limits**, and Valve's own documentation states why.

📖 The three most useful files if you want to **reuse** or **re-check** this work:

- 🔑 [**docs/06-dia-chi.en.md**](docs/06-dia-chi.en.md) — **every verified
  reverse-engineered address** per feature: RVAs, vtable slot numbers, string anchors, the
  game version and md5 of the reference binaries, and **how to re-derive all of it** after a
  Valve update.
- 🛑 [**docs/07-het-huong.en.md**](docs/07-het-huong.en.md) — every direction searched,
  measured, and rejected, plus **what is still uncertain**.
- 📊 [**docs/08-phanloai-entity.en.md**](docs/08-phanloai-entity.en.md) — the **screening
  data** `noedict` and `swap` actually use: all 557 classnames in `server.dll`, the six
  conditions stated in full with the classes that fail each one, and **which maps the plugin
  helps**. Start here if you want to add a class to `noedict.txt`.

---

## ⚠️ Limitations — read before installing

**This plugin does not fully prevent `no free edicts`.**

It does four things: **reclaims edicts at the right moment**, **allows a just-freed slot
to be reused**, **stops classes that genuinely do not use networking from taking an
edict**, and **swaps entity classes for cheaper equivalents**. If a map genuinely needs
more than 2048 **networked** entities alive at once, nothing here saves it.

### Raising the limit is possible — but it does not solve the problem

Two things are commonly conflated. They are not the same:

| | |
|---|---|
| Raising the edict count to **4096 or higher** | ✅ **doable** — the code is in this repo, disabled by default |
| Putting a **networked** entity at index ≥ 2048 | ❌ **impossible**, and never achievable by patching the server |

The reason for the second: an entity index is encoded in the network packet as an
**11-bit field** (max 2047). That is the **wire format**, and it lives on both ends of
the connection. The server has no way to make a client understand index 2048 — the
client decodes a completely different index. Fixing it would mean patching every
player's `client.dll`.

⇒ The free space in range 2048–4095 can **only hold non-networked entities**. And the
engine **already has** a mechanism for exactly that — `EFL_SERVER_ONLY` — with no byte
patching at all. That is what `noedict` uses.

So raising the ceiling is not technically wrong, it simply **does not solve the
problem**. Measurement also shows it does harm: enabling `bigarray`+`snapshot` without
`pinmax`/`pinglobals` pushes `num_edicts` to **2060**, and **random** entities spill
above 2047 — instability appears immediately. That switch group also **breaks the
respawn loop during a wipe**, which destroys `wipeclear` as a side effect.

Per-byte detail lives in `src/sample_mm.cpp`, in the header block.

### A map that nearly could not be saved — and how it was

```
312 point_spotlight + 312 spotlight_end + 312 beam = 936 edicts (45.7%)
spent purely on light effects. All three classes MUST be networked.
```

An earlier version of this document said *"the only fix is for the mapper to cut the
effects."* **That is no longer true.**

`point_spotlight` **spawns two child entities** — `spotlight_end` + `beam` — so 312
lines in the entity lump become 936 edicts. `beam_spotlight` does the same job but is
drawn entirely client-side ⇒ **1 edict**. The `swap` mechanism substitutes the class at
creation time:

```
live 1954 -> 1330    exactly 624 fewer    headroom 93 -> 718 slots
```

The client still receives the entity and still draws the light shaft. The only cost is
a halo 6× larger, because `client.dll` hardcodes `HaloScale = 60.0`.

**But this is the exception, not the rule.** Scanning all 557 classes in `server.dll`
against the 16 maps measured produced **exactly one** usable substitution pair. The
reason: almost every class is already at coefficient 1 — there is nothing left to cut.

`env_sprite` is the clearest example — the most numerous class measured (**2539**, of
which **2280** in `the_hive` alone) — and it has **no substitute**, because it is
already at coefficient 1 and reaching 0 is impossible.

For those classes the old sentence still holds: **the fix belongs to the mapper.**

### Which maps can be saved

Maps that are heavy because of things that **do not need sending to the client** —
decals, baked lights, positional markers. On one such map: from dying at 2048 edicts to
loading at `num_edicts=1178`.

**Blunt conclusion:** this is a tool for widening the margin, not for raising the ceiling.

---

## ⚠️ What has actually been verified — read carefully

This is the easiest section to misread. **The static data is broad; the runtime data is
narrow.**

### Actually run on a real server

| map | what was measured |
|---|---|
| `the_hive_m3` | `noedict`, `swap`, `loadprobe`, `mapclear` in observe mode |
| `the_hive_m4` | the above, plus a full inventory at the moment edicts ran out |
| `the_hive_m5` | `swap` with a cap |
| `c1m1_hotel` | used as a control map |
| `ch04_pripyat03` | `noedict` (historical — the map this project started from) |

**Essentially one custom campaign, plus one stock map as a control.**

### Static analysis only, NOT run

Entity lumps were read and edict cost computed with `tools/bsp_cost.py` for **exactly
three campaigns**:

```
the_hive     5 maps
anemoia      6 maps   (the "backroom" folder)
chernobyl    5 maps
--------------------
            16 maps
```

**That is reading files, not running a server.**

⚠️ **The figure "16 maps" appearing throughout this document is NOT a broad sample.**
It is precisely those three campaigns and nothing else. All three were made by the same
kind of community author, heavy on decoration. Conclusions drawn from them **may not
hold** for Valve's stock maps, finale maps, Versus maps, or other authors' work.

### Therefore

- **Not tested across maps of varying complexity.** There is no runtime data for finale
  maps, maps with large scripted events, Versus/Scavenge/Survival maps, VScript-heavy
  maps, or other authors' maps.
- **Not tested in game modes other than Coop.** `CleanUpMap()` runs on every round
  restart (Versus, Survival from round 2 on) — nobody has touched that path.
- **Not tested with many players.** Most measurements had 1–4.
- The formula's measured error is **2–6%, always on the high side**. Use it as an upper
  bound and a ranking, **not as a verdict** on which map will die.
- **The three `noedict` classes added on 21–22 Aug are not validated long-term.** They are
  known to work and to cost nothing visible; what is **not** known is how they behave across
  campaigns of differing complexity, or after weeks of uptime. `func_nav_blocker` ships
  **disabled** and has never run. Details:
  [docs/07-het-huong.en.md](docs/07-het-huong.en.md) section 9.

Anyone deploying this should run with `mapclear=1` and `heartbeat=300` (log-only, they
touch no entity) for a few days first, read the log, and only then enable the
mechanisms that intervene.

## The problem

A busy L4D2 server running heavy community maps dies after a while. The log contains
exactly one line — `ED_Alloc: no free edicts` — and then the process is gone.

The usual reading is *"the map uses too many entities, raise the 2048 cap to 4096."*
That direction is **wrong and dangerous** — the entity index in the Source protocol is
**11 bits** (max 2047), so a networked entity at index ≥2048 decodes to garbage on the
client.

This plugin goes the other way: **do not raise the ceiling, reduce the demand.**

---

## Root diagnosis

What cost this project its first two days was a misunderstanding:

> `num_edicts` is **not** the number of entities in use. It is the allocator's
> **high-water mark**, and it **never decreases**.

Live entities = `num_edicts` − (slots carrying the `FL_EDICT_FREE` flag).

Because of that confusion every earlier measurement was read wrong. Once the two
quantities were separated, three real causes emerged — each needing a different fix.

---

## Four mechanisms

### 1. `wipeclear` — clean up when the survivor team wipes

When the whole team goes down (*a wipe*), the engine **does** clean the map — via
`CleanUpMap` — but **far too late**: the player respawn loop runs first and consumes
whatever edicts remain.

The plugin hooks **vtable slot 178** of `CTerrorGameRules` (`RestartRound`) and runs
**exactly the cleanup half of `CleanUpMap`, but earlier** — before the respawn loop. It
uses the game's own 38-class preserve list rather than inventing a keep set.

The gate opens once from the `mission_lost` event and resets on every map load.

```
Measured on c6m1_riverbank:
  WIPE #1 t= 52.63  -> removed 1133, kept 205, freed 924 slots
  WIPE #2 t=102.93  -> removed 1101, kept 199, freed 892 slots
  WIPE #3 t=149.33  -> removed 1129, kept 229, freed 918 slots
  num_edicts: 2012 -> 2030 -> 2030, stable. Before: died on the first wipe.
```

> ⚠️ The keep set must be **minimal**. Adding the `weapon_` prefix (keeping ~190 weapon
> entities) made things **distinctly worse**: 2041 live / 7 free, instead of 1042 live /
> 1006 free. During a wipe, deleted entities are **rebuilt** from the entity lump —
> keeping more only narrows the margin.

### 2. `freegate` — allow a just-freed slot to be reused

Cleanup alone still died. The diagnostic dump gave an absurd number:

```
*** ED_ALLOC ABOUT TO FAIL *** num_edicts=2048 | plugin counted 999 free slots
```

**Dying with 999 free edicts.** The cause is inside `ED_Alloc` itself, in `engine.dll`:

```asm
101E01F4  test cl, 1                        ; FL_EDICT_FREE ?
101E01F7  je   101E022C                     ; not free -> skip
101E0201  comiss xmm0, [esi*4+0x106B3A58]   ; compare 2.0f with freetime[i]
101E0209  mov  ebx, esi                     ; remember the free slot just seen
101E020B  ja   101E0295                     ; freetime < 2.0 -> TAKE (map start)
101E0216  call sv.GetTime()
101E021B  fsub [esi*4+0x106B3A58]           ; curtime - freetime
101E022A  jae  101E0295                     ; >= 1.0 SECOND -> TAKE  <-- the wait
101E022C  ...                               ; not yet -> SKIP
```

The engine **refuses to reuse** an edict for **1 second** after it is freed. A wipe
deletes and recreates hundreds of entities in the **same instant** — not one of them
qualifies.

This is the Source 2009 engine bug the CEF author described: *"running out of edicts
when you have 1000 free"*.

`freegate` changes **one byte** at `0x101E022A`: `jae` → `jmp`. The jump target is
unchanged and the instruction length is identical. It is located by **signature scan**,
so it can find itself again after a relocation.

#### 🛑 30 Aug 2026: the unconditional byte patch breaks item transfer

The old "safe because of `sv_useexplicitdelete`" argument is **right at snapshot level and
wrong at frame level** — two events inside one frame have no snapshot boundary between them
for an explicit delete to travel through.

The L4D2 engine only lets players hand over `weapon_pain_pills` and `weapon_adrenaline`.
Plugins such as **Gear Transfer** extend this to seven more classes by **destroying and
recreating** the item inside one frame ⇒ the unconditional patch hands back the index just
freed ⇒ **ghost weapon**.

> 🟠 **The bug is verified**, by disassembly end to end: `RemoveEdict` (vtable slot 23)
> → `ED_Free` stamps `freetime[i] = GetTime()`; `GiveNamedItem` → `CreateEntityByName` →
> `ED_Alloc` in the same frame. With the gate intact `ED_Alloc` **must** hand out a different
> index; with the byte patched it hands back the same one. The plugin's own changelog records
> both symptoms (v2.16 ghost weapon, v2.19 "Invalid edict").
>
> ⚠️ **`freegate` as a whole is still NOT fully tested or validated.** It has never been
> exercised on a busy server over a long uptime, and the original A/B measurement that
> justified it was taken before `wipeclear` existed in its current form. Treat it as the
> least-proven of the four mechanisms.

**From this build `freegate` has three modes:**

| | |
|---|---|
| `0` | off — the engine's 1-second gate stays intact |
| **`1`** | **denylist mode (default)** — hooks `IVEngineServer::RemoveEdict` (vtable slot 23); a class not listed in `freekeep.txt` gets `freetime = 0.0` ⇒ reusable immediately, a listed class keeps its 1-second quarantine |
| `2` | the unconditional byte patch (legacy, for comparison) |

`ED_Free` has **exactly one entry point**, so one hook at slot 23 covers **100%** of edict
frees. The wipe headroom is unaffected: `wipeclear` calls `AllowImmediateEdictReuse()`
(slot 95) right after `CleanupDeleteList()`, zeroing `freetime` for **every** free edict.

Details: [docs/01-co-che.en.md](docs/01-co-che.en.md) · addresses:
[docs/06-dia-chi.en.md](docs/06-dia-chi.en.md) section 3.

```
Controlled comparison - same situation, num_edicts=2048, ~999 free slots:
  freegate=0 -> DIES immediately
  freegate=1 -> keeps running, num_edicts=2048 with 946 slots reused
```

> 🛑 **Warning for anyone running SourceMod plugins.** `wipeclear` destroys entities
> **earlier** than normal. The game's preserve list holds only **38 classes**, so most of a
> map's entities are removed at this point.
>
> A plugin still holding a reference to one of them ends up with a **dangling** reference,
> and the server can **crash**. Release your plugin's references on the **`mission_lost`**
> event — it fires **before** `RestartRound`.
>
> If the plugin cannot be fixed, set `wipeclear=1` (observe only) or `0`.

### 3. `noedict` — stop non-networked entities from taking an edict

Heavy maps die **during load**, before play even starts. Inventory for `ch04_pripyat03`:

```
853 infodecal   <- 42% of all 2048 edicts
215 func_brush
134 prop_physics_multiplayer
...
```

`infodecal` paints one decal on a wall and is done. It does **not need** sending to the
client — `StaticDecal()` carries the index of the **surface being painted**, not its own.

The engine already has a citizenship class for this. `CBaseEntity::PostConstructor`
decides:

```asm
10055620:
  mov  eax, [esi+0x138]     ; m_iEFlags
  shr  edx, 9
  test dl, 1                ; bit 9 = EFL_SERVER_ONLY
  je   <TAKE-AN-EDICT path> ; = 0 -> AddNetworkableEntity, range 0-2047
  mov  ecx, gEntList
  call AddNonNetworkableEntity   ; = 1 -> range 2049-4095, NO EDICT
```

Range 2049–4095 (**2047 slots**) is the **engine's original design**, not something
patched in. Overflowing it merely prints a warning and returns an invalid handle — it
**does not kill the server**, unlike `ED_Alloc`.

The plugin replaces **vtable slot 29** (`+0x74` = `PostConstructor`) for those classes
only, sets bit 9, then calls the original. No byte patching, no `engine.dll` involvement.

```
ch04_pripyat03:
  before: DIES at num_edicts=2048, 0 free slots
  after:  loads, num_edicts=1178, ~870 slots to spare
  ~1041 entities per load marked EFL_SERVER_ONLY
  Verified visually: no decals lost, lighting correct.
```

#### Current class list

Six classes are enabled in `noedict.txt`, plus one shipped **disabled**:

| class | count across 17 maps | status |
|---|---|---|
| `infodecal` | 853 on `ch04_pripyat03` alone | 🟢 live from the start |
| `light`, `light_spot` | — | 🟢 live from the start |
| `path_track` | 25 on `the_hive_m4` | 🟢 live from the start |
| `func_areaportal` | 179 | 🟢 live since 21 Aug |
| `info_zombie_spawn` | 86 | 🟢 live since 22 Aug |
| `func_nav_blocker` | 64 | ⏸️ **disabled** — remove the `#` to enable, and **test it alone** |

> ⚠️ **The last three are not validated long-term.** They are known to **work** and to cost
> **nothing visible**; what is **not** known is how they behave across campaigns of differing
> complexity, or after weeks of uptime. `func_nav_blocker`'s failure mode is **invisible** —
> it must be judged by **AI behaviour**. Details in
> [docs/07-het-huong.en.md](docs/07-het-huong.en.md) section 9.

> 🛑 **Patching is by VTABLE, not by CLASSNAME.** **20 groups** of classnames share a vtable —
> enabling one name can **drag in the whole group**. And **8 classes** currently resolve to
> the wrong vtable; they are listed in [docs/06-dia-chi.en.md](docs/06-dia-chi.en.md).
> **Do not add new classes** before reading the full six conditions written inside
> `noedict.txt` itself.

### 4. `swap` — substitute a cheaper entity class

Different in kind from the three above: **nothing is un-networked and nothing is
deleted.** The client still receives the entity and still draws it — it is simply a
class that does the same job for fewer edicts.

`point_spotlight` **spawns two children** (`spotlight_end` + `beam`) ⇒ **3 edicts** per
line in the lump. `beam_spotlight` is drawn entirely client-side and spawns nothing ⇒
**1 edict**. Both are stock L4D2 classes, and Valve uses both in their own official maps
(up to 77 and 94 respectively).

It hooks `CEntityFactoryDictionary::Create` — vtable slot 1 of the dictionary returned
by the function at `0x1020CA70` — and substitutes the classname string before calling
the original. A full `.text` sweep found 562 calls to that function, of which **exactly
3 use slot 1** (`CreateEntityByName` plus two branches of the BSP lump parser). Patching
**one vtable pointer** therefore covers both map load and runtime.

```
Measured, matching the prediction exactly:
  the_hive_m4   live 1954 -> 1330   (312 lights, 624 fewer = 312 x 2)
                headroom 93 -> 718 slots
  the_hive_m3   live 1591 -> 1431   (80 lights, 160 fewer)
```

**The substitution table lives in `swap.txt`**, one pair per line; adding a pair needs no
rebuild.

> ⚠️ The cost: `client.dll` hardcodes halo size to 60.0 while maps typically set
> `HaloScale 10` — so the **halo is 6× larger**. The light shaft is not lost, only bigger.

> **This table has been scanned to exhaustion.** All 557 classes in `server.dll`,
> compared against the 16 maps measured, yield **exactly one usable pair**. The reason:
> almost every class already sits at coefficient 1.
>
> `env_sprite` — **2539 instances**, the most numerous across those three campaigns —
> has no substitute: it is already at coefficient 1, and reaching 0 is impossible,
> because costing **0 server edicts while still being drawn** would require the client to
> build the entity from its own lump, and `client.dll` only does that for **exactly two
> classes**, `prop_physics` / `prop_physics_multiplayer`.

---

## Installation

See `configs/`. Copy `addons/` into `left4dead2/`, restart, verify with `meta list`.

Every switch lives in `patches.txt` — **edit the file and restart; no rebuild needed**.

### Uninstall

Nothing needs uninstalling. Two ways, least to most thorough:

1. Set `stage.txt` to `0` — the plugin still loads but stays **completely inert**, hooking no
   vtable at all.
2. Delete `addons/metamod/edictbudget.vdf` and the `addons/edictbudget/` folder, then restart.

Everything the plugin changes exists **only in process memory** — it writes no game file, edits no
BSP, and leaves no trace once switched off.

### Run in observe mode first (recommended)

Your server has different maps, a different player count, different game modes. Run for
a few days at the least intrusive setting first:

```
noedict=1     strongest, zero cost - enable from the start
freegate=0    OFF BY DEFAULT - the least-validated mechanism. Only enable it
              if you ACTUALLY hit ED_Alloc during a wipe, and then use = 1
              (denylist mode), NEVER = 2.
wipeclear=2   survived 5 consecutive wipes under measurement
trap=1        log-only, fires when edicts are about to run out
mapclear=1    OBSERVE ONLY, deletes nothing
heartbeat=300 writes measurements every 5 minutes
swap=0        OFF at first
```

Read `edictbudget.log` for a few days, learn the real entity counts of your own maps,
and only then turn on `swap=2`.

### Every switch

| switch | default | meaning |
|---|---|---|
| `noedict` | 1 | set `EFL_SERVER_ONLY` for classes in `noedict.txt` ⇒ no edict consumed |
| `freegate` | **0** | reuse a freed edict immediately, **except** classes in `freekeep.txt`. `0` off · `1` denylist · `2` unconditional (breaks item transfer). **Ships OFF** — least-validated mechanism |
| `wipeclear` | 2 | clean up on a team wipe. `0` off · `1` observe · `2` clean |
| `swap` | 2 | substitute classes per `swap.txt`. `0` off · `1` observe · `2` substitute |
| `swapmax` | 0 | cap on substitutions. `0` = unlimited |
| `mapclear` | 1 | level transition. **Leave at `1`** — level `2` has shown no proven benefit |
| `mapclearcarry` | 0 | **leave at `0`**. Setting `1` deletes carry-over entities = **crash** |
| `mapclearmax` | 100 | cap on deletions when `mapclear=2` |
| `trap` | 1 | print a full inventory when edicts are about to run out |
| `heartbeat` | 300 | seconds between measurement writes. `0` off |
| `loadprobe` | 8 | frames to sample after a map load |
| `logconsole` | 0 | `1` = also print to the server console |
| `stage.txt` | 1 | `0` = plugin loads but stays **completely inert** |

Leave the `bigarray` / `snapshot` / `pinmax` / `pinglobals` / `markfree` group at `0` —
see "Raising the ceiling to 4096" below.

---

## Design principles

**Fail closed, never fail messily.** Before hooking anything, the plugin checks:

- the prologue signature of the target function
- whether the vtable slot still points at that function (catches another plugin having
  hooked it first)
- whether the preserve list reads back correctly (`[0]=="ai_network"`,
  `[33]=="predicted_viewmodel"`)

If any check fails, that feature **disables itself and logs why**. The server keeps
running.

**Every switch is independent.** Turning one off does not affect the others.

**Observe modes.** `wipeclear=1` and `mapclear=1` only count and log — they delete
nothing. Use them to gather numbers before allowing any intervention.

---

## Directions tried and rejected

This section is probably more useful than the one above, because it saves the next
person time.

### Raising the ceiling to 4096 — **harmful**

Patch `SV_AllocateEdicts` to hand out 4096 edicts. Result: `num_edicts` climbs to
**2060**, **random** networked entities end up above index 2047, and clients decode them
wrong. That switch group also **breaks the respawn loop during a wipe** — it destroys
the thing that was working.

The entity index in the protocol is **11 bits**. No amount of ceiling-raising fixes that.

### `nonetkill` — rename the classname in the entity lump

An entity whose classname the engine does not recognise is **never spawned** and costs
nothing. Sounds reasonable.

But it also **kills `Spawn()`/`Activate()`** — and for `infodecal`/`light` the **entire
value lies in those spawn-time side effects**: painting the decal, setting the
lightstyle. Result: **missing decals, wrong lighting**.

The life-or-death difference from `noedict`: `noedict` still creates the entity and still
runs `Spawn()`/`Activate()`, it merely withholds the edict.

### `killent` — deleting entities from the map outright: **the biggest direction, rejected**

This was the **largest measured** opportunity in the whole project: **6200 edicts** across
16 maps, of which `the_hive_m4` alone accounts for **1227** — while that map's entire
live-or-die margin is **122**.

The mechanism was fully reverse-engineered: return `false` from
`IMapEntityFilter::ShouldCreateEntity` (**vtable slot 0**) on all three filters. The entity
does not exist. A five-condition automatic filter decided what was eligible.

**The fatal gap: the condition set never asked whether an entity has COLLISION.**

Valve's own documentation,
[`prop_dynamic`](https://developer.valvesoftware.com/wiki/Prop_dynamic):

> **Collisions (solid)**: `0` Not solid · `2` Use bounding box · **`6` Use VPhysics (default)**

A `prop_dynamic` **with no `solid` key written** is still **solid**.

Re-measured across **60 stock Valve BSPs**: **809 solid `prop_dynamic`** pass all five
conditions — **472/602 = 78%** in `left4dead2/maps` alone. Among them: **30 bridge railings**
(`bridge_rail`), **12 crypt walls** (`crypts_wall`), **89 gates**, and **39 concrete/plywood
barricades**. Delete 30 bridge railings and players **fall off the bridge**.

Three external confirmations:

1. **Valve states plainly that freeing edicts and losing collision are the same act.** The
   `DisableBoneFollowers` key: *"`phys_bone_followers` **can quickly eat up the edict
   count**... **This will however make the collision model no longer function**."*
2. **SourceMod REMOVED** lump manipulation from `LevelInit`
   ([PR #1534](https://github.com/alliedmodders/sourcemod/pull/1534)) — *"some maps have over
   16MB of entity data"*.
3. **Fifteen years of Stripper:Source use has never deleted by CLASS** — the community
   deletes individual instances by `hammerid`, and always **edits the nav mesh alongside**.
   This project is forbidden from editing the BSP ⇒ it can **never compensate the nav mesh**
   ⇒ stuck bots, wrong Director flow.

The minimum conditions for anyone continuing — along with what would remain (**~4450**
instead of 6200) — are in [docs/07-het-huong.en.md](docs/07-het-huong.en.md) section 4.

### Un-networking the spotlight trio — **closed**, but the problem was solved another way

The original plan was to add `point_spotlight` / `spotlight_end` / `beam` to
`noedict.txt` to reclaim 936 edicts. **Not possible**, with machine-code evidence:

| class | fails condition | evidence |
|---|---|---|
| `spotlight_end` | 1 | `vtable[9]` → `0x1082377C` = `CSpotlightEnd`, **has its own SendTable** |
| `beam` | 1 | `vtable[9]` → `0x107DAF94` = `CBeam`, **has its own SendTable** |
| `point_spotlight` | 5 | `1018E5C9`: `spotlight_end->SetOwnerEntity(point_spotlight)`, and `m_hOwnerEntity` (+0x20C) is a **SendProp** of `DT_BaseEntity` |

The mechanism behind condition 5, read out of `SendProxy_EHandleToInt` @`101CCFE0`:

```asm
and edx, 0xfff     ; 12-bit index
shl eax, 0xb       ; serial shifted by 11
or  eax, edx       ; <- 12 bits forced into an 11-bit field => overflows the serial
```

This also disproves an old worry: **`CBaseHandle` can represent indices ≥ 2048**
(`NUM_ENT_ENTRY_BITS = 12`, mask `0xFFF`, range 0–4095). **The 11-bit limit belongs to
the network protocol, not to the server-side handle.** Do not conflate the two.

**Those 936 edicts were still reclaimed** — by `swap`, not by un-networking. See
mechanism 4.

### `env_sprite` — **fully closed**, no route at all

The most numerous class measured across the three campaigns, **2539 instances**:

```
the_hive    2280   m3 730 | m2 639 | m5 439 | m1 236 | m4 236
anemoia      174
chernobyl     85
------------------
16 maps     2539   <- this is ALL the data, not a broad sample
```

Valve uses at most **162**, averaging **30** — so `the_hive_m3` alone is **4.5×** Valve's
highest.

- **Un-network: no.** `vtable[9]` → `0x10823D14` = its own ServerClass `CSprite`, with 11
  of its own SendProps (`m_flSpriteScale`, `m_nBrightness`, `m_flFrame`…). The client
  **needs** that data to draw anything.
- **Substitute: no.** It is **already at coefficient 1** — 730 lump lines produce exactly
  730 edicts at runtime. Costing **0 server edicts while still being drawn** would require
  the client to build it from its own lump, and `client.dll` only does that for **exactly
  two classes** (`C_PhysPropClientside::ParseAllEntities` @`0x10176950`, exactly two
  `strcmp` instructions).
- `env_glow` is an **exact alias**: same vtable, same constructor, same datamap.
  Substituting changes nothing whatsoever.

The only remaining route is deleting it from the lump — which loses the visual. Not
implemented.

### `prop_physics` / `prop_physics_multiplayer` — **never delete server-side**

`client.dll` builds `C_PhysPropClientside` from **its own** lump; it receives nothing
from the server. `CPhysicsProp::Spawn` @`0x101A5F40` and
`C_PhysPropClientside::Initialize` @`0x10176410` are **mirror images**: each prop is
owned by exactly **one** side, decided by `m_iPhysicsMode` and the
`sv_pushaway_clientside_size` cvar.

Deleting server-side loses whatever that side was responsible for. **Not a goldmine
either**: 1132 of 2983 props are client-owned, but deleting them from the lump saves
**0 edicts** — `DispatchSpawn` returns `-1` once it sees `EFL_KILLME`, and
`CleanupDeleteList()` runs inside the same loop.

---

## Methodology notes

This project made every kind of mistake. The expensive ones all share a shape:
**acting on unverified information**.

A three-tier scale; every conclusion carries a label:

| Tier | Meaning | Good for |
|---|---|---|
| 🟡 read | from a wiki / SDK / inference | forming a hypothesis |
| 🟠 verified in binary | code read inside **L4D2's own** binary, with an address | designing |
| 🟢 measured | ran on a real server, numbers in the log | concluding |

*(The [Stringtable_Fix](https://github.com/thienwu/Stringtable_Fix) repo uses **this same ladder** —
same person setting the problems, so the same vocabulary. ⚪ = not determined.)*

A few examples of jumping straight from 🟡 into production:

- *"`mission_lost` only fires at a finale"* (inferred from strings sitting next to each
  other in the binary) → 1155 entities deleted at `t=1.00` right after map load,
  **destroying the map**
- *"cutting `infodecal` saves almost nothing"* → true at steady state, **false during map
  load**, where all 853 exist at once
- *"a class with a model cannot be removed"* → wrong; the right question is *"is it
  **sent** to the client?"* — a model plus `EF_NODRAW` is still safe
- the SDK's `bspfile.h` declares `lump_t { fileofs, filelen, version, fourCC }` → real
  L4D2 BSPs are **v21** with the order **reversed**:
  `{ version, fileofs, filelen, fourCC }`

And one concrete trap worth remembering for anyone working with Source binaries:

> A prologue containing an **absolute address** **cannot be compared as one block** —
> those four bytes are rewritten by the loader when the module lands at a different base.
> Use a mask.

The same trap bit this project a second time in a different disguise: a safety gate
compared a **static** ServerClass pointer against the **relocated** value read from a
live vtable, silently rejecting all four working classes and disabling `noedict`
entirely. Any address constant taken from a disassembler must be rebased before
comparison.

---

## Building

```
build.bat
```

Requires the L4D2 SDK and Metamod:Source next to the repo. `SOURCE_ENGINE` **must** be
15 (LEFT4DEAD2) in Metamod's numbering — building as 11 (TF2) shifts every vtable index
and makes `SH_CALL` invoke the wrong engine function.

---

## Purpose

This project exists to **improve entity handling and keep entity counts stable** on Left 4 Dead 2
servers — not to extend or circumvent the engine's limits.

Three things specifically:

- **reclaim** edicts at the right moment, instead of letting the engine clean up too late
- **allow reuse** of a just-freed slot, instead of leaving it idle for nothing
- **withhold** edicts from entities that never use networking in the first place

Every conclusion in this documentation carries either a measurement from a real server or an address
in the binary. Anything not verified is stated as such, and does not go into a running build.

## Who wrote this

This project is the result of **two distinct jobs that cannot be separated**.

### The idea, the problem and the direction — **thienwu**, a real server operator

The problem came from a real failure on a running server, not from an exercise. The
operator decided every major direction — and, more importantly, decided the directions
that were **not** to be taken:

- **banned the 4096 direction**, with a one-line self-check: *"Does it need `bigarray`?"*
- **banned touching the `phys` / `prop_physics` family** — losing physics means walking
  through solid objects
- **banned editing BSP files** — they may only be **read** for measurement
- demanded a **GENERAL RULE**: it must apply to **every** map, including maps never seen,
  and the plugin must **self-check at runtime** rather than rely on a hand-written list
- **warned in advance that the `killent` direction was dangerous** — which led directly to
  finding the **collision** gap that the automatic condition set had missed entirely

And they ran the tests, captured the logs, measured on a live server, and **rejected many
of the AI's wrong conclusions**. Several places in the source read *"WRONG, corrected"* —
that is the trace of those rejections.

### Reverse engineering, code, measurement and documentation — **Claude (Anthropic)**, in Claude Code

Reverse engineered `server.dll` / `engine.dll` / `client.dll`, designed and wrote all of
the source, ran the measurements, and wrote every document in this repo.

### Why the split is stated plainly

For two reasons:

1. **Anyone reading the code should know where it came from** and decide for themselves
   how much to trust it.
2. **Many conclusions come from reverse engineering, not official documentation.** Each
   carries a function address and an instruction listing so it can be re-checked.
   Anything that could not be verified is labelled *not determined* rather than guessed.

That is also why the **"What has actually been verified"** section near the top matters
more than usual here: most of the data is static analysis, and the runtime portion covers
only a handful of maps.

## License

**GNU General Public License v3.0**

| file | contents |
|---|---|
| `LICENSE` | **the full GPLv3 text**, downloaded verbatim from gnu.org, unmodified |
| `NOTICE` | copyright, authorship, scope, and what is **not** covered by this license |

GPLv3 was chosen because Metamod:Source is GPLv3 — a plugin linking against it makes this
the **compatible** choice, not an arbitrary one.

**Not covered by this license:** Valve's SDK, the game binaries, and Metamod:Source —
none of which are in this repository. The function addresses and assembly listings in the
comments are **behavioural descriptions** for interoperability, not copied code.
