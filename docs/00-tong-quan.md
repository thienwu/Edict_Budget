# Tổng quan — nhiệm vụ, giới hạn, số liệu

*Tiếng Việt (bản chính) · [English](00-tong-quan.en.md)*

---

## Tác giả — đọc trước

Dự án là kết quả của **hai phần việc khác nhau, không tách rời được**.

**Ý tưởng, bài toán và định hướng — `thienwu`, người vận hành máy chủ thật.** Họ đặt bài
toán từ một sự cố thật, quyết định mọi hướng đi lớn, và quan trọng hơn là quyết định
**những hướng KHÔNG được đi**: cấm hướng 4096, cấm động vào họ `phys`, cấm sửa file BSP,
và yêu cầu **công thức chung** — phải áp cho mọi map, plugin phải **tự kiểm lúc chạy**.
Họ chạy thử, chụp log, đo trên máy chủ thật, và **bác bỏ nhiều kết luận sai của AI**.
Nhiều đoạn trong tài liệu này ghi rõ *"SAI, đã sửa"* chính là vì thế.

**Dịch ngược, viết mã, đo đạc và tài liệu — Claude (Anthropic), chạy trong Claude Code.**
Đọc ngược `server.dll`/`engine.dll`/`client.dll`, thiết kế và viết toàn bộ mã nguồn, chạy
các phép đo, và viết cả những ghi chú bạn đang đọc.

Nói rõ điều này vì hai lý do:

1. Ai đọc mã nên biết nó đến từ đâu để tự quyết định mức độ tin tưởng.
2. Nhiều kết luận ở đây rút ra từ **dịch ngược nhị phân**, không phải từ tài liệu chính
   thức. Chúng đều kèm **địa chỉ hàm** và **đoạn lệnh** để kiểm lại được. Cái gì không xác
   minh được thì **ghi thẳng là KHÔNG XÁC ĐỊNH**.

**Giấy phép: GPLv3.** Xem file `LICENSE`.

---

## Nhiệm vụ

Giữ số entity **đang sống** dưới trần **2048**.

**KHÔNG nâng trần** — chỉ số entity trong giao thức Source rộng **11 bit** (tối đa 2047),
nên entity **có mạng** nằm ở chỉ số ≥ 2048 sẽ bị client giải mã sai.

### ⚠️ Giới hạn — phải đọc

**Bản vá này KHÔNG ngăn được hoàn toàn `ED_Alloc: no free edicts`.**

Nó chỉ làm mấy việc: **thu hồi edict đúng lúc**, **cho phép tái dùng slot vừa giải phóng**,
**gỡ edict khỏi những lớp thực sự không dùng mạng**, và **đổi lớp entity sang lớp rẻ hơn**.

Nếu bản thân map cần nhiều hơn 2048 entity **có mạng** cùng lúc thì **không có cách nào
cứu** — đó là trần của **giao thức**, không phải của bản vá.

> **Ví dụ đo được:** một map cộng đồng dùng 312 `point_spotlight` + 312 `spotlight_end`
> + 312 `beam` = **936 edict (45,7%)** chỉ riêng cho hiệu ứng ánh sáng. Cả ba lớp đều phải
> có mạng.
>
> *(Câu này đã được cập nhật — cơ chế `swap` **đã** xử lý được đúng trường hợp này. Xem
> mục "Kết quả đo được" bên dưới.)*

---

## Bốn cơ chế đang chạy

### 1. `wipeclear` — dọn khi đội survivor thua

Lúc wipe, game **có** dọn entity nhưng dọn **muộn**. Trình tự thật:

```
CDirector::Restart -> RestartRound (slot 178)
                        |-- hồi sinh player       <== ăn hết edict Ở ĐÂY
                        |-- CleanUpMap (slot 179) <== mới dọn, ĐÃ MUỘN
```

Móc vào **đầu** `RestartRound`, làm nửa "dọn" của `CleanUpMap` **trước** đoạn ngốn kia:

```
UTIL_Remove(ngoài preserve list) -> CleanupDeleteList() -> AllowImmediateEdictReuse()
```

rồi để game chạy tiếp; `CleanUpMap` tự dựng lại map từ entity lump.

> Chỉ dọn khi có `mission_lost` đang chờ (cờ một-lần). Không có cổng thì nó dọn ngay lúc
> `t=1.00` khi map vừa nạp và **PHÁ MAP**.

