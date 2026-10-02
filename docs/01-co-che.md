# Các cơ chế

Địa chỉ trong bài tính theo ImageBase `0x10000000`. Bảng đầy đủ: [04-dia-chi.md](04-dia-chi.md).

---

## noedict

Không cấp edict cho những lớp entity không cần gửi về client.

Bảng entity của máy chủ có **4096 ô** (chỉ số 12 bit, 0–4095), chia sẵn làm hai phần cố định:

| phần | ô | dùng cho | tối đa |
|---|---|---|---|
| có mạng | 0–2047 | entity gửi về client, mỗi cái một edict | 2048 — trần của giao thức (chỉ số 11 bit trong gói tin) |
| không mạng | 2049–4095 | entity chỉ sống trên máy chủ (`EFL_SERVER_ONLY`) | **2047** |

(Ô 2048 không dùng.)

Khi tạo entity, `CBaseEntity::PostConstructor` xem cờ `EFL_SERVER_ONLY` (bit 9 của `m_iEFlags`)
để chọn phần. Cả hai phần đều có sẵn trong `server.dll` gốc. Entity không mạng chỉ nhận ô
2049–4095, không bao giờ nhận ô dưới 2049.

Dự án ban đầu thử nâng số edict lên 4096 nhưng không giữ được ổn định, nên chuyển sang cắt
giảm: dùng phần không mạng có sẵn này thay vì nới phần có mạng. Xem
[05-huong-da-bo.md](05-huong-da-bo.md).

**Giới hạn thực tế của phần không mạng:**

- 2047 ô dùng **chung** cho mọi entity không mạng: các lớp vốn server-only của game (46 lớp) và
  các lớp do `noedict` cắt. Thêm lớp vào `noedict.txt` là lấy bớt ô của phần này.
- Hết ô: game in `CBaseEntityList::AddNonNetworkableEntity: no free slots!` và trả handle
  không hợp lệ. Máy chủ không dừng, nhưng entity đó không có chỗ trong bảng (hệ quả cụ thể chưa
  đo).
- Đo với `noedict.txt` hiện tại trên 231 nguồn bản đồ, đếm mọi dòng lump như cùng sống một lúc:
  nhiều nhất `anemoia_kitty` **1486 / 2047**. Chưa có lần nào log in dòng `no free slots`.
  Con số này chưa tính entity tạo thêm lúc chơi.

Plugin thay **vtable slot 29** (`PostConstructor`) của các lớp trong `noedict.txt`: bật cờ rồi
gọi hàm gốc. Entity vẫn được tạo, vẫn chạy `Spawn()`/`Activate()`, chỉ không có edict.

Đo được: `ch04_pripyat03` trước đây chết ở 2048 edict lúc nạp; sau khi bật nạp được với
`num_edicts=1178`.

Chi tiết, điều kiện, cách thêm lớp: [02-noedict.md](02-noedict.md).

---

## swap

Đổi một lớp sang lớp rẻ hơn ngay lúc tạo. Client vẫn nhận entity như bình thường.

Chỉ có một cặp dùng được: `point_spotlight` → `beam_spotlight`. `point_spotlight` tự tạo thêm
`spotlight_end` + `beam` (3 edict); `beam_spotlight` vẽ ở client (1 edict).

Móc `CEntityFactoryDictionary::Create` (vtable slot 1 của dictionary) — phủ cả lúc nạp map
lẫn entity tạo lúc chơi.

Đo được trên `the_hive_m4`: entity sống 1954 → 1330 (giảm 624).

Cái giá: `client.dll` cố định kích thước quầng sáng là 60, nên map đặt `HaloScale 10` sẽ thấy
quầng sáng to gấp 6.

---

## wipeclear

Khi cả đội thua, game dọn map **muộn**: vòng hồi sinh người chơi chạy trước và ăn hết edict
còn lại, rồi mới tới `CleanUpMap`.

```
RestartRound (vtable slot 178)
  ├─ hồi sinh người chơi      <- edict cạn ở đây
  └─ CleanUpMap               <- game mới dọn ở đây
```

`wipeclear` móc đầu `RestartRound` và làm phần **dọn** của `CleanUpMap` **sớm hơn**, trước vòng
hồi sinh:

