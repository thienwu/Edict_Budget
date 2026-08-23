# `mapclear` — dọn entity lúc chuyển màn

*Tiếng Việt (bản chính) · [English](02-mapclear.en.md)*

> **Đọc mục này ngay cả khi bạn không bao giờ bật `mapclear`.** Nó chứa bài học đắt nhất
> của cả dự án: **xoá một entity mang sang màn sau thì máy chủ sập** — và quy tắc
> *"xoá ít đi"* mà ai cũng nghĩ ra đầu tiên là **quy tắc sai**.

---

## 1. `mapclearcarry` — vì sao KHÔNG được xoá cái mang sang

```
1 = CHỈ xoá entity có FCAP_ACROSS_TRANSITION
0 = xoá tất cả (mặc định, hành vi cũ)
```

> 🛑 **ĐỂ MẶC ĐỊNH 0.** Chế độ 1 đã được thử lúc **09:44 ngày 14/08** và **giết server ngay**.

```
MAPCLEAR #1 (chế độ 2, chỉ-mang-sang=1):
  tổng 1551 | mang sang 295 | gỡ 200 (mang sang 200), giữ 156,
  bỏ qua vì không mang sang 1153, chạm trần 42
  -> "Server is hibernating" + khởi động lại.
     Không ED_Alloc, không assert.
```

### Quy tắc đúng — giải thích được CẢ BA lần chết

Quy tắc cũ *"ngưỡng 1300"* đã sai. Quy tắc đúng:

| lần thử | số cái mang sang bị xoá | kết quả |
|---|---|---|
| cấp 100 | 9 | **SỐNG** |
| > 1300 | ~270 | **CHẾT** |
| `carry=1` | 200 | **CHẾT** |

> **KHÔNG PHẢI SỐ LƯỢNG GIẾT. LÀ XOÁ CÁI MANG SANG GIẾT.**
>
> *"Ngưỡng 1300"* chỉ là trùng hợp: xoá càng nhiều thì càng vơ phải nhiều cái mang sang.

### Cơ chế

Hook chạy **POST** nên `PrepareLevelChange` gốc **đã lập xong** danh sách chuyển màn.
Entity có `FCAP_ACROSS_TRANSITION` nằm sẵn trong danh sách đó. Xoá sau khi danh sách đã
lập ⇒ danh sách trỏ vào **vùng đã giải phóng** ⇒ sập khi engine xử lý chuyển màn.

Hook **PRE** cũng không cứu được: engine vẫn phải đọc chính những entity ấy để lập danh sách.

### Hệ quả

Chỉ cái **mang sang** mới tốn edict ở map sau, mà cái mang sang thì **không được đụng vào**.

> ⇒ **`mapclear` về nguyên tắc KHÔNG giải quyết được bài toán "m3 → m4".**
> Giữ công tắc này lại chỉ để **ghi lại thí nghiệm**, **không phải để bật**.

---

## 2. Cơ chế đầy đủ

> 🛑 **KHÔNG PHẢI hướng 4096.** Không `bigarray`/`snapshot`/`pinmax`/`pinglobals`/`markfree`.
> Không vá một byte nào. Chỉ móc vtable, giống hệt `wipeclear`.

### ⚠️ Khác `wipeclear` ở điểm sinh tử — đọc trước khi sửa

| | xoá nhầm thì sao | luật |
|---|---|---|
| **`wipeclear`** | cùng map — entity **được dựng lại từ lump** | giữ **TỐI THIỂU** (`wipekeep.txt` để **RỖNG**) |
| **`mapclear`** | map khác — **MẤT VĨNH VIỄN** đồ người chơi | giữ **TỐI ĐA**. Không chắc thì **GIỮ**. |

> **TUYỆT ĐỐI** không bưng tập giữ của hai bên cho nhau.

### Cơ chế (đã đọc trên binary)

- Map mới bắt đầu bằng **bảng edict MỚI**. Rác **ngoài** vùng chuyển tiếp tự biến mất ⇒
  dọn nó vô ích. Chỉ thứ **nằm trong danh sách mang sang** mới đáng dọn.
- `CBaseEntity::ObjectCaps()` `0x10056160` **mặc định** trả `FCAP_ACROSS_TRANSITION` ⇒
  gần như **mọi thứ** trong vùng đều được mang sang, kể cả xác và mảnh vỡ.
- **Hai đường mang sang:**

| đường | cơ chế | tốn edict? |
|---|---|---|
| (a) đồ **trên tay** | `CTerrorGameRules` slot 38 serialize thành KeyValues (`weaponID`, `currentMagazine`, `extraAmmo`...) | **KHÔNG** |
| (b) đồ **rơi dưới đất** | `trigger_transition` chuẩn của Source | **CÓ** |

  ⇒ chỉ **(b)** là vấn đề.
