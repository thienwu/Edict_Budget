# L4D2 entity classification — the screening data behind `noedict` and `swap`

*English translation. **The Vietnamese file is the master copy**
([08-phanloai-entity.md](08-phanloai-entity.md)) and is kept the most current. Where this
translation disagrees with it, trust the Vietnamese.*

This document answers two questions:

1. **Which classes `noedict` and `swap` can use** — and which are disqualified, and why.
2. **Which maps the plugin helps** — and by how much.

Mechanisms: [01-co-che.en.md](01-co-che.en.md) · addresses: [06-dia-chi.en.md](06-dia-chi.en.md) ·
why there are no new candidates left: [07-het-huong.en.md](07-het-huong.en.md).

Data source: a scan of every classname registered in `server.dll`, cross-referenced against
the entity lump of **17 maps across 4 campaigns** running on a live server. Machine-readable
form of the classification table: [`../data/phanloai_entity_l4d2.json`](../data/phanloai_entity_l4d2.json).

---

## 0. ⚠️ TWO DIFFERENT MEASUREMENTS — read this before using any number below

This document contains two kinds of numbers. **Mixing them is wrong.**

| | what it measures |
|---|---|
| **lump count** | how many **lines** exist in the map's entity lump |
| **runtime count** | how many entities are **actually alive** at one instant |

### Why they diverge — by up to 8x in places

Many classes **delete themselves** inside `Spawn()` or `Activate()`:

```
CLight::Spawn        m_iszName == 0    ->  UTIL_Remove(this)   ; unnamed "helper" light
CDecal::Activate     no targetname     ->  paint decal, then remove self
weapon_*_spawn                         ->  removes itself after spawning the weapon
```

Counted across the 17 maps:

| class | has `targetname` | **unnamed ⇒ self-removes** | self-removal rate |
|---|---:|---:|---:|
| `light` | 490 | **3789** | **89%** |
| `light_spot` | 87 | **1243** | **93%** |
| `infodecal` | 0 | **3652** | **100%** |

Concrete example: on `ch04_pripyat03`, `light` has **153 lump lines** but only **19** survive
at runtime — an **8x** difference.

### 🔑 Consequence for `noedict` — stated plainly so the plugin is not over-credited

`noedict` does **not** deserve credit for entities that **remove themselves with or without
the plugin**. Its real value splits in two:

| when | `noedict` removes | why |
|---|---:|---|
| **peak, during map load** | **10161** | `UTIL_Remove` is **deferred to end of frame** — during the load frame **they all coexist** |
| steady-state play | **~1477** | the self-removing portion is already gone |

> This is **not a weakness**. `ED_Alloc: no free edicts` happens **exactly at the load peak** —
> `ch04_pripyat03` dies the moment it loads, not half an hour into play. But do not read
> **10161** as a *permanent* saving; it is a *peak* saving.

⇒ Every table below uses **lump counts**, i.e. the **load peak**.

---
## 1. `noedict` — screening 557 classes down to 6

### 1.1 Three filter tiers

| tier | remaining | meaning |
|---|---:|---|
| total classnames registered in `server.dll` | **557** | |
| minus classes with their **own** ServerClass | **−322** | the client needs those SendProps to draw them ⇒ removing networking loses the visual |
| minus classes **already** `EFL_SERVER_ONLY` | **−46** | already in the 2049–4095 range; cutting them **reclaims no slot** in 0–2047 |
| **actual candidates for further screening** | **183** | inherit `DT_BaseEntity` **and** currently consume an edict |

> ⚠️ Two scan passes produced **322** and **320** forbidden classes — a difference of 2. The
> appendix uses 320. The discrepancy is **not yet traced**; recorded here so nobody mistakes
> it for a settled number.

---

### 1.2 The six conditions — stated in full

A class must pass **all six** to be added to `noedict.txt`. Failing **one** disqualifies it.

#### C1 — The class must NOT have its own SendTable.

**How to check:** `GetServerClass()` = **vtable slot 9**, body is `mov eax, imm32 ; ret`. If `imm32` equals the ServerClass of `CBaseEntity` ⇒ no own SendTable ⇒ **allowed**; anything else ⇒ **rejected**.

This is the **only condition a machine can check on a human's behalf**, and it is the strongest filter — it eliminates **322/557** immediately.

> ⚠️ `imm32` is an address in the **static image**. At runtime the module loads at a different base, so the comparison **must** be against `base + RVA`. Comparing raw once caused all four classes to be silently rejected.

**Fails:** `env_sprite` (`CSprite`) · `beam` (`CBeam`) · `spotlight_end` (`CSpotlightEnd`) · `light_dynamic` (`DT_DynamicLight`) · the whole `trigger_*` family (`CBaseTrigger`)

---

#### C2 — The class must NOT be solid and must NOT move.

**How to check:** read `Spawn()` for a `SetSolid()` call with anything other than `SOLID_NONE`; check whether the class takes part in traces or spatial partition.

`SolidMoved` and `TriggerMoved` both take an **`edict_t*`**. An entity with no edict **cannot update the spatial partition** ⇒ collision and traces break **silently**.

> 🔑 **This is the tier that killed `prop_physics`** — and it sits **below** the three cheap tiers. Anyone who stops after tier 3 and concludes will let through exactly the most dangerous classes.

**Fails:** every `trigger_*` · every class with collision

---

#### C3 — The class must NOT put ITS OWN index into a network message.

**How to check:** find every engine-interface call inside the code of the class and check whether the first argument is its own `entindex`.

Drop the edict and that index becomes meaningless; the client decodes garbage.

**Fails:** **`ambient_generic`** — calls `EmitAmbientSound(entindex, ...)`. This class is the **9th most numerous** across the 17 maps (**902 instances**) and passes C1, but fails C3, so it is **untouchable**.

---

#### C4 — The class must NOT be server-only already.

**How to check:** does the constructor already set the server-only flag.

If it is already in the 2049–4095 range then setting the flag again **reclaims no slot** in 0–2047 — useless for this problem.

**Fails: exactly 46 classes.** They are already in the 2049–4095 range, so this list is
valuable **as an exclusion list** — adding any of them to `noedict.txt` **gains nothing**.