```
xoá mọi entity ngoài preserve list của game (38 lớp) và ngoài wipekeep.txt
-> CleanupDeleteList() -> AllowImmediateEdictReuse()
```

Sau đó game chạy tiếp bình thường, `CleanUpMap` dựng lại map từ entity lump.

Chỉ dọn khi vừa có sự kiện `mission_lost` (cờ một lần, xoá khi nạp map). Không có cổng này thì
nó dọn ngay lúc map vừa nạp và phá map — đã xảy ra thật.

Đo được trên `c6m1_riverbank`: 3 lần thua liên tiếp, mỗi lần giải phóng ~900 slot; trước đó
máy chủ chết ngay lần thua đầu.

### wipekeep.txt — cho plugin SourceMod không sửa được

Vì dọn **sớm hơn** game, plugin nào giữ tham chiếu tới entity bị dọn sẽ cầm tham chiếu treo và
máy chủ có thể sập.

- Sửa được plugin: dọn tham chiếu ở sự kiện `mission_lost` — nó bắn trước `RestartRound`.
- **Không sửa được plugin**: ghi lớp entity mà plugin giữ tham chiếu vào `wipekeep.txt`.

Lớp có trong `wipekeep.txt` thì `wipeclear` **không xoá sớm**. Nó vẫn bị `CleanUpMap` của game
xoá sau đó, **đúng thời điểm như khi không có plugin**, rồi map dựng lại từ entity lump. Tức là:
ghi một lớp vào đây là đưa lớp đó về hành vi gốc của game lúc thua.

```
weapon_gascan        <- khớp đúng tên lớp
weapon_              <- dòng kết thúc bằng '_' = khớp mọi lớp bắt đầu bằng tiền tố đó
```

- Tối đa 128 lớp, tên chỉ gồm `a-z A-Z 0-9 _` (dòng sai bị bỏ, có ghi log).
- Đọc **một lần khi plugin nạp** — sửa xong phải khởi động lại máy chủ.
- Giữ một lớp là giữ **mọi** entity của lớp đó, kể cả entity của bản đồ.
- Mỗi lần thua, log ghi `go X, giu Y (trong do Z do wipekeep.txt)`.

Cái giá: entity giữ lại vẫn chiếm edict trong lúc vòng hồi sinh chạy. Đã thử giữ cả họ `weapon_`
(~190 entity): máy chủ chết **sớm hơn** (2041 sống / 7 trống, so với 1042 / 1006 khi không giữ).
Chỉ ghi đúng lớp plugin cần.

Hướng dẫn đầy đủ nằm ngay trong file `configs/wipekeep.txt`.

Không dùng `wipeclear` được thì đặt `wipeclear=1` (chỉ ghi log) hoặc `0`.

---

## freegate

**Cơ chế yếu, gần như không có tác dụng thật. Gói phát hành để tắt (`freegate=0`).**

Engine không cấp lại một edict trong 1 giây sau khi nó được giải phóng. `freegate` bỏ khoảng
chờ đó.

Lý do nó ít tác dụng: chỗ cần nhất là lúc thua, và `wipeclear` đã gọi
`AllowImmediateEdictReuse()` sau khi dọn — tức đã bỏ khoảng chờ cho đúng lúc đó.

| giá trị | làm gì |
|---|---|
| `0` | tắt |
| `1` | móc `RemoveEdict` (slot 23); lớp không có trong `freekeep.txt` thì tái dùng ngay |
| `2` | sửa một byte trong `engine.dll`, vô điều kiện |

**Không dùng `2`.** Plugin chuyển vật phẩm (vd Gear Transfer) huỷ rồi tạo lại vật phẩm trong
cùng một frame; chế độ `2` trả lại đúng chỉ số vừa giải phóng nên client thấy vũ khí "ma".
`freekeep.txt` mặc định liệt kê các vật phẩm đó cho chế độ `1`.

---

## mapclear

Quan sát entity lúc chuyển màn (móc `PrepChangelevel`, slot 38). Gói phát hành để `1` = chỉ
ghi log.

Không dùng `2` (dọn thật): xoá entity **được mang sang màn sau** làm máy chủ sập; còn ở mức an
toàn thì gần như không lấy được gì. `mapclearcarry` luôn để `0`.
