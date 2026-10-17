# edictbudget v2.1.1

*Tiếng Việt · English below*

## Thay đổi (đã kiểm)

- **Sửa:** `func_nav_blocker` trong `noedict.txt` không chặn nav (lỗi có từ v2.1.0). Map `defl_3`:
  cửa mở ngược hoặc không mở. Cảm ơn @zengzhijun-975 đã báo lỗi (#1).
  Kiểm trên máy chủ cục bộ, `defl_3`, 4 bot: số vùng nav bị chặn bằng game gốc
  (2/2 · 52/52 · 7/7 · 1/1); v2.1.0 chặn 0 vùng.
- **Thêm module `hopbao`** để sửa lỗi trên (gói để `hopbao=1`).
- **Tự kiểm địa chỉ, sai thì tự ngắt** như các module khác: tìm lệnh neo (phải có đúng 1 lần), so
  đủ 0x64 byte của hàm; trượt thì không ghi byte nào và ghi lý do vào log. Đã thử trên 6 bản
  `server.dll`: bản gốc thì vá được, gỡ trả lại đúng byte cũ; lệch mẫu, neo 2 lần, mất neo, hoặc chỗ
  vá đã bị sửa trước thì tự ngắt.

Nâng từ v2.1.0: thay `bin/edictbudget_mm.dll`, thêm `hopbao=1` vào `patches.txt`.

`edictbudget_mm.dll`: 237 056 byte, md5 `7ee5bd52800e9973b28cf99bb96d9283`.

---

# edictbudget v2.1.1 (English)

## Changes (tested)

- **Fixed:** `func_nav_blocker` in `noedict.txt` did not block nav (bug since v2.1.0). Map
  `defl_3`: the gate opened the wrong way or not at all. Thanks to @zengzhijun-975 for the report
  (#1). Tested on a local server, `defl_3`, 4 bots: the number of blocked nav areas equals the
  stock game (2/2 · 52/52 · 7/7 · 1/1); v2.1.0 blocked none.
- **Added module `hopbao`** to fix it (the package sets `hopbao=1`).
- **Self-checks its address and turns itself off if wrong**, like the other modules: looks for its
  anchor instruction (must occur exactly once) and matches all 0x64 bytes of the function; on any
  failure it writes nothing and logs why. Tried on 6 copies of `server.dll`: the original is
  patched, and unloading restores the original bytes; a pattern mismatch, a doubled anchor, a
  missing anchor, or a spot already patched by someone else turns it off.

Upgrading from v2.1.0: replace `bin/edictbudget_mm.dll`, add `hopbao=1` to `patches.txt`.

`edictbudget_mm.dll`: 237,056 bytes, md5 `7ee5bd52800e9973b28cf99bb96d9283`.
