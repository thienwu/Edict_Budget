# Mechanisms

*[Tiếng Việt](01-co-che.md) · English*

Addresses assume ImageBase `0x10000000`. Full table: [04-dia-chi.en.md](04-dia-chi.en.md).

---

## noedict

Does not give an edict to entity classes that never need to reach the client.

The server's entity table has **4096 slots** (12-bit index, 0–4095), split into two fixed parts:

| part | slots | used for | maximum |
|---|---|---|---|
| networked | 0–2047 | entities sent to clients, one edict each | 2048 — protocol limit (11-bit index in packets) |
| non-networked | 2049–4095 | entities that live only on the server (`EFL_SERVER_ONLY`) | **2047** |

(Slot 2048 is unused.)

When an entity is created, `CBaseEntity::PostConstructor` looks at the `EFL_SERVER_ONLY` flag
(bit 9 of `m_iEFlags`) to choose the part. Both parts exist in the stock `server.dll`. A
non-networked entity only ever gets slots 2049–4095, never anything below 2049.

The project first tried raising the edict count to 4096 but could not keep it stable, so it
switched to reduction: use this existing non-networked part instead of widening the networked
part. See [05-huong-da-bo.en.md](05-huong-da-bo.en.md).

**Practical limits of the non-networked part:**

- The 2047 slots are **shared** by every non-networked entity: the game's own server-only classes
  (46 classes) and the classes cut by `noedict`. Adding a class to `noedict.txt` takes slots
  from this part.
- When it is full the game prints `CBaseEntityList::AddNonNetworkableEntity: no free slots!` and
  returns an invalid handle. The server keeps running, but that entity has no place in the table
  (exact consequences not measured).
- Measured with the current `noedict.txt` over 231 map sources, counting every lump line as alive
  at once: the highest is `anemoia_kitty` with **1486 / 2047**. The `no free slots` line has never
  appeared in the logs. This does not include entities created during play.

The plugin replaces **vtable slot 29** (`PostConstructor`) of the classes in `noedict.txt`: set
the flag, then call the original. The entity is still created and still runs
`Spawn()`/`Activate()`, it just has no edict.

Measured: `ch04_pripyat03` used to crash at 2048 edicts while loading; with `noedict` it loads at
`num_edicts=1178`.

Details, conditions, how to add a class: [02-noedict.en.md](02-noedict.en.md).

---

## swap

Turns one class into a cheaper class at creation time. Clients still receive the entity as usual.

Only one pair works: `point_spotlight` → `beam_spotlight`. `point_spotlight` creates an extra
`spotlight_end` + `beam` (3 edicts); `beam_spotlight` is drawn on the client (1 edict).

Hooks `CEntityFactoryDictionary::Create` (vtable slot 1 of the dictionary) — covers both map load
and entities created during play.

Measured on `the_hive_m4`: live entities 1954 → 1330 (624 fewer).

Cost: `client.dll` hard-codes the halo size to 60, so a map that sets `HaloScale 10` will see
halos 6 times larger. To keep the right halo with as few edicts: `spotlight=2` (next section,
v2.1.0).

---

## spotlight

