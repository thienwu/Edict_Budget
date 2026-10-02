# edictbudget

*[Tiếng Việt](README.md) · English*

A Metamod:Source plugin for **Left 4 Dead 2 dedicated servers (Windows)**. It reduces the number
of edicts a map uses, so the server hits this error less often:

```
ED_Alloc: no free edicts
```

No SourceMod required. It does not raise the engine's edict limit.

---

## Limits — read first

- The edict limit is **2048**. Entity indices in network packets are **11 bits**, so an entity
  that **is sent to clients** cannot sit at index ≥ 2048. The plugin cannot change that.
- Entities that `noedict` takes off the network live in slots 2049–4095 — a range that already
  exists in the game's entity table, **at most 2047 non-networked entities at once** (including
  the game's own server-only classes). See [docs/02-noedict.en.md](docs/02-noedict.en.md).
- The plugin only removes **wasted** edicts. A map that really needs more than 2048 networked
  entities at once will still crash.
- Tested on live servers mostly in **Coop**, few players, on a limited set of maps. Versus,
  Survival, Scavenge and busy servers are untested. See **Tested scope**.

---

## What the plugin does

| mechanism | job | status |
|---|---|---|
| `noedict` | does not give an edict to entity classes that never need to reach the client | main, running |
| `swap` | turns `point_spotlight` (3 edicts) into `beam_spotlight` (1 edict) at creation | running |
| `wipeclear` | when the whole team loses, clears entities **earlier** than the game (before the respawn loop) | running |
| `freegate` | lets a freed edict be reused immediately | **weak, almost no real effect — keep off** |
| `mapclear` | observes entities during map transitions | log only |

Details: [docs/01-co-che.en.md](docs/01-co-che.en.md).

---

## ⚠️ Known issues — important if you run SourceMod plugins

### 1. A plugin that creates an entity of a non-networked class gets a **negative number**

`noedict` works per **class**. Every entity of that class has no edict — including entities that
**SourceMod plugins create at runtime**.

For an entity without an edict, SourceMod's `CreateEntityByName()` returns **a negative
reference** (e.g. `-2117514541`) instead of a positive index. Consequences:

| native | when given a negative number |
|---|---|
| `EntIndexToEntRef()` | error `Invalid entity index`, the rest of that code does not run |
| `RemoveEdict()` | called on an entity with no edict — may **crash** |

Seen in practice:

- `logic_script` + plugin `l4d2_script_cmd_swap`: 35 errors, `script` commands silently did nothing.
- `env_shake` + plugin `l4d_plane_crash`: hit both problems above.

Fix — pick one:

