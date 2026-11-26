# edictbudget v2.1.2

*Tiếng Việt · English below*

## Thay đổi (đã kiểm)

- **Sửa:** plugin làm sập máy chủ lúc nạp sau bản cập nhật game ngày 25/11/2026. Module `swap` gọi
  địa chỉ cũ trong `server.dll` trước khi kiểm.
- **Chạy được trên bản game 25/11/2026.** Bảng địa chỉ cho từng bản `server.dll`, nhận bản bằng
  `TimeDateStamp` + cỡ (số phiên bản game vẫn là 2.2.4.3 / 10097 nên không dùng được). Mười hàm plugin
  móc vào giống hệt bản cũ, chỉ dời chỗ.
- **Tự tắt khi gặp bản `server.dll` lạ:** tắt `noedict`, `swap`, `spotlight`, `wipeclear`, `mapclear`, ghi
  lý do vào log; `freegate`, `trap`, `hopbao` tự dò nên vẫn chạy. Mọi lời gọi tới địa chỉ ghi cứng đều kiểm
  prologue trước.

Đã kiểm trên máy chủ cục bộ chạy bản game 25/11, 4 bot:
- `c6m1_riverbank`: 3 lần wipe liên tiếp;
- `ch04_pripyat03`: 2 lần wipe;
- chuyển màn;
- `defl_3`: nav blocker chặn 7/7;
- `the_hive_m4` / `m2`: quầng sáng.

Thử ngoài game: bản lạ và prologue hỏng đều tự tắt, không gọi gì.

Nâng từ v2.1.1: chỉ thay `bin/edictbudget_mm.dll`.

`edictbudget_mm.dll`: 238 592 byte, md5 `c59ec50b25a704f538bafa14a2cd7094`.

---

# edictbudget v2.1.2 (English)

## Changes (tested)

- **Fixed:** the plugin crashed the server at load after the 25/11/2026 game update. The `swap` module
  called an old address in `server.dll` before checking it.
- **Works on the 25/11/2026 game build.** An address table per `server.dll` build, identified by
  `TimeDateStamp` + size (the game version number is still 2.2.4.3 / 10097, so it can't be used). The ten
  hooked functions are identical to the old build, only moved.
- **Turns itself off on an unknown `server.dll`:** `noedict`, `swap`, `spotlight`, `wipeclear`, `mapclear`
  are disabled and the reason is logged; `freegate`, `trap`, `hopbao` find their own targets and keep
  running. Every call to a hard-coded address checks the prologue first.

Tested on a local server running the 25/11 build, 4 bots:
- `c6m1_riverbank`: 3 wipes in a row;
- `ch04_pripyat03`: 2 wipes;
- a map change;
- `defl_3`: nav blockers block 7/7;
- `the_hive_m4` / `m2`: halos.

Tried outside the game: an unknown build and a corrupted prologue both turn the plugin off without calling
anything.

Upgrading from v2.1.1: only replace `bin/edictbudget_mm.dll`.

`edictbudget_mm.dll`: 238,592 bytes, md5 `c59ec50b25a704f538bafa14a2cd7094`.