🟢 **Đo được:** 5 wipe liên tiếp (map thường), 3 wipe liên tiếp (`c6m1_riverbank`).

### 2. `freegate` — cho phép tái dùng slot vừa giải phóng

`ED_Alloc` **từ chối** tái dùng một edict trong **1 giây** sau khi nó được giải phóng.
Wipe xoá rồi tạo lại **hàng trăm** entity trong **cùng một khoảnh khắc**, nên không cái nào
qua nổi cổng đó ⇒ **chết trong khi còn ~999 slot trống**.

Đổi **một byte** trong `engine.dll`: `jae` → `jmp`. Định vị bằng **quét chữ ký**.

An toàn nhờ `sv_useexplicitdelete` (mặc định bật) — Valve thiết kế nó **thay cho** thời
gian chờ này.

🟢 **Đo được (đối chứng):** cùng tình huống `num_edicts=2048` + ~999 slot trống,
`freegate=0` → **CHẾT**, `freegate=1` → **chạy tiếp bình thường**.

> ⚠️ **Chưa nghiệm thu dài hạn.** Có nghi ngờ (chưa chứng minh) rằng chạy lâu thì gây nghẽn
> hệ thống, và làm sai lệch số entity ở máy chủ ≥ 4 người chơi. Gói xuất bản để
> `freegate=1` vì nó đã qua phép đo đối chứng; máy chủ đông người thấy tickrate lạ thì
> **đặt về 0 trước tiên**.

### 3. `noedict` — khiến lớp không dùng mạng KHÔNG lấy edict

`CBaseEntity::PostConstructor` xét **bit 9** của `m_iEFlags` (`EFL_SERVER_ONLY`):

| bit 9 | đường đi | tốn edict? |
|---|---|---|
| `0` | `AddNetworkableEntity` → dải 0–2047 | **CÓ** |
| `1` | `AddNonNetworkableEntity` → dải 2049–4095 | **KHÔNG** |

Dải 2049–4095 (2047 ô) là **thiết kế gốc của engine**. Tràn nó chỉ **in cảnh báo** rồi trả
handle không hợp lệ — **KHÔNG giết server**.

Thay vtable **slot 29** (`+0x74`) của riêng các lớp được bật, bật bit rồi gọi hàm gốc.
**Không vá byte, không đụng `engine.dll`.**

🟢 **Đo được:** một map tự **CHẾT ở 2048 edict** → nạp được với `num_edicts=1178`.

### 4. `swap` — đổi lớp entity sang lớp rẻ hơn lúc tạo

Móc `CEntityFactoryDictionary::Create` (**vtable slot 1**), ghi đè classname ngay lúc tạo.

**Cặp duy nhất dùng được:** `point_spotlight` → `beam_spotlight` (3 edict → 1).

🟢 **Đo được:** `sống 1954 → 1330`, giảm đúng **624**, chỗ thở `93 → 718` slot.

Xem chi tiết ở [01-co-che.md](01-co-che.md), địa chỉ ở [06-dia-chi.md](06-dia-chi.md).

---

## Kết quả đo được trên ba chiến dịch căng nhất

Trần engine: `max_edicts = 2048`. Cột **"EDICT dự kiến"** đọc từ lump 0 của BSP bằng
`tools/bsp_cost.py`; cột **"đo thật"** là `num_edicts` lúc chạy trên máy chủ.

```
EDICT = (entity trong lump) - (lớp trong noedict.txt)
        + 2 x point_spotlight có spawnflags&1
```

### 1. `chernobyl` (5 map) — chứa `ch04_pripyat03`, map khởi nguồn của cả dự án

| map | lump | `noedict` gỡ | EDICT dự kiến | tổng lump |
|---|---|---|---|---|
| `ch01_jupiter` | 1532 | 316 | 1216 | 1532 |
| `ch02_pripyat01` | 2204 | 1138 | 1067 | 2205 |
| `ch03_pripyat02` | 1686 | 869 | 816 | 1685 |
| `ch04_pripyat03` | 2246 | 1039 | **1212** | 2251 |
| `ch05_pripyat04` | 940 | 301 | 648 | 949 |

