# Overview — mission, limits, measurements

*English translation of [00-tong-quan.md](00-tong-quan.md), which is extracted from
`src/sample_mm.cpp`. Function addresses are kept exactly as in the source so they can be
cross-checked. If the two versions disagree, trust the Vietnamese.*

```
edictbudget - keep an L4D2 server from dying with "ED_Alloc: no free edicts"
===========================================================================

===========================================================================
 AUTHORSHIP - READ FIRST

 This project is the result of TWO distinct jobs that cannot be separated.

 IDEA, PROBLEM AND DIRECTION - thienwu, a real server OPERATOR.
   They set the problem from a real failure on a running server, decided every
   major direction, and - more importantly - decided the directions NOT to
   take: banned the 4096 direction, banned touching the phys family, banned
   editing BSP files, and demanded a GENERAL RULE that applies to every map
   with the plugin self-checking at runtime. They ran the tests, captured the
   logs, measured on a live server, and rejected many of the AI's wrong
   conclusions. That is why several places here read "WRONG, corrected".

 REVERSE ENGINEERING, CODE, MEASUREMENT, DOCS - Claude (Anthropic), in Claude
   Code. Reverse engineered server.dll/engine.dll/client.dll, designed and
   wrote all of the source, ran the measurements, and wrote the notes you are
   reading.

 Stated plainly for two reasons:
   1. Anyone reading this code should know where it came from and decide for
      themselves how much to trust it.
   2. Many conclusions here come from reverse engineering, not from official
      documentation. Each carries a function address and an instruction
      listing so it can be re-checked. Anything that could not be verified is
      labelled NOT DETERMINED.

 License: GPLv3. See the LICENSE file.
===========================================================================

MISSION: keep the number of LIVE entities below the 2048 ceiling.
It does NOT raise the ceiling - the entity index in the Source protocol is
11 bits wide (max 2047), so a NETWORKED entity at index >=2048 decodes to
garbage on the client.

!!  LIMITATIONS - MUST READ:
  This plugin does NOT fully prevent "ED_Alloc: no free edicts".
  It does two things: reclaims edicts AT THE RIGHT MOMENT, and strips edicts
  from classes that GENUINELY DO NOT USE NETWORKING. If the map itself needs
  more than 2048 NETWORKED entities at once, nothing can save it - that is a
  protocol ceiling, not a plugin ceiling.
  Measured example: a community map spends 312 point_spotlight + 312
  spotlight_end + 312 beam = 936 edicts (45.7%) on light effects alone. All
  three classes MUST be networked. The plugin cannot touch them.

---------------------------------------------------------------------------
THREE MECHANISMS

1. wipeclear - clean up when the survivor team wipes
   On a wipe the game DOES clean the map, but TOO LATE. The real order is:
     CDirector::Restart -> RestartRound(slot 178)
                             |-- respawn players     <== eats every edict HERE
                             |-- CleanUpMap(slot 179) <== cleans, TOO LATE
   Hook the START of RestartRound and do the "clean" half of CleanUpMap
   before that greedy step:
     UTIL_Remove(everything outside the preserve list) -> CleanupDeleteList()
     -> AllowImmediateEdictReuse()
   then let the game continue; CleanUpMap rebuilds the map from the entity
   lump by itself.
   Only cleans when a mission_lost is pending (a one-shot gate). Without the
   gate it cleans at t=1.00 the instant a map loads, and DESTROYS THE MAP.
   MEASURED: 5 consecutive wipes (ordinary map), 3 consecutive (c6m1_riverbank).

2. freegate - allow a just-freed slot to be reused

   !! THREE MODES from the 30 Aug 2026 build:
        0 = off, the engine's 1-second gate stays intact
        1 = DENYLIST (default) - hooks IVEngineServer::RemoveEdict (vtable slot
            23); a class NOT listed in freekeep.txt gets freetime = 0.0 so
            ED_Alloc's FIRST branch takes it at once, a listed class keeps its
            1-second quarantine
        2 = the unconditional byte patch (legacy, for comparison)

   !! MODE 2 BREAKS HANDING ITEMS TO TEAMMATES. The engine only lets players
      hand over weapon_pain_pills and weapon_adrenaline; plugins such as Gear
      Transfer extend this by DESTROYING and RECREATING the item inside ONE
      frame, so mode 2 hands back the very index just freed and the client
      never sees a delete/create boundary -> "ghost weapon".
      This bug IS VERIFIED, traced end to end by disassembly.

   !! freegate is the LEAST-VALIDATED of the four mechanisms - it has NOT been
      fully tested or signed off. It has never run for long on a busy server,
      and the A/B measurement that justified it predates the current wipeclear.
   ED_Alloc REFUSES to reuse an edict for 1 SECOND after it is freed. A wipe
   deletes and recreates hundreds of entities in the SAME instant, so not one
   of them passes that gate => the server dies with ~999 free slots.
   Change one byte in engine.dll: jae -> jmp. Located by SIGNATURE SCAN.
   Safe because of sv_useexplicitdelete (on by default) - Valve designed it
   as a REPLACEMENT for this wait.
   MEASURED (controlled): same situation, num_edicts=2048 + ~999 free slots,
   freegate=0 -> DIES, freegate=1 -> keeps running normally.

3. noedict - stop non-networked classes from TAKING an edict
   CBaseEntity::PostConstructor tests bit 9 of m_iEFlags (EFL_SERVER_ONLY):
     = 0 -> AddNetworkableEntity    -> range 0-2047, COSTS an edict
     = 1 -> AddNonNetworkableEntity -> range 2049-4095, COSTS NOTHING
   Range 2049-4095 (2047 slots) is the ENGINE'S ORIGINAL DESIGN. Overflowing
   it merely prints a warning and returns an invalid handle - it does NOT
   kill the server.
   Replace vtable slot 29 (+0x74) for CLight/CDecal only, set the bit, then
   call the original. No byte patching, no engine.dll involvement.
   MEASURED: a map that DIED at 2048 edicts now loads at num_edicts=1178.

---------------------------------------------------------------------------
MEASUREMENTS ACROSS THE THREE TIGHTEST CAMPAIGNS

Engine ceiling: max_edicts = 2048. The "EDICT predicted" column is read from
BSP lump 0 with tools\bsp_cost.py; the "measured" column is num_edicts on a
running server.
Formula:  EDICT = (entities in lump) - (classes in noedict.txt)
                  + 2 x point_spotlight with spawnflags&1

1. chernobyl  (5 maps) - contains ch04_pripyat03, where this project started
     map              lump   noedict cuts  EDICT predicted  lump total
     ch01_jupiter     1532       316          1216             1532
     ch02_pripyat01   2204      1138          1067             2205
     ch03_pripyat02   1686       869           816             1685
     ch04_pripyat03   2246      1039          1212             2251
     ch05_pripyat04    940       301           648              949

   RETRODICTION:
     ch04_pripyat03 before noedict existed: DIED at 2048 during load.
     Formula predicted (with noedict): 1212. Measured on the server: 1178.
     Off by +34, i.e. 2.9%. The formula was written AFTER, and matches an
     incident that happened BEFORE.

   !! LIMITATION TO REMEMBER - DO NOT TURN THESE NUMBERS INTO A VERDICT:
     The "lump total" column is NOT the number of entities alive at once. It
     is just the number of lines in the lump. In reality entities are
     ACTIVATED GRADUALLY:
       - weapon_*_spawn calls UTIL_Remove on itself right after spawning the weapon
       - StartDisabled entities are not active yet
       - point_template spawns late
       - the Director spawns progressively as play advances
     ch02_pripyat01 has a "lump total" of 2205 and NEVER DIED, not even before
     noedict existed. ch04_pripyat03 at 2251 did die. The two numbers are only
     46 apart => there is no clean threshold here.
     The measured error is ALWAYS on the high side:
       the_hive m3  predicted 1688 -> measured 1592  (-96)
       the_hive m4  predicted 2067 -> measured 1955  (-112)
       pripyat03    predicted 1212 -> measured 1178  (-34)
     => Use the formula as an UPPER BOUND and a RANKING. To know whether a map
        will actually die you must MEASURE: loadprobe (first 8 frames) and
        heartbeat (every 5 minutes).

2. the_hive  (5 maps)
     map   EDICT predicted   note
     m1         966
     m2        1834          639 env_sprite - CANNOT be un-networked, see below
     m3        1688          80 point_spotlight (coefficient 3)
     m4        2067          OVER THE CEILING. 312 point_spotlight = 936 edicts
     m5        1343
     Measured on the server, m4: peak num_edicts=1955, free=0, 93 slots of
     headroom. After enabling swap: live 1954 -> 1330. Headroom 93 -> 718.
     m3 after enabling swap: live 1591 -> 1431.

3. anemoia / backroom  (6 maps)
     map           lump   noedict cuts  EDICT predicted
     arcade        1246       433           812
     kitty         2954      1444          1509   <- noedict SAVES this map
     party         2198       399          1798      964 prop_dynamic
     poolrooms      921       306           640
     poolrooms2     914       313           626
     reality       1351       488           862
     kitty is the strongest evidence for noedict: without it the map needs
     ~2953 edicts, 900 over the ceiling, and dies for certain during load.

TOTAL REDUCTION (entities marked EFL_SERVER_ONLY / classes substituted):
     noedict   anemoia kitty   1444 entities |  chernobyl ch02  1138
               chernobyl ch04  1039          |  chernobyl ch03   869
               anemoia reality  488          |  the_hive m4      465
               the_hive m3      443
     Only ONE case is PROVEN to be "dies without it":
       ch04_pripyat03 - actually died at 2048 before noedict existed, then
       loaded at num_edicts=1178. The other maps are only large lump numbers,
       NOT PROVEN.
     swap      the_hive m4   624 edicts (312 x 2)
               the_hive m3   160 edicts (80 x 2)
               the_hive m5     8 edicts (only 4 of 12 have spawnflags&1)
               anemoia      ~26 edicts/map (only poolrooms has 13) - negligible

THE RISK OF `swap` HAS BEEN QUANTIFIED (Aug 15) - FAR SMALLER THAN FIRST FEARED:
  `beam_spotlight` keeps FCAP_ACROSS_TRANSITION while `point_spotlight` drops
  it, so the carry-over count was initially assumed to rise sharply.
  Re-measured against real PVS data:
    m4 -> m5 :  +0   (NONE of m4's 312 beam_spotlight are in the landmark PVS)
    m3 -> m4 : +48   (48 point_spotlight sit in the PVS of landmark_m4 on m3)
  Actual transition lists: m3->m4 = 22 entities, m4->m5 = 32. Engine cap 512.
  Trading 48 edicts for 784 => DO NOT FIX.

  Why 739/1051 was estimated wrongly before: server.dll has 54 distinct
  ObjectCaps functions, 31 of which also do `and eax,0xFFFFFFFD` (dropping the
  flag) but with a byte pattern different from CPointEntity, so they were
  missed. In the_hive specifically: CSprite@1009A5D0 (env_sprite 236),
  CBeam@10081580 (beam 312), CSpotlightEnd@101DEEB0 (spotlight_end 312) all
  drop the flag.

  CChangeLevel::BuildChangeList @101FF060 does NOT walk gEntList. It walks
  UTIL_EntitiesInPVS(landmark) @10209BC0 - only entities inside the PVS of an
  info_landmark (1-4% of the map) - AND contains `cmp dword [esi+0x28],0 ; je`
  which skips entities with NO EDICT. => noedict is COMPLETELY IMMUNE to level
  transitions.
  Exceeding 512 calls tier0!Warning (NOT Error), keeps the first 512 entries,
  and drops the remainder.

WHAT THE PLUGIN STILL CANNOT HANDLE:
     the_hive m2  = 1834, culprit 639 env_sprite.
     anemoia party = 1798, culprit 964 prop_dynamic.
     Both classes have their OWN SendTable (CSprite, CDynamicProp) so they
     cannot be un-networked, and both sit at coefficient 1 so swap is useless.
     A different mechanism is needed - the direction under study is editing the
     entity lump inside Hook_LevelInit (see RewriteLump).
     WHEN A GENERAL FORMULA FOR anemoia EXISTS, ADD IT HERE.

---------------------------------------------------------------------------
CONFIGURATION FILES  (left4dead2\addons\edictbudget\)
  stage.txt      0 = completely inert  |  1 = active
  patches.txt    per-feature switches; edit then restart the server
                 wipeclear = 0 off / 1 observe only / 2 clean for real
  noedict.txt    classes to mark EFL_SERVER_ONLY. Before adding a class it
                 must pass all 6 conditions - written inside that file.
  wipekeep.txt   extra classes to KEEP during a wipeclear. EMPTY is correct:
                 during a wipe, deleted entities are REBUILT from the entity
                 lump, so keeping more only narrows the margin.
  mapkeep.txt    classes that must NOT be cleaned at a level transition (only
                 used when mapclear>=2). The opposite of wipekeep: at a
                 transition, deleting the wrong thing is PERMANENT LOSS.
  freekeep.txt   classes whose edict must NOT be reused immediately (freegate=1
                 only). Ships with the 9 item classes Gear Transfer can hand
                 over, plus their 9 "_spawn" copies. If this file is EMPTY then
                 EVERY class is reused at once and item transfer breaks.

BUILD
  SOURCE_ENGINE MUST be 15 (LEFT4DEAD2) in Metamod's numbering.
  Building as 11 (TF2) shifts every vtable index and makes SH_CALL invoke the
  wrong engine function.

---------------------------------------------------------------------------
THE 4096 DIRECTION: RAISING THE LIMIT IS DOABLE. THE 11-BIT LIMIT IS NOT.

Two different things, which must not be conflated:

  (a) RAISING the edict count to 4096 or higher  ->  DOABLE, bytes listed below.
  (b) Putting a NETWORKED entity at index >= 2048  ->  IMPOSSIBLE, and never
      achievable by patching server.dll/engine.dll.

Why (b) is impossible: the entity index is encoded in the packet as an 11-bit
field (max 2047). That is the WIRE FORMAT, present on both ends - client and
server. The server cannot make a client understand index 2048; the client
decodes an entirely different index. Fixing it would mean patching every
player's client.dll, which is not feasible.

=> The free space in range 2048-4095 can ONLY hold NON-NETWORKED entities.
   And the engine ALREADY has a mechanism for that: EFL_SERVER_ONLY plus the
   upper half of m_EntPtrArray (see noedict). No byte patching required.

---------------------------------------------------------------------------
THE LIMIT-RAISING BYTES - RECORDED FOR REFERENCE, ALL DISABLED BY DEFAULT

  bigarray   SV_AllocateEdicts allocates 4096 edicts instead of 2048.
             Signature in engine.dll:
                 B8 00 08 00 00   mov eax, 0x800      <- 2048
                 89 86 18 02 00 00
                 A3 ?? ?? ?? ??
             Overwrite the 4 bytes at m+1 with the desired count
             (EXT_LIMIT = 4096). 8192 also works - the array is sized from it.

  snapshot   Redirect the 7 access sites for m_pPackedData / m_pSerialNumber
             to a 4096-entry buffer. MANDATORY alongside bigarray: a 4096-edict
             array with 2048-entry snapshot tables is WORSE than doing nothing
             - the extra edicts overwrite neighbouring memory.
             7 signatures of the form  8B 84 B1 9C ...  ->  8B 04 B5 <new addr>

  pinmax     LevelInit pins sv.max_edicts back to 2048.
  pinglobals LevelInit pins gpGlobals->maxEntities back to 2048.
             These two hold the ENGINE'S OWN CEILING at 2048 so its allocator
             never places entities in the high range on its own. Without them
             num_edicts climbs past 2047 and NETWORKED entities spill into the
             high range - exactly case (b) above.

  markfree   LevelInit stamps FL_EDICT_FREE across slots 2048-4095.

---------------------------------------------------------------------------
WHY THEY ARE ALL STILL DISABLED

  1. This group BREAKS THE RESPAWN LOOP DURING A WIPE - destroying wipeclear,
     the one thing that does handle the largest burst.
  2. It does not solve the original problem. The free space in the high range
     can only hold non-networked entities, and noedict already achieves that
     through the engine's OFFICIAL path, with no byte patching.
  3. Measured in practice: enabling bigarray+snapshot without pinmax/pinglobals
     gives num_edicts = 2060, with RANDOM entities spilling above 2047 -
     instability appears immediately.

  The code is kept for reference and for anyone who wants to re-measure. It
  must not be enabled in a production build.
```
