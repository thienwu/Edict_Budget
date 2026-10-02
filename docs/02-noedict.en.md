# noedict

*[Tiếng Việt](02-noedict.md) · English*

Does not give an edict to entity classes that never need to reach the client.

---

## Mechanism

The engine has a flag `EFL_SERVER_ONLY` (bit 9 of `m_iEFlags`, `[this+0x138]`). When an entity
is created, `CBaseEntity::PostConstructor` (RVA `0x55620`) checks it: if set, the entity goes
into slots 2049–4095 and **gets no edict**.

The plugin replaces **vtable slot 29** (`PostConstructor`) of each class in `noedict.txt` with a
small function: set the flag, then call the original. No engine byte is patched.

### The non-networked part: slots 2049–4095, at most 2047

History: the project started by **raising the edict limit to 4096** (`bigarray`, `snapshot`… in
`engine.dll`). That was not stable: `num_edicts` reached 2060, networked entities spilled above
2047 (clients decode them wrongly), and the respawn loop after a team loss broke. While
investigating it turned out that slots ≥ 2048 can only be used by entities that are **not** sent
to clients — and the game already has a place for that kind of entity. From then on the project
moved to **reduction** instead of raising the limit; `noedict` is the result. Details on the 4096
approach: [05-huong-da-bo.en.md](05-huong-da-bo.en.md).

The `server.dll` entity table (`CBaseEntityList`) has **4096 slots** to begin with. Its
constructor (`0x100B7890`) puts slots **2049–4095** on the free list for non-networked entities:

```
100B78B8  mov ebx, 0x1000         ; 4096 slots, 0x10 bytes each
100B78DF  lea eax, [edi+0x801C]   ; starting at slot 2049
100B78E5  mov esi, 0x7FF          ; 2047 slots
```

This code is in the stock `server.dll` (md5 matches Steam; the plugin does not touch this
function). Non-networked entities only take slots from that list, so they **never sit in slots
0–2048**. Slots 0–2047 belong to networked entities — their slot is their edict number;
`RemoveEntity` (`0x100B7970`) also only returns slots `≥ 0x800` to the non-networked list.
A measurement agrees: a `logic_script` taken off the network had the reference `-2117514541` =
`0x81C94AD3`, i.e. index `0xAD3` = 2771.

Limit: **at most 2047 non-networked entities alive at once** — the game's own server-only classes
plus the classes cut by `noedict`. When it is full, `AddNonNetworkableEntity` (`0x100B7BD0`)
prints `CBaseEntityList::AddNonNetworkableEntity: no free slots!` and returns an invalid handle;
the server keeps running, but that entity has no place in the table.

Counted over 231 map sources with the current `noedict.txt`, treating every lump line as alive at
once: the highest is `anemoia_kitty` with 1486 (10 from the game + 1476 from `noedict`). The
`no free slots` line has never appeared in the logs. This does not count entities created during
play — add them when you add a class.

An entity without an edict still:
- is created, runs `Spawn()` / `Activate()`, receives and fires inputs/outputs;
- can be found by name or class through the server's entity list (12-bit handle, 0–4095).

But:
- clients never receive it;
- `ent_text`, `ent_dump`… don't see it;
- any function that only considers entities with an edict (`[entity+0x28] != 0`) skips it —
  see condition 7.

---

## noedict.txt

- One **class name per line, exact match**. Lines starting with `#` are comments.
- **No prefix matching** in this file (a line `light_` matches nothing). The "ends with `_`" rule
  only applies to `wipekeep.txt`.
- Up to 1024 lines (since `v2.0.2`; older builds silently stopped at line 32).

Each class block in the file has:
- **TRANG THAI** (status): verified / tested / new, off by default / known bug, off;
- **BAN DO DE KIEM CHUNG** (maps to verify): maps containing that class and the count on each.

### Patched per vtable, not per name

Several class names share one vtable. Enabling one name cuts **every** name sharing that
vtable. Examples:

| enable | also cut |
|---|---|
| `light` | `light_spot`, `light_directional`, `light_glspot` |
| `info_landmark` | `info_player_start`, `info_teleport_destination`, `info_player_logo`, `info_hang_lighting`, `logic_proximity` |
| `info_ambient_mob_end` | `info_ambient_mob_start` |

Blocks in `noedict.txt` that pull in other classes also list the maps of those classes.

---

## What the plugin checks at runtime

Before patching a class, the plugin checks:

1. The start of `PostConstructor` looks as expected (checked once for the whole plugin).
2. Slot 29 of the vtable still points at `PostConstructor` — if it is already patched (vtable
   shared with an earlier class) or the class overrides it, skip.
3. **Condition 1**: `GetServerClass` (slot 9) returns `DT_BaseEntity` — a class with its own
   SendTable is refused.

If a check fails, that class is skipped and the reason logged; the server keeps running.

**Only condition 1 is checked by the plugin.** Conditions 2–8 below are the responsibility of
whoever adds the class.