| class | 17 maps | vtable | ctor | wiki |
|---|---:|---|---|---|
| `env_fade` | **6** | `105FA194` | `100C5460` | [w](https://developer.valvesoftware.com/wiki/env_fade) |
| `env_global` | 0 | `1061627C` | `10115660` | [w](https://developer.valvesoftware.com/wiki/env_global) |
| `env_particlelight` | 0 | `1061E804` | `1012AEC0` | [w](https://developer.valvesoftware.com/wiki/env_particlelight) |
| `env_soundscape` | **2** | `106427F4` | `101DC1D0` | [w](https://developer.valvesoftware.com/wiki/env_soundscape) |
| `env_soundscape_proxy` | 0 | `10642EF4` | `101DC400` | [w](https://developer.valvesoftware.com/wiki/env_soundscape_proxy) |
| `env_soundscape_triggerable` | **92** | `10642BB4` | `101DC270` | [w](https://developer.valvesoftware.com/wiki/env_soundscape_triggerable) |
| `event_queue_saveload_proxy` | 0 | `105E5624` | `10065020` | [w](https://developer.valvesoftware.com/wiki/event_queue_saveload_proxy) |
| `filter_activator_mass_greater` | 0 | `105FF95C` | `100CD790` | [w](https://developer.valvesoftware.com/wiki/filter_activator_mass_greater) |
| `filter_enemy` | 0 | `1060061C` | `100CD990` | [w](https://developer.valvesoftware.com/wiki/filter_enemy) |
| `filter_multi` | 0 | `105FE30C` | `100CCCC0` | [w](https://developer.valvesoftware.com/wiki/filter_multi) |
| `fog_volume` | **33** | `10675ECC` | `10297DA0` | [w](https://developer.valvesoftware.com/wiki/fog_volume) |
| `func_detail_blocker` | **91** | `105EF184` | `10065020` | [w](https://developer.valvesoftware.com/wiki/func_detail_blocker) |
| `info_zombie_border` | 0 | `106F04E4` | `1048C170` | [w](https://developer.valvesoftware.com/wiki/info_zombie_border) |
| `keyframe_track` | 0 | `1061C2BC` | `10065020` | [w](https://developer.valvesoftware.com/wiki/keyframe_track) |
| `logic_active_autosave` | 0 | `10616F1C` | `10065020` | [w](https://developer.valvesoftware.com/wiki/logic_active_autosave) |
| `logic_autosave` | 0 | `10614DFC` | `10065020` | [w](https://developer.valvesoftware.com/wiki/logic_autosave) |
| `logic_branch` | **9** | `106176C4` | `10115960` | [w](https://developer.valvesoftware.com/wiki/logic_branch) |
| `logic_branch_listener` | 0 | `106179EC` | `101159C0` | [w](https://developer.valvesoftware.com/wiki/logic_branch_listener) |
| `logic_case` | **64** | `106168CC` | `10114820` | [w](https://developer.valvesoftware.com/wiki/logic_case) |
| `logic_collision_pair` | **2** | `10615124` | `10065020` | [w](https://developer.valvesoftware.com/wiki/logic_collision_pair) |
| `logic_game_event` | **19** | `1061544C` | `10065020` | [w](https://developer.valvesoftware.com/wiki/logic_game_event) |
| `logic_lineto` | 0 | `10615904` | `10115500` | [w](https://developer.valvesoftware.com/wiki/logic_lineto) |
| `logic_measure_movement` | **5** | `10612FD4` | `10110C90` | [w](https://developer.valvesoftware.com/wiki/logic_measure_movement) |
| `logic_navigation` | 0 | `10613504` | `101110D0` | [w](https://developer.valvesoftware.com/wiki/logic_navigation) |
| `logic_relay` | **83** | `106195E4` | `10119400` | [w](https://developer.valvesoftware.com/wiki/logic_relay) |
| `logic_timer` | **34** | `106147AC` | `10113490` | [w](https://developer.valvesoftware.com/wiki/logic_timer) |
| `logic_versus_random` | 0 | `1061739C` | `101156D0` | [w](https://developer.valvesoftware.com/wiki/logic_versus_random) |
| `math_colorblend` | 0 | `10615F54` | `101155F0` | [w](https://developer.valvesoftware.com/wiki/math_colorblend) |
| `math_counter` | **4** | `106165A4` | `101147B0` | [w](https://developer.valvesoftware.com/wiki/math_counter) |
| `math_remap` | 0 | `10615C2C` | `10115580` | [w](https://developer.valvesoftware.com/wiki/math_remap) |
| `move_keyframed` | 0 | `1061C5E4` | `10065020` | [w](https://developer.valvesoftware.com/wiki/move_keyframed) |
| `multisource` | 0 | `10614AD4` | `101134E0` | [w](https://developer.valvesoftware.com/wiki/multisource) |
| `phys_constraint` | 0 | `1062356C` | `101334F0` | [w](https://developer.valvesoftware.com/wiki/phys_constraint) |
| `phys_constraintsystem` | 0 | `10622724` | `10065020` | [w](https://developer.valvesoftware.com/wiki/phys_constraintsystem) |
| `phys_convert` | 0 | `1062A224` | `101656F0` | [w](https://developer.valvesoftware.com/wiki/phys_convert) |
| `phys_hinge` | **18** | `10622BCC` | `10132F40` | [w](https://developer.valvesoftware.com/wiki/phys_hinge) |
| `phys_lengthconstraint` | **5** | `10623BCC` | `10133930` | [w](https://developer.valvesoftware.com/wiki/phys_lengthconstraint) |
| `phys_motor` | 0 | `1062132C` | `1012F380` | [w](https://developer.valvesoftware.com/wiki/phys_motor) |
| `phys_pulleyconstraint` | 0 | `1062389C` | `10133680` | [w](https://developer.valvesoftware.com/wiki/phys_pulleyconstraint) |
| `phys_ragdollconstraint` | 0 | `10623EFC` | `10133B90` | [w](https://developer.valvesoftware.com/wiki/phys_ragdollconstraint) |
| `phys_slideconstraint` | 0 | `1062323C` | `101332F0` | [w](https://developer.valvesoftware.com/wiki/phys_slideconstraint) |
| `physics_entity_solver` | 0 | `10626634` | `10065020` | [w](https://developer.valvesoftware.com/wiki/physics_entity_solver) |
| `physics_npc_solver` | 0 | `106262EC` | `10151150` | [w](https://developer.valvesoftware.com/wiki/physics_npc_solver) |
| `point_script_template` | 0 | `106318BC` | `10191D50` | [w](https://developer.valvesoftware.com/wiki/point_script_template) |
| `point_template` | **138** | `106314DC` | `101908C0` | [w](https://developer.valvesoftware.com/wiki/point_template) |
| `sky_camera` | **9** | `1064016C` | `101D0DC0` | [w](https://developer.valvesoftware.com/wiki/sky_camera) |

Shared evidence: the constructor sets the server-only flag up front. The `ctor` column is the
constructor address — many classes share `10065020` because they have no constructor of their
own.

---

#### C5 — NO networked class may reference it through an EHANDLE.

**How to check:** scan for every EHANDLE assignment / `SetOwnerEntity` call within the code region of the class.

An `EHANDLE` is encoded inside a SendProp as a **12-bit index + serial**. A networked class holding a handle to an edict-less entity ⇒ the client decodes a garbage pointer.

> ⚠️ The scan **must match how virtual calls are actually encoded**: `mov reg,[reg+disp]` then `call reg`. Scanning for `call rel32` returns **0 results** and then wrongly concludes *"nobody calls it"*.

**Fails:** **`point_spotlight`** — `spotlight_end` calls `SetOwnerEntity(point_spotlight)`, and `m_hOwnerEntity` is a **SendProp of `DT_BaseEntity`**

---

#### C6 — The class must NOT be transmitted to the client.

**How to check:** `UpdateTransmitState()` = **vtable slot 21**. Read whether it returns `FL_EDICT_DONTSEND` or `FL_EDICT_ALWAYS`.

🛑 **COMMON FALSE CLAIM:** *"a class with a model cannot be cut"*. Wrong. The correct criterion is **"is it TRANSMITTED to the client"** — an entity with a model plus `EF_NODRAW` is still safe.

**Fails:** **`info_target`** — sets `FL_EDICT_ALWAYS` when `spawnflags & 2`. On `ch04_pripyat03` **20/20 have it set** ⇒ forbidden. Across 45 official maps **0/85 set it** ⇒ safe.

> 🔑 The most instructive case in the project: **the same class can be safe on one map and forbidden on another.** A verdict keyed on classname alone is not enough once `UpdateTransmitState` reads `spawnflags`.

---

### 1.3 Check ORDER — as important as the checks themselves

```
1  C1  own SendTable?                     <- machine-checkable, eliminates 322/557
2  C4  already server-only?               <- cheap, eliminates 46
3  C6  does the client receive it?        <- read vtable slot 21
4  C2  does it need partition/trace?      <- THE TIER THAT KILLED prop_physics
5  C5  who holds an EHANDLE to it?
6  C3  does it put entindex in a net msg?
```

The first three tiers are **cheap and automatic**. Tier 4 sits **below** them and is the
hardest one.

---

### 1.4 Result — the 6 classes in use

| class | decisive evidence |
|---|---|
| `infodecal` | decal painting uses the **surface** index, not its own |
| `light`, `light_spot` | setting a lightstyle carries **no entity index at all** |
| `path_track` | `SetSolid(SOLID_NONE)`, no model, nobody references it via SendProp |
| `func_areaportal` | `UpdateTransmitState` = `SetTransmitState(DONTSEND)` **unconditionally** |
| `info_zombie_spawn` | the Director finds it via `FindEntityByClassname`, which **never reads the edict pointer** |

Plus `func_nav_blocker`, **fully vetted but left disabled** — its failure mode is invisible;
you have to watch AI behaviour to see it.

### 1.5 Still open — but the practical gain is 0

`env_fire` · `info_game_event_proxy` · `point_viewcontrol` · `logic_script` ·
`info_director` · `escape_route` · `info_target_instructor_hint` · `point_deathfall_camera` ·
`point_devshot_camera` · `point_playermoveconstraint` · `env_debughistory` ·
`phys_ballsocket` · `player_pickup`

What is missing: either the `UpdateTransmitState` override has not been read, or there is a
virtual call on another object whose **argument could not be resolved**.

> None of these classes appears on either of the two maps that actually need rescuing.
> **Resolving all of them would add zero edicts.** That is why the work stopped.

---

## 2. `swap` — scanning for classes with a MULTIPLIER > 1

`swap` does not remove networking. It finds classes where **one lump line produces more than
one edict**, then swaps them to a class that does the same job with a lower multiplier. The
client still receives the entity and still draws it.

Scanning every class with a multiplier > 1 across the 17 maps (29,408 lump lines):

| class, multiplier > 1 | lump lines | what decides the multiplier | surplus edicts |
|---|---:|---|---:|
| **`point_spotlight`** | **459** | `spawnflags&1` set on **430** / clear on 29 | **860** |
| **`env_fire`** | **66** | `spawnflags&4` set on some | **44** |
| `prop_dynamic` | 2196 | depends on the **model**, not on a keyvalue | — forbidden |
| `prop_dynamic_override` | 103 | ,, | — forbidden |
| `env_laser` | 8 | **0 have end sprites** ⇒ multiplier is already 1 | 0 |
| `func_ladderendpoint` · `entity_blocker` · `func_fish_pool` · `tank_rock` | **0** | no lines at all | 0 |

⇒ **All of the leverage sits in exactly two classes.** Every other class with a multiplier > 1
either has 0 lines or is already at multiplier 1.

### 2.1 `swap` only matters on one campaign

| map | `point_spotlight` set/total | edicts `swap` removes |
|---|---|---:|
| `the_hive_m4` | **312/312** | **624** |
| `the_hive_m3` | **80/80** | **160** |
| `anemoia_poolrooms` | 13/13 | **26** |
| `anemoia_poolrooms2` | 13/13 | **26** |
| `ch05_pripyat04` | 5/7 | **10** |
| `the_hive_m5` | 4/12 | **8** |
| `ch04_pripyat03` | 3/4 | **6** |
| `the_hive_m2` | 0/18 | **0** |

**860** edicts in total, of which **784** sit on the two maps `the_hive_m4` and `the_hive_m3`.

⇒ Enabling `swap` on a campaign with no `point_spotlight` **gains nothing**. It is not a
general mechanism — it is a mechanism for **exactly one mapping style**.

`the_hive_m2` is the sharpest example: it has **18** `point_spotlight` but **0 with the flag
set** ⇒ the multiplier is already 1 ⇒ `swap` removes **0**. That map is heavy because of 2280
`env_sprite`, which neither mechanism can touch.

---

## 3. WHICH MAPS THE PLUGIN HELPS

The most practical answer in this document: **which maps are over the ceiling, and how far the
plugin pulls them down.**

```
BEFORE = lump lines  +  2 x point_spotlight(flag set)  +  1 x env_fire(flag set)
AFTER  = BEFORE  -  noedict removals  -  swap removals
```

| map | campaign | BEFORE | `noedict` | `swap` | **AFTER** | cut |
|---|---|---:|---:|---:|---:|---:|
| `anemoia_kitty` | anemoia | **2954** | 1455 | 0 | **1499** | 49.3% |
| `the_hive_m4` | the_hive | **2533** | 473 | 624 | **1436** | 43.3% |
| `ch04_pripyat03` | chernobyl | **2252** | 1042 | 6 | **1204** | 46.5% |
| `the_hive_m2` | the_hive | **2231** | 437 | 0 | **1794** | 19.6% |
| `ch02_pripyat01` | chernobyl | **2206** | 1138 | 0 | **1068** | 51.6% |
| `anemoia_party` | anemoia | **2198** | 441 | 0 | **1757** | 20.1% |
| `the_hive_m3` | the_hive | **2132** | 496 | 160 | **1476** | 30.8% |
| `the_hive_m5` | the_hive | **2062** | 751 | 8 | **1303** | 36.8% |
| `toyz4_v7` | toyz4 | 1823 | 536 | 0 | **1287** | 29.4% |
| `ch03_pripyat02` | chernobyl | 1686 | 872 | 0 | **814** | 51.7% |
| `ch01_jupiter` | chernobyl | 1533 | 316 | 0 | **1217** | 20.6% |
| `anemoia_reality` | anemoia | 1351 | 488 | 0 | **863** | 36.1% |
| `the_hive_m1` | the_hive | 1268 | 335 | 0 | **933** | 26.4% |
| `anemoia_arcade` | anemoia | 1246 | 451 | 0 | **795** | 36.2% |
| `ch05_pripyat04` | chernobyl | 950 | 307 | 10 | **633** | 33.4% |
| `anemoia_poolrooms` | anemoia | 947 | 308 | 26 | **613** | 35.3% |
| `anemoia_poolrooms2` | anemoia | 940 | 315 | 26 | **599** | 36.3% |
| **TOTAL, 17 maps** | | **30312** | **10161** | **860** | **19291** | **36.4%** |

### 🔑 8 maps were OVER the 2048 ceiling before the plugin — all 8 end up under it

| map | BEFORE | AFTER | over the ceiling by |
|---|---:|---:|---:|
| `anemoia_kitty` | **2954** | 1499 | **+906** |
| `the_hive_m4` | **2533** | 1436 | **+485** |
| `ch04_pripyat03` | **2252** | 1204 | **+204** |
| `the_hive_m2` | **2231** | 1794 | **+183** |
| `ch02_pripyat01` | **2206** | 1068 | **+158** |
| `anemoia_party` | **2198** | 1757 | **+150** |
| `the_hive_m3` | **2132** | 1476 | **+84** |
| `the_hive_m5` | **2062** | 1303 | **+14** |

`toyz4_v7` sits at **1823** — right against the ceiling, 225 slots of headroom left.

> ⚠️ **This table measures the LOAD PEAK** (section 0) — precisely the moment `ED_Alloc` dies.
> In steady-state play most of `light`/`infodecal` has already self-removed.
>
> ⚠️ **The BEFORE column is an UPPER BOUND.** Lump line count is not the same as the number of
> entities alive simultaneously: `weapon_*_spawn` self-destructs, `StartDisabled` entities are
> not active yet, `point_template` spawns late, the Director spawns gradually. The measured
> error **always runs 2–6% high**. Use it as a **ranking**, not a verdict — if you want to know
> whether a map dies, you have to **measure**.

### 3.1 Where `noedict` is strongest

| campaign | total | removed | % | heavy because of |
|---|---:|---:|---:|---|
| `chernobyl` | 8608 | 3675 | **42.7%** | decals — 3161 `infodecal` |
| `anemoia` | 9584 | 3458 | **36.1%** | lights — 2569 `light` |
| `toyz4` | 1791 | 536 | **29.9%** | `path_track` — 473 |
| `the_hive` | 9425 | 2492 | **26.4%** | `env_sprite` — 2280 (**cannot be removed**) |
| **total** | **29408** | **10161** | **34.6%** | |

Each campaign is heavy for a different reason — and that decides how much the plugin can help.
`the_hive` has the lowest coverage because the thing making it heavy (`env_sprite`) fails
**C1**.

### 3.2 What each `noedict` class contributes

The six classes are **not alike** — they split into two opposite kinds of contribution, and
conflating them leads to misreading what the plugin does.

| class | toyz4 | chern | anemo | hive | **total** | kind of contribution |
|---|---:|---:|---:|---:|---:|---|
| `light` | 58 | 453 | 2569 | 1199 | **4279** | 🅐 peak — only **490** named, 89% self-remove |
| `infodecal` | 2 | 3161 | 132 | 357 | **3652** | 🅐 peak — **0** named, 100% self-remove |
| `light_spot` | 3 | 42 | 585 | 700 | **1330** | 🅐 peak — only **87** named, 93% self-remove |
| `path_track` | 473 | 7 | 97 | 58 | **635** | 🅑 persistent — alive for the whole map |
| `func_areaportal` | 0 | 1 | 75 | 103 | **179** | 🅑 persistent — alive for the whole map |
| `info_zombie_spawn` | 0 | 11 | 0 | 75 | **86** | 🅑 persistent — alive for the whole map |

**🅐 The first three = 9261/10161 (91%) of the peak figure, but near 0 in steady-state play.**
They remove themselves in `Spawn()`/`Activate()`, and `UTIL_Remove` is **deferred to end of
frame** — so they only exist during the map-load frame. That frame is **exactly the one that
kills the server**, so the credit is real; just do not read it as a permanent saving
(see **section 0**).

**🅑 The last three = 900 entities, but 900 that live for the whole map.** This is the only part
that survives once the map settles — 10x smaller, but it **does not evaporate**.

#### Evidence and remaining uncertainty, per class

| class | evidence for passing the conditions | remaining uncertainty |
|---|---|---|
| `infodecal` | `StaticDecal()` sends the **index of the SURFACE being painted**, not its own index ⇒ passes C3 | — |
| `light` | `LightStyle(style, pattern)` carries **no entity index at all** ⇒ passes C3 | ⚠️ do **not** generalise to `light_dynamic` — that class **has its own SendTable** (`DT_DynamicLight`), failing C1 |
| `light_spot` | same as `light` — same `LightStyle` path | as above |
| `path_track` | `SetSolid(SOLID_NONE)`, no model, nobody references it via SendProp | — |
| `func_areaportal` | C6: vtable **slot 21** = `0x100DA8F0`, exactly 3 instructions: `push 0x10` (`FL_EDICT_DONTSEND`) · `call SetTransmitState` · `ret` ⇒ **unconditional**, the client never receives it. C2: `Spawn 0x100DA8A0` does **not** call `SetSolid`; the engine calls `SetAreaPortalState` with `m_portalNumber` + `m_state` = **two integers**, no edict | 0/179 have `model`, 0/179 have `parentname` — clean |
| `info_zombie_spawn` | C6: 0/86 have `model` ⇒ `GetModelIndex()` returns 0 ⇒ `DONTSEND` branch. C2: the Director finds it via `FindEntityByClassname` (`0x100B47F0`) — that function walks the `CEntInfo` list, compares `m_iClassname`, and **never reads `[this+0x28]`** ⇒ an edict-less entity **is still findable** | ⚠️ **1/86 has a `parentname`** (a scripted spawn point attached to a body). It does not change the C6 verdict and the handle path is safe, but this is the **only uncertain spot** across all six classes |

#### How thoroughly each class has been tested

🟢 All six have **run on a live server**. But "has run" and "has run on the map where it is
concentrated" are two different levels:

- **`path_track` — tested on `toyz4`, the map it actually piles up on.** `473/635` of all
  `path_track` across the four campaigns sits on that one map; the other three campaigns have
  only 7–97 each. That test was the **right test in the right place** — if this class breaks
  anything, `toyz4` is where it would show.
- **`func_areaportal`** — loaded 2 maps, survived 2 wipes, **0 `ED_ALLOC` lines**.
- **`info_zombie_spawn`** — running since the `ResolveClassVtable` fix. ⚠️ The sessions before
  that **were not tests**: the older DLL could not resolve the vtable and **skipped it
  silently** (*"vtable not found, SKIPPING"*) for 6 sessions. Read the logged total of patched
  vtables to know whether a class is genuinely active — do not assume it runs just because it
  is listed in the config file.
- **`light` · `light_spot` · `infodecal`** — running for a long time with no incident. But they
  are kind 🅐, so they are **almost unobservable during play**; the thing to watch is the
  **load frame**.

#### A seventh class, fully vetted but LEFT DISABLED

`func_nav_blocker` (**64 instances / 17 maps**) passes all six conditions on paper but has
**never been run** and is **disabled** by default. To enable it, test it **on its own**.

- C6: `Spawn 0x1048C80E`: `push 0x20` (`EF_NODRAW`) · `call AddEffects`. 0/64 have a
  `parentname` ⇒ parent handle = −1 ⇒ `DONTSEND` branch.
  ⚠️ **`EF_NODRAW` WITH a parent is still transmitted** — it has to follow the state of the
  parent. No map currently assigns a parent, but **a new map could break that**.
- C2: this is a real brush entity, so this tier matters most here. `Spawn` calls
  `SetSolid(SOLID_NONE)` (`0x100940D0`) + `AddSolidFlags(FSOLID_NOT_SOLID)` (`0x10093D80`) ⇒
  it was never solid to begin with. The consumer at `@0x1048BD58` walks **its own private
  table** (`0x107C31A4`) through a direct pointer, reading raw offsets — it does **not** use
  the spatial partition.
- 🛑 **READING TRAP:** this class reads `[esi+0x28]` **6 times**, but `esi` is the **nav mesh
  structure**, not `this` (`[esi+0x24]` = cell flags, `[esi+0x28]` = cell count, used to clamp
  an index). False positive — do not conclude it is an edict access.
- **What to watch when testing:** whether zombies/bots walk into an area that should be
  blocked. There is **no visual symptom** — you have to observe **AI behaviour**.

---

## 4. Appendix — ALL 557 classes

The verdict here is **C1 only** (own SendTable or not). **CANDIDATE does not mean safe** —
it has passed 1 of the 6 conditions in section 1.2.

The `17 maps` column = lump lines across the 17 maps of the 4 campaigns; `0` means the class
exists in `server.dll` but no map uses it. The `vtable` column is for cross-checking while
reverse-engineering. ✅ = currently enabled in `noedict.txt`.

### 4.1 CANDIDATE — passes C1 (229 classes — 76 present on a map, 153 not)

| class | toyz4 | chern | anemo | hive | **17 maps** | vtable | wiki |
|---|---:|---:|---:|---:|---:|---|---|
| **`light`** ✅ | 58 | 453 | 2569 | 1199 | **4279** | `10612744` | [w](https://developer.valvesoftware.com/wiki/light) |
| **`infodecal`** ✅ | 2 | 3161 | 132 | 357 | **3652** | `1065E224` | [w](https://developer.valvesoftware.com/wiki/infodecal) |
| **`light_spot`** ✅ | 3 | 42 | 585 | 700 | **1330** | `10612744` | [w](https://developer.valvesoftware.com/wiki/light_spot) |
| **`ambient_generic`** | 54 | 134 | 387 | 327 | **902** | `105D54A4` | [w](https://developer.valvesoftware.com/wiki/ambient_generic) |
| **`path_track`** ✅ | 473 | 7 | 97 | 58 | **635** | `10620784` | [w](https://developer.valvesoftware.com/wiki/path_track) |
| **`weapon_item_spawn`** | 23 | 140 | 153 | 147 | **463** | `10679B0C` | [w](https://developer.valvesoftware.com/wiki/weapon_item_spawn) |
| **`point_spotlight`** | 0 | 11 | 26 | 422 | **459** | `10630E64` | [w](https://developer.valvesoftware.com/wiki/point_spotlight) |
| **`func_areaportal`** ✅ | 0 | 1 | 75 | 103 | **179** | `106040A4` | [w](https://developer.valvesoftware.com/wiki/func_areaportal) |
| **`point_template`** | 1 | 128 | 1 | 8 | **138** | `106314DC` | [w](https://developer.valvesoftware.com/wiki/point_template) |
| **`info_particle_target`** | 0 | 30 | 5 | 96 | **131** | `105E0F34` | [w](https://developer.valvesoftware.com/wiki/info_particle_target) |
| **`env_instructor_hint`** | 33 | 22 | 13 | 58 | **126** | `105F62BC` | [w](https://developer.valvesoftware.com/wiki/env_instructor_hint) |
| **`env_soundscape_triggerable`** | 0 | 63 | 0 | 29 | **92** | `10642BB4` | [w](https://developer.valvesoftware.com/wiki/env_soundscape_triggerable) |
| **`func_detail_blocker`** | 0 | 90 | 0 | 1 | **91** | `105EF184` | [w](https://developer.valvesoftware.com/wiki/func_detail_blocker) |
| **`info_teleport_destination`** | 41 | 0 | 49 | 0 | **90** | `105D517C` | [w](https://developer.valvesoftware.com/wiki/info_teleport_destination) |
| **`info_zombie_spawn`** ✅ | 0 | 11 | 0 | 75 | **86** | `10678B8C` | [w](https://developer.valvesoftware.com/wiki/info_zombie_spawn) |
| **`logic_relay`** | 7 | 38 | 10 | 28 | **83** | `106195E4` | [w](https://developer.valvesoftware.com/wiki/logic_relay) |
| **`func_illusionary`** | 0 | 29 | 47 | 0 | **76** | `105E2814` | [w](https://developer.valvesoftware.com/wiki/func_illusionary) |
| **`env_explosion`** | 52 | 1 | 18 | 0 | **71** | `105FD96C` | [w](https://developer.valvesoftware.com/wiki/env_explosion) |
| **`env_fire`** | 32 | 3 | 1 | 30 | **66** | `1060155C` | [w](https://developer.valvesoftware.com/wiki/env_fire) |
| **`func_wall_toggle`** | 0 | 0 | 66 | 0 | **66** | `105E2194` | [w](https://developer.valvesoftware.com/wiki/func_wall_toggle) |
| **`func_nav_blocker`** | 0 | 10 | 2 | 52 | **64** | `106EFB14` | [w](https://developer.valvesoftware.com/wiki/func_nav_blocker) |
| **`logic_case`** | 0 | 39 | 0 | 25 | **64** | `106168CC` | [w](https://developer.valvesoftware.com/wiki/logic_case) |
| **`info_remarkable`** | 0 | 63 | 0 | 0 | **63** | `106787B4` | [w](https://developer.valvesoftware.com/wiki/info_remarkable) |
| **`upgrade_spawn`** | 0 | 5 | 1 | 50 | **56** | `10679E34` | [w](https://developer.valvesoftware.com/wiki/upgrade_spawn) |
| **`info_target`** | 0 | 29 | 20 | 1 | **50** | `105E0C0C` | [w](https://developer.valvesoftware.com/wiki/info_target) |
| **`env_spark`** | 3 | 18 | 15 | 12 | **48** | `105FC604` | [w](https://developer.valvesoftware.com/wiki/env_spark) |
| **`env_shake`** | 10 | 27 | 0 | 5 | **42** | `105FBD6C` | [w](https://developer.valvesoftware.com/wiki/env_shake) |
| **`commentary_zombie_spawner`** | 0 | 0 | 35 | 3 | **38** | `106A9D8C` | [w](https://developer.valvesoftware.com/wiki/commentary_zombie_spawner) |
| **`func_wall`** | 2 | 0 | 32 | 0 | **34** | `105E1E1C` | [w](https://developer.valvesoftware.com/wiki/func_wall) |
| **`logic_timer`** | 0 | 3 | 18 | 13 | **34** | `106147AC` | [w](https://developer.valvesoftware.com/wiki/logic_timer) |
| **`fog_volume`** | 0 | 32 | 1 | 0 | **33** | `10675ECC` | [w](https://developer.valvesoftware.com/wiki/fog_volume) |
| **`info_landmark`** | 0 | 8 | 11 | 8 | **27** | `105D517C` | [w](https://developer.valvesoftware.com/wiki/info_landmark) |
| **`info_target_instructor_hint`** | 0 | 13 | 10 | 0 | **23** | `105F663C` | [w](https://developer.valvesoftware.com/wiki/info_target_instructor_hint) |
| **`info_director`** | 1 | 5 | 10 | 5 | **21** | `10667D1C` | [w](https://developer.valvesoftware.com/wiki/info_director) |
| **`logic_auto`** | 0 | 7 | 9 | 4 | **20** | `10613874` | [w](https://developer.valvesoftware.com/wiki/logic_auto) |
| **`logic_game_event`** | 0 | 19 | 0 | 0 | **19** | `1061544C` | [w](https://developer.valvesoftware.com/wiki/logic_game_event) |
| **`filter_activator_team`** | 0 | 2 | 10 | 6 | **18** | `105FF2FC` | [w](https://developer.valvesoftware.com/wiki/filter_activator_team) |
| **`light_directional`** | 0 | 11 | 6 | 1 | **18** | `10612744` | [w](https://developer.valvesoftware.com/wiki/light_directional) |
| **`phys_hinge`** | 0 | 7 | 11 | 0 | **18** | `10622BCC` | [w](https://developer.valvesoftware.com/wiki/phys_hinge) |
| **`info_player_start`** | 1 | 5 | 6 | 5 | **17** | `105D517C` | [w](https://developer.valvesoftware.com/wiki/info_player_start) |
| **`info_game_event_proxy`** | 1 | 0 | 0 | 15 | **16** | `1065DEFC` | [w](https://developer.valvesoftware.com/wiki/info_game_event_proxy) |
| **`light_environment`** | 1 | 5 | 8 | 2 | **16** | `10612A6C` | [w](https://developer.valvesoftware.com/wiki/light_environment) |
| **`point_hurt`** | 0 | 13 | 0 | 0 | **13** | `106328DC` | [w](https://developer.valvesoftware.com/wiki/point_hurt) |
| **`phys_ballsocket`** | 0 | 12 | 0 | 0 | **12** | `10622EFC` | [w](https://developer.valvesoftware.com/wiki/phys_ballsocket) |
| **`info_map_parameters`** | 0 | 5 | 5 | 0 | **10** | `106841A4` | [w](https://developer.valvesoftware.com/wiki/info_map_parameters) |
| **`logic_branch`** | 0 | 9 | 0 | 0 | **9** | `106176C4` | [w](https://developer.valvesoftware.com/wiki/logic_branch) |
| **`sky_camera`** | 1 | 5 | 1 | 2 | **9** | `1064016C` | [w](https://developer.valvesoftware.com/wiki/sky_camera) |
| **`env_physexplosion`** | 0 | 0 | 8 | 0 | **8** | `10629EDC` | [w](https://developer.valvesoftware.com/wiki/env_physexplosion) |
| **`env_texturetoggle`** | 0 | 8 | 0 | 0 | **8** | `105F86F4` | [w](https://developer.valvesoftware.com/wiki/env_texturetoggle) |
| **`filter_damage_type`** | 0 | 1 | 2 | 5 | **8** | `105FFC8C` | [w](https://developer.valvesoftware.com/wiki/filter_damage_type) |
| **`info_elevator_floor`** | 4 | 0 | 2 | 2 | **8** | `10607CCC` | [w](https://developer.valvesoftware.com/wiki/info_elevator_floor) |
| **`phys_spring`** | 0 | 8 | 0 | 0 | **8** | `106289D4` | [w](https://developer.valvesoftware.com/wiki/phys_spring) |
| **`info_gamemode`** | 1 | 4 | 0 | 2 | **7** | `1067759C` | [w](https://developer.valvesoftware.com/wiki/info_gamemode) |
| **`env_fade`** | 0 | 2 | 0 | 4 | **6** | `105FA194` | [w](https://developer.valvesoftware.com/wiki/env_fade) |
| **`logic_script`** | 0 | 0 | 6 | 0 | **6** | `1061415C` | [w](https://developer.valvesoftware.com/wiki/logic_script) |
| **`point_viewcontrol_multiplayer`** | 0 | 4 | 0 | 2 | **6** | `106A0884` | [w](https://developer.valvesoftware.com/wiki/point_viewcontrol_multiplayer) |
| **`logic_measure_movement`** | 0 | 5 | 0 | 0 | **5** | `10612FD4` | [w](https://developer.valvesoftware.com/wiki/logic_measure_movement) |
| **`phys_lengthconstraint`** | 0 | 5 | 0 | 0 | **5** | `10623BCC` | [w](https://developer.valvesoftware.com/wiki/phys_lengthconstraint) |
| **`ambient_music`** | 0 | 4 | 0 | 0 | **4** | `10666294` | [w](https://developer.valvesoftware.com/wiki/ambient_music) |
| **`math_counter`** | 0 | 4 | 0 | 0 | **4** | `106165A4` | [w](https://developer.valvesoftware.com/wiki/math_counter) |
| **`env_outtro_stats`** | 0 | 1 | 1 | 1 | **3** | `105FB20C` | [w](https://developer.valvesoftware.com/wiki/env_outtro_stats) |
| **`func_clip_vphysics`** | 0 | 0 | 3 | 0 | **3** | `105E2F04` | [w](https://developer.valvesoftware.com/wiki/func_clip_vphysics) |
| **`point_servercommand`** | 0 | 0 | 2 | 1 | **3** | `105E640C` | [w](https://developer.valvesoftware.com/wiki/point_servercommand) |
| **`env_firesource`** | 0 | 2 | 0 | 0 | **2** | `10600EFC` | [w](https://developer.valvesoftware.com/wiki/env_firesource) |
| **`env_soundscape`** | 0 | 0 | 2 | 0 | **2** | `106427F4` | [w](https://developer.valvesoftware.com/wiki/env_soundscape) |
| **`logic_collision_pair`** | 0 | 0 | 2 | 0 | **2** | `10615124` | [w](https://developer.valvesoftware.com/wiki/logic_collision_pair) |
| **`trigger_transition`** | 0 | 1 | 0 | 1 | **2** | `1064CF6C` | [w](https://developer.valvesoftware.com/wiki/trigger_transition) |
| **`filter_activator_class`** | 0 | 0 | 1 | 0 | **1** | `105FEFCC` | [w](https://developer.valvesoftware.com/wiki/filter_activator_class) |
| **`filter_activator_infected_class`** | 0 | 0 | 1 | 0 | **1** | `105FF62C` | [w](https://developer.valvesoftware.com/wiki/filter_activator_infected_class) |
| **`filter_activator_model`** | 0 | 0 | 1 | 0 | **1** | `105FE96C` | [w](https://developer.valvesoftware.com/wiki/filter_activator_model) |
| **`logic_director_query`** | 0 | 1 | 0 | 0 | **1** | `106682DC` | [w](https://developer.valvesoftware.com/wiki/logic_director_query) |
| **`logic_scene_list_manager`** | 0 | 1 | 0 | 0 | **1** | `1063D5CC` | [w](https://developer.valvesoftware.com/wiki/logic_scene_list_manager) |
| **`phys_thruster`** | 0 | 0 | 1 | 0 | **1** | `106216A4` | [w](https://developer.valvesoftware.com/wiki/phys_thruster) |
| **`point_clientcommand`** | 0 | 0 | 0 | 1 | **1** | `105E60E4` | [w](https://developer.valvesoftware.com/wiki/point_clientcommand) |
| **`point_teleport`** | 0 | 0 | 0 | 1 | **1** | `10632CA4` | [w](https://developer.valvesoftware.com/wiki/point_teleport) |
| **`point_viewcontrol`** | 0 | 0 | 0 | 1 | **1** | `1064DD4C` | [w](https://developer.valvesoftware.com/wiki/point_viewcontrol) |
| `ai_changehintgroup` | 0 | 0 | 0 | 0 | 0 | `1064CB54` | [w](https://developer.valvesoftware.com/wiki/ai_changehintgroup) |
| `ai_changetarget` | 0 | 0 | 0 | 0 | 0 | `1064C81C` | [w](https://developer.valvesoftware.com/wiki/ai_changetarget) |
| `ai_sound` | 0 | 0 | 0 | 0 | 0 | `10641A94` | [w](https://developer.valvesoftware.com/wiki/ai_sound) |
| `commentary_auto` | 0 | 0 | 0 | 0 | 0 | `105E8C7C` | [w](https://developer.valvesoftware.com/wiki/commentary_auto) |
| `entity_blocker` | 0 | 0 | 0 | 0 | 0 | `105F289C` | [w](https://developer.valvesoftware.com/wiki/entity_blocker) |
| `env_beverage` | 0 | 0 | 0 | 0 | 0 | `105EEA74` | [w](https://developer.valvesoftware.com/wiki/env_beverage) |
| `env_blood` | 0 | 0 | 0 | 0 | 0 | `105EE404` | [w](https://developer.valvesoftware.com/wiki/env_blood) |
| `env_bubbles` | 0 | 0 | 0 | 0 | 0 | `105ED3D4` | [w](https://developer.valvesoftware.com/wiki/env_bubbles) |
| `env_credits` | 0 | 0 | 0 | 0 | 0 | `10667494` | [w](https://developer.valvesoftware.com/wiki/env_credits) |
| `env_debughistory` | 0 | 0 | 0 | 0 | 0 | `105F489C` | [w](https://developer.valvesoftware.com/wiki/env_debughistory) |
| `env_dustpuff` | 0 | 0 | 0 | 0 | 0 | `106070FC` | [w](https://developer.valvesoftware.com/wiki/env_dustpuff) |
| `env_entity_igniter` | 0 | 0 | 0 | 0 | 0 | `105F31C4` | [w](https://developer.valvesoftware.com/wiki/env_entity_igniter) |
| `env_entity_maker` | 0 | 0 | 0 | 0 | 0 | `105F5B4C` | [w](https://developer.valvesoftware.com/wiki/env_entity_maker) |
| `env_firesensor` | 0 | 0 | 0 | 0 | 0 | `10601234` | [w](https://developer.valvesoftware.com/wiki/env_firesensor) |
| `env_funnel` | 0 | 0 | 0 | 0 | 0 | `105EE73C` | [w](https://developer.valvesoftware.com/wiki/env_funnel) |
| `env_global` | 0 | 0 | 0 | 0 | 0 | `1061627C` | [w](https://developer.valvesoftware.com/wiki/env_global) |
| `env_gunfire` | 0 | 0 | 0 | 0 | 0 | `105EFE8C` | [w](https://developer.valvesoftware.com/wiki/env_gunfire) |
| `env_hudhint` | 0 | 0 | 0 | 0 | 0 | `105FA634` | [w](https://developer.valvesoftware.com/wiki/env_hudhint) |
| `env_message` | 0 | 0 | 0 | 0 | 0 | `105FAEE4` | [w](https://developer.valvesoftware.com/wiki/env_message) |
| `env_microphone` | 0 | 0 | 0 | 0 | 0 | `105FB7AC` | [w](https://developer.valvesoftware.com/wiki/env_microphone) |
| `env_muzzleflash` | 0 | 0 | 0 | 0 | 0 | `105EF81C` | [w](https://developer.valvesoftware.com/wiki/env_muzzleflash) |
| `env_particlelight` | 0 | 0 | 0 | 0 | 0 | `1061E804` | [w](https://developer.valvesoftware.com/wiki/env_particlelight) |
| `env_physimpact` | 0 | 0 | 0 | 0 | 0 | `10629144` | [w](https://developer.valvesoftware.com/wiki/env_physimpact) |
| `env_physwire` | 0 | 0 | 0 | 0 | 0 | `105EF4E4` | [w](https://developer.valvesoftware.com/wiki/env_physwire) |
| `env_player_blocker` | 0 | 0 | 0 | 0 | 0 | `105F4154` | [w](https://developer.valvesoftware.com/wiki/env_player_blocker) |
| `env_player_surface_trigger` | 0 | 0 | 0 | 0 | 0 | `105F713C` | [w](https://developer.valvesoftware.com/wiki/env_player_surface_trigger) |
| `env_ragdoll_boogie` | 0 | 0 | 0 | 0 | 0 | `1063BDF4` | [w](https://developer.valvesoftware.com/wiki/env_ragdoll_boogie) |
| `env_rock_launcher` | 0 | 0 | 0 | 0 | 0 | `106A5D0C` | [w](https://developer.valvesoftware.com/wiki/env_rock_launcher) |
| `env_rotorshooter` | 0 | 0 | 0 | 0 | 0 | `105F0C74` | [w](https://developer.valvesoftware.com/wiki/env_rotorshooter) |
| `env_shooter` | 0 | 0 | 0 | 0 | 0 | `105EDD7C` | [w](https://developer.valvesoftware.com/wiki/env_shooter) |
| `env_soundscape_proxy` | 0 | 0 | 0 | 0 | 0 | `10642EF4` | [w](https://developer.valvesoftware.com/wiki/env_soundscape_proxy) |
| `env_splash` | 0 | 0 | 0 | 0 | 0 | `105EFB54` | [w](https://developer.valvesoftware.com/wiki/env_splash) |
| `env_tilt` | 0 | 0 | 0 | 0 | 0 | `105FC0BC` | [w](https://developer.valvesoftware.com/wiki/env_tilt) |
| `env_tracer` | 0 | 0 | 0 | 0 | 0 | `105ED70C` | [w](https://developer.valvesoftware.com/wiki/env_tracer) |
| `env_viewpunch` | 0 | 0 | 0 | 0 | 0 | `105F01D4` | [w](https://developer.valvesoftware.com/wiki/env_viewpunch) |
| `env_zoom` | 0 | 0 | 0 | 0 | 0 | `105F9854` | [w](https://developer.valvesoftware.com/wiki/env_zoom) |
| `escape_route` | 0 | 0 | 0 | 0 | 0 | `106747B4` | [w](https://developer.valvesoftware.com/wiki/escape_route) |
| `event_queue_saveload_proxy` | 0 | 0 | 0 | 0 | 0 | `105E5624` | [w](https://developer.valvesoftware.com/wiki/event_queue_saveload_proxy) |
| `filter_activator_context` | 0 | 0 | 0 | 0 | 0 | `105FEC9C` | [w](https://developer.valvesoftware.com/wiki/filter_activator_context) |
| `filter_activator_mass_greater` | 0 | 0 | 0 | 0 | 0 | `105FF95C` | [w](https://developer.valvesoftware.com/wiki/filter_activator_mass_greater) |
| `filter_activator_name` | 0 | 0 | 0 | 0 | 0 | `105FE63C` | [w](https://developer.valvesoftware.com/wiki/filter_activator_name) |
| `filter_base` | 0 | 0 | 0 | 0 | 0 | `105FDFDC` | [w](https://developer.valvesoftware.com/wiki/filter_base) |
| `filter_enemy` | 0 | 0 | 0 | 0 | 0 | `1060061C` | [w](https://developer.valvesoftware.com/wiki/filter_enemy) |
| `filter_health` | 0 | 0 | 0 | 0 | 0 | `106002EC` | [w](https://developer.valvesoftware.com/wiki/filter_health) |
| `filter_melee_damage` | 0 | 0 | 0 | 0 | 0 | `105FFFBC` | [w](https://developer.valvesoftware.com/wiki/filter_melee_damage) |
| `filter_multi` | 0 | 0 | 0 | 0 | 0 | `105FE30C` | [w](https://developer.valvesoftware.com/wiki/filter_multi) |
| `func_fish_pool` | 0 | 0 | 0 | 0 | 0 | `106027B4` | [w](https://developer.valvesoftware.com/wiki/func_fish_pool) |
| `func_ladderendpoint` | 0 | 0 | 0 | 0 | 0 | `10608ED4` | [w](https://developer.valvesoftware.com/wiki/func_ladderendpoint) |
| `func_nav_attribute_region` | 0 | 0 | 0 | 0 | 0 | `1068F6CC` | [w](https://developer.valvesoftware.com/wiki/func_nav_attribute_region) |
| `func_nav_avoidance_obstacle` | 0 | 0 | 0 | 0 | 0 | `106F01BC` | [w](https://developer.valvesoftware.com/wiki/func_nav_avoidance_obstacle) |
| `func_nav_connection_blocker` | 0 | 0 | 0 | 0 | 0 | `106EF7DC` | [w](https://developer.valvesoftware.com/wiki/func_nav_connection_blocker) |
| `func_proprrespawnzone` | 0 | 0 | 0 | 0 | 0 | `105DA274` | [w](https://developer.valvesoftware.com/wiki/func_proprrespawnzone) |
| `func_timescale` | 0 | 0 | 0 | 0 | 0 | `1060AD54` | [w](https://developer.valvesoftware.com/wiki/func_timescale) |
| `func_traincontrols` | 0 | 0 | 0 | 0 | 0 | `1064A4A4` | [w](https://developer.valvesoftware.com/wiki/func_traincontrols) |
| `func_vehicleclip` | 0 | 0 | 0 | 0 | 0 | `105E24CC` | [w](https://developer.valvesoftware.com/wiki/func_vehicleclip) |
| `func_weight_button` | 0 | 0 | 0 | 0 | 0 | `1065D624` | [w](https://developer.valvesoftware.com/wiki/func_weight_button) |
| `game_end` | 0 | 0 | 0 | 0 | 0 | `10619FB4` | [w](https://developer.valvesoftware.com/wiki/game_end) |
| `game_gib_manager` | 0 | 0 | 0 | 0 | 0 | `1063AC44` | [w](https://developer.valvesoftware.com/wiki/game_gib_manager) |
| `game_player_equip` | 0 | 0 | 0 | 0 | 0 | `1061A604` | [w](https://developer.valvesoftware.com/wiki/game_player_equip) |
| `game_player_team` | 0 | 0 | 0 | 0 | 0 | `1061A92C` | [w](https://developer.valvesoftware.com/wiki/game_player_team) |
| `game_score` | 0 | 0 | 0 | 0 | 0 | `10619C8C` | [w](https://developer.valvesoftware.com/wiki/game_score) |
| `game_text` | 0 | 0 | 0 | 0 | 0 | `1061A2DC` | [w](https://developer.valvesoftware.com/wiki/game_text) |
| `game_ui` | 0 | 0 | 0 | 0 | 0 | `1060B3B4` | [w](https://developer.valvesoftware.com/wiki/game_ui) |
| `game_weapon_manager` | 0 | 0 | 0 | 0 | 0 | `1060F49C` | [w](https://developer.valvesoftware.com/wiki/game_weapon_manager) |
| `game_zone_player` | 0 | 0 | 0 | 0 | 0 | `1061AC54` | [w](https://developer.valvesoftware.com/wiki/game_zone_player) |
| `gibshooter` | 0 | 0 | 0 | 0 | 0 | `105EDA44` | [w](https://developer.valvesoftware.com/wiki/gibshooter) |
| `hammer_updateignorelist` | 0 | 0 | 0 | 0 | 0 | `1065CA7C` | [w](https://developer.valvesoftware.com/wiki/hammer_updateignorelist) |
| `handle_dummy` | 0 | 0 | 0 | 0 | 0 | `10648B34` | [w](https://developer.valvesoftware.com/wiki/handle_dummy) |
| `info_ambient_mob_end` | 0 | 0 | 0 | 0 | 0 | `106DBA84` | [w](https://developer.valvesoftware.com/wiki/info_ambient_mob_end) |
| `info_ambient_mob_start` | 0 | 0 | 0 | 0 | 0 | `106DBA84` | [w](https://developer.valvesoftware.com/wiki/info_ambient_mob_start) |
| `info_constraint_anchor` | 0 | 0 | 0 | 0 | 0 | `10622374` | [w](https://developer.valvesoftware.com/wiki/info_constraint_anchor) |
| `info_goal_infected_chase` | 0 | 0 | 0 | 0 | 0 | `10677E74` | [w](https://developer.valvesoftware.com/wiki/info_goal_infected_chase) |
| `info_hang_lighting` | 0 | 0 | 0 | 0 | 0 | `105D517C` | [w](https://developer.valvesoftware.com/wiki/info_hang_lighting) |
| `info_infected_zoo_maker` | 0 | 0 | 0 | 0 | 0 | `10676F44` | [w](https://developer.valvesoftware.com/wiki/info_infected_zoo_maker) |
| `info_intermission` | 0 | 0 | 0 | 0 | 0 | `10610FAC` | [w](https://developer.valvesoftware.com/wiki/info_intermission) |
| `info_item_position` | 0 | 0 | 0 | 0 | 0 | `106781CC` | [w](https://developer.valvesoftware.com/wiki/info_item_position) |
| `info_l4d1_survivor_spawn` | 0 | 0 | 0 | 0 | 0 | `10677AB4` | [w](https://developer.valvesoftware.com/wiki/info_l4d1_survivor_spawn) |
| `info_map_parameters_versus` | 0 | 0 | 0 | 0 | 0 | `106844CC` | [w](https://developer.valvesoftware.com/wiki/info_map_parameters_versus) |
| `info_null` | 0 | 0 | 0 | 0 | 0 | `106455FC` | [w](https://developer.valvesoftware.com/wiki/info_null) |
| `info_player_deathmatch` | 0 | 0 | 0 | 0 | 0 | `10645944` | [w](https://developer.valvesoftware.com/wiki/info_player_deathmatch) |
| `info_player_logo` | 0 | 0 | 0 | 0 | 0 | `105D517C` | [w](https://developer.valvesoftware.com/wiki/info_player_logo) |
| `info_player_teamspawn` | 0 | 0 | 0 | 0 | 0 | `1064769C` | [w](https://developer.valvesoftware.com/wiki/info_player_teamspawn) |
| `info_projecteddecal` | 0 | 0 | 0 | 0 | 0 | `1065E54C` | [w](https://developer.valvesoftware.com/wiki/info_projecteddecal) |
| `info_vehicle_groundspawn` | 0 | 0 | 0 | 0 | 0 | `106479C4` | [w](https://developer.valvesoftware.com/wiki/info_vehicle_groundspawn) |
| `info_view_parameters` | 0 | 0 | 0 | 0 | 0 | `10663984` | [w](https://developer.valvesoftware.com/wiki/info_view_parameters) |
| `info_zombie_border` | 0 | 0 | 0 | 0 | 0 | `106F04E4` | [w](https://developer.valvesoftware.com/wiki/info_zombie_border) |
| `keyframe_track` | 0 | 0 | 0 | 0 | 0 | `1061C2BC` | [w](https://developer.valvesoftware.com/wiki/keyframe_track) |
| `light_glspot` | 0 | 0 | 0 | 0 | 0 | `10612744` | [w](https://developer.valvesoftware.com/wiki/light_glspot) |
| `logic_active_autosave` | 0 | 0 | 0 | 0 | 0 | `10616F1C` | [w](https://developer.valvesoftware.com/wiki/logic_active_autosave) |
| `logic_autosave` | 0 | 0 | 0 | 0 | 0 | `10614DFC` | [w](https://developer.valvesoftware.com/wiki/logic_autosave) |
| `logic_branch_listener` | 0 | 0 | 0 | 0 | 0 | `106179EC` | [w](https://developer.valvesoftware.com/wiki/logic_branch_listener) |
| `logic_compare` | 0 | 0 | 0 | 0 | 0 | `10616BF4` | [w](https://developer.valvesoftware.com/wiki/logic_compare) |
| `logic_lineto` | 0 | 0 | 0 | 0 | 0 | `10615904` | [w](https://developer.valvesoftware.com/wiki/logic_lineto) |
| `logic_multicompare` | 0 | 0 | 0 | 0 | 0 | `10614484` | [w](https://developer.valvesoftware.com/wiki/logic_multicompare) |
| `logic_navigation` | 0 | 0 | 0 | 0 | 0 | `10613504` | [w](https://developer.valvesoftware.com/wiki/logic_navigation) |
| `logic_proximity` | 0 | 0 | 0 | 0 | 0 | `105D517C` | [w](https://developer.valvesoftware.com/wiki/logic_proximity) |
| `logic_versus_random` | 0 | 0 | 0 | 0 | 0 | `1061739C` | [w](https://developer.valvesoftware.com/wiki/logic_versus_random) |
| `math_colorblend` | 0 | 0 | 0 | 0 | 0 | `10615F54` | [w](https://developer.valvesoftware.com/wiki/math_colorblend) |
| `math_remap` | 0 | 0 | 0 | 0 | 0 | `10615C2C` | [w](https://developer.valvesoftware.com/wiki/math_remap) |
| `move_keyframed` | 0 | 0 | 0 | 0 | 0 | `1061C5E4` | [w](https://developer.valvesoftware.com/wiki/move_keyframed) |
| `multisource` | 0 | 0 | 0 | 0 | 0 | `10614AD4` | [w](https://developer.valvesoftware.com/wiki/multisource) |
| `path_corner` | 0 | 0 | 0 | 0 | 0 | `1062003C` | [w](https://developer.valvesoftware.com/wiki/path_corner) |
| `path_corner_crash` | 0 | 0 | 0 | 0 | 0 | `10620364` | [w](https://developer.valvesoftware.com/wiki/path_corner_crash) |
| `phys_constraint` | 0 | 0 | 0 | 0 | 0 | `1062356C` | [w](https://developer.valvesoftware.com/wiki/phys_constraint) |
| `phys_constraintsystem` | 0 | 0 | 0 | 0 | 0 | `10622724` | [w](https://developer.valvesoftware.com/wiki/phys_constraintsystem) |
| `phys_convert` | 0 | 0 | 0 | 0 | 0 | `1062A224` | [w](https://developer.valvesoftware.com/wiki/phys_convert) |
| `phys_keepupright` | 0 | 0 | 0 | 0 | 0 | `10620FB4` | [w](https://developer.valvesoftware.com/wiki/phys_keepupright) |
| `phys_motor` | 0 | 0 | 0 | 0 | 0 | `1062132C` | [w](https://developer.valvesoftware.com/wiki/phys_motor) |
| `phys_pulleyconstraint` | 0 | 0 | 0 | 0 | 0 | `1062389C` | [w](https://developer.valvesoftware.com/wiki/phys_pulleyconstraint) |
| `phys_ragdollconstraint` | 0 | 0 | 0 | 0 | 0 | `10623EFC` | [w](https://developer.valvesoftware.com/wiki/phys_ragdollconstraint) |
| `phys_ragdollmagnet` | 0 | 0 | 0 | 0 | 0 | `105EA03C` | [w](https://developer.valvesoftware.com/wiki/phys_ragdollmagnet) |
| `phys_slideconstraint` | 0 | 0 | 0 | 0 | 0 | `1062323C` | [w](https://developer.valvesoftware.com/wiki/phys_slideconstraint) |
| `phys_torque` | 0 | 0 | 0 | 0 | 0 | `106219D4` | [w](https://developer.valvesoftware.com/wiki/phys_torque) |
| `physics_entity_solver` | 0 | 0 | 0 | 0 | 0 | `10626634` | [w](https://developer.valvesoftware.com/wiki/physics_entity_solver) |
| `physics_npc_solver` | 0 | 0 | 0 | 0 | 0 | `106262EC` | [w](https://developer.valvesoftware.com/wiki/physics_npc_solver) |
| `player_loadsaved` | 0 | 0 | 0 | 0 | 0 | `1062C5BC` | [w](https://developer.valvesoftware.com/wiki/player_loadsaved) |
| `player_pickup` | 0 | 0 | 0 | 0 | 0 | `10686ED4` | [w](https://developer.valvesoftware.com/wiki/player_pickup) |
| `player_speedmod` | 0 | 0 | 0 | 0 | 0 | `1062C8E4` | [w](https://developer.valvesoftware.com/wiki/player_speedmod) |
| `player_weaponstrip` | 0 | 0 | 0 | 0 | 0 | `1062C294` | [w](https://developer.valvesoftware.com/wiki/player_weaponstrip) |
| `point_anglesensor` | 0 | 0 | 0 | 0 | 0 | `1063212C` | [w](https://developer.valvesoftware.com/wiki/point_anglesensor) |
| `point_bonusmaps_accessor` | 0 | 0 | 0 | 0 | 0 | `1062F6E4` | [w](https://developer.valvesoftware.com/wiki/point_bonusmaps_accessor) |
| `point_broadcastclientcommand` | 0 | 0 | 0 | 0 | 0 | `105E6734` | [w](https://developer.valvesoftware.com/wiki/point_broadcastclientcommand) |
| `point_deathfall_camera` | 0 | 0 | 0 | 0 | 0 | `10667864` | [w](https://developer.valvesoftware.com/wiki/point_deathfall_camera) |
| `point_devshot_camera` | 0 | 0 | 0 | 0 | 0 | `1062FA8C` | [w](https://developer.valvesoftware.com/wiki/point_devshot_camera) |
| `point_enable_motion_fixup` | 0 | 0 | 0 | 0 | 0 | `10634EA4` | [w](https://developer.valvesoftware.com/wiki/point_enable_motion_fixup) |
| `point_entity_finder` | 0 | 0 | 0 | 0 | 0 | `1062FFB4` | [w](https://developer.valvesoftware.com/wiki/point_entity_finder) |
| `point_gamestats_counter` | 0 | 0 | 0 | 0 | 0 | `1060EB14` | [w](https://developer.valvesoftware.com/wiki/point_gamestats_counter) |
| `point_message` | 0 | 0 | 0 | 0 | 0 | `1061B8AC` | [w](https://developer.valvesoftware.com/wiki/point_message) |
| `point_nav_attribute_region` | 0 | 0 | 0 | 0 | 0 | `1068F6CC` | [w](https://developer.valvesoftware.com/wiki/point_nav_attribute_region) |
| `point_playermoveconstraint` | 0 | 0 | 0 | 0 | 0 | `1063039C` | [w](https://developer.valvesoftware.com/wiki/point_playermoveconstraint) |
| `point_proximity_sensor` | 0 | 0 | 0 | 0 | 0 | `10632454` | [w](https://developer.valvesoftware.com/wiki/point_proximity_sensor) |
| `point_push` | 0 | 0 | 0 | 0 | 0 | `10629B44` | [w](https://developer.valvesoftware.com/wiki/point_push) |
| `point_script_template` | 0 | 0 | 0 | 0 | 0 | `106318BC` | [w](https://developer.valvesoftware.com/wiki/point_script_template) |
| `point_surroundtest` | 0 | 0 | 0 | 0 | 0 | `10663D14` | [w](https://developer.valvesoftware.com/wiki/point_surroundtest) |
| `point_viewcontrol_survivor` | 0 | 0 | 0 | 0 | 0 | `106A052C` | [w](https://developer.valvesoftware.com/wiki/point_viewcontrol_survivor) |
| `scene_manager` | 0 | 0 | 0 | 0 | 0 | `1063D8F4` | [w](https://developer.valvesoftware.com/wiki/scene_manager) |
| `script_clip_vphysics` | 0 | 0 | 0 | 0 | 0 | `105E322C` | [w](https://developer.valvesoftware.com/wiki/script_clip_vphysics) |
| `script_nav_attribute_region` | 0 | 0 | 0 | 0 | 0 | `1068FA04` | [w](https://developer.valvesoftware.com/wiki/script_nav_attribute_region) |
| `script_nav_blocker` | 0 | 0 | 0 | 0 | 0 | `106EFE7C` | [w](https://developer.valvesoftware.com/wiki/script_nav_blocker) |
| `simple_physics_brush` | 0 | 0 | 0 | 0 | 0 | `10628DCC` | [w](https://developer.valvesoftware.com/wiki/simple_physics_brush) |
| `soundent` | 0 | 0 | 0 | 0 | 0 | `10641624` | [w](https://developer.valvesoftware.com/wiki/soundent) |
| `spark_shower` | 0 | 0 | 0 | 0 | 0 | `105FD634` | [w](https://developer.valvesoftware.com/wiki/spark_shower) |
| `spraycan` | 0 | 0 | 0 | 0 | 0 | `1062BC44` | [w](https://developer.valvesoftware.com/wiki/spraycan) |
| `tanktrain_ai` | 0 | 0 | 0 | 0 | 0 | `106464EC` | [w](https://developer.valvesoftware.com/wiki/tanktrain_ai) |
| `tanktrain_aitarget` | 0 | 0 | 0 | 0 | 0 | `10646854` | [w](https://developer.valvesoftware.com/wiki/tanktrain_aitarget) |
| `target_cdaudio` | 0 | 0 | 0 | 0 | 0 | `105E98DC` | [w](https://developer.valvesoftware.com/wiki/target_cdaudio) |
| `target_changegravity` | 0 | 0 | 0 | 0 | 0 | `105E9C04` | [w](https://developer.valvesoftware.com/wiki/target_changegravity) |
| `te_tester` | 0 | 0 | 0 | 0 | 0 | `106F6D5C` | [w](https://developer.valvesoftware.com/wiki/te_tester) |
| `test_effect` | 0 | 0 | 0 | 0 | 0 | `105EE0CC` | [w](https://developer.valvesoftware.com/wiki/test_effect) |
| `trigger_brush` | 0 | 0 | 0 | 0 | 0 | `1061BC84` | [w](https://developer.valvesoftware.com/wiki/trigger_brush) |
| `trigger_vphysics_motion` | 0 | 0 | 0 | 0 | 0 | `1064D29C` | [w](https://developer.valvesoftware.com/wiki/trigger_vphysics_motion) |
| `trigger_wind` | 0 | 0 | 0 | 0 | 0 | `1064E0FC` | [w](https://developer.valvesoftware.com/wiki/trigger_wind) |
| `vomit_particle` | 0 | 0 | 0 | 0 | 0 | `106A8624` | [w](https://developer.valvesoftware.com/wiki/vomit_particle) |

---

### 4.2 FORBIDDEN — has its own SendTable (320 classes — 108 present on a map, 212 not)

| class | toyz4 | chern | anemo | hive | **17 maps** | vtable | wiki |
|---|---:|---:|---:|---:|---:|---|---|
| **`env_sprite`** | 0 | 85 | 174 | 2280 | **2539** | `10643FA4` | [w](https://developer.valvesoftware.com/wiki/env_sprite) |
| **`prop_dynamic`** | 56 | 334 | 1577 | 229 | **2196** | `10636994` | [w](https://developer.valvesoftware.com/wiki/prop_dynamic) |
| **`prop_physics`** | 0 | 182 | 623 | 807 | **1612** | `10634744` | [w](https://developer.valvesoftware.com/wiki/prop_physics) |
| **`func_brush`** | 131 | 831 | 161 | 257 | **1380** | `10609E04` | [w](https://developer.valvesoftware.com/wiki/func_brush) |
| **`prop_physics_multiplayer`** | 0 | 512 | 726 | 0 | **1238** | `10634744` | [w](https://developer.valvesoftware.com/wiki/prop_physics_multiplayer) |
| **`info_particle_system`** | 0 | 82 | 209 | 466 | **757** | `1061F04C` | [w](https://developer.valvesoftware.com/wiki/info_particle_system) |
| **`keyframe_rope`** | 1 | 387 | 46 | 98 | **532** | `1063C37C` | [w](https://developer.valvesoftware.com/wiki/keyframe_rope) |
| **`func_simpleladder`** | 157 | 205 | 54 | 11 | **427** | `1060A4A4` | [w](https://developer.valvesoftware.com/wiki/func_simpleladder) |
| **`prop_door_rotating`** | 0 | 17 | 283 | 70 | **370** | `10638D8C` | [w](https://developer.valvesoftware.com/wiki/prop_door_rotating) |
| **`trigger_once`** | 53 | 13 | 131 | 48 | **245** | `1064F57C` | [w](https://developer.valvesoftware.com/wiki/trigger_once) |
| **`func_breakable`** | 33 | 9 | 104 | 77 | **223** | `10605324` | [w](https://developer.valvesoftware.com/wiki/func_breakable) |
| **`func_button`** | 94 | 42 | 47 | 33 | **216** | `105E3E6C` | [w](https://developer.valvesoftware.com/wiki/func_button) |
| **`trigger_hurt`** | 42 | 13 | 72 | 85 | **212** | `106523DC` | [w](https://developer.valvesoftware.com/wiki/trigger_hurt) |
| **`move_rope`** | 1 | 183 | 6 | 9 | **199** | `1063C37C` | [w](https://developer.valvesoftware.com/wiki/move_rope) |
| **`trigger_soundscape`** | 0 | 107 | 0 | 85 | **192** | `10643254` | [w](https://developer.valvesoftware.com/wiki/trigger_soundscape) |
| **`weapon_spawn`** | 27 | 98 | 6 | 54 | **185** | `1067A9C4` | [w](https://developer.valvesoftware.com/wiki/weapon_spawn) |
| **`weapon_first_aid_kit_spawn`** | 9 | 53 | 66 | 45 | **173** | `10680404` | [w](https://developer.valvesoftware.com/wiki/weapon_first_aid_kit_spawn) |
| **`trigger_teleport`** | 58 | 0 | 88 | 0 | **146** | `1064FC1C` | [w](https://developer.valvesoftware.com/wiki/trigger_teleport) |
| **`weapon_melee_spawn`** | 2 | 44 | 51 | 37 | **134** | `1067A18C` | [w](https://developer.valvesoftware.com/wiki/weapon_melee_spawn) |
| **`prop_physics_override`** | 0 | 131 | 2 | 0 | **133** | `10634744` | [w](https://developer.valvesoftware.com/wiki/prop_physics_override) |
| **`func_movelinear`** | 0 | 0 | 12 | 114 | **126** | `1060967C` | [w](https://developer.valvesoftware.com/wiki/func_movelinear) |
| **`func_dustmotes`** | 0 | 0 | 120 | 1 | **121** | `106077D4` | [w](https://developer.valvesoftware.com/wiki/func_dustmotes) |
| **`func_rotating`** | 35 | 2 | 12 | 69 | **118** | `105E2B6C` | [w](https://developer.valvesoftware.com/wiki/func_rotating) |
| **`prop_ragdoll`** | 0 | 0 | 13 | 100 | **113** | `10626A94` | [w](https://developer.valvesoftware.com/wiki/prop_ragdoll) |
| **`prop_dynamic_override`** | 48 | 11 | 44 | 0 | **103** | `10636994` | [w](https://developer.valvesoftware.com/wiki/prop_dynamic_override) |
| **`func_tracktrain`** | 71 | 3 | 5 | 12 | **91** | `1064B8FC` | [w](https://developer.valvesoftware.com/wiki/func_tracktrain) |
| **`trigger_multiple`** | 24 | 4 | 25 | 37 | **90** | `1064E774` | [w](https://developer.valvesoftware.com/wiki/trigger_multiple) |
| **`weapon_pain_pills_spawn`** | 0 | 8 | 64 | 14 | **86** | `1068081C` | [w](https://developer.valvesoftware.com/wiki/weapon_pain_pills_spawn) |
| **`weapon_ammo_spawn`** | 16 | 35 | 15 | 17 | **83** | `10682CF4` | [w](https://developer.valvesoftware.com/wiki/weapon_ammo_spawn) |
| **`env_steam`** | 0 | 0 | 0 | 81 | **81** | `106F9F0C` | [w](https://developer.valvesoftware.com/wiki/env_steam) |
| **`func_physbox`** | 20 | 2 | 6 | 44 | **72** | `1062A61C` | [w](https://developer.valvesoftware.com/wiki/func_physbox) |
| **`func_areaportalwindow`** | 0 | 63 | 0 | 0 | **63** | `10604784` | [w](https://developer.valvesoftware.com/wiki/func_areaportalwindow) |
| **`weapon_pistol_spawn`** | 0 | 41 | 0 | 22 | **63** | `1067ADDC` | [w](https://developer.valvesoftware.com/wiki/weapon_pistol_spawn) |
| **`func_precipitation`** | 0 | 1 | 1 | 57 | **59** | `105F1A3C` | [w](https://developer.valvesoftware.com/wiki/func_precipitation) |
| **`weapon_adrenaline_spawn`** | 1 | 14 | 36 | 6 | **57** | `10680C34` | [w](https://developer.valvesoftware.com/wiki/weapon_adrenaline_spawn) |
| **`func_button_timed`** | 1 | 1 | 35 | 18 | **55** | `105E4B3C` | [w](https://developer.valvesoftware.com/wiki/func_button_timed) |
| **`weapon_molotov_spawn`** | 22 | 30 | 0 | 2 | **54** | `1067F78C` | [w](https://developer.valvesoftware.com/wiki/weapon_molotov_spawn) |
| **`info_survivor_position`** | 4 | 20 | 3 | 24 | **51** | `1068BEE4` | [w](https://developer.valvesoftware.com/wiki/info_survivor_position) |
| **`func_door`** | 13 | 10 | 14 | 10 | **47** | `105EA974` | [w](https://developer.valvesoftware.com/wiki/func_door) |
| **`weapon_pipe_bomb_spawn`** | 12 | 21 | 2 | 3 | **38** | `1067F374` | [w](https://developer.valvesoftware.com/wiki/weapon_pipe_bomb_spawn) |
| **`color_correction`** | 0 | 28 | 8 | 0 | **36** | `105E7A44` | [w](https://developer.valvesoftware.com/wiki/color_correction) |
| **`beam_spotlight`** | 4 | 0 | 3 | 24 | **31** | `105E189C` | [w](https://developer.valvesoftware.com/wiki/beam_spotlight) |
| **`weapon_rifle_ak47_spawn`** | 0 | 8 | 18 | 5 | **31** | `1067C254` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_ak47_spawn) |
| **`func_door_rotating`** | 6 | 0 | 1 | 23 | **30** | `105EACFC` | [w](https://developer.valvesoftware.com/wiki/func_door_rotating) |
| **`env_fog_controller`** | 0 | 13 | 11 | 5 | **29** | `10602C64` | [w](https://developer.valvesoftware.com/wiki/env_fog_controller) |
| **`weapon_pistol_magnum_spawn`** | 0 | 4 | 14 | 11 | **29** | `1067CE9C` | [w](https://developer.valvesoftware.com/wiki/weapon_pistol_magnum_spawn) |
| **`postprocess_controller`** | 0 | 27 | 0 | 0 | **27** | `10633154` | [w](https://developer.valvesoftware.com/wiki/postprocess_controller) |
| **`weapon_defibrillator_spawn`** | 2 | 8 | 12 | 5 | **27** | `1068104C` | [w](https://developer.valvesoftware.com/wiki/weapon_defibrillator_spawn) |
| **`prop_door_rotating_checkpoint`** | 0 | 8 | 10 | 8 | **26** | `10639774` | [w](https://developer.valvesoftware.com/wiki/prop_door_rotating_checkpoint) |
| **`weapon_rifle_spawn`** | 0 | 0 | 18 | 7 | **25** | `1067B60C` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_spawn) |
| **`weapon_shotgun_spas_spawn`** | 0 | 0 | 17 | 5 | **22** | `1067CA84` | [w](https://developer.valvesoftware.com/wiki/weapon_shotgun_spas_spawn) |
| **`water_lod_control`** | 0 | 7 | 5 | 5 | **17** | `1065C6A4` | [w](https://developer.valvesoftware.com/wiki/water_lod_control) |
| **`worldspawn`** | 1 | 5 | 6 | 5 | **17** | `1065DB7C` | [w](https://developer.valvesoftware.com/wiki/worldspawn) |
| **`shadow_control`** | 0 | 5 | 6 | 5 | **16** | `1063FBBC` | [w](https://developer.valvesoftware.com/wiki/shadow_control) |
| **`trigger_gravity`** | 0 | 0 | 16 | 0 | **16** | `1065060C` | [w](https://developer.valvesoftware.com/wiki/trigger_gravity) |
| **`env_tonemap_controller`** | 0 | 5 | 6 | 4 | **15** | `105F910C` | [w](https://developer.valvesoftware.com/wiki/env_tonemap_controller) |
| **`weapon_rifle_m60_spawn`** | 2 | 0 | 6 | 7 | **15** | `106828DC` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_m60_spawn) |
| **`weapon_scavenge_item_spawn`** | 0 | 13 | 2 | 0 | **15** | `1067A5AC` | [w](https://developer.valvesoftware.com/wiki/weapon_scavenge_item_spawn) |
| **`weapon_vomitjar_spawn`** | 6 | 8 | 1 | 0 | **15** | `1067FBA4` | [w](https://developer.valvesoftware.com/wiki/weapon_vomitjar_spawn) |
| **`env_screenoverlay`** | 0 | 0 | 0 | 13 | **13** | `105F7FB4` | [w](https://developer.valvesoftware.com/wiki/env_screenoverlay) |
| **`info_changelevel`** | 0 | 3 | 6 | 4 | **13** | `10666C2C` | [w](https://developer.valvesoftware.com/wiki/info_changelevel) |
| **`weapon_chainsaw_spawn`** | 1 | 1 | 4 | 7 | **13** | `106820AC` | [w](https://developer.valvesoftware.com/wiki/weapon_chainsaw_spawn) |
| **`func_breakable_surf`** | 0 | 12 | 0 | 0 | **12** | `10606794` | [w](https://developer.valvesoftware.com/wiki/func_breakable_surf) |
| **`func_tanktrain`** | 0 | 0 | 12 | 0 | **12** | `10646B7C` | [w](https://developer.valvesoftware.com/wiki/func_tanktrain) |
| **`env_tonemap_controller_ghost`** | 0 | 5 | 6 | 0 | **11** | `105F910C` | [w](https://developer.valvesoftware.com/wiki/env_tonemap_controller_ghost) |
| **`env_tonemap_controller_infected`** | 0 | 5 | 6 | 0 | **11** | `105F910C` | [w](https://developer.valvesoftware.com/wiki/env_tonemap_controller_infected) |
| **`env_weaponfire`** | 11 | 0 | 0 | 0 | **11** | `1067432C` | [w](https://developer.valvesoftware.com/wiki/env_weaponfire) |
| **`env_lightglow`** | 4 | 4 | 2 | 0 | **10** | `1061231C` | [w](https://developer.valvesoftware.com/wiki/env_lightglow) |
| **`func_playerinfected_clip`** | 0 | 10 | 0 | 0 | **10** | `1064D5C4` | [w](https://developer.valvesoftware.com/wiki/func_playerinfected_clip) |
| **`prop_minigun_l4d1`** | 10 | 0 | 0 | 0 | **10** | `106C76F4` | [w](https://developer.valvesoftware.com/wiki/prop_minigun_l4d1) |
| **`weapon_autoshotgun_spawn`** | 0 | 0 | 3 | 7 | **10** | `1067B1F4` | [w](https://developer.valvesoftware.com/wiki/weapon_autoshotgun_spawn) |
| **`weapon_rifle_sg552_spawn`** | 0 | 0 | 10 | 0 | **10** | `1067E72C` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_sg552_spawn) |
| **`func_playerghostinfected_clip`** | 0 | 9 | 0 | 0 | **9** | `1064D8EC` | [w](https://developer.valvesoftware.com/wiki/func_playerghostinfected_clip) |
| **`trigger_auto_crouch`** | 0 | 0 | 0 | 9 | **9** | `106519EC` | [w](https://developer.valvesoftware.com/wiki/trigger_auto_crouch) |
| **`weapon_hunting_rifle_spawn`** | 2 | 0 | 4 | 3 | **9** | `1067BA24` | [w](https://developer.valvesoftware.com/wiki/weapon_hunting_rifle_spawn) |
| **`env_laser`** | 0 | 0 | 0 | 8 | **8** | `105FA9D4` | [w](https://developer.valvesoftware.com/wiki/env_laser) |
| **`func_rot_button`** | 0 | 0 | 8 | 0 | **8** | `105E44CC` | [w](https://developer.valvesoftware.com/wiki/func_rot_button) |
| **`weapon_first_aid_kit`** | 0 | 8 | 0 | 0 | **8** | `106C01C4` | [w](https://developer.valvesoftware.com/wiki/weapon_first_aid_kit) |
| **`weapon_rifle_desert_spawn`** | 0 | 0 | 3 | 5 | **8** | `1067BE3C` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_desert_spawn) |
| **`momentary_rot_button`** | 0 | 1 | 6 | 0 | **7** | `105E47FC` | [w](https://developer.valvesoftware.com/wiki/momentary_rot_button) |
| **`trigger_finale`** | 1 | 1 | 3 | 2 | **7** | `1067514C` | [w](https://developer.valvesoftware.com/wiki/trigger_finale) |
| **`weapon_smg_spawn`** | 0 | 2 | 2 | 3 | **7** | `1067D2B4` | [w](https://developer.valvesoftware.com/wiki/weapon_smg_spawn) |
| **`env_sun`** | 0 | 5 | 1 | 0 | **6** | `10645DCC` | [w](https://developer.valvesoftware.com/wiki/env_sun) |
| **`func_precipitation_blocker`** | 0 | 2 | 0 | 4 | **6** | `105ECDC4` | [w](https://developer.valvesoftware.com/wiki/func_precipitation_blocker) |
| **`trigger_push`** | 0 | 3 | 3 | 0 | **6** | `1064F8CC` | [w](https://developer.valvesoftware.com/wiki/trigger_push) |
| **`weapon_shotgun_chrome_spawn`** | 0 | 1 | 2 | 3 | **6** | `1067DEFC` | [w](https://developer.valvesoftware.com/wiki/weapon_shotgun_chrome_spawn) |
| **`weapon_sniper_military_spawn`** | 0 | 0 | 4 | 2 | **6** | `1067C66C` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_military_spawn) |
| **`env_wind`** | 0 | 5 | 0 | 0 | **5** | `105F0F9C` | [w](https://developer.valvesoftware.com/wiki/env_wind) |
| **`light_dynamic`** | 0 | 0 | 1 | 4 | **5** | `105EC504` | [w](https://developer.valvesoftware.com/wiki/light_dynamic) |
| **`prop_mounted_machine_gun`** | 2 | 2 | 1 | 0 | **5** | `106C8D8C` | [w](https://developer.valvesoftware.com/wiki/prop_mounted_machine_gun) |
| **`func_elevator`** | 2 | 0 | 1 | 1 | **4** | `106082BC` | [w](https://developer.valvesoftware.com/wiki/func_elevator) |
| **`logic_choreographed_scene`** | 0 | 4 | 0 | 0 | **4** | `1063DCA4` | [w](https://developer.valvesoftware.com/wiki/logic_choreographed_scene) |
| **`point_prop_use_target`** | 0 | 1 | 3 | 0 | **4** | `106BC43C` | [w](https://developer.valvesoftware.com/wiki/point_prop_use_target) |
| **`weapon_grenade_launcher_spawn`** | 0 | 0 | 0 | 4 | **4** | `106824C4` | [w](https://developer.valvesoftware.com/wiki/weapon_grenade_launcher_spawn) |
| **`weapon_smg_silenced_spawn`** | 0 | 1 | 0 | 3 | **4** | `1067DAE4` | [w](https://developer.valvesoftware.com/wiki/weapon_smg_silenced_spawn) |
| **`env_smokestack`** | 0 | 3 | 0 | 0 | **3** | `106F9534` | [w](https://developer.valvesoftware.com/wiki/env_smokestack) |
| **`sound_mix_layer`** | 0 | 3 | 0 | 0 | **3** | `1068BB6C` | [w](https://developer.valvesoftware.com/wiki/sound_mix_layer) |
| **`weapon_upgradepack_explosive_spawn`** | 1 | 2 | 0 | 0 | **3** | `10681C94` | [w](https://developer.valvesoftware.com/wiki/weapon_upgradepack_explosive_spawn) |
| **`trigger_playermovement`** | 0 | 0 | 0 | 2 | **2** | `1065134C` | [w](https://developer.valvesoftware.com/wiki/trigger_playermovement) |
| **`weapon_sniper_awp_spawn`** | 0 | 0 | 2 | 0 | **2** | `1067EB44` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_awp_spawn) |
| **`env_detail_controller`** | 0 | 1 | 0 | 0 | **1** | `105F4D2C` | [w](https://developer.valvesoftware.com/wiki/env_detail_controller) |
| **`func_orator`** | 0 | 1 | 0 | 0 | **1** | `106759F4` | [w](https://developer.valvesoftware.com/wiki/func_orator) |
| **`prop_fuel_barrel`** | 0 | 1 | 0 | 0 | **1** | `10688974` | [w](https://developer.valvesoftware.com/wiki/prop_fuel_barrel) |
| **`prop_minigun`** | 1 | 0 | 0 | 0 | **1** | `106C8D8C` | [w](https://developer.valvesoftware.com/wiki/prop_minigun) |
| **`trigger_changelevel`** | 0 | 1 | 0 | 0 | **1** | `10666C2C` | [w](https://developer.valvesoftware.com/wiki/trigger_changelevel) |
| **`weapon_gascan_spawn`** | 0 | 1 | 0 | 0 | **1** | `10681464` | [w](https://developer.valvesoftware.com/wiki/weapon_gascan_spawn) |
| **`weapon_pumpshotgun_spawn`** | 0 | 0 | 0 | 1 | **1** | `1067D6CC` | [w](https://developer.valvesoftware.com/wiki/weapon_pumpshotgun_spawn) |
| **`weapon_upgradepack_incendiary_spawn`** | 0 | 1 | 0 | 0 | **1** | `1068187C` | [w](https://developer.valvesoftware.com/wiki/weapon_upgradepack_incendiary_spawn) |
| `_firesmoke` | 0 | 0 | 0 | 0 | 0 | `10601FF4` | [w](https://developer.valvesoftware.com/wiki/_firesmoke) |
| `_plasma` | 0 | 0 | 0 | 0 | 0 | `106F7B34` | [w](https://developer.valvesoftware.com/wiki/_plasma) |
| `ability_charge` | 0 | 0 | 0 | 0 | 0 | `106A3F5C` | [w](https://developer.valvesoftware.com/wiki/ability_charge) |
| `ability_leap` | 0 | 0 | 0 | 0 | 0 | `106A45FC` | [w](https://developer.valvesoftware.com/wiki/ability_leap) |
| `ability_lunge` | 0 | 0 | 0 | 0 | 0 | `106A4B54` | [w](https://developer.valvesoftware.com/wiki/ability_lunge) |
| `ability_selfdestruct` | 0 | 0 | 0 | 0 | 0 | `106A5044` | [w](https://developer.valvesoftware.com/wiki/ability_selfdestruct) |
| `ability_spit` | 0 | 0 | 0 | 0 | 0 | `106A542C` | [w](https://developer.valvesoftware.com/wiki/ability_spit) |
| `ability_throw` | 0 | 0 | 0 | 0 | 0 | `106A590C` | [w](https://developer.valvesoftware.com/wiki/ability_throw) |
| `ability_tongue` | 0 | 0 | 0 | 0 | 0 | `106A6C34` | [w](https://developer.valvesoftware.com/wiki/ability_tongue) |
| `ability_vomit` | 0 | 0 | 0 | 0 | 0 | `106A8204` | [w](https://developer.valvesoftware.com/wiki/ability_vomit) |
| `beam` | 0 | 0 | 0 | 0 | 0 | `105E138C` | [w](https://developer.valvesoftware.com/wiki/beam) |
| `boomer` | 0 | 0 | 0 | 0 | 0 | `106E1B44` | [w](https://developer.valvesoftware.com/wiki/boomer) |
| `charger` | 0 | 0 | 0 | 0 | 0 | `106E3CCC` | [w](https://developer.valvesoftware.com/wiki/charger) |
| `client_path` | 0 | 0 | 0 | 0 | 0 | `10674B04` | [w](https://developer.valvesoftware.com/wiki/client_path) |
| `color_correction_volume` | 0 | 0 | 0 | 0 | 0 | `105E7F4C` | [w](https://developer.valvesoftware.com/wiki/color_correction_volume) |
| `commentary_dummy` | 0 | 0 | 0 | 0 | 0 | `106A96A4` | [w](https://developer.valvesoftware.com/wiki/commentary_dummy) |
| `cs_gamerules` | 0 | 0 | 0 | 0 | 0 | `1065FA9C` | [w](https://developer.valvesoftware.com/wiki/cs_gamerules) |
| `cs_ragdoll` | 0 | 0 | 0 | 0 | 0 | `106617D4` | [w](https://developer.valvesoftware.com/wiki/cs_ragdoll) |
| `cs_team_manager` | 0 | 0 | 0 | 0 | 0 | `10663074` | [w](https://developer.valvesoftware.com/wiki/cs_team_manager) |
| `cycler_flex` | 0 | 0 | 0 | 0 | 0 | `105DE8D4` | [w](https://developer.valvesoftware.com/wiki/cycler_flex) |
| `dynamic_prop` | 0 | 0 | 0 | 0 | 0 | `10636994` | [w](https://developer.valvesoftware.com/wiki/dynamic_prop) |
| `entityflame` | 0 | 0 | 0 | 0 | 0 | `105F3504` | [w](https://developer.valvesoftware.com/wiki/entityflame) |
| `env_airstrike_indoors` | 0 | 0 | 0 | 0 | 0 | `105F050C` | [w](https://developer.valvesoftware.com/wiki/env_airstrike_indoors) |
| `env_airstrike_outdoors` | 0 | 0 | 0 | 0 | 0 | `105F08C4` | [w](https://developer.valvesoftware.com/wiki/env_airstrike_outdoors) |
| `env_beam` | 0 | 0 | 0 | 0 | 0 | `105F9BF4` | [w](https://developer.valvesoftware.com/wiki/env_beam) |
| `env_dof_controller` | 0 | 0 | 0 | 0 | 0 | `105F50EC` | [w](https://developer.valvesoftware.com/wiki/env_dof_controller) |
| `env_dusttrail` | 0 | 0 | 0 | 0 | 0 | `106F8B34` | [w](https://developer.valvesoftware.com/wiki/env_dusttrail) |
| `env_effectscript` | 0 | 0 | 0 | 0 | 0 | `105F562C` | [w](https://developer.valvesoftware.com/wiki/env_effectscript) |
| `env_embers` | 0 | 0 | 0 | 0 | 0 | `105F12C4` | [w](https://developer.valvesoftware.com/wiki/env_embers) |
| `env_entity_dissolver` | 0 | 0 | 0 | 0 | 0 | `105F2C44` | [w](https://developer.valvesoftware.com/wiki/env_entity_dissolver) |
| `env_fire_trail` | 0 | 0 | 0 | 0 | 0 | `106F818C` | [w](https://developer.valvesoftware.com/wiki/env_fire_trail) |
| `env_glow` | 0 | 0 | 0 | 0 | 0 | `10643FA4` | [w](https://developer.valvesoftware.com/wiki/env_glow) |
| `env_movieexplosion` | 0 | 0 | 0 | 0 | 0 | `106F7094` | [w](https://developer.valvesoftware.com/wiki/env_movieexplosion) |
| `env_particle_performance_monitor` | 0 | 0 | 0 | 0 | 0 | `1064483C` | [w](https://developer.valvesoftware.com/wiki/env_particle_performance_monitor) |
| `env_particle_trail` | 0 | 0 | 0 | 0 | 0 | `105F3C7C` | [w](https://developer.valvesoftware.com/wiki/env_particle_trail) |
| `env_particlefire` | 0 | 0 | 0 | 0 | 0 | `106F742C` | [w](https://developer.valvesoftware.com/wiki/env_particlefire) |
| `env_particlescript` | 0 | 0 | 0 | 0 | 0 | `105F6CE4` | [w](https://developer.valvesoftware.com/wiki/env_particlescript) |
| `env_particlesmokegrenade` | 0 | 0 | 0 | 0 | 0 | `106F777C` | [w](https://developer.valvesoftware.com/wiki/env_particlesmokegrenade) |
| `env_physics_blocker` | 0 | 0 | 0 | 0 | 0 | `105F44CC` | [w](https://developer.valvesoftware.com/wiki/env_physics_blocker) |
| `env_projectedtexture` | 0 | 0 | 0 | 0 | 0 | `105F76B4` | [w](https://developer.valvesoftware.com/wiki/env_projectedtexture) |
| `env_quadraticbeam` | 0 | 0 | 0 | 0 | 0 | `105F16DC` | [w](https://developer.valvesoftware.com/wiki/env_quadraticbeam) |
| `env_rockettrail` | 0 | 0 | 0 | 0 | 0 | `106F880C` | [w](https://developer.valvesoftware.com/wiki/env_rockettrail) |
| `env_screeneffect` | 0 | 0 | 0 | 0 | 0 | `105F7C2C` | [w](https://developer.valvesoftware.com/wiki/env_screeneffect) |
| `env_smoketrail` | 0 | 0 | 0 | 0 | 0 | `106F84C4` | [w](https://developer.valvesoftware.com/wiki/env_smoketrail) |
| `env_sporeexplosion` | 0 | 0 | 0 | 0 | 0 | `106F91EC` | [w](https://developer.valvesoftware.com/wiki/env_sporeexplosion) |
| `env_sporetrail` | 0 | 0 | 0 | 0 | 0 | `106F8EC4` | [w](https://developer.valvesoftware.com/wiki/env_sporetrail) |
| `env_sprite_oriented` | 0 | 0 | 0 | 0 | 0 | `106442CC` | [w](https://developer.valvesoftware.com/wiki/env_sprite_oriented) |
| `env_spritetrail` | 0 | 0 | 0 | 0 | 0 | `106F9B6C` | [w](https://developer.valvesoftware.com/wiki/env_spritetrail) |
| `env_steamjet` | 0 | 0 | 0 | 0 | 0 | `106F9F0C` | [w](https://developer.valvesoftware.com/wiki/env_steamjet) |
| `finale_trigger` | 0 | 0 | 0 | 0 | 0 | `1067514C` | [w](https://developer.valvesoftware.com/wiki/finale_trigger) |
| `fire_cracker_blast` | 0 | 0 | 0 | 0 | 0 | `106AA964` | [w](https://developer.valvesoftware.com/wiki/fire_cracker_blast) |
| `fish` | 0 | 0 | 0 | 0 | 0 | `106023CC` | [w](https://developer.valvesoftware.com/wiki/fish) |
| `funCBaseFlex` | 0 | 0 | 0 | 0 | 0 | `105DE194` | [w](https://developer.valvesoftware.com/wiki/funCBaseFlex) |
| `func_block_charge` | 0 | 0 | 0 | 0 | 0 | `10609E04` | [w](https://developer.valvesoftware.com/wiki/func_block_charge) |
| `func_conveyor` | 0 | 0 | 0 | 0 | 0 | `105E3554` | [w](https://developer.valvesoftware.com/wiki/func_conveyor) |
| `func_dustcloud` | 0 | 0 | 0 | 0 | 0 | `106074AC` | [w](https://developer.valvesoftware.com/wiki/func_dustcloud) |
| `func_extinguisher` | 0 | 0 | 0 | 0 | 0 | `106533CC` | [w](https://developer.valvesoftware.com/wiki/func_extinguisher) |
| `func_guntarget` | 0 | 0 | 0 | 0 | 0 | `106102DC` | [w](https://developer.valvesoftware.com/wiki/func_guntarget) |
| `func_lod` | 0 | 0 | 0 | 0 | 0 | `1060924C` | [w](https://developer.valvesoftware.com/wiki/func_lod) |
| `func_occluder` | 0 | 0 | 0 | 0 | 0 | `10609A8C` | [w](https://developer.valvesoftware.com/wiki/func_occluder) |
| `func_physbox_multiplayer` | 0 | 0 | 0 | 0 | 0 | `1063556C` | [w](https://developer.valvesoftware.com/wiki/func_physbox_multiplayer) |
| `func_plat` | 0 | 0 | 0 | 0 | 0 | `1064A7EC` | [w](https://developer.valvesoftware.com/wiki/func_plat) |
| `func_platrot` | 0 | 0 | 0 | 0 | 0 | `1064AF2C` | [w](https://developer.valvesoftware.com/wiki/func_platrot) |
| `func_pushable` | 0 | 0 | 0 | 0 | 0 | `1060571C` | [w](https://developer.valvesoftware.com/wiki/func_pushable) |
| `func_ragdoll_fader` | 0 | 0 | 0 | 0 | 0 | `1069E40C` | [w](https://developer.valvesoftware.com/wiki/func_ragdoll_fader) |
| `func_reflective_glass` | 0 | 0 | 0 | 0 | 0 | `1060A12C` | [w](https://developer.valvesoftware.com/wiki/func_reflective_glass) |
| `func_smokevolume` | 0 | 0 | 0 | 0 | 0 | `1060A934` | [w](https://developer.valvesoftware.com/wiki/func_smokevolume) |
| `func_trackautochange` | 0 | 0 | 0 | 0 | 0 | `1064B5AC` | [w](https://developer.valvesoftware.com/wiki/func_trackautochange) |
| `func_trackchange` | 0 | 0 | 0 | 0 | 0 | `1064B26C` | [w](https://developer.valvesoftware.com/wiki/func_trackchange) |
| `func_train` | 0 | 0 | 0 | 0 | 0 | `1064AB2C` | [w](https://developer.valvesoftware.com/wiki/func_train) |
| `func_useableladder` | 0 | 0 | 0 | 0 | 0 | `106089BC` | [w](https://developer.valvesoftware.com/wiki/func_useableladder) |
| `func_water` | 0 | 0 | 0 | 0 | 0 | `105EA974` | [w](https://developer.valvesoftware.com/wiki/func_water) |
| `func_water_analog` | 0 | 0 | 0 | 0 | 0 | `1060967C` | [w](https://developer.valvesoftware.com/wiki/func_water_analog) |
| `game_ragdoll_manager` | 0 | 0 | 0 | 0 | 0 | `1063B8A4` | [w](https://developer.valvesoftware.com/wiki/game_ragdoll_manager) |
| `game_scavenge_progress_display` | 0 | 0 | 0 | 0 | 0 | `1068A8BC` | [w](https://developer.valvesoftware.com/wiki/game_scavenge_progress_display) |
| `gib` | 0 | 0 | 0 | 0 | 0 | `1060FC94` | [w](https://developer.valvesoftware.com/wiki/gib) |
| `grenade` | 0 | 0 | 0 | 0 | 0 | `105DF07C` | [w](https://developer.valvesoftware.com/wiki/grenade) |
| `grenade_launcher_projectile` | 0 | 0 | 0 | 0 | 0 | `106C23C4` | [w](https://developer.valvesoftware.com/wiki/grenade_launcher_projectile) |
| `handle_test` | 0 | 0 | 0 | 0 | 0 | `10648E84` | [w](https://developer.valvesoftware.com/wiki/handle_test) |
| `holiday_gift` | 0 | 0 | 0 | 0 | 0 | `1067636C` | [w](https://developer.valvesoftware.com/wiki/holiday_gift) |
| `inferno` | 0 | 0 | 0 | 0 | 0 | `106AA2D4` | [w](https://developer.valvesoftware.com/wiki/inferno) |
| `info_infected_zoo_puppet` | 0 | 0 | 0 | 0 | 0 | `10676914` | [w](https://developer.valvesoftware.com/wiki/info_infected_zoo_puppet) |
| `info_ladder` | 0 | 0 | 0 | 0 | 0 | `1060A4A4` | [w](https://developer.valvesoftware.com/wiki/info_ladder) |
| `info_ladder_dismount` | 0 | 0 | 0 | 0 | 0 | `1060862C` | [w](https://developer.valvesoftware.com/wiki/info_ladder_dismount) |
| `info_overlay_accessor` | 0 | 0 | 0 | 0 | 0 | `10610C2C` | [w](https://developer.valvesoftware.com/wiki/info_overlay_accessor) |
| `insect_swarm` | 0 | 0 | 0 | 0 | 0 | `106AA61C` | [w](https://developer.valvesoftware.com/wiki/insect_swarm) |
| `instanced_scripted_scene` | 0 | 0 | 0 | 0 | 0 | `1063EBDC` | [w](https://developer.valvesoftware.com/wiki/instanced_scripted_scene) |
| `item_sodacan` | 0 | 0 | 0 | 0 | 0 | `105EEDAC` | [w](https://developer.valvesoftware.com/wiki/item_sodacan) |
| `material_modify_control` | 0 | 0 | 0 | 0 | 0 | `1061B344` | [w](https://developer.valvesoftware.com/wiki/material_modify_control) |
| `molotov_projectile` | 0 | 0 | 0 | 0 | 0 | `106C85EC` | [w](https://developer.valvesoftware.com/wiki/molotov_projectile) |
| `momentary_door` | 0 | 0 | 0 | 0 | 0 | `1060967C` | [w](https://developer.valvesoftware.com/wiki/momentary_door) |
| `phys_bone_follower` | 0 | 0 | 0 | 0 | 0 | `10625084` | [w](https://developer.valvesoftware.com/wiki/phys_bone_follower) |
| `phys_magnet` | 0 | 0 | 0 | 0 | 0 | `1062A94C` | [w](https://developer.valvesoftware.com/wiki/phys_magnet) |
| `physics_cannister` | 0 | 0 | 0 | 0 | 0 | `106254EC` | [w](https://developer.valvesoftware.com/wiki/physics_cannister) |
| `physics_prop` | 0 | 0 | 0 | 0 | 0 | `10634744` | [w](https://developer.valvesoftware.com/wiki/physics_prop) |
| `physics_prop_ragdoll` | 0 | 0 | 0 | 0 | 0 | `10626A94` | [w](https://developer.valvesoftware.com/wiki/physics_prop_ragdoll) |
| `pipe_bomb_projectile` | 0 | 0 | 0 | 0 | 0 | `106CAC0C` | [w](https://developer.valvesoftware.com/wiki/pipe_bomb_projectile) |
| `player_manager` | 0 | 0 | 0 | 0 | 0 | `1062EC54` | [w](https://developer.valvesoftware.com/wiki/player_manager) |
| `point_commentary_node` | 0 | 0 | 0 | 0 | 0 | `105E88D4` | [w](https://developer.valvesoftware.com/wiki/point_commentary_node) |
| `point_commentary_viewpoint` | 0 | 0 | 0 | 0 | 0 | `105E84DC` | [w](https://developer.valvesoftware.com/wiki/point_commentary_viewpoint) |
| `point_posecontroller` | 0 | 0 | 0 | 0 | 0 | `1063089C` | [w](https://developer.valvesoftware.com/wiki/point_posecontroller) |
| `point_script_use_target` | 0 | 0 | 0 | 0 | 0 | `10688124` | [w](https://developer.valvesoftware.com/wiki/point_script_use_target) |
| `predicted_viewmodel` | 0 | 0 | 0 | 0 | 0 | `1069F3BC` | [w](https://developer.valvesoftware.com/wiki/predicted_viewmodel) |
| `prop_car_alarm` | 0 | 0 | 0 | 0 | 0 | `106373E4` | [w](https://developer.valvesoftware.com/wiki/prop_car_alarm) |
| `prop_car_glass` | 0 | 0 | 0 | 0 | 0 | `1063787C` | [w](https://developer.valvesoftware.com/wiki/prop_car_glass) |
| `prop_dynamic_ornament` | 0 | 0 | 0 | 0 | 0 | `10637D14` | [w](https://developer.valvesoftware.com/wiki/prop_dynamic_ornament) |
| `prop_fuel_barrel_piece` | 0 | 0 | 0 | 0 | 0 | `10688E34` | [w](https://developer.valvesoftware.com/wiki/prop_fuel_barrel_piece) |
| `prop_health_cabinet` | 0 | 0 | 0 | 0 | 0 | `1068965C` | [w](https://developer.valvesoftware.com/wiki/prop_health_cabinet) |
| `prop_physics2` | 0 | 0 | 0 | 0 | 0 | `10633E6C` | [w](https://developer.valvesoftware.com/wiki/prop_physics2) |
| `prop_physics_respawnable` | 0 | 0 | 0 | 0 | 0 | `10636F34` | [w](https://developer.valvesoftware.com/wiki/prop_physics_respawnable) |
| `prop_ragdoll_attached` | 0 | 0 | 0 | 0 | 0 | `10626FF4` | [w](https://developer.valvesoftware.com/wiki/prop_ragdoll_attached) |
| `prop_sphere` | 0 | 0 | 0 | 0 | 0 | `106381B4` | [w](https://developer.valvesoftware.com/wiki/prop_sphere) |
| `prop_vehicle` | 0 | 0 | 0 | 0 | 0 | `1065546C` | [w](https://developer.valvesoftware.com/wiki/prop_vehicle) |
| `prop_vehicle_driveable` | 0 | 0 | 0 | 0 | 0 | `10655914` | [w](https://developer.valvesoftware.com/wiki/prop_vehicle_driveable) |
| `prop_wall_breakable` | 0 | 0 | 0 | 0 | 0 | `1063927C` | [w](https://developer.valvesoftware.com/wiki/prop_wall_breakable) |
| `raggib` | 0 | 0 | 0 | 0 | 0 | `1060F8C4` | [w](https://developer.valvesoftware.com/wiki/raggib) |
| `script_func_button` | 0 | 0 | 0 | 0 | 0 | `105E419C` | [w](https://developer.valvesoftware.com/wiki/script_func_button) |
| `script_trigger_hurt` | 0 | 0 | 0 | 0 | 0 | `10653784` | [w](https://developer.valvesoftware.com/wiki/script_trigger_hurt) |
| `script_trigger_multiple` | 0 | 0 | 0 | 0 | 0 | `1064F22C` | [w](https://developer.valvesoftware.com/wiki/script_trigger_multiple) |
| `script_trigger_once` | 0 | 0 | 0 | 0 | 0 | `10653AD4` | [w](https://developer.valvesoftware.com/wiki/script_trigger_once) |
| `script_trigger_push` | 0 | 0 | 0 | 0 | 0 | `10654174` | [w](https://developer.valvesoftware.com/wiki/script_trigger_push) |
| `scripted_item_drop` | 0 | 0 | 0 | 0 | 0 | `10611934` | [w](https://developer.valvesoftware.com/wiki/scripted_item_drop) |
| `scripted_scene` | 0 | 0 | 0 | 0 | 0 | `1063DCA4` | [w](https://developer.valvesoftware.com/wiki/scripted_scene) |
| `simple_physics_prop` | 0 | 0 | 0 | 0 | 0 | `1062946C` | [w](https://developer.valvesoftware.com/wiki/simple_physics_prop) |
| `spitter_projectile` | 0 | 0 | 0 | 0 | 0 | `106D3C4C` | [w](https://developer.valvesoftware.com/wiki/spitter_projectile) |
| `spotlight_end` | 0 | 0 | 0 | 0 | 0 | `10643B54` | [w](https://developer.valvesoftware.com/wiki/spotlight_end) |
| `survivor_bot` | 0 | 0 | 0 | 0 | 0 | `106B0E3C` | [w](https://developer.valvesoftware.com/wiki/survivor_bot) |
| `survivor_death_model` | 0 | 0 | 0 | 0 | 0 | `10695914` | [w](https://developer.valvesoftware.com/wiki/survivor_death_model) |
| `tank` | 0 | 0 | 0 | 0 | 0 | `106ECAB4` | [w](https://developer.valvesoftware.com/wiki/tank) |
| `tank_rock` | 0 | 0 | 0 | 0 | 0 | `106A610C` | [w](https://developer.valvesoftware.com/wiki/tank_rock) |
| `team_manager` | 0 | 0 | 0 | 0 | 0 | `10647014` | [w](https://developer.valvesoftware.com/wiki/team_manager) |
| `terror_gamerules` | 0 | 0 | 0 | 0 | 0 | `1068E43C` | [w](https://developer.valvesoftware.com/wiki/terror_gamerules) |
| `terror_player_manager` | 0 | 0 | 0 | 0 | 0 | `1069D1FC` | [w](https://developer.valvesoftware.com/wiki/terror_player_manager) |
| `test_proxytoggle` | 0 | 0 | 0 | 0 | 0 | `106492C4` | [w](https://developer.valvesoftware.com/wiki/test_proxytoggle) |
| `test_traceline` | 0 | 0 | 0 | 0 | 0 | `1064994C` | [w](https://developer.valvesoftware.com/wiki/test_traceline) |
| `trigger` | 0 | 0 | 0 | 0 | 0 | `1064E424` | [w](https://developer.valvesoftware.com/wiki/trigger) |
| `trigger_active_weapon_detect` | 0 | 0 | 0 | 0 | 0 | `1065208C` | [w](https://developer.valvesoftware.com/wiki/trigger_active_weapon_detect) |
| `trigger_autosave` | 0 | 0 | 0 | 0 | 0 | `106502BC` | [w](https://developer.valvesoftware.com/wiki/trigger_autosave) |
| `trigger_callback` | 0 | 0 | 0 | 0 | 0 | `10651D3C` | [w](https://developer.valvesoftware.com/wiki/trigger_callback) |
| `trigger_cdaudio` | 0 | 0 | 0 | 0 | 0 | `1065095C` | [w](https://developer.valvesoftware.com/wiki/trigger_cdaudio) |
| `trigger_escape` | 0 | 0 | 0 | 0 | 0 | `1064EB8C` | [w](https://developer.valvesoftware.com/wiki/trigger_escape) |
| `trigger_fog` | 0 | 0 | 0 | 0 | 0 | `1060323C` | [w](https://developer.valvesoftware.com/wiki/trigger_fog) |
| `trigger_hurt_ghost` | 0 | 0 | 0 | 0 | 0 | `1065272C` | [w](https://developer.valvesoftware.com/wiki/trigger_hurt_ghost) |
| `trigger_impact` | 0 | 0 | 0 | 0 | 0 | `10650FFC` | [w](https://developer.valvesoftware.com/wiki/trigger_impact) |
| `trigger_look` | 0 | 0 | 0 | 0 | 0 | `10653E24` | [w](https://developer.valvesoftware.com/wiki/trigger_look) |
| `trigger_proximity` | 0 | 0 | 0 | 0 | 0 | `10650CAC` | [w](https://developer.valvesoftware.com/wiki/trigger_proximity) |
| `trigger_remove` | 0 | 0 | 0 | 0 | 0 | `1064EEDC` | [w](https://developer.valvesoftware.com/wiki/trigger_remove) |
| `trigger_serverragdoll` | 0 | 0 | 0 | 0 | 0 | `1065169C` | [w](https://developer.valvesoftware.com/wiki/trigger_serverragdoll) |
| `trigger_togglesave` | 0 | 0 | 0 | 0 | 0 | `1064FF6C` | [w](https://developer.valvesoftware.com/wiki/trigger_togglesave) |
| `trigger_tonemap` | 0 | 0 | 0 | 0 | 0 | `105F8DBC` | [w](https://developer.valvesoftware.com/wiki/trigger_tonemap) |
| `trigger_upgrade_laser_sight` | 0 | 0 | 0 | 0 | 0 | `106A1AAC` | [w](https://developer.valvesoftware.com/wiki/trigger_upgrade_laser_sight) |
| `upgrade_ammo_explosive` | 0 | 0 | 0 | 0 | 0 | `106A11F4` | [w](https://developer.valvesoftware.com/wiki/upgrade_ammo_explosive) |
| `upgrade_ammo_incendiary` | 0 | 0 | 0 | 0 | 0 | `106A0DAC` | [w](https://developer.valvesoftware.com/wiki/upgrade_ammo_incendiary) |
| `upgrade_laser_sight` | 0 | 0 | 0 | 0 | 0 | `106A163C` | [w](https://developer.valvesoftware.com/wiki/upgrade_laser_sight) |
| `vgui_screen` | 0 | 0 | 0 | 0 | 0 | `10656BAC` | [w](https://developer.valvesoftware.com/wiki/vgui_screen) |
| `vgui_screen_team` | 0 | 0 | 0 | 0 | 0 | `10656BAC` | [w](https://developer.valvesoftware.com/wiki/vgui_screen_team) |
| `vgui_slideshow_display` | 0 | 0 | 0 | 0 | 0 | `10640784` | [w](https://developer.valvesoftware.com/wiki/vgui_slideshow_display) |
| `viewmodel` | 0 | 0 | 0 | 0 | 0 | `105E0754` | [w](https://developer.valvesoftware.com/wiki/viewmodel) |
| `vomitjar_projectile` | 0 | 0 | 0 | 0 | 0 | `106D7CAC` | [w](https://developer.valvesoftware.com/wiki/vomitjar_projectile) |
| `vote_controller` | 0 | 0 | 0 | 0 | 0 | `1065725C` | [w](https://developer.valvesoftware.com/wiki/vote_controller) |
| `waterbullet` | 0 | 0 | 0 | 0 | 0 | `1065C25C` | [w](https://developer.valvesoftware.com/wiki/waterbullet) |
| `weapon_adrenaline` | 0 | 0 | 0 | 0 | 0 | `106C3ABC` | [w](https://developer.valvesoftware.com/wiki/weapon_adrenaline) |
| `weapon_autoshotgun` | 0 | 0 | 0 | 0 | 0 | `106BA7BC` | [w](https://developer.valvesoftware.com/wiki/weapon_autoshotgun) |
| `weapon_basecsgrenade` | 0 | 0 | 0 | 0 | 0 | `1066413C` | [w](https://developer.valvesoftware.com/wiki/weapon_basecsgrenade) |
| `weapon_boomer_claw` | 0 | 0 | 0 | 0 | 0 | `106BB974` | [w](https://developer.valvesoftware.com/wiki/weapon_boomer_claw) |
| `weapon_chainsaw` | 0 | 0 | 0 | 0 | 0 | `106BC7FC` | [w](https://developer.valvesoftware.com/wiki/weapon_chainsaw) |
| `weapon_charger_claw` | 0 | 0 | 0 | 0 | 0 | `106BD264` | [w](https://developer.valvesoftware.com/wiki/weapon_charger_claw) |
| `weapon_cola_bottles` | 0 | 0 | 0 | 0 | 0 | `106BF1B4` | [w](https://developer.valvesoftware.com/wiki/weapon_cola_bottles) |
| `weapon_cs_base` | 0 | 0 | 0 | 0 | 0 | `106652B4` | [w](https://developer.valvesoftware.com/wiki/weapon_cs_base) |
| `weapon_csbase_gun` | 0 | 0 | 0 | 0 | 0 | `106659C4` | [w](https://developer.valvesoftware.com/wiki/weapon_csbase_gun) |
| `weapon_defibrillator` | 0 | 0 | 0 | 0 | 0 | `106C44BC` | [w](https://developer.valvesoftware.com/wiki/weapon_defibrillator) |
| `weapon_fireworkcrate` | 0 | 0 | 0 | 0 | 0 | `106BF9C4` | [w](https://developer.valvesoftware.com/wiki/weapon_fireworkcrate) |
| `weapon_gascan` | 0 | 0 | 0 | 0 | 0 | `106C0A84` | [w](https://developer.valvesoftware.com/wiki/weapon_gascan) |
| `weapon_gnome` | 0 | 0 | 0 | 0 | 0 | `106C135C` | [w](https://developer.valvesoftware.com/wiki/weapon_gnome) |
| `weapon_grenade_launcher` | 0 | 0 | 0 | 0 | 0 | `106C1AB4` | [w](https://developer.valvesoftware.com/wiki/weapon_grenade_launcher) |
| `weapon_hegrenade_spawn` | 0 | 0 | 0 | 0 | 0 | `1067FFBC` | [w](https://developer.valvesoftware.com/wiki/weapon_hegrenade_spawn) |
| `weapon_hunter_claw` | 0 | 0 | 0 | 0 | 0 | `106C2BCC` | [w](https://developer.valvesoftware.com/wiki/weapon_hunter_claw) |
| `weapon_hunting_rifle` | 0 | 0 | 0 | 0 | 0 | `106D2C7C` | [w](https://developer.valvesoftware.com/wiki/weapon_hunting_rifle) |
| `weapon_jockey_claw` | 0 | 0 | 0 | 0 | 0 | `106C6A34` | [w](https://developer.valvesoftware.com/wiki/weapon_jockey_claw) |
| `weapon_melee` | 0 | 0 | 0 | 0 | 0 | `106D611C` | [w](https://developer.valvesoftware.com/wiki/weapon_melee) |
| `weapon_molotov` | 0 | 0 | 0 | 0 | 0 | `106C7D8C` | [w](https://developer.valvesoftware.com/wiki/weapon_molotov) |
| `weapon_oxygentank` | 0 | 0 | 0 | 0 | 0 | `106C9354` | [w](https://developer.valvesoftware.com/wiki/weapon_oxygentank) |
| `weapon_pain_pills` | 0 | 0 | 0 | 0 | 0 | `106C9B14` | [w](https://developer.valvesoftware.com/wiki/weapon_pain_pills) |
| `weapon_pipe_bomb` | 0 | 0 | 0 | 0 | 0 | `106CA374` | [w](https://developer.valvesoftware.com/wiki/weapon_pipe_bomb) |
| `weapon_pistol` | 0 | 0 | 0 | 0 | 0 | `106CB2EC` | [w](https://developer.valvesoftware.com/wiki/weapon_pistol) |
| `weapon_pistol_magnum` | 0 | 0 | 0 | 0 | 0 | `106CBA84` | [w](https://developer.valvesoftware.com/wiki/weapon_pistol_magnum) |
| `weapon_propanetank` | 0 | 0 | 0 | 0 | 0 | `106CC21C` | [w](https://developer.valvesoftware.com/wiki/weapon_propanetank) |
| `weapon_pumpshotgun` | 0 | 0 | 0 | 0 | 0 | `106CC994` | [w](https://developer.valvesoftware.com/wiki/weapon_pumpshotgun) |
| `weapon_rifle` | 0 | 0 | 0 | 0 | 0 | `106BA00C` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle) |
| `weapon_rifle_ak47` | 0 | 0 | 0 | 0 | 0 | `106CD12C` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_ak47) |
| `weapon_rifle_desert` | 0 | 0 | 0 | 0 | 0 | `106CD8C4` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_desert) |
| `weapon_rifle_m60` | 0 | 0 | 0 | 0 | 0 | `106CE074` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_m60) |
| `weapon_rifle_sg552` | 0 | 0 | 0 | 0 | 0 | `106CE804` | [w](https://developer.valvesoftware.com/wiki/weapon_rifle_sg552) |
| `weapon_shotgun_chrome` | 0 | 0 | 0 | 0 | 0 | `106CEFA4` | [w](https://developer.valvesoftware.com/wiki/weapon_shotgun_chrome) |
| `weapon_shotgun_spas` | 0 | 0 | 0 | 0 | 0 | `106CF754` | [w](https://developer.valvesoftware.com/wiki/weapon_shotgun_spas) |
| `weapon_smg` | 0 | 0 | 0 | 0 | 0 | `106D4284` | [w](https://developer.valvesoftware.com/wiki/weapon_smg) |
| `weapon_smg_mp5` | 0 | 0 | 0 | 0 | 0 | `106CFECC` | [w](https://developer.valvesoftware.com/wiki/weapon_smg_mp5) |
| `weapon_smg_mp5_spawn` | 0 | 0 | 0 | 0 | 0 | `1067E314` | [w](https://developer.valvesoftware.com/wiki/weapon_smg_mp5_spawn) |
| `weapon_smg_silenced` | 0 | 0 | 0 | 0 | 0 | `106D0664` | [w](https://developer.valvesoftware.com/wiki/weapon_smg_silenced) |
| `weapon_smoker_claw` | 0 | 0 | 0 | 0 | 0 | `106D0DFC` | [w](https://developer.valvesoftware.com/wiki/weapon_smoker_claw) |
| `weapon_sniper_awp` | 0 | 0 | 0 | 0 | 0 | `106D15AC` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_awp) |
| `weapon_sniper_military` | 0 | 0 | 0 | 0 | 0 | `106D1D4C` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_military) |
| `weapon_sniper_scout` | 0 | 0 | 0 | 0 | 0 | `106D24E4` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_scout) |
| `weapon_sniper_scout_spawn` | 0 | 0 | 0 | 0 | 0 | `1067EF5C` | [w](https://developer.valvesoftware.com/wiki/weapon_sniper_scout_spawn) |
| `weapon_spitter_claw` | 0 | 0 | 0 | 0 | 0 | `106D341C` | [w](https://developer.valvesoftware.com/wiki/weapon_spitter_claw) |
| `weapon_tank_claw` | 0 | 0 | 0 | 0 | 0 | `106D4AEC` | [w](https://developer.valvesoftware.com/wiki/weapon_tank_claw) |
| `weapon_upgradepack_explosive` | 0 | 0 | 0 | 0 | 0 | `106C53B4` | [w](https://developer.valvesoftware.com/wiki/weapon_upgradepack_explosive) |
| `weapon_upgradepack_incendiary` | 0 | 0 | 0 | 0 | 0 | `106C5B54` | [w](https://developer.valvesoftware.com/wiki/weapon_upgradepack_incendiary) |
| `weapon_vomitjar` | 0 | 0 | 0 | 0 | 0 | `106C62C4` | [w](https://developer.valvesoftware.com/wiki/weapon_vomitjar) |
| `window_pane` | 0 | 0 | 0 | 0 | 0 | `1060626C` | [w](https://developer.valvesoftware.com/wiki/window_pane) |
| `witch` | 0 | 0 | 0 | 0 | 0 | `106DEEE4` | [w](https://developer.valvesoftware.com/wiki/witch) |
| `world_items` | 0 | 0 | 0 | 0 | 0 | `10611CFC` | [w](https://developer.valvesoftware.com/wiki/world_items) |

---

### 4.3 C1 unreadable (8 classes)

`vtable[9]` could not be resolved — either the class has no static factory, or the function
body does not follow the `mov eax, imm32 ; ret` shape. **No verdict, therefore not usable.**

| class | toyz4 | chern | anemo | hive | **17 maps** | vtable | wiki |
|---|---:|---:|---:|---:|---:|---|---|
| `info_survivor_rescue` | 0 | 9 | 21 | 30 | **60** | `1068C3C4` | [w](https://developer.valvesoftware.com/wiki/info_survivor_rescue) |
| `hunter` | 0 | 0 | 0 | 0 | 0 | `106E5854` | [w](https://developer.valvesoftware.com/wiki/hunter) |
| `infected` | 0 | 0 | 0 | 0 | 0 | `105D5CD8` | [w](https://developer.valvesoftware.com/wiki/infected) |
| `info_transitioning_player` | 0 | 0 | 0 | 0 | 0 | `1069FF14` | [w](https://developer.valvesoftware.com/wiki/info_transitioning_player) |
| `jockey` | 0 | 0 | 0 | 0 | 0 | `106E7504` | [w](https://developer.valvesoftware.com/wiki/jockey) |
| `player` | 0 | 0 | 0 | 0 | 0 | `106957C8` | [w](https://developer.valvesoftware.com/wiki/player) |
| `smoker` | 0 | 0 | 0 | 0 | 0 | `106E90F4` | [w](https://developer.valvesoftware.com/wiki/smoker) |
| `spitter` | 0 | 0 | 0 | 0 | 0 | `106EAE0C` | [w](https://developer.valvesoftware.com/wiki/spitter) |