*Added in v2.1.0, off by default (`spotlight=0`). Run on a local server with 9 maps (numbers in
[06-do-dac.en.md](06-do-dac.en.md#spotlight)); not yet run on a busy server.*

Fixes the cost of `swap`. Instead of swapping the **whole** `point_spotlight` class, the plugin
decides **per light** while the map parses its keyvalue block (`MapEntity_ParseEntity`), before the
entity is created:

| the light's keyvalue block | what the plugin does | edicts |
|---|---|---|
| `HaloScale` = 60, or absent (the default is 60) | creates `beam_spotlight` instead of `point_spotlight` — draws the same | 1 |
| `HaloScale` ≠ 60, no `parentname` | keeps Valve's `point_spotlight`; it and its `spotlight_end` get no edict (`EFL_SERVER_ONLY`), only the `beam` has one | 1, correct halo |
| `HaloScale` ≠ 60, has `parentname` | left as Valve made it | 3 while lit |

Why the second row is safe (addresses in [04-dia-chi.en.md](04-dia-chi.en.md#spotlight-serverdll-clientdll)):

- A `point_spotlight` without a parent is the "efficient" kind: its `beam` is built from two
  points, not from the other two entities.
- `point_spotlight` is never sent to clients anyway.
- On the client, `spotlight_end` only creates a dynamic light whose brightness is multiplied by
  alpha/255, and the server sets alpha to 0 or 1 — practically dark.
- Looking entities up by name does not filter on edicts, so `LightOn` / `LightOff` from the map
  still work.

Each light in the second row takes 2 slots of the non-networked range 2049–4095 (README,
**Limits**). Heaviest seen: `the_hive_m4`, 624 slots plus `noedict`'s share ≈ 1133 / 2047.

**With `swap`:** when `spotlight=2`, a `point_spotlight` read from the map (including via
`point_template`) is decided by this module and **skips** the `point_spotlight` line in
`swap.txt`. A `point_spotlight` created during play by a plugin or script still goes through
`swap`.

`spotlight=1` only logs the decisions and changes nothing. Log with `spotlight=2`:

```
SPOTLIGHT [the_hive_m4] (lam that): 312 point_spotlight | doi beam_spotlight 0 | giu + khong edict 312 | giu nguyen 0 (spotlight_end: xem dong het map)
SPOTLIGHT [the_hive_m4] het map: spotlight_end khong edict 312
```

The second line is printed when the next map loads, because `spotlight_end` is created in
`Activate()`, after the first line.

**Self-check at load** — if any step fails, the whole module turns itself off and logs why: the
prologue of the 3 target functions; a scan of all of `.text` must find exactly 2 `call`s to
`MapEntity_ParseEntity`, exactly 3 to `SpotlightCreate` and no `jmp`; the vtables of
`point_spotlight` / `spotlight_end` are as expected and slot 29 is untouched (i.e. neither class is
in `noedict.txt`); a `beam_spotlight` factory exists; the dictionary is as expected. No
hand-written map list — every decision comes from the keyvalue block itself.

---

## hopbao

*Added in v2.1.1, **on** in the package (`hopbao=1`). Run on a local server (map `defl_3`,
measurements in [06-do-dac.en.md](06-do-dac.en.md#hopbao)); not yet run on a busy server.*

Fixes a v2.1.0 bug ([03-loi-da-biet.en.md](03-loi-da-biet.en.md), item 11). An entity without an
edict never gets its **surrounding bounds** recomputed, because
`CCollisionProperty::MarkSurroundingBoundsDirty` only marks them dirty when the entity has an edict:

```
10093A86  mov eax,[ecx+0x28]   ; edict
10093A89  test eax,eax
10093A8B  je 10093AE2          ; no edict -> return, nothing marked
```

So those bounds stay a single point at the origin. `func_nav_blocker` uses exactly these bounds to
pick nav areas (`BlockNav`, `UnblockNav`), so with it in `noedict.txt` it blocks no area at all.

`hopbao` replaces the 7 bytes at `10093A86` with a jump to a stub:

| entity | what the stub does |
|---|---|
| has an edict | continues on the original path — nothing changes |
| no edict, has `EFL_SERVER_ONLY` | sets `EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS` (`0x4000`): the next time a function asks for the bounds, they are recomputed |
| no edict, no `EFL_SERVER_ONLY` | returns, as the original does |

The rest of the original function (spatial partition, PVS) is skipped for entities without an
edict: such an entity is never in the partition, and PVS only means something with an edict. All 12
calls to this function (position / angle changes, `SetModel`, `SetSolid`, model scale, physics…) go
through the patched spot.

**Self-check at load** — if a step fails, nothing is patched and the reason is logged: the
instruction `or dword [ecx+0x138],0x4000` must occur exactly once in `.text` (function start = that
instruction − 0x1D); all 0x64 bytes of the function must match the pattern, except 2 absolute
addresses. On unload the 7 bytes are restored, if that spot still holds the plugin's jump.

Log:

```
HOPBAO: da va MarkSurroundingBoundsDirty @... (RVA 0x93A80), stub @... (48 byte) - ...
HOPBAO: stub = 8B 41 28 85 C0 0F 85 ...
HOPBAO: 4 lan danh dau hop bao cho thuc the server-only tu lan bao truoc (tong 4)
```

The last line is printed at every map load. If `noedict.txt` has `func_nav_blocker` but `hopbao` is
not running, the log has the line `!! HOPBAO: noedict.txt co func_nav_blocker ma hopbao KHONG chay
...` — then put a `#` in front of the `func_nav_blocker` line.

---

## wipeclear

When the whole team loses, the game clears the map **too late**: the player respawn loop runs
first and eats the remaining edicts, and only then comes `CleanUpMap`.

```
RestartRound (vtable slot 178)
  ├─ respawn players          <- edicts run out here
  └─ CleanUpMap               <- the game only clears here
```

`wipeclear` hooks the start of `RestartRound` and does the **clearing** part of `CleanUpMap`
**earlier**, before the respawn loop:

```
remove every entity not in the game's preserve list (38 classes) and not in wipekeep.txt
-> CleanupDeleteList() -> AllowImmediateEdictReuse()
```

Then the game continues normally and `CleanUpMap` rebuilds the map from the entity lump.

It only clears right after a `mission_lost` event (one-shot flag, reset on map load). Without
this gate it cleared as soon as the map loaded and broke the map — that really happened.

Measured on `c6m1_riverbank`: 3 losses in a row, ~900 slots freed each time; before, the server
died on the first loss.

### wipekeep.txt — for SourceMod plugins you can't modify

Because it clears **earlier** than the game, a plugin that holds a reference to a cleared entity
ends up with a dangling reference and the server may crash.

- If you can modify the plugin: drop the references on the `mission_lost` event — it fires
  before `RestartRound`.
- **If you can't**: list the entity class the plugin holds in `wipekeep.txt`.

A class listed in `wipekeep.txt` is **not cleared early** by `wipeclear`. It is still removed
by the game's own `CleanUpMap` afterwards, **at the same moment as without the plugin**, and the
map is rebuilt from the entity lump. In other words: listing a class here returns that class to
the game's original behaviour on a loss.

```
weapon_gascan        <- exact class name
weapon_              <- a line ending in '_' matches every class starting with that prefix
```

- At most 128 classes, names only `a-z A-Z 0-9 _` (bad lines are skipped and logged).
- Read **once when the plugin loads** — restart the server after editing.
- Keeping a class keeps **every** entity of that class, including the map's own.
- Each loss logs `go X, giu Y (trong do Z do wipekeep.txt)` (removed X, kept Y, Z of them because of wipekeep.txt).

Cost: kept entities still hold their edicts while the respawn loop runs. Keeping the whole
`weapon_` family (~190 entities) was tested: the server died **sooner** (2041 live / 7 free,
versus 1042 / 1006 without it). Only list the classes the plugin needs.

Full instructions are in `configs/wipekeep.txt` itself.

If `wipeclear` can't be used, set `wipeclear=1` (log only) or `0`.

---

## freegate

**A weak mechanism with almost no real effect. Shipped off (`freegate=0`).**

The engine refuses to hand out an edict again within 1 second of it being freed. `freegate`
removes that wait.

Why it does little: the case that matters most is a team loss, and `wipeclear` already calls
`AllowImmediateEdictReuse()` after clearing — which removes the wait exactly then.

| value | does |
|---|---|
| `0` | off |
| `1` | hooks `RemoveEdict` (slot 23); classes not in `freekeep.txt` are reused immediately |
| `2` | patches one byte in `engine.dll`, unconditional |

**Don't use `2`.** Item-transfer plugins (e.g. Gear Transfer) destroy and recreate an item in the
same frame; mode `2` hands back the exact index just freed and clients see a "ghost" weapon.
`freekeep.txt` lists those items by default for mode `1`.

---

## mapclear

Observes entities during map transitions (hooks `PrepChangelevel`, slot 38). Shipped as `1` = log
only.

Don't use `2` (really clear): removing an entity that **carries over** to the next map crashes the
server; at a safe level almost nothing is gained. Always keep `mapclearcarry` at `0`.
