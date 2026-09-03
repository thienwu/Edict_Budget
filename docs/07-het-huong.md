# Hết hướng — vì sao không còn gì để cắt nữa

*Tiếng Việt (bản chính) · [English](07-het-huong.en.md)*

Tài liệu này ghi lại **kết quả âm tính**: những hướng đã tìm, đã đo, và đã bác bỏ.
Nó tồn tại để **không ai phải suy lại chúng lần nữa**.

> **Tuyên bố ngày 23/08/2026:** dự án đã đạt giới hạn của cách tiếp cận này. Bốn cơ chế
> đang chạy (`noedict`, `swap`, `freegate`, `wipeclear` + `mapclear`) đã lấy hết những gì
> lấy được **mà không phải trả giá bằng thứ người chơi nhìn thấy hoặc chạm vào**.

---

## 1. `noedict` đã hết đường mở rộng — vì chỉ có ba cửa

Một entity chỉ có thể bỏ edict nếu **tác dụng của nó sống sót qua việc nó không được gửi
đi**. Điều đó chỉ xảy ra khi nó ghi trạng thái vào một **kho nằm ngoài chính nó**.

Quét toàn bộ `server.dll` tìm mọi lệnh gọi ghi trạng thái ra ngoài entity: **đúng ba cửa**.

| cửa | offset | số chỗ gọi | trạng thái |
|---|---|---|---|
| `LightStyle` | `IVEngineServer + 0xA0` | 6 | ✅ đã dùng — `light`, `light_spot` |
| `StaticDecal` | `IVEngineServer + 0xA4` | 1 | ✅ đã dùng — `infodecal` |
| `EmitAmbientSound` | `IVEngineServer + 0x70` | 3 | ❌ **cấm** — tham số đầu là `entindex` của chính nó |

Cửa thứ ba đóng vì **ĐK3**: entity tự nhét chỉ số của mình vào gói tin. Bỏ edict thì chỉ
số đó vô nghĩa. Đây là lý do `ambient_generic` — **848 cái trên 16 map** — không đụng được.

**Không có cửa thứ tư.** Các lớp còn lại đều thuộc một trong hai loại: hoặc client phải
nhận liên tục (⇒ cần edict), hoặc hiệu ứng đã nướng sẵn vào map (⇒ bỏ đi cũng chẳng được
gì thêm).

### Quét lại toàn bộ không gian lớp

> Bảng phân loại đầy đủ: [08-phanloai-entity.md](08-phanloai-entity.md).

- **557 lớp** của `server.dll`, đối chiếu **16 map** thật của ba chiến dịch.
- **40 lớp** chưa từng phán quyết: **tất cả trượt ĐK1** (có SendTable riêng).
- Sau khi sửa lỗi giải vtable (22/08), **137 lớp** mới trở nên phân giải được — quét lại:
  **không có ứng viên mới nào**.

---

## 2. `swap` — quét hết, đúng một cặp

Đối chiếu **108 lớp nguồn × 549 lớp đích**. Điều kiện: cùng hình dạng với người chơi,
nhưng rẻ hơn về edict.

**Kết quả: đúng một cặp** — `point_spotlight` → `beam_spotlight` (3 edict → 1).

Lý do không có cặp thứ hai: **hầu hết lớp đã ở hệ số 1**. `point_spotlight` là ngoại lệ
vì nó **tự tạo thêm** hai entity con lúc spawn. Không lớp nào khác trong 16 map làm vậy.

`env_sprite` là ví dụ rõ nhất cho việc hết đường: **lớp đông nhất đã đo được**
(2539 cái, riêng `the_hive` 2280) — **không có cặp thay thế nào**, vì nó đã ở hệ số 1 và
xuống 0 là bất khả thi.

---

## 3. `nonetkill` — tập rỗng, chứng minh bằng phản chứng

Ý tưởng: đổi tên lớp trong lump để entity **không bao giờ được tạo**.

Bác bỏ: entity không được tạo thì `Spawn()` và `Activate()` **không chạy**. Mà chính hai
hàm đó mới là chỗ sinh ra tác dụng phụ mà ta muốn giữ. Nếu một lớp **không** có tác dụng
phụ nào đáng giữ thì nó đã là ứng viên `noedict` rồi — và `noedict` **rẻ hơn**, vì entity
vẫn tồn tại.

⇒ Tập "dùng được `nonetkill` mà không dùng được `noedict`" là **tập rỗng**.

Xem [04-nonetkill.md](04-nonetkill.md).

---

## 4. `killent` — hướng lớn nhất, và lý do bác bỏ

Đây là hướng **đo được nhiều nhất** trong toàn dự án: **6200 edict** trên 16 map, riêng
`the_hive_m4` **1227** trong khi biên độ sống/chết của map đó chỉ **122**.

