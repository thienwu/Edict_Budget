# Out of directions — why there is nothing left to cut

*[Tiếng Việt (original)](07-het-huong.md) · English*

This file records the **negative results**: directions that were searched, measured, and
rejected. It exists so that **nobody has to re-derive them**.

> **Declared 23 August 2026:** this project has reached the limit of its approach. The four
> live mechanisms (`noedict`, `swap`, `freegate`, `wipeclear` + `mapclear`) have taken
> everything that can be taken **without paying for it in something players can see or touch**.

---

## 1. `noedict` has run out of room — because there are only three doors

An entity can give up its edict only if **its effect survives not being networked**. That
happens only when it writes state into a store that lives **outside itself**.

A sweep of all of `server.dll` for calls that write state outside the entity found
**exactly three doors**.

| door | offset | call sites | status |
|---|---|---|---|
| `LightStyle` | `IVEngineServer + 0xA0` | 6 | ✅ used — `light`, `light_spot` |
| `StaticDecal` | `IVEngineServer + 0xA4` | 1 | ✅ used — `infodecal` |
| `EmitAmbientSound` | `IVEngineServer + 0x70` | 3 | ❌ **forbidden** — first argument is the entity's own `entindex` |

The third door is closed by **condition 3**: the entity puts its own index into the packet.
Remove the edict and that index is meaningless. This is why `ambient_generic` — **848
instances across 16 maps** — cannot be touched.

**There is no fourth door.** Every remaining class falls into one of two buckets: either the
client must keep receiving it (⇒ it needs an edict), or its effect is already baked into the
map (⇒ removing it gains nothing further).

### The whole class space was re-swept

> The full classification table: [08-phanloai-entity.en.md](08-phanloai-entity.en.md).

- **557 classes** in `server.dll`, cross-referenced against **16 real maps** from three
  campaigns.
- **40 classes** never previously judged: **all fail condition 1** (they have their own
  SendTable).
- After the vtable-resolution fix (22 Aug), **137 classes** became newly resolvable — re-swept:
  **no new candidates**.

---

## 2. `swap` — fully swept, exactly one pair

Cross-referenced **108 source classes × 549 target classes**. Requirement: same appearance to
the player, cheaper in edicts.

**Result: exactly one pair** — `point_spotlight` → `beam_spotlight` (3 edicts → 1).

Why there is no second pair: **almost every class is already at factor 1.**
`point_spotlight` is the exception because it **spawns two child entities of its own** at
spawn time. No other class in the 16 maps does that.

`env_sprite` is the clearest illustration of being out of room: it is the **most numerous
class measured** (2539 instances, 2280 in `the_hive` alone) — and there is **no substitute
class**, because it is already at factor 1 and going to 0 is impossible.

---

## 3. `nonetkill` — provably the empty set

The idea: rename the classname in the lump so the entity is **never created**.

Refuted: an entity that is never created never runs `Spawn()` or `Activate()`. But those two
functions are exactly where the side effect we wanted to keep is produced. And if a class has
**no** side effect worth keeping, it was already a `noedict` candidate — and `noedict` is
**cheaper**, because the entity still exists.

⇒ The set *"usable with `nonetkill` but not with `noedict`"* is **empty**.

See [04-nonetkill.en.md](04-nonetkill.en.md).

---

## 4. `killent` — the biggest direction, and why it was rejected

This was the **largest measured** opportunity in the whole project: **6200 edicts** across 16
maps, of which `the_hive_m4` alone accounts for **1227** — while that map's entire
live-or-die margin is only **122**.

Mechanism: return `false` from `IMapEntityFilter::ShouldCreateEntity` (**vtable slot 0**) on
all three filters — the entity **does not exist**.

### Why it was rejected

**The condition set never asked whether the entity has COLLISION.**