- **Don't cut that class** — remove (or put `#` in front of) its line in `noedict.txt`.
- **Fix the plugin**: a negative number is already a reference, keep it as is and don't pass it
  to `EntIndexToEntRef`; use `RemoveEntity()` instead of `RemoveEdict()`. Sample code in
  [docs/02-noedict.en.md](docs/02-noedict.en.md#sourcemod-plugins).

Before enabling a class, search your plugins for `CreateEntityByName("<that class>")`. `.smx`
files are compressed — search the `.sp` sources.

### 2. `wipeclear` clears earlier than the game

A plugin holding a reference to a cleared entity ends up with a dangling reference. Fix: the
plugin drops its references on the `mission_lost` event; if you **can't modify the plugin**, list
that class in `wipekeep.txt` so it isn't cleared early. See
[docs/01-co-che.en.md](docs/01-co-che.en.md#wipeclear).

Full list: [docs/03-loi-da-biet.en.md](docs/03-loi-da-biet.en.md).

---

## Installation

1. Build the DLL (see **Build**) or take the artifact from GitHub Actions.
2. Copy into the server folder:

```
left4dead2/addons/metamod/edictbudget.vdf          <- configs/edictbudget.vdf
left4dead2/addons/edictbudget/bin/edictbudget_mm.dll
left4dead2/addons/edictbudget/*.txt                <- files from configs/
```

3. Restart, run `meta list` to check.

Editing a `.txt` file only needs a server restart, no rebuild.

**Turning it off:** set `stage.txt` to `0` (loaded but does nothing), or delete
`edictbudget.vdf`. The plugin only changes memory; it never writes game files or BSPs.

### Try it first

Run a few days with minimal intervention, read `addons/edictbudget/edictbudget.log`, then enable more.

---

## Switches (`patches.txt`)

| switch | shipped | meaning |
|---|---|---|
| `noedict` | `1` | use `noedict.txt` |
| `swap` | `2` | `0` off · `1` log only · `2` really swap, per `swap.txt` |
| `wipeclear` | `2` | `0` off · `1` log only · `2` really clear |
| `freegate` | `0` | `0` off · `1` per `freekeep.txt` · `2` unconditional (**breaks item transfer**) |
| `mapclear` | `1` | `1` log only. **Don't use `2`** |
| `mapclearcarry` | `0` | **keep `0`** — enabling it crashes on map transition |
| `trap` | `1` | write an inventory when edicts are about to run out |
| `heartbeat` | `300` | write stats every N seconds, `0` off |
| `loadprobe` | `8` | number of frames sampled after a map loads |
| `logconsole` | `0` | `1` = also print to the console |

The switches `bigarray`, `snapshot`, `pinmax`, `pinglobals`, `markfree`, `freetime`,
`indexbounds`, `forcedindex`, `detour`, `reuse`, `nonetkill`, `nonethigh`: **keep `0`**. These are
abandoned approaches, see [docs/05-huong-da-bo.en.md](docs/05-huong-da-bo.en.md).

| file | used for |
|---|---|
| `stage.txt` | `0` = idle, `1` = active |
| `noedict.txt` | classes that get no edict. Each class has a status and a list of maps to check |
| `swap.txt` | class swap pairs |
| `wipekeep.txt` | classes **not** cleared early by `wipeclear` (for SourceMod plugins you can't modify) |
| `freekeep.txt` | classes whose edict is not reused immediately (only with `freegate=1`) |
| `mapkeep.txt` | classes not cleared on map transition (only with `mapclear=2`) |

---

## Tested scope

- Live: custom campaigns `the_hive`, `chernobyl` (`ch04_pripyat03`), some official maps
  (`c1m1_hotel`, `c1m3_mall`, `c2m4_barns`, `c6m1_riverbank`…). Coop, 1–4 players.
- Long runs: 7 long play sessions, all 7 ended with **fewer** entities than they started with;
  0 `ED_Alloc` errors in 105 sessions.
- Most other numbers come from **reading map files** (not running them) and are estimates.

---

## Versions

| tag | change |
|---|---|
| `v2.0.0` | first release |
| `v2.0.1` | fix: `noedict` could pick another class's vtable (seen with `upgrade_spawn`) |
| `v2.0.2` | fix: server crashed at startup when `noedict.txt` contained one of 9 classes (e.g. `beam_spotlight`); `noedict.txt` is no longer silently cut after 32 lines |

---

## Build

```
build.bat
```

Place next to the project folder: `hl2sdk-l4d2/` and `metamod-source-1.12.0.1225/`. MSVC 32-bit.
Use the defines in `build.bat` as they are.

---

## Documentation

[docs/README.md](docs/README.md)

## Authors

- **thienwu** — posed the problem, runs the live server, measured and checked, decided the
  direction and what not to do (no raising the limit to 4096, no touching the `phys` family, no
  editing BSP files).
- **Claude (Anthropic)** — reverse engineering, code and documentation.

Many conclusions come from reverse engineering the binaries and carry addresses so they can be
checked. Anything not verified is marked as such.

## License

GPLv3 (same as Metamod:Source). See `LICENSE` and `NOTICE`. The repository contains no Valve code
or binaries.
