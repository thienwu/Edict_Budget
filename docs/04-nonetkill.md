# `nonetkill` — đổi tên classname trong lump (ĐÃ LOẠI BỎ)

*Tiếng Việt (bản chính) · [English](04-nonetkill.en.md)*

Đổi tên classname **tại chỗ** trong entity lump, ở `LevelInit`, để entity **không bao giờ
được tạo ra**.

**Kết luận: đã loại bỏ.** Giữ lại tài liệu để không ai suy lại rồi bật nhầm.

---

## Cơ chế (đã xác minh trên binary)

Classname lạ thì:

```
CEntityFactoryDictionary::Create (0x10206A40)
  -> DevWarning("Attempted to create unknown entity type %s!")
  -> trả NULL
MapEntity_ParseEntity (0x101198F0)
  -> DevWarning("Can't init %s"), KHÔNG deref NULL
MapEntity_ParseAllEntities (0x1011A600)
  -> bỏ qua NULL
```

⇒ Entity **im lặng không được spawn**, không tốn edict, không tốn hạn mức nào.

Khớp tài liệu Valve: *"Entities... not recognized by the server do not create edicts...
they are simply not spawned."*

---

## ⚠️ Hai đường giết server — phải tránh

| địa chỉ | tình huống | hậu quả |
|---|---|---|
| `0x1011A6C0` | khối không mở bằng `{` | `tier0!Error` (import `0x105C1224`) |
| `0x10119943` | khối thiếu khoá `classname` | `tier0!Error` |

> **TUYỆT ĐỐI** không xoá khối, không đổi độ dài chuỗi. **CHỈ ghi đè giá trị.**

### Cách đổi an toàn

Thay **đúng một ký tự đầu** thành `~`:

```
infodecal  ->  ~nfodecal
```

Bảo đảm cùng độ dài, và **không classname nào của L4D2 bắt đầu bằng `~`** (đã liệt kê
557 classname, không cái nào).

---

## 🛑 Danh sách mặc định: RỖNG. Đừng thêm `light*` hay `infodecal` vào đây.

Mặc định cứng tay `{ infodecal, light, light_spot }` là **SAI** — nó làm
`ch04_pripyat03` **hiển thị sai ánh sáng**.

### Nguyên nhân gốc — `nonetkill` khác `noedict` ở một điểm sinh tử

| | entity có được tạo? | `Spawn()` / `Activate()` có chạy? | tác dụng phụ |
|---|---|---|---|
| **`noedict`** | **có** — chỉ không cấp edict | **có** | **giữ nguyên** ⇒ ánh sáng đúng |
| **`nonetkill`** | **không bao giờ tồn tại** | **không** | **mất sạch** |

⇒ `nonetkill` **sai về bản chất** với mọi entity mà **giá trị của nó nằm ở tác dụng phụ
lúc spawn**.

### Đã xác minh trên binary (`output/binscan/step_light.py`)

**`CLight::Spawn` `0x1010FA10`** — dùng chung cho `light` / `light_spot` /
`light_directional`; `light_environment` là `jmp` tới đây:

```asm
[esi+0x140] m_iszName == 0  ->  UTIL_Remove(this)          ; đèn "trơ", tự xoá
[esi+0x140] m_iszName != 0  ->  nếu m_iStyle >= 32:
                                  engine->LightStyle(m_iStyle, pattern)
                                  ; 0x107F7698 = g_pEngineServer, vtable +0xA0
```

Đèn **có tên** = đèn **bật/tắt được**. VRAD nướng nó thành một lightstyle riêng lúc
compile; entity lúc chạy là thứ **duy nhất** đặt trạng thái đầu cho lớp lightmap đó.

> Cắt entity ⇒ `LightStyle()` không chạy ⇒ lớp đó giữ mặc định ⇒ **sáng sai**.
> Đèn **không tên** thì đã tự xoá sẵn, cắt cũng **không được gì**.

**`CDecal::Spawn` `0x102362A0` / `CDecal::Activate` `0x10236D10`:**

```
Spawn:    m_nTexture < 0  hoặc  (deathmatch && lowprio)  ->  UTIL_Remove
          còn lại -> SỐNG. Máy chủ dedicated không phải deathmatch => SỐNG.
Activate: không có targetname -> jmp StaticDecal()   (dán decal rồi TỰ XOÁ)
```

⇒ `infodecal` **chưa từng giữ edict lâu dài**. Cắt nó tiết kiệm **gần bằng 0**, đổi lại
**toàn bộ decal của map**. Lỗ vốn nặng.

*(`infodecal` do VScript tạo thì sinh lúc chạy, không qua lump ⇒ không dính.)*

> ⚠️ Muốn giảm edict cho họ `light` / `infodecal` thì dùng **`noedict`**, không phải đây.

---

## Nếu vẫn muốn dùng

Đọc từ `nonetkill.txt` nếu có (mỗi dòng một classname). Trước khi thêm **bất cứ** lớp nào,
phải trả lời được:

> **"`Spawn()` / `Activate()` của nó có làm gì không?"** — Có ⇒ **KHÔNG được cắt.**

🛑 **Không** thêm lớp nhóm "sống lâu dài": `logic_auto`, `func_nav_attribute_region`,
`info_gamemode`, `info_survivor_position`...

---

## Vì sao hạng mục này đóng hẳn

Tập *"dùng được `nonetkill` mà không dùng được `noedict`"* là **tập rỗng** — chứng minh
bằng phản chứng:

- Nếu một lớp **có** tác dụng phụ đáng giữ ⇒ `nonetkill` phá nó ⇒ không dùng được.
- Nếu một lớp **không** có tác dụng phụ nào đáng giữ ⇒ nó đã là ứng viên `noedict` rồi,
  mà `noedict` **rẻ hơn** vì entity vẫn tồn tại.

Xem [07-het-huong.md](07-het-huong.md) mục 3.