Test on 02/10/2026: all 465 classes that are not already server-only were put in `noedict.txt`.
The plugin behaved exactly as predicted (172 classes cut, 293 refused). But **52 of the 172 cut
classes fail other conditions** — if someone adds them, the plugin will still cut them. List:
[03-loi-da-biet.en.md](03-loi-da-biet.en.md#4-classes-that-pass-the-self-check).

### Classes resolved to the wrong vtable

The plugin finds a class's vtable by reading the machine code of its factory. For some classes
this returns the vtable of the **parent class**. Do not add these to `noedict.txt`:

`point_commentary_viewpoint`, `env_soundscape_proxy`, `env_soundscape_triggerable`,
`prop_vehicle_driveable`, `player`, `weapon_first_aid_kit`, `weapon_defibrillator`,
`env_fire_trail`

In particular `env_soundscape_proxy` / `env_soundscape_triggerable` resolve to the vtable of
`env_soundscape`, and that vtable **passes all 3 checks** — the plugin would cut a different
class without any warning. (Both are already server-only, so nobody needs to add them.)

---

## Conditions before adding a class

| # | condition | example of a class that fails |
|---|---|---|
| 1 | no SendTable of its own (`GetServerClass` returns `DT_BaseEntity`) — **checked by the plugin** | `env_sprite`, `beam`, `light_dynamic` |
| 2 | not solid, not moving — the engine updates collision through `edict_t*` | every `trigger_*`, `func_wall`, `func_clip_vphysics` |
| 3 | does not put its own index into a network message | `ambient_generic` (`EmitAmbientSound(entindex)`) |
| 4 | not already server-only | — |
| 5 | not pointed at through a handle by a networked entity | `point_spotlight` (`spotlight_end` calls `SetOwnerEntity`) |
| 6 | not sent to clients — no model, or `EF_NODRAW` and no child entities | `info_target`, `point_viewcontrol`, `func_illusionary` |
| 7 | not looked up by a function that only considers entities with an edict | `info_ambient_mob_start` (`FindEntityByClassnameNearest` `0x100B5370`) |
| 8 | no plugin creates that class at runtime — or the plugin handles negative numbers, see below | `logic_script`, `env_shake` with some plugins |

If you are not sure about a condition, don't add the class.

---

## SourceMod plugins

An entity without an edict has no positive index in SourceMod. `CreateEntityByName()` returns a
**reference**, which is a **negative number** (e.g. `-2117514541`).

| native | with a negative number |
|---|---|
| `EntIndexToEntRef()` | error `Invalid entity index` — the following code stops |
| `RemoveEdict()` | there is no edict to remove — may crash |

Natives that accept references (`IsValidEntity`, `DispatchSpawn`, `AcceptEntityInput`,
`RemoveEntity`, …) still work with that negative number.

Seen in practice:
- `logic_script` + `l4d2_script_cmd_swap`: 35 errors, `script` commands silently did nothing.
- `env_shake` + `l4d_plane_crash`: uses both `EntIndexToEntRef()` and `RemoveEdict()`.

Fix in the plugin:

```sourcepawn
// A negative number is already a reference - don't pass it to EntIndexToEntRef again.
stock int SafeRef(int entity)
{
    if (entity < 0)
        return entity;
    if (entity == 0 || !IsValidEntity(entity))
        return INVALID_ENT_REFERENCE;
    return EntIndexToEntRef(entity);
}

// RemoveEntity goes through the server's entity list and works even without an edict.
stock void SafeRemove(int entity)
{
    if (entity != INVALID_ENT_REFERENCE && IsValidEntity(entity))
        RemoveEntity(entity);
}
```

Replace every `EntIndexToEntRef(x)` with `SafeRef(x)` and `RemoveEdict(x)` with `SafeRemove(x)`.

If you can't modify the plugin, don't cut the classes it creates. Search the `.sp` files (`.smx`
files are compressed, a text search won't find anything) for `CreateEntityByName("<class>")`.

---

## How to check a class

1. Enable **one** class at a time.
2. Start the server and open `addons/edictbudget/edictbudget.log`. You should see:

```
NOEDICT: '<class>' vtable=... slot29 -> thunk (OK)
NOEDICT: da sua N vtable / M lop yeu cau
```

   (`da sua N vtable / M lop yeu cau` = patched N vtables / M classes requested.) A line
   `khong tim duoc vtable, BO QUA` (vtable not found, skipped) or `TRUOT DIEU KIEN 1, TU CHOI`
   (failed condition 1, refused) means the class is **not** cut.
3. Load a map from that class's **BAN DO DE KIEM CHUNG** list. Compare live entities /
   `num_edicts` at `round_start` with the class on and off.
4. Play through the part of the map that uses the class and look for anything missing (sound,
   effects, events, bot paths).

### Counting entities: include the `.lmp` files

`update/maps/` contains `<map>_l_0.lmp`, `_h_0.lmp`, `_s_0.lmp` files. They **replace** the entity
lump of the `.bsp` (the `update` folder comes first in SearchPaths) and can differ from the `.bsp`
— e.g. `c12m3_bridge` has 49 `func_nav_blocker` in the `.bsp` but 51 in the `.lmp`. The map lists
in `noedict.txt` already include the `.lmp` files. Which file the engine uses for which game mode
is not verified (it looks like `l` = Coop, `h` = Versus, `s` = Survival).
