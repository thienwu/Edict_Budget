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
halos 6 times larger.

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