**Phép kiểm ngược:** `ch04_pripyat03` **trước** khi có `noedict`: **CHẾT ở 2048** lúc nạp.
Công thức dự đoán (có `noedict`): **1212**. Đo thật trên máy chủ: **1178**. Sai lệch **+34**,
tức **2,9%**.

> Công thức viết ra **SAU**, khớp với sự cố xảy ra **TRƯỚC**.

### ⚠️ Giới hạn phải nhớ — đừng biến con số này thành phán quyết

Cột **"tổng lump" KHÔNG PHẢI** số entity cùng sống một lúc. Nó chỉ là **số dòng trong lump**.
Thực tế entity được **kích hoạt dần**:

- `weapon_*_spawn` tự `UTIL_Remove` chính nó ngay sau khi sinh vũ khí
- `StartDisabled` chưa kích hoạt
- `point_template` sinh muộn
- Director sinh dần theo tiến trình chơi

`ch02_pripyat01` có "tổng lump" **2205** nhưng **KHÔNG HỀ CHẾT**, kể cả trước khi có
`noedict`. `ch04_pripyat03` ở **2251** thì chết. Hai con số chỉ cách nhau **46** ⇒ **không
có ngưỡng sạch nào ở đây**.

Sai số đo được, **lần nào cũng thừa**:

| map | dự đoán | đo thật | lệch |
|---|---|---|---|
| `the_hive_m3` | 1688 | 1592 | −96 |
| `the_hive_m4` | 2067 | 1955 | −112 |
| `pripyat03` | 1212 | 1178 | −34 |

> ⇒ Dùng công thức làm **CẬN TRÊN** và **BẢNG XẾP HẠNG**. Muốn biết map có chết không thì
> phải **ĐO**: `loadprobe` (8 frame đầu) và `heartbeat` (mỗi 5 phút).

### 2. `the_hive` (5 map)

| map | EDICT dự kiến | ghi chú |
|---|---|---|
| `m1` | 966 | |
| `m2` | 1834 | 639 `env_sprite` — **KHÔNG gỡ được**, xem dưới |
| `m3` | 1688 | 80 `point_spotlight` (hệ số 3) |
| `m4` | **2067** | **VƯỢT TRẦN.** 312 `point_spotlight` = 936 edict |
| `m5` | 1343 | |

Đo thật trên máy chủ, `m4`: đỉnh `num_edicts=1955`, trống = 0, **chỗ thở 93 slot**.

Sau khi bật `swap`: **sống 1954 → 1330**, chỗ thở **93 → 718 slot**.
`m3` sau khi bật `swap`: **sống 1591 → 1431**.

### 3. `anemoia` / `backroom` (6 map)

| map | lump | `noedict` gỡ | EDICT dự kiến |
|---|---|---|---|
| `arcade` | 1246 | 433 | 812 |
| `kitty` | 2954 | **1444** | 1509 ← `noedict` **CỨU** map này |
| `party` | 2198 | 399 | 1798 (964 `prop_dynamic`) |
| `poolrooms` | 921 | 306 | 640 |
| `poolrooms2` | 914 | 313 | 626 |
| `reality` | 1351 | 488 | 862 |

`kitty` là bằng chứng mạnh nhất cho `noedict`: không có nó thì map ~2953 edict, **vượt trần
900 slot**, chết chắc lúc nạp.

### Tổng kết giảm được bao nhiêu

**`noedict`** (số entity được đặt `EFL_SERVER_ONLY`):

| map | số entity |
|---|---|
| `anemoia kitty` | **1444** |
| `chernobyl ch02` | 1138 |
| `chernobyl ch04` | 1039 |
| `chernobyl ch03` | 869 |
| `anemoia reality` | 488 |
| `the_hive m4` | 465 |
| `the_hive m3` | 443 |

> Chỉ **MỘT** trường hợp đã **CHỨNG MINH** được là *"không có nó thì chết"*:
> `ch04_pripyat03` — chết thật ở 2048 trước khi có `noedict`, sau đó nạp được với
> `num_edicts=1178`. Các map khác chỉ là **con số lump lớn, CHƯA CHỨNG MINH**.

**`swap`:**

| map | edict giảm |
|---|---|
| `the_hive m4` | 624 (312 × 2) |
| `the_hive m3` | 160 (80 × 2) |
| `the_hive m5` | 8 (chỉ 4/12 cái có `spawnflags&1`) |
| `anemoia` | ~26/map (chỉ `poolrooms` có 13 cái) — không đáng |