- `the_hive_m4` vào map chỉ còn **31 slot trống** ⇒ mang sang ~32 là chạm trần.

### Điểm móc

**`CTerrorGameRules` vtable slot 38 = `0x102B8140`**, hook **POST**.

In ra `"Preparing player entities for changelevel"`. `__thiscall`, `ret 4`.
Nằm trên **map cũ**, **sau** ảnh chụp người chơi, **trước** khi bộ máy save khởi động.
Cùng vtable mà `wipeclear` đang móc (slot 178 = `RestartRound` = `0x102E0650`).

> 🛑 **ĐÃ BỎ slot 27** (`BuildAdjacentMapList`): nó chạy ở 3 chỗ, **2 chỗ ở MAP MỚI**
> (`CSaveRestore::LoadAdjacentEnts` + đường nạp `.HL2`). Hook mù = xoá entity map mới ngay
> lúc nạp. Và tại slot 27 thì `SaveGameState` đã gọi `PreSave` ⇒ bảng entity đã dựng ⇒ xoá
> có nguy cơ **con trỏ treo**.

> 🛑 **ĐỪNG** chạy `g_debug_transitions` để "xem engine in ra": cvar đó **chặn luôn** việc
> chuyển màn, đặt `m_pfnTouch = 0` ⇒ **cửa phòng an toàn thành cửa chết**.

### Tập giữ mặc định

An toàn; đọc thêm từ `mapkeep.txt`:

- **toàn bộ preserve list CỦA GAME** (dùng chung hàm với `wipeclear`) — bảo thủ
- `player`, `weapon_` (trùm cả `weapon_*_spawn`, gascan / propanetank / oxygentank)
- `prop_fuel_barrel` (trùm cả `_piece`)
- **hạ tầng chuyển màn**: `info_landmark`, `trigger/info_changelevel`, `trigger_transition`
  — xoá mấy cái này là hỏng **chính** việc chuyển màn

### Công tắc

```
mapclear = 0  tắt
           1  CHỈ QUAN SÁT (đếm + ghi log, không xoá)
           2  dọn thật
```

*(`g_MapClear` khai báo ở đầu file, cạnh các công tắc khác.)*

---

## 3. `WillCarryOver` — hỏi chính engine

Hỏi thẳng engine: **entity này CÓ được mang sang map mới không?**

`ObjectCaps()` là hàm ảo **slot 40** (`+0xA0`). Bit `0x2` = `FCAP_ACROSS_TRANSITION`.

Cả `InTransitionVolume` (`0x101FEFB0`) lẫn `ComputeEntitySaveFlags` (`0x101F8D80`) đều
gọi đúng chỗ này ⇒ hỏi thẳng, khỏi phải truyền tên vùng.

> ⭐ **ĐÂY LÀ KHÁC BIỆT CỐT LÕI SO VỚI BẢN 1** (bản 1 làm **chết** server):
> bản 1 quét **cả 1659 entity** rồi xoá **1497** → đụng cả thứ engine đang cần.
> Bản này **chỉ** đụng tới entity **thật sự sẽ đi theo** ⇒ phạm vi nhỏ hơn hẳn.

---

## 4. Danh sách không được dọn

| nhóm | classname |
|---|---|
| vật phẩm của người chơi | `weapon_`, `prop_fuel_barrel*` |
| điểm chuyển map | `info_landmark`, `trigger/info_changelevel`, `trigger_transition` |
| cửa an toàn | `prop_door_rotating_checkpoint` |
| người chơi | `player` |

**Cộng toàn bộ preserve list CỦA GAME** (`worldspawn`, `terror_gamerules`, `soundent`,
`scene_manager`... — xoá mấy cái này là **chết ngay**).

---

## 5. Cổng an toàn 1: prologue

Prologue của `0x102B8140` phải đúng.

> ⚠️ **BÀI HỌC 09/08: prologue này CÓ ĐỊA CHỈ TUYỆT ĐỐI, KHÔNG được so cả 16 byte.**

```
55 8B EC 56 8B 35 | E0 7A 89 10 | 8B 06 8B 50 68 8B
                    ^^^^^^^^^^^ mov esi,[0x10897AE0]
```

Bốn byte đó bị **trình nạp ghi lại** khi `server.dll` nạp ở base khác ⇒ so nguyên 16 byte
thì **không bao giờ khớp**, cổng an toàn chặn oan.

*(`wipeclear` không dính, vì prologue của nó không có địa chỉ tuyệt đối.)*

⇒ Dùng **mặt nạ**: `'?'` = byte bị relocate, bỏ qua.