Cơ chế: trả `false` ở `IMapEntityFilter::ShouldCreateEntity` (**vtable slot 0**) của cả
ba bộ lọc — entity **không tồn tại**.

### Vì sao bác bỏ

**Bộ điều kiện chưa bao giờ hỏi entity có VA CHẠM hay không.**

Tài liệu Valve, [`prop_dynamic`](https://developer.valvesoftware.com/wiki/Prop_dynamic):

> **Collisions (solid)**: `0` Not solid · `2` Use bounding box · **`6` Use VPhysics (default)**

Một `prop_dynamic` **không ghi khoá `solid`** vẫn là **vật đặc**.

Đo lại trên **60 file BSP gốc của Valve**: **809 `prop_dynamic` ĐẶC** lọt qua cả năm điều
kiện. Riêng `left4dead2/maps`: **472/602 = 78%**. Chúng là:

| số | model | thực chất |
|---|---|---|
| 30 | `bridge_rail`, `bridge_rail_dlc2` | **lan can cầu** |
| 89 | `cemetery_gate_128/64/32` | **cổng** |
| 39 | `barricade001_128`, `concrete_barrier001_96`, `plywood_01/02` | **rào chắn lối** |
| 12 | `crypts_wall` | **tường** |
| 10 | `fence002` | hàng rào |

Xoá 30 lan can cầu ⇒ người chơi **rơi khỏi cầu**.

### Ba xác nhận từ bên ngoài

1. **Valve nói thẳng: cắt edict và mất va chạm là cùng một việc.** Khoá
   `DisableBoneFollowers`: *"`phys_bone_followers` **can quickly eat up the edict count**...
   **This will however make the collision model no longer function**."*
   ⇒ Đây là tài liệu gốc chứng thực **luật cấm động vào họ `phys`** của dự án.

2. **SourceMod đã GỠ BỎ cơ chế sửa lump ở `LevelInit`**
   ([PR #1534](https://github.com/alliedmodders/sourcemod/pull/1534), asherkin):
   *"some maps have over 16MB of entity data — far larger than our 2MB limit. There is no
   sane way we can currently handle this."*

3. **Cộng đồng 15 năm dùng Stripper:Source chưa bao giờ xoá theo LỚP** — họ xoá theo
   **từng cá thể**, định danh bằng `hammerid`, sau khi vào game chỉ tay vào nó. Và họ luôn
   **sửa nav mesh kèm theo**. Ta bị cấm sửa BSP ⇒ **không bao giờ bù được nav** ⇒ bot kẹt,
   Director tính sai đường.

### Nếu ai đó muốn làm tiếp

Điều kiện tối thiểu: thêm **X7 — từ chối mọi thứ có thể đặc** (chỉ xoá khi `solid=0`,
`spawnflags&128`, `spawnflags&256`, hoặc lớp vốn không bao giờ đặc); định danh bằng
`hammerid` (đo được **67258/67280 = 100%** entity L4D2 có khoá này) chứ **không** bằng số
thứ tự; chứng minh `pMapEntities` và `GetMapEntitiesString()` là cùng một chuỗi; chạy chế
độ chỉ-đếm một vòng chiến dịch đầy đủ trước.

Sau X7 ước còn **~4450 edict** thay vì 6200 — mất `prop_dynamic` và `prop_ragdoll`, giữ
nguyên các lớp trang trí không đặc.

Hồ sơ đầy đủ ở repo phát triển: `tools/killent-nguy-hiem.md`.

---

## 5. Họ `phys` — cấm vĩnh viễn

`phys_bone_follower` ≈ **587 edict** (🟠 tính ra, chưa đo trực tiếp). Đây là lượng chưa thu
hoạch **lớn nhất còn nhìn thấy được** — và nó **không được đụng vào**.

Lý do: bone follower **chính là va chạm**. Không có nó thì model mất vật lý, người chơi
**đi xuyên qua**. Valve xác nhận điều này bằng chính tài liệu của `DisableBoneFollowers`.

Cùng luật đó áp cho `prop_physics`, `prop_physics_multiplayer`: `client.dll` **tự đọc lump**
và tự tạo `C_PhysPropClientside` cho **đúng hai lớp này**. Can thiệp phía máy chủ ⇒ lệch
hai đầu.

---

## 6. Hướng 4096 — đã đóng, và phải giữ đóng

Nâng trần edict lên 4096 **làm được** (mã có sẵn, mặc định tắt) nhưng **không giải quyết
bài toán**: chỉ số entity mã hoá trong gói tin bằng **trường 11 bit** (tối đa 2047). Đó là
**định dạng gói tin**, nằm ở cả hai đầu dây.

Đo thực tế còn cho thấy nó **có hại**: `num_edicts` leo lên 2060, entity **ngẫu nhiên**
tràn lên trên 2047, và nhóm công tắc đó **phá luôn vòng hồi sinh** (`wipeclear`).

**Một câu để tự kiểm:** *"Nó có cần `bigarray` không?"* — Có ⇒ **dừng**.

Xem [03-huong-4096.md](03-huong-4096.md).

---

## 7. Tích tụ entity lúc chơi — đo được là KHÔNG có

Câu hỏi mở lớn nhất một thời: *"có phải entity cứ tích dần lên trong lúc chơi không?"*

🟢 **Đã đo, câu trả lời là KHÔNG:**

- Nhật ký máy chủ thật qua **7 phiên chơi dài**: cả 7 đều **kết thúc thấp hơn lúc bắt đầu**
  (trung bình **−114**).
- Đỉnh entity sống: **1375/2048**.
- **0 lần `ED_ALLOC`** trong **105 phiên**.

⇒ Hạng mục "dọn lúc chơi" (CEF) **không cần mở lại**. `freegate` đã lo phần việc đó.

---

## 8. Còn lại gì

| hạng mục | lượng | vì sao chưa lấy |
|---|---|---|
| `phys_bone_follower` | ~587 | **cấm vĩnh viễn** — nó chính là va chạm |
| `ambient_generic` | 848 | vi phạm ĐK3 (tự nhét `entindex` vào gói tin) |
| `env_sprite` | 2539 | không có cơ chế nào — đã thử ba đường, đều tắc |
| `prop_dynamic` | 1646 | `killent` + X7 từ chối vì phần lớn là vật đặc |
| `killent` sau X7 | ~4450 | **chưa cài một dòng nào**; cần bốn điều kiện ở mục 4 |

Cả năm dòng đều **có giá**. Không còn thứ gì **miễn phí**.

---

## 9. ⚠️ Điều chưa biết — đọc trước khi tin vào ba lớp mới

Ba lớp thêm ngày 21–22/08 (`func_areaportal`, `info_zombie_spawn`, `func_nav_blocker`)
**chưa được nghiệm thu dài hạn**.

Cái đã biết:
- 🟢 Chúng **chạy được** và **không mất gì nhìn thấy được** trong các phiên đã đo.
- 🟢 `func_areaportal`: chạy thật từ 21/08, qua 2 map, sống qua 2 lần wipe, 0 dòng `ED_ALLOC`.
- 🟢 `info_zombie_spawn`: chỉ thực sự được bật từ bản DLL 22/08 23:29 (trước đó là **no-op
  âm thầm** suốt 6 phiên do lỗi giải vtable).
- 🟢 Đo có kiểm soát cùng một map: `num_edicts` **1116 → 1106** = **−10 edict**, quy được
  hết về nguyên nhân.

Cái **chưa** biết:
- ❌ **Chưa kiểm trên nhiều chiến dịch có độ phức tạp khác nhau.** Số liệu tập trung ở
  `the_hive` và `pripyat`.
- ❌ **Chưa có dữ liệu vận hành dài ngày** — mới vài ngày, không phải vài tuần.
- ❌ `func_nav_blocker` **đang tắt**, chưa chạy lần nào. Triệu chứng của nó **không nhìn
  thấy được** — phải quan sát **hành vi AI** (zombie/bot có đi vào vùng lẽ ra bị chặn không).
- ❌ `info_zombie_spawn`: **1/86** có `parentname` (`the_hive_m4`, `alexi_5000_hunter_spawner`
  gắn vào `alexi_5000_body`). Không đổi phán quyết ĐK6, nhưng đây là **chỗ duy nhất chưa
  chắc chắn**.
- ❌ `func_nav_blocker` ĐK6 phụ thuộc **không map nào gắn cha**. Đúng với 17 map đã đo.
  **Map mới có thể phá điều đó** — có `EF_NODRAW` mà **có cha** thì entity **sẽ** được gửi.

> **Khuyến nghị:** bật từng lớp một, mỗi lần một biến, và đọc `edictbudget.log` để xác
> nhận số vtable đã vá khớp số lớp đã bật.

---

## 10. Việc an toàn còn mở (không phải để lấy thêm edict)

1. **Cổng chặn vtable dùng chung.** Có **20 nhóm** classname chia nhau một vtable. Chưa có
   cổng nào chặn việc bật một tên rồi vô tình kéo theo cả nhóm.
2. **8 lớp giải sai vtable** — liệt kê ở [06-dia-chi.md](06-dia-chi.md) mục 1. Không được
   thêm vào `noedict.txt` cho tới khi có cổng ở mục 1.
3. **`freegate` chưa nghiệm thu dài hạn** với ≥ 4 người chơi. Gói xuất bản để `freegate=1`
   (đã qua phép đo đối chứng); máy chủ đông người thấy tickrate lạ thì **đặt về 0 trước tiên**.
