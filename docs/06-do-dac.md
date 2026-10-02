# Đo đạc

Các phần dưới đây chỉ ghi số liệu, không xoá hay đổi entity nào.

---

## Hai con số khác nhau

- **`num_edicts`**: mốc cao nhất bộ cấp phát từng dùng tới. **Không bao giờ giảm.**
- **số entity sống** = `num_edicts` − số ô đang trống (`FL_EDICT_FREE`).

Log in cả hai, dạng `song=... num_edicts=... trong=...`. Muốn biết map đang dùng bao nhiêu thì
xem `song`; muốn biết còn bao xa mới chạm 2048 thì xem `num_edicts` và `trong`.

---

## File log

```
left4dead2/addons/edictbudget/edictbudget.log
```

Ghi nối tiếp, có giờ, đẩy xuống đĩa sau mỗi dòng (máy chủ chết đột ngột vẫn còn log).
`logconsole=1` để in cả ra console.

---

## loadprobe

Ghi số edict trong N frame đầu sau khi nạp map (`loadprobe=8`).

Cần vì số lúc nạp chưa phải số thật: một số lớp tạo entity con sau khi nạp (vd
`point_spotlight`), một số lớp tự xoá nhưng việc xoá dời tới cuối frame (vd `weapon_*_spawn` —
mỗi cái chiếm 2 edict trong frame nạp).

```
NAP[frame 0]: num_edicts=685 trong=182 song=503 (dinh=685)
```

---

## heartbeat

Mỗi N giây (`heartbeat=300`) ghi một dòng tổng và các lớp có số lượng thay đổi so với lần trước.
Dùng để xem có lớp nào tăng dần trong lúc chơi.

---

## trap

Khi `ED_Alloc` sắp báo hết edict, ghi lại toàn bộ kiểm kê: lớp nào đang chiếm bao nhiêu slot.

Lưu ý: `trap` **sửa 8 byte** trong `engine.dll`, ở nhánh lỗi của `ED_Alloc` (RVA `0x1E0247`),
để nhảy vào hàm ghi log rồi chạy lại đúng hai nhánh gốc. Nhánh này chỉ chạy khi engine sắp chết.

---

## Plugin SourceMod `src/ent_test.sp`

Plugin phụ để đo bằng tay trong game. Lệnh (quyền root):

| lệnh | việc |
|---|---|
| `sm_ent_report` | số entity sống / `num_edicts` / trống |
| `sm_ent_classes` | xếp hạng lớp đông nhất |
| `sm_ent_snap` / `sm_ent_diff` | chụp mốc rồi so, tìm lớp tăng dần |
| `sm_ent_hud` | bật/tắt HUD |
| `sm_ent_add` / `sm_ent_clear` | sinh / xoá `info_target` để thử tải |

Tự in ra console ở `round_start`, `mission_lost` và khi survivor gục:

```
[ent_test] MOC round_start: song=472 num_edicts=685 trong=213
```

Đây là cách so một lớp `noedict` khi bật và khi tắt (cùng map, cùng mốc `round_start`).

---

## Kết quả đã có

- 7 phiên chơi dài trên máy chủ thật: cả 7 kết thúc với **ít** entity hơn lúc bắt đầu (trung
  bình −114). Đỉnh entity sống 1375/2048. Không thấy entity tích tụ trong lúc chơi.
- 0 lần `ED_Alloc` trong 105 phiên.