---

## Rủi ro của `swap` đã được định lượng (15/08)

**Nhỏ hơn nhiều so với lo ngại ban đầu.**

`beam_spotlight` **giữ** `FCAP_ACROSS_TRANSITION` còn `point_spotlight` thì **bỏ**, nên ban
đầu tưởng số entity mang sang tăng mạnh. Đo lại bằng **PVS thật**:

| chuyển màn | tăng thêm |
|---|---|
| `m4 → m5` | **+0** (312 `beam_spotlight` của m4 **không cái nào** trong PVS landmark) |
| `m3 → m4` | **+48** (48 `point_spotlight` nằm trong PVS của `landmark_m4` trên m3) |

Danh sách chuyển màn thật: `m3→m4` = 22 entity, `m4→m5` = 32. **Trần engine 512.**

> Đổi **48 edict** lấy **784** ⇒ **KHÔNG SỬA.**

### Vì sao trước đó ước nhầm 739/1051

`server.dll` có **54 hàm `ObjectCaps` khác nhau**; **31** trong số đó cùng
`and eax,0xFFFFFFFD` (bỏ cờ) nhưng **khuôn byte khác** `CPointEntity` nên bị bỏ sót.

Riêng `the_hive`: `CSprite@1009A5D0` (`env_sprite` 236), `CBeam@10081580` (`beam` 312),
`CSpotlightEnd@101DEEB0` (`spotlight_end` 312) **đều bỏ cờ**.

### 🔑 `noedict` MIỄN NHIỄM HOÀN TOÀN với chuyển màn

`CChangeLevel::BuildChangeList` @`0x101FF060` **KHÔNG** duyệt `gEntList`. Nó duyệt
`UTIL_EntitiesInPVS(landmark)` @`0x10209BC0` — chỉ entity trong PVS của `info_landmark`
(**1–4% bản đồ**) — **VÀ** có dòng:

```asm
cmp dword [esi+0x28], 0 ; je    ; bỏ qua entity KHÔNG CÓ EDICT
```

Vượt 512 gọi `tier0!Warning` (**KHÔNG** phải `Error`), giữ 512 mục đầu, bỏ phần dư.

---

## Chỗ plugin chưa xử lý được

| map | EDICT | thủ phạm |
|---|---|---|
| `the_hive m2` | 1834 | **639 `env_sprite`** |
| `anemoia party` | 1798 | **964 `prop_dynamic`** |

Cả hai lớp đều **CÓ SendTable riêng** (`CSprite`, `CDynamicProp`) nên **không gỡ mạng được**,
và đều **hệ số 1** nên `swap` vô dụng.

> **Cập nhật 23/08:** hướng sửa entity lump (`nonetkill` → `killent`) đã được điều tra đầy
> đủ và **bác bỏ**. Xem [07-het-huong.md](07-het-huong.md) mục 4. Với hai lớp này thì câu
> cũ vẫn đúng: **cách chữa nằm ở người làm map.**

---

## File cấu hình

Đặt trong `left4dead2/addons/edictbudget/`:

| file | tác dụng |
|---|---|
| `stage.txt` | `0` = nằm im hoàn toàn · `1` = hoạt động |
| `patches.txt` | công tắc từng phần; đổi xong chỉ **khởi động lại server** |
| `noedict.txt` | lớp bật `EFL_SERVER_ONLY`. Trước khi thêm lớp mới phải qua **đủ 6 điều kiện** — ghi trong chính file đó |
| `swap.txt` | cặp đổi lớp |
| `wipekeep.txt` | lớp **GIỮ THÊM** khi `wipeclear` dọn. **ĐỂ TRỐNG mới đúng**: ở wipe, entity bị xoá sẽ **được dựng lại** từ entity lump, nên giữ thêm chỉ làm hẹp biên độ |
| `mapkeep.txt` | lớp **KHÔNG ĐƯỢC DỌN** khi chuyển màn (chỉ dùng khi `mapclear >= 2`). **Ngược với `wipekeep`**: ở chuyển màn, xoá nhầm là **MẤT VĨNH VIỄN** |

---

## Build

