# Known issues

*[Tiếng Việt](03-loi-da-biet.md) · English*

---

## 1. A SourceMod plugin that creates an entity of a cut class gets a negative number

If a class is in `noedict.txt`, every entity of it has no edict, including entities created by a
plugin at runtime. `CreateEntityByName()` then returns a negative reference. `EntIndexToEntRef()`
fails with `Invalid entity index`; `RemoveEdict()` may crash.

Seen with `logic_script` (`l4d2_script_cmd_swap`) and `env_shake` (`l4d_plane_crash`).

How to handle it, with sample code: [02-noedict.en.md](02-noedict.en.md#sourcemod-plugins).

---

## 2. `wipeclear` clears earlier than the game

A plugin holding a reference to a cleared entity ends up with a dangling reference. Drop the
references on `mission_lost`, or list that class in `wipekeep.txt`. See
[01-co-che.en.md](01-co-che.en.md#wipeclear).

---

## 3. `info_ambient_mob_end` breaks the ambient mob lookup

`info_ambient_mob_end` shares a vtable with `info_ambient_mob_start`, so enabling it cuts both.
`info_ambient_mob_start` is looked up with `FindEntityByClassnameNearest` (`0x100B5370`), which
skips entities without an edict:

```
100B53C1  cmp dword ptr [esi+0x28], 0   ; no edict -> skip
1035EF78  push 'info_ambient_mob_start'
1035EF82  call 100B5370
```

Result: the caller at `0x1035EF78` never finds a start point. This class is **off** in
`noedict.txt`.

---

## 4. Classes that pass the self-check

The plugin only checks condition 1 by itself. The classes below **pass** that check — so if
someone adds them to `noedict.txt` the plugin **will** cut them — but fail at least one other
condition. **Don't add them.**

From the test on 02/10/2026 (every class that is not already server-only put in `noedict.txt`;
the server log matched the prediction for all 465 classes). Column 2 is the most on a single map,
counted over official maps, the `.lmp` overrides and workshop maps.

Conditions: [02-noedict.en.md](02-noedict.en.md#conditions-before-adding-a-class). `5?`, `6?` =
flagged by the scanner for manual reading, not a conclusion.

| class | most on one map | fails condition |
|---|---|---|
| `weapon_item_spawn` | 321 | 5? |
| `point_spotlight` | 312 | 6, 5? |
| `ambient_generic` | 170 | 3 |
| `info_particle_target` | 166 | 6 |
| `func_illusionary` | 127 | 6 (has model, no EF_NODRAW) |
| `info_target` | 60 | 6 |
| `func_nav_attribute_region` | 58 | 2 SetSolid[1, 2] |
| `func_clip_vphysics` | 50 | 2 SetSolid[6] |
| `env_player_blocker` | 40 | 2 SetSolid[0, 2] |
| `env_instructor_hint` | 33 | 3 |
| `env_fire` | 32 | 6, 5? |
| `script_clip_vphysics` | 30 | 2 SetSolid[3] |
| `func_wall` | 26 | 2 SetSolid[1], 6 (has model, no EF_NODRAW) |
| `func_wall_toggle` | 24 | 2 SetSolid[1] |
| `info_game_event_proxy` | 17 | 6, 3 |
| `point_viewcontrol` | 16 | 6, 5? |
| `point_deathfall_camera` | 15 | 6 |
| `point_nav_attribute_region` | 14 | 2 SetSolid[1, 2] |
| `filter_activator_team` | 12 | 4 |
| `info_target_instructor_hint` | 9 | 6 |
| `ambient_music` | 8 | 3 |
| `filter_health` | 7 | 4 |
| `filter_activator_name` | 5 | 4 |
| `logic_scene_list_manager` | 5 | 4 |
| `point_viewcontrol_multiplayer` | 5 | 6 |
| `info_ambient_mob_start` | 4 | 7 |
| `point_viewcontrol_survivor` | 4 | 6 |
| `filter_activator_class` | 3 | 4 |
| `filter_activator_model` | 2 | 4 |
| `filter_melee_damage` | 2 | 4 |
| `filter_activator_infected_class` | 1 | 4 |
| `filter_damage_type` | 1 | 4 |
| `func_nav_connection_blocker` | 1 | 2 SetSolid[1] |
| `logic_compare` | 1 | 4 |
| `script_nav_attribute_region` | 1 | 2 SetSolid[3] |
| `ai_changehintgroup` | 0 | CHUA SANG 6  |
| `entity_blocker` | 0 | 2 SetSolid[2] |
| `env_bubbles` | 0 | 6 (has model, no EF_NODRAW) |
| `env_message` | 0 | 3 |
| `escape_route` | 0 | 6? |
| `filter_activator_context` | 0 | 4 |
| `filter_base` | 0 | 4 |
| `func_fish_pool` | 0 | 6 (has model, no EF_NODRAW) |
| `func_vehicleclip` | 0 | 2 SetSolid[1] |
| `func_weight_button` | 0 | 2 SetSolid[6], 6 (has model, no EF_NODRAW) |
| `gibshooter` | 0 | 2 SetSolid[0, 2] |
| `logic_multicompare` | 0 | 4 |
| `point_devshot_camera` | 0 | 6 |
| `point_gamestats_counter` | 0 | 3 |
| `point_playermoveconstraint` | 0 | 6 |
| `simple_physics_brush` | 0 | 2 SetSolid[6], 6 (has model, no EF_NODRAW) |
| `vomit_particle` | 0 | 2 SetSolid[2] |

---

## 5. Classes resolved to the wrong vtable

8 classes for which the plugin's vtable lookup returns the parent class's vtable.
`env_soundscape_proxy` and `env_soundscape_triggerable` even pass all 3 self-checks (they point at
the vtable of `env_soundscape`). List:
[02-noedict.en.md](02-noedict.en.md#classes-resolved-to-the-wrong-vtable).

---

## 6. `freegate=2` breaks item transfer

Item-transfer plugins (e.g. Gear Transfer) destroy and recreate an item in the same frame. Mode
`2` hands back the exact index just freed and clients see a "ghost" weapon. Don't use `2`.

---

## 7. `mapclear` removing an entity that carries over crashes the server

The carry-over list is already built before the hook runs; removing an entity on that list makes
the engine read freed memory. Crashed for real with `mapclear=2` and with `mapclearcarry=1`. Keep
`mapclear=1`, `mapclearcarry=0`.

---

## 8. `swap`: bigger halos, more entities carried over

- `client.dll` hard-codes the halo size of `beam_spotlight` to 60. A map that sets
  `HaloScale 10` on `point_spotlight` will see halos 6 times larger.
- `beam_spotlight` carries over to the next map (`point_spotlight` does not). Measured on
  `the_hive` m3 → m4: 48 more entities in the carry-over list; the engine limit is 512.

---

## 9. Not a plugin bug

`c1m3_mall` prints many lines like:

```
env_entity_maker maker_mannequin_setup_male-InstanceAutoNN failed to find template @template_mannequin_setup_male.
```

The map's two `point_template`s `@template_mannequin_setup_male` / `_female` have the output
`OnEntitySpawned !self,Kill,,1,-1` — they delete themselves 1 second after their first spawn, while
154 `env_entity_maker`s point at them. Makers that run later find no template. The bug is in the
map itself.

---

## Fixed

| tag | bug |
|---|---|
| `v2.0.1` | the vtable lookup read past the end of a function and picked up another class's vtable (seen with `upgrade_spawn` → vtable of `weapon_melee_spawn`) |
| `v2.0.2` | the server crashed at startup when `noedict.txt` contained `beam_spotlight`, `func_breakable`, `logic_choreographed_scene`, `logic_scene_list_manager`, `scene_manager`, `scripted_scene`, `sky_camera`, `weapon_basecsgrenade` or `weapon_hunter_claw` (derived pointers were not bounds-checked) |
| `v2.0.2` | only the first 32 lines of `noedict.txt` were read, the rest silently dropped |
