# Abandoned approaches

*[Tiếng Việt](05-huong-da-bo.md) · English*

The switches below are still in the code for reference. In `patches.txt` they must be `0`.

---

## Raising the limit to 4096

This was the project's **first** approach. It could not be kept stable, and the project moved to
reduction (`noedict`, `swap`, `wipeclear`).

Switches: `bigarray`, `snapshot`, `pinmax`, `pinglobals`, `markfree`, `freetime`,
`indexbounds`, `forcedindex`, `detour`.

Raising the engine's edict count to 4096 **can be done** (`bigarray` patches the constant
`0x800` in `SV_AllocateEdicts`). But it does not solve the problem:

- Entity indices in network packets are only 11 bits (0–2047). A networked entity at index
  ≥ 2048 is decoded wrongly by clients. This cannot be fixed from the server side.
- The free slots in 2048–4095 can only hold entities that are not sent to clients — and the
  engine already has a way to do that: `EFL_SERVER_ONLY`, which is what `noedict` uses.

Measured: enabling `bigarray` + `snapshot` without `pinmax` / `pinglobals` pushed `num_edicts` to
2060 and networked entities spilled above 2047. This group also broke the respawn loop after a
team loss (it broke `wipeclear`).

A question to ask when changing the code: *"Does this need `bigarray`?"* — if yes, stop.

---

## nonetkill — renaming classes in the entity lump

Switch `nonetkill`, list `nonetkill.txt` (no file by default = does nothing).

At `LevelInit`, the first character of the class name in the entity lump is replaced with `~`
(`infodecal` → `~nfodecal`). The engine doesn't know that class, so the entity is **never
created** — it costs no edict.

Why it's not used: if the entity is never created, `Spawn()` / `Activate()` never run either. For
`infodecal` and the `light` family that is where all their work happens (applying decals, setting
light states). Tested for real on `ch04_pripyat03`: the lighting was wrong. Decals are not applied
either, because `CDecal::Activate` (which calls `StaticDecal`) never runs. `noedict` does the same
job without losing anything, because the entity is still created.

It is kept in the code because it may be useful for a class that does nothing at spawn time. If
you use it:

- only add classes whose `Spawn()` / `Activate()` do nothing worth keeping;
- never remove a block or change a string's length in the lump — a block that doesn't start with
  `{` or lacks a `classname` key makes the engine call `Error` and stop the server. The plugin
  only changes one character, so the length stays the same.

`nonethigh` (moving entities to the high range instead of not creating them) only works with
`bigarray` + `snapshot`, i.e. it belongs to the 4096 group above. It crashed when tested.

---

## reuse

The first version of immediate edict reuse (calls `AllowImmediateEdictReuse`). Replaced by
`freegate`, and `freegate` itself has almost no effect. Keep `0`.

---

## mapclear = 2

Clearing entities during map transitions. Removing an entity that carries over to the next map
crashes the server; at a safe level only a handful can be removed. Keep `mapclear=1` (log only)
and `mapclearcarry=0`.

---

## Don't touch the `phys` and `prop_physics` families

- `phys_bone_follower` is the collision of a model. Remove it and players walk through things.
  Valve's own documentation (the `DisableBoneFollowers` key) says it plainly: dropping bone
  followers to save edicts means the collision model no longer works.
- `prop_physics` / `prop_physics_multiplayer`: `client.dll` builds part of these props itself from
  its own copy of the entity lump. Interfering on the server side puts the two out of sync.

---

## No way for `env_sprite`

The most numerous class on the custom maps measured (2539 on `the_hive`, `anemoia`, `chernobyl`).

- Can't be taken off the network: it has its own SendTable (`CSprite`), clients need that data to
  draw it.
- Can't be swapped: one lump line is already one edict, and there is no cheaper class that still
  draws.

Only the map author can reduce it.

---

## swap: only one pair

Every class that creates extra child entities at spawn was checked. Only `point_spotlight` has a
replacement (`beam_spotlight`). The others are either already at 1 edict or have no equivalent
class.