> `SOURCE_ENGINE` **PHẢI** = **15** (`LEFT4DEAD2`) theo cách đánh số của Metamod.
>
> Build nhầm **11** (TF2) làm **lệch mọi chỉ số vtable**, `SH_CALL` gọi nhầm hàm engine.

---

## Hướng 4096: nâng giới hạn là làm được. Giới hạn 11 bit mới là không thể.

Phải nói rõ **hai chuyện khác nhau**, đừng gộp làm một:

| | |
|---|---|
| (a) **Nâng số edict** lên 4096 hoặc cao hơn | ✅ **LÀM ĐƯỢC** — mã có sẵn, mặc định tắt |
| (b) Đặt entity **CÓ MẠNG** ở chỉ số ≥ 2048 | ❌ **KHÔNG THỂ**, và không bao giờ làm được bằng cách vá server |

**Lý do (b):** chỉ số entity được mã hoá trong gói tin bằng **trường 11 bit** (tối đa 2047).
Đó là **định dạng GÓI TIN**, nằm ở **cả hai đầu dây** — client và server. Server không làm
client hiểu được chỉ số 2048; client sẽ giải mã ra một chỉ số khác hẳn. Muốn sửa thì phải
sửa cả `client.dll` của **từng người chơi**, tức không khả thi.

> ⇒ Chỗ trống ở dải 2048–4095 **CHỈ** dùng được cho entity **KHÔNG CÓ MẠNG**. Mà engine
> **ĐÃ CÓ SẴN** cơ chế cho việc đó: `EFL_SERVER_ONLY` + nửa trên của `m_EntPtrArray` — chính
> là `noedict`. **Không cần vá byte nào.**

### Các byte nâng giới hạn — ghi lại để đối chiếu, mặc định TẮT HẾT

| công tắc | làm gì |
|---|---|
| `bigarray` | `SV_AllocateEdicts` cấp 4096 edict thay vì 2048. Ghi đè 4 byte tại `m+1` bằng số ô muốn cấp (`EXT_LIMIT = 4096`). Đặt 8192 cũng chạy |
| `snapshot` | Đổi **7 chỗ** truy cập `m_pPackedData` / `m_pSerialNumber` sang bộ đệm 4096 ô. **BẮT BUỘC đi kèm `bigarray`** — mảng 4096 edict với bảng snapshot 2048 ô thì **TỆ HƠN là không làm gì** |
| `pinmax` | `LevelInit` ghim `sv.max_edicts` về 2048 |
| `pinglobals` | `LevelInit` ghim `gpGlobals->maxEntities` về 2048 |
| `markfree` | `LevelInit` đóng dấu `FL_EDICT_FREE` lên các ô 2048–4095 |

Chữ ký `bigarray` trong `engine.dll`:
```asm
B8 00 08 00 00        mov eax, 0x800      ; <- 2048
89 86 18 02 00 00
A3 ?? ?? ?? ??
```

Chữ ký `snapshot` (7 chỗ): `8B 84 B1 9C ...` → `8B 04 B5 <địa chỉ mới>`

`pinmax` + `pinglobals` giữ **TRẦN CỦA ENGINE** ở 2048 để bộ cấp phát của engine không tự
đặt entity lên dải cao. Thiếu chúng thì `num_edicts` leo qua 2047 và entity **CÓ MẠNG** tràn
lên dải cao — **đúng điều (b) nói trên**.

### 🛑 Vì sao vẫn TẮT HẾT

1. Nhóm này **LÀM HỎNG VÒNG HỒI SINH LÚC WIPE** — tức **phá luôn `wipeclear`**, thứ duy nhất
   đang giải quyết được đợt bùng lớn nhất.
2. Nó **không giải được bài toán gốc**. Chỗ trống ở dải cao chỉ chứa được entity không có
   mạng, mà việc đó `noedict` làm được bằng **đường CHÍNH THỨC** của engine, không cần vá byte.
3. **Đo thực tế:** bật `bigarray`+`snapshot` mà thiếu `pinmax`/`pinglobals` thì
   `num_edicts = 2060`, entity **NGẪU NHIÊN** tràn lên trên 2047 — **mất ổn định**.

Mã vẫn còn để đối chiếu và để ai muốn đo lại thì có sẵn. **Không được bật trong bản chạy.**

Chi tiết từng byte: [03-huong-4096.md](03-huong-4096.md).
