# edictbudget

Plugin Metamod:Source cho **máy chủ Left 4 Dead 2 (dedicated, Windows)**. Mục đích: giảm số
edict mà map tiêu tốn, để máy chủ ít gặp lỗi:

```
ED_Alloc: no free edicts
```

Không cần SourceMod. Không nâng giới hạn edict của engine.

*Bản tiếng Anh ([README.en.md](README.en.md), `docs/*.en.md`) là bản cũ, chưa cập nhật theo bản này.*

---

## Giới hạn — đọc trước

- Edict tối đa là **2048**. Chỉ số entity trong gói tin chỉ có **11 bit**, nên entity **có gửi
  về client** không thể nằm ở chỉ số ≥ 2048. Plugin **không** đổi được điều này.
- Entity bị `noedict` cắt mạng nằm ở dải 2049–4095 — dải có sẵn trong bảng entity của game,
  **tối đa 2047 entity không mạng cùng lúc** (tính cả các lớp vốn server-only của game).
  Xem [docs/02-noedict.md](docs/02-noedict.md).
- Plugin chỉ giảm phần edict bị **lãng phí**. Map thật sự cần hơn 2048 entity có mạng cùng lúc
  thì vẫn chết.
- Đã chạy thật chủ yếu ở **chế độ Coop**, ít người chơi, trên một số map. Chưa kiểm Versus,
  Survival, Scavenge, máy chủ đông người. Xem mục **Phạm vi đã kiểm**.

---

## Plugin làm gì

| cơ chế | việc | trạng thái |
|---|---|---|
| `noedict` | không cấp edict cho những lớp entity không cần gửi về client | chính, đang chạy |
| `swap` | đổi `point_spotlight` (3 edict) thành `beam_spotlight` (1 edict) lúc tạo | đang chạy |
| `wipeclear` | khi cả đội thua, dọn entity **sớm hơn** game (trước vòng hồi sinh) | đang chạy |
| `freegate` | cho tái dùng ngay edict vừa giải phóng | **yếu, gần như không có tác dụng thật — để tắt** |
| `mapclear` | quan sát entity lúc chuyển màn | chỉ ghi log |

Chi tiết: [docs/01-co-che.md](docs/01-co-che.md).

---

## ⚠️ Lỗi đã biết — quan trọng với ai dùng plugin SourceMod

### 1. Plugin tạo entity của lớp đã cắt mạng sẽ nhận **số âm**

`noedict` cắt mạng theo **lớp**. Mọi entity của lớp đó đều không có edict — kể cả entity do
**plugin SourceMod tạo lúc chạy**.

Với entity không có edict, `CreateEntityByName()` của SourceMod trả về **một tham chiếu âm**
(ví dụ `-2117514541`) thay vì chỉ số dương. Hậu quả:

| native | khi nhận số âm |
|---|---|
| `EntIndexToEntRef()` | lỗi `Invalid entity index`, đoạn mã phía sau không chạy |
| `RemoveEdict()` | gọi trên entity không có edict — có thể **sập** |

Đã gặp thật:

- `logic_script` + plugin `l4d2_script_cmd_swap`: 35 lỗi, lệnh `script` im lặng không chạy.
- `env_shake` + plugin `l4d_plane_crash`: dính cả hai lỗi trên.

Cách xử lý, chọn một:

