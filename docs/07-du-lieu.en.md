# Class classification data

*[Tiếng Việt](07-du-lieu.md) · English*

File `data/phanloai_entity_l4d2.json`: all 557 class names registered in `server.dll`.

| field | meaning |
|---|---|
| `ten` | class name |
| `vt` | vtable address (RVA + `0x10000000`) |
| `dk1` | `UNGVIEN` (229) = passes condition 1 · `CAM` (320) = has its own SendTable · `?` (8) = could not be read |
| `pripyat03` | number of entities of that class on `ch04_pripyat03` |

Notes:

- `UNGVIEN` only means **passes condition 1**. Most of those fail other conditions. Don't use this
  column as a list of classes to add to `noedict.txt`.
- `vt` tells you which classes **share a vtable** (enabling one cuts all of them).
- Per-map counts for the classes in use are in `noedict.txt` itself (the BAN DO DE KIEM CHUNG
  section), including the `update/maps/*.lmp` overrides and workshop maps.
