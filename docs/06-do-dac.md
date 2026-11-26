# Đo đạc

*Tiếng Việt · [English](06-do-dac.en.md)*

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

## spotlight

Đo 08–09/10/2026 trên máy chủ cục bộ (Coop, không người chơi): một lần khởi động, đổi map 9 lần
bằng `changelevel`. Đối chứng: cùng map, `swap=2` (mọi `point_spotlight` → `beam_spotlight`) với
`spotlight=0` hoặc `1`. `song` lấy ở dòng `NAP[frame 7]` của `loadprobe`.

| map | đèn / đổi `beam_spotlight` / giữ, không edict / giữ nguyên | `spotlight_end` không edict | `song`: đối chứng → `spotlight=2` |
|---|---|---:|---|
| `the_hive_m4` | 312 / 0 / 312 / 0 | 312 | 1293 → 1293 |
| `the_hive_m3` | 80 / 0 / 80 / 0 | 80 | 1368 → 1368 |
| `the_hive_m2` | 18 / 18 / 0 / 0 | 0 | 1705 → 1705 |
| `the_hive_m5` | 12 / 12 / 0 / 0 | 0 | 1233 → 1233 |
| `c2m3_coaster` | 77 / 1 / 76 / 0 | 76 | 638 → 639 |
| `c14m2_lighthouse` | 7 / 2 / 5 / 0 | 5 | 684 → 686 |
| `c8m2_subway` | 29 / 29 / 0 / 0 | 0 | 1101 → 1097 |
| `c1m4_atrium` | 2 / 0 / 2 / 0 | 2 | 398 → 397 |
| `ch05_pripyat04` | 7 / 1 / 4 / 2 | 2 | 506 → 504 |

- `song` bằng đối chứng, lệch tối đa 4 (`c8m2_subway` không đổi đèn nào mà cũng lệch 4 — nhiễu
  của vật phẩm và director). Tức đèn giữ đúng lớp và quầng mà **không tốn thêm edict** so với
  `swap`.
- `ch05_pripyat04`: 2 trong 4 đèn ở cột "giữ, không edict" tắt sẵn, nên lúc nạp chỉ sinh 2
  `spotlight_end`.
- Chưa kiểm quầng sáng bằng mắt trên client; chưa chạy khi có người chơi.

---

## hopbao

Đo 17/10/2026 trên máy chủ cục bộ, map `defl_3` (addon 3798061830), Coop, 4 bot thêm bằng `sb_add`
(cần để có `round_start`, lúc `logic_auto` của map gọi `BlockNav`). Đếm bằng VScript: với mỗi
`func_nav_blocker`, lấy các vùng nav chồng lên nó (`NavMesh.GetNavAreasOverlappingEntityExtent`) rồi
đếm vùng có `IsBlocked(3, false)`.

| cấu hình | blocker có edict? | vùng bị chặn / vùng chồng lên, 4 blocker | blocker sát cửa sau `UnblockNav` | stub đếm |
|---|---|---|---|---:|
| `func_nav_blocker` không trong `noedict.txt` (như game gốc) | có | 2/2 · 52/52 · 7/7 · 1/1 | 4/7 | 0 |
| v2.1.0: có `func_nav_blocker`, không có `hopbao` | không | 0/2 · 0/52 · 0/7 · 0/1 | — | — |
| v2.1.1: có `func_nav_blocker`, `hopbao=1` | không | 2/2 · 52/52 · 7/7 · 1/1 | 4/7; `BlockNav` lại → 7/7 | 4 |

- `hopbao=1` cho đúng số của game gốc. 4/7 sau `UnblockNav` là hành vi gốc: 4 vùng nằm dưới cánh cửa
  đang đóng.
- Không có `hopbao` thì log có dòng cảnh báo `!! HOPBAO: noedict.txt co func_nav_blocker ma hopbao KHONG chay`.
- Byte stub đọc từ log dịch ngược khớp thiết kế ([04-dia-chi.md](04-dia-chi.md#hopbao-serverdll)).
- Cổng tự kiểm thử ngoài game: một chương trình nạp bản sao `server.dll` (không chạy `DllMain`, nạp
  lệch base nên có relocation) rồi chạy đúng mã `hopbao.cpp`. Bản gốc → vá, stub chạy đúng
  (`0x200` → `0x4200`; entity không `EFL_SERVER_ONLY` giữ nguyên), gỡ trả đúng 7 byte. Lệch 1 byte
  trong hàm, neo có 2 lần, mất neo, chỗ vá đã bị sửa trước → không ghi byte nào, log ghi lý do.
  Lệch trong 2 địa chỉ tuyệt đối (có mặt nạ) → vẫn vá.
- Chưa chạy khi có người chơi; chưa đo map khác.

---

## v2.1.2 trên bản game 25/11/2026

Máy chủ cục bộ, `server.dll` `5e4d1e32`, 26/11/2026; 4 bot (`sb_add`); wipe bằng `sm_slay @all`. Chưa có người
chơi thật.

| map / bước | kết quả |
|---|---|
| nạp v2.1.1 | sập (`server.dll+0x20CA70`, docs/03 mục 13) |
| nạp v2.1.2 | `DIACHI` nhận bản 25/11; mọi module móc được |
| `c6m1_riverbank` nạp | `num_edicts` 1895, trống 971, sống 881; 637 không edict |
| `c6m1_riverbank` 3 lần wipe liên tiếp | `wipeclear` dọn cả 3; sau `RestartRound` trống 968 / 968 / 968; máy chủ sống |
| `c1m1_hotel` 2 wipe → chuyển màn `c1m2_streets` | sống; trống 419 sau mỗi wipe |
| `ch04_pripyat03` nạp + 2 wipe | `num_edicts` 1151 (sống 962), 2215 không edict; sau wipe trống 141; sống |
| `defl_3` (`hopbao`) | `hub_blocker_0` chặn 7/7, `hub_blocker_1` 1/1; hộp bao xung quanh của cả 7 blocker đúng toạ độ (5 blocker ở cống không chồng nav vì cống không có nav) |
| `the_hive_m4` (`spotlight=2`) | 312 `point_spotlight` + 312 `spotlight_end` không edict, 312 `beam` có edict |
| `the_hive_m2` | 18 đèn có cha → `beam_spotlight` (có edict, quầng 60) |

Cổng nhận bản thử ngoài game (nạp bản sao `server.dll`): bản cũ / bản mới nhận đúng; `TimeDateStamp` lạ →
`BAN LA`, không gọi gì; prologue `EntityFactoryDictionary` hỏng → không gọi.

---

## Kết quả đã có

- 7 phiên chơi dài trên máy chủ thật: cả 7 kết thúc với **ít** entity hơn lúc bắt đầu (trung
  bình −114). Đỉnh entity sống 1375/2048. Không thấy entity tích tụ trong lúc chơi.
- 0 lần `ED_Alloc` trong 105 phiên.
