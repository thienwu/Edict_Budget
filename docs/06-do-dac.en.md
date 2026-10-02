# Measurement

*[Tiếng Việt](06-do-dac.md) · English*

Everything below only records numbers; nothing removes or changes any entity.

---

## Two different numbers

- **`num_edicts`**: the highest slot the allocator has ever reached. **It never goes down.**
- **live entities** = `num_edicts` − number of free slots (`FL_EDICT_FREE`).

The log prints both, as `song=... num_edicts=... trong=...` (`song` = live, `trong` = free). To
know how much a map uses, look at `song`; to know how far you are from 2048, look at
`num_edicts` and `trong`.

---

## Log file

```
left4dead2/addons/edictbudget/edictbudget.log
```

Appended, timestamped, flushed to disk after every line (the log survives a sudden crash).
`logconsole=1` also prints to the console.

---

## loadprobe

Records edict counts for the first N frames after a map loads (`loadprobe=8`).

Needed because the count at load time isn't the real one yet: some classes create child entities
after loading (e.g. `point_spotlight`), and some remove themselves but the removal is deferred to
the end of the frame (e.g. `weapon_*_spawn` — each takes 2 edicts during the load frame).

```
NAP[frame 0]: num_edicts=685 trong=182 song=503 (dinh=685)
```

---

## heartbeat

Every N seconds (`heartbeat=300`) writes a summary line and the classes whose count changed since
the previous one. Used to see whether any class keeps growing during play.

---

## trap

When `ED_Alloc` is about to report that edicts have run out, writes a full inventory: which class
holds how many slots.

Note: `trap` **patches 8 bytes** in `engine.dll`, in the error branch of `ED_Alloc` (RVA
`0x1E0247`), to jump into the logging function and then run the two original branches exactly as
before. That branch only runs when the engine is about to die.

---

## SourceMod plugin `src/ent_test.sp`

A helper plugin for measuring by hand in game. Commands (root access):

| command | does |
|---|---|
| `sm_ent_report` | live entities / `num_edicts` / free |
| `sm_ent_classes` | ranks the most numerous classes |
| `sm_ent_snap` / `sm_ent_diff` | take a snapshot, then compare, to find growing classes |
| `sm_ent_hud` | toggle the HUD |
| `sm_ent_add` / `sm_ent_clear` | spawn / remove `info_target` entities to load-test |

It prints to the console automatically at `round_start`, `mission_lost` and when a survivor goes
down:

```
[ent_test] MOC round_start: song=472 num_edicts=685 trong=213
```

This is how to compare a `noedict` class on and off (same map, same `round_start` mark).

---

## Results so far

- 7 long play sessions on a live server: all 7 ended with **fewer** entities than they started
  with (−114 on average). Peak live entities 1375/2048. No build-up of entities during play.
- 0 `ED_Alloc` errors in 105 sessions.
