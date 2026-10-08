# edictbudget v2.1.0

*Tiếng Việt · English below*

## Trong gói

```
left4dead2/addons/metamod/edictbudget.vdf
left4dead2/addons/edictbudget/bin/edictbudget_mm.dll
left4dead2/addons/edictbudget/*.txt        cấu hình, giống configs/ của tag này
```

Giải nén vào thư mục gốc của máy chủ (thư mục chứa `left4dead2/`). Cần Metamod:Source 1.12.
Khởi động lại, gõ `meta list`: phiên bản phải là `2.1.0`.

`edictbudget_mm.dll`: 235 008 byte, md5 `8873a3db5f19fb5a9ddaca80457225de`, build từ `src/` của tag
này (MSVC x86, `/O2`, định nghĩa như `build.bat`).

**Đang chạy bản cũ:** chỉ cần thay `bin/edictbudget_mm.dll`. Giữ nguyên `noedict.txt`, `swap.txt`,
`wipekeep.txt`… của bạn. Muốn thử tính năng mới thì thêm dòng `spotlight=1` (chỉ ghi log) vào
`patches.txt`.

## Thay đổi

- **Thêm `spotlight`** (mặc định tắt). Quyết từng `point_spotlight` lúc map đọc khối keyvalue của
  nó: `HaloScale` = 60 → `beam_spotlight`; ≠ 60 và không có cha → giữ `point_spotlight` của Valve,
  nó và `spotlight_end` không cấp edict; có cha → giữ nguyên. Kết quả: ít edict như `swap` nhưng
  quầng sáng đúng. Xem `docs/01-co-che.md`, mục spotlight.
- **Mã chia module:** `src/sample_mm.cpp` (vòng đời plugin) và `src/src/*.cpp` (mỗi tính năng một
  file), vẫn một đơn vị biên dịch. Đầu `src/sample_mm.cpp` có bản đồ móc và danh sách đoạn mã còn lặp.
- **Gỡ khỏi mã và `patches.txt`** các công tắc của những hướng đã bỏ (trừ `nonetkill`).
  `patches.txt` cũ còn các khoá đó thì plugin bỏ qua.
- **Lỗi đã biết mới ghi nhận:** `mapclear` bỏ qua dòng kiểu CRLF trong `mapkeep.txt` / `mapkill.txt`
  (`docs/03-loi-da-biet.md`, mục 10). Các file `.txt` trong gói này là kiểu LF.

## Đã kiểm

- `spotlight` trên máy chủ cục bộ, 9 map, chế độ 1 và 2: số quyết định khớp từng map với số đọc từ
  file map; số entity có edict bằng đối chứng `swap=2` (lệch ≤ 4). Bảng ở `docs/06-do-dac.md`.
- DLL trong gói so từng hàm (qua file `.map`) với bản đã chạy thử: chỉ khác hằng chuỗi phiên bản và
  hàm `Unload` (bỏ một dòng log ghi sau khi đã đóng log).
- **Chưa** chạy trên máy chủ đông người; quầng sáng chưa kiểm bằng mắt trên client.

---

# edictbudget v2.1.0 (English)

## In the package

```
left4dead2/addons/metamod/edictbudget.vdf
left4dead2/addons/edictbudget/bin/edictbudget_mm.dll
left4dead2/addons/edictbudget/*.txt        configuration, same as configs/ of this tag
```

Unzip into the server's root folder (the one that contains `left4dead2/`). Needs Metamod:Source
1.12. Restart and run `meta list`: the version must be `2.1.0`.

`edictbudget_mm.dll`: 235,008 bytes, md5 `8873a3db5f19fb5a9ddaca80457225de`, built from `src/` of
this tag (MSVC x86, `/O2`, defines as in `build.bat`).

**Upgrading:** only replace `bin/edictbudget_mm.dll`. Keep your own `noedict.txt`, `swap.txt`,
`wipekeep.txt`… To try the new feature, add `spotlight=1` (log only) to `patches.txt`.

## Changes

- **Added `spotlight`** (off by default). Decides per `point_spotlight` while the map parses its
  keyvalue block: `HaloScale` = 60 → `beam_spotlight`; ≠ 60 and no parent → keep Valve's
  `point_spotlight`, it and its `spotlight_end` get no edict; has a parent → left as is. Result: as
  few edicts as `swap`, but the right halo. See `docs/01-co-che.en.md`, section spotlight.
- **Code split into modules:** `src/sample_mm.cpp` (plugin lifecycle) and `src/src/*.cpp` (one file
  per feature), still one translation unit. The top of `src/sample_mm.cpp` has a hook map and a list
  of the code that is still duplicated.
- **Removed from the code and `patches.txt`:** the switches of the abandoned approaches (except
  `nonetkill`). An older `patches.txt` that still has those keys is ignored for them.
- **Newly recorded known issue:** `mapclear` drops CRLF lines in `mapkeep.txt` / `mapkill.txt`
  (`docs/03-loi-da-biet.en.md`, item 10). The `.txt` files in this package use LF.

## Tested

- `spotlight` on a local server, 9 maps, modes 1 and 2: the decision counts match, map by map, the
  counts read from the map files; the number of entities with an edict equals the `swap=2`
  baseline (within 4). Table in `docs/06-do-dac.en.md`.
- The DLL in this package was compared function by function (via the `.map` file) with the build
  that was test-run: only the version string and `Unload` differ (one log line written after the
  log was closed was removed).
- **Not** yet run on a busy server; halos not yet checked by eye on a client.