- **Không cắt lớp đó** — bỏ (hoặc thêm `#` trước) dòng của nó trong `noedict.txt`.
- **Sửa plugin**: số âm đã là tham chiếu thì giữ nguyên, đừng đưa qua `EntIndexToEntRef`;
  dùng `RemoveEntity()` thay cho `RemoveEdict()`. Mẫu ở
  [docs/02-noedict.md](docs/02-noedict.md#plugin-sourcemod).

Trước khi bật một lớp, hãy tìm trong plugin của bạn xem có chỗ nào gọi
`CreateEntityByName("<lớp đó>")` không. Lưu ý file `.smx` đã nén, phải tìm trong `.sp`.

### 2. `wipeclear` dọn sớm hơn game

Plugin nào giữ tham chiếu tới entity bị dọn sẽ cầm tham chiếu treo. Cách xử lý: plugin dọn
tham chiếu của mình ở sự kiện `mission_lost`; **không sửa được plugin** thì ghi lớp đó vào
`wipekeep.txt` để không bị dọn sớm. Xem [docs/01-co-che.md](docs/01-co-che.md#wipeclear).

Danh sách đầy đủ: [docs/03-loi-da-biet.md](docs/03-loi-da-biet.md).

---

## Cài đặt

1. Build DLL (mục **Build**) hoặc lấy artifact từ GitHub Actions.
2. Chép vào thư mục máy chủ:

```
left4dead2/addons/metamod/edictbudget.vdf          <- configs/edictbudget.vdf
left4dead2/addons/edictbudget/bin/edictbudget_mm.dll
left4dead2/addons/edictbudget/*.txt                <- các file trong configs/
```

3. Khởi động lại, gõ `meta list` để kiểm.

Sửa file `.txt` chỉ cần khởi động lại máy chủ, không phải build lại.

**Tắt plugin:** đặt `stage.txt` = `0` (nạp nhưng không làm gì), hoặc xoá
`edictbudget.vdf`. Plugin chỉ sửa trong bộ nhớ, không ghi vào file game hay BSP.

### Chạy thử trước

Nên chạy vài ngày ở chế độ ít can thiệp, đọc `addons/edictbudget/edictbudget.log`, rồi mới bật thêm.

---

## Công tắc (`patches.txt`)

| công tắc | gói phát hành | nghĩa |
|---|---|---|
| `noedict` | `1` | dùng `noedict.txt` |
| `swap` | `2` | `0` tắt · `1` chỉ ghi log · `2` đổi thật, theo `swap.txt` |
| `wipeclear` | `2` | `0` tắt · `1` chỉ ghi log · `2` dọn thật |
| `freegate` | `0` | `0` tắt · `1` theo danh sách `freekeep.txt` · `2` vô điều kiện (**làm hỏng chuyển vật phẩm**) |
| `mapclear` | `1` | `1` chỉ ghi log. **Không dùng `2`** |
| `mapclearcarry` | `0` | **giữ `0`** — bật là sập khi chuyển màn |
| `trap` | `1` | ghi kiểm kê khi sắp hết edict |
| `heartbeat` | `300` | ghi số liệu mỗi N giây, `0` tắt |
| `loadprobe` | `8` | số frame lấy mẫu sau khi nạp map |
| `logconsole` | `0` | `1` = in cả ra console |

Các công tắc `bigarray`, `snapshot`, `pinmax`, `pinglobals`, `markfree`, `freetime`,
`indexbounds`, `forcedindex`, `detour`, `reuse`, `nonetkill`, `nonethigh`: **giữ `0`**. Đó là
các hướng đã bỏ, xem [docs/05-huong-da-bo.md](docs/05-huong-da-bo.md).

| file | dùng cho |
|---|---|
| `stage.txt` | `0` = nằm im, `1` = hoạt động |
| `noedict.txt` | lớp không cấp edict. Mỗi lớp có ghi trạng thái và bản đồ để tự kiểm |
| `swap.txt` | cặp đổi lớp |
| `wipekeep.txt` | lớp **không** bị `wipeclear` dọn sớm (cho plugin SourceMod không sửa được) |
| `freekeep.txt` | lớp không được tái dùng edict ngay (chỉ khi `freegate=1`) |
| `mapkeep.txt` | lớp không được dọn lúc chuyển màn (chỉ khi `mapclear=2`) |

---

## Phạm vi đã kiểm

- Chạy thật: chiến dịch tuỳ chỉnh `the_hive`, `chernobyl` (`ch04_pripyat03`), một số map
  chính thức (`c1m1_hotel`, `c1m3_mall`, `c2m4_barns`, `c6m1_riverbank`…). Chế độ Coop,
  1–4 người.
- Đo dài ngày: 7 phiên chơi dài, cả 7 kết thúc với ít entity hơn lúc bắt đầu; 0 lần
  `ED_Alloc` trong 105 phiên.
- Phần lớn số liệu còn lại là **đọc file map** (không chạy), dùng làm ước lượng.

---

## Phiên bản

| tag | thay đổi |
|---|---|
| `v2.0.0` | bản đầu |
| `v2.0.1` | sửa: `noedict` có thể nhận nhầm vtable của lớp khác (gặp ở `upgrade_spawn`) |
| `v2.0.2` | sửa: máy chủ sập lúc khởi động khi `noedict.txt` có một trong 9 lớp (vd `beam_spotlight`); `noedict.txt` không còn bị cắt im lặng sau 32 dòng |

---

## Build

```
build.bat
```

Đặt cạnh thư mục dự án: `hl2sdk-l4d2/` và `metamod-source-1.12.0.1225/`. MSVC 32-bit.
Dùng nguyên các định nghĩa trong `build.bat`.

---

## Tài liệu

[docs/README.md](docs/README.md)

## Tác giả

- **thienwu** — đặt bài toán, chạy máy chủ thật, đo và kiểm, quyết định hướng đi và những
  hướng không được đi (cấm nâng trần 4096, cấm động vào họ `phys`, cấm sửa file BSP).
- **Claude (Anthropic)** — dịch ngược, viết mã và tài liệu.

Nhiều kết luận rút ra từ dịch ngược nhị phân, kèm địa chỉ để kiểm lại. Cái gì chưa xác minh
được thì ghi là chưa.

## Giấy phép

GPLv3 (giống Metamod:Source). Xem `LICENSE` và `NOTICE`. Kho không chứa mã hay nhị phân của Valve.