Valve's own documentation,
[`prop_dynamic`](https://developer.valvesoftware.com/wiki/Prop_dynamic):

> **Collisions (solid)**: `0` Not solid · `2` Use bounding box · **`6` Use VPhysics (default)**

A `prop_dynamic` **with no `solid` key written** is still **solid**.

Re-measured across **60 stock Valve BSPs**: **809 solid `prop_dynamic`** pass all five
conditions. In `left4dead2/maps` alone: **472/602 = 78%**. They are:

| count | model | what it actually is |
|---|---|---|
| 30 | `bridge_rail`, `bridge_rail_dlc2` | **bridge railings** |
| 89 | `cemetery_gate_128/64/32` | **gates** |
| 39 | `barricade001_128`, `concrete_barrier001_96`, `plywood_01/02` | **path barricades** |
| 12 | `crypts_wall` | **a wall** |
| 10 | `fence002` | fencing |

Delete 30 bridge railings and players **fall off the bridge**.

### Three external confirmations

1. **Valve states plainly that freeing edicts and losing collision are the same act.** The
   `DisableBoneFollowers` key: *"`phys_bone_followers` **can quickly eat up the edict
   count**... **This will however make the collision model no longer function**."*
   ⇒ This is primary documentation backing the project's **permanent ban on touching the
   `phys` family**.

2. **SourceMod REMOVED lump manipulation from `LevelInit`**
   ([PR #1534](https://github.com/alliedmodders/sourcemod/pull/1534), asherkin):
   *"some maps have over 16MB of entity data — far larger than our 2MB limit. There is no
   sane way we can currently handle this."*

3. **Fifteen years of Stripper:Source use has never deleted by CLASS** — the community
   deletes **individual instances**, identified by `hammerid`, after pointing at them in
   game. And they always **edit the nav mesh alongside**. This project is forbidden from
   editing the BSP ⇒ it can **never compensate the nav mesh** ⇒ stuck bots, wrong Director
   flow.

### If someone wants to continue

Minimum conditions: add **X7 — refuse anything that could be solid** (delete only when
`solid=0`, `spawnflags&128`, `spawnflags&256`, or the class is never solid by nature);
identify by `hammerid` (measured: **67258/67280 = 100%** of L4D2 entities carry it) rather
than by ordinal position; prove that `pMapEntities` and `GetMapEntitiesString()` are the same
string; and run a count-only mode through a full campaign first.

After X7 the estimate drops to **~4450 edicts** instead of 6200 — losing `prop_dynamic` and
`prop_ragdoll`, keeping the non-solid decorative classes intact.

Full dossier in the development repo: `tools/killent-nguy-hiem.md`.

---

## 5. The `phys` family — permanently forbidden

`phys_bone_follower` ≈ **587 edicts** (🟠 computed, not directly measured). This is the
**largest unharvested quantity still visible** — and it is **off limits**.

Reason: the bone follower **is** the collision. Without it the model loses its physics and
players **walk through it**. Valve confirms this in the `DisableBoneFollowers` documentation.

The same rule covers `prop_physics` and `prop_physics_multiplayer`: `client.dll` **parses the
lump itself** and creates `C_PhysPropClientside` for **exactly those two classes**.
Interfering server-side desynchronises the two ends.

---

## 6. The 4096 direction — closed, and must stay closed

Raising the edict ceiling to 4096 **is possible** (the code is in the repo, disabled by
default) but it **does not solve the problem**: entity indices are encoded in the packet as
an **11-bit field** (max 2047). That is the **packet format**, and it lives on **both ends of
the wire**.

Measurement also shows it is **harmful**: `num_edicts` climbs to 2060, **random** entities
spill above 2047, and that switch group also **breaks the respawn round** (`wipeclear`).

**One question to self-check:** *"Does it need `bigarray`?"* — Yes ⇒ **STOP.**

See [03-huong-4096.en.md](03-huong-4096.en.md).

---

## 7. Entity accumulation during play — measured, does NOT happen

Once the project's biggest open question: *"do entities accumulate steadily during play?"*

🟢 **Measured; the answer is NO:**

- Real server logs across **7 long sessions**: all 7 ended **lower** than they started
  (average **−114**).
- Peak live entities: **1375/2048**.
- **0 `ED_ALLOC` events** in **105 sessions**.

⇒ The "clean up during play" line of work (CEF) **need not be reopened**. `freegate` already
covers that job.

---

## 8. What is left

| item | amount | why it has not been taken |
|---|---|---|
| `phys_bone_follower` | ~587 | **permanently forbidden** — it *is* the collision |
| `ambient_generic` | 848 | violates condition 3 (puts its own `entindex` in the packet) |
| `env_sprite` | 2539 | no mechanism exists — three approaches tried, all dead ends |
| `prop_dynamic` | 1646 | `killent` + X7 refuses it because most instances are solid |
| `killent` after X7 | ~4450 | **not a single line implemented**; needs the four conditions in section 4 |

All five lines **cost something**. Nothing free remains.

---

## 9. ⚠️ What is not yet known — read before trusting the three new classes

The three classes added on 21–22 August (`func_areaportal`, `info_zombie_spawn`,
`func_nav_blocker`) have **not been validated long-term**.

What is known:
- 🟢 They **work** and **cost nothing visible** in the sessions measured.
- 🟢 `func_areaportal`: live since 21 Aug, across 2 maps, survived 2 wipes, 0 `ED_ALLOC` lines.
- 🟢 `info_zombie_spawn`: only genuinely enabled from the 22 Aug 23:29 DLL (before that it was
  a **silent no-op** for six sessions due to the vtable-resolution bug).
- 🟢 Controlled measurement on the same map: `num_edicts` **1116 → 1106** = **−10 edicts**,
  fully attributable.

What is **not** known:
- ❌ **Not tested across campaigns of varying complexity.** The data clusters on `the_hive`
  and `pripyat`.
- ❌ **No long-running operational data** — days, not weeks.
- ❌ `func_nav_blocker` is **disabled** and has never run. Its failure mode is **invisible** —
  it must be judged by **AI behaviour** (do zombies/bots enter an area that should be blocked?).
- ❌ `info_zombie_spawn`: **1/86** carries a `parentname` (`the_hive_m4`,
  `alexi_5000_hunter_spawner` attached to `alexi_5000_body`). It does not change the
  condition-6 verdict, but it is **the one place that is not certain**.
- ❌ `func_nav_blocker`'s condition 6 depends on **no map attaching a parent**. True for the
  17 maps measured. **A new map could break that** — with `EF_NODRAW` *and* a parent, the
  entity **will** be transmitted.

> **Recommendation:** enable one class at a time, one variable per test, and read
> `edictbudget.log` to confirm the number of patched vtables matches the number of enabled
> classes.

---

## 10. Safety work still open (not about gaining edicts)

1. **A guard against shared vtables.** **20 groups** of classnames share a vtable. Nothing
   currently prevents enabling one name from silently dragging in the whole group.
2. **8 classes that resolve to the wrong vtable** — listed in
   [06-dia-chi.en.md](06-dia-chi.en.md) section 1. They must not be added to `noedict.txt`
   until the guard in item 1 exists.
3. **`freegate` is not validated long-term** with ≥ 4 players. The package ships
   `freegate=1` (it passed a controlled A/B measurement); if a busy server shows an odd
   tickrate, **set it to 0 first**.
