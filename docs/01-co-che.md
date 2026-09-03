# Bốn cơ chế đang chạy

*Tiếng Việt (bản chính) · [English](01-co-che.en.md)*

Địa chỉ đầy đủ cho từng cơ chế: [06-dia-chi.md](06-dia-chi.md).

> ⚠️ **Lưu ý về địa chỉ:** mọi RVA ở đây tính theo ImageBase `0x10000000`. Lúc chạy module
> nạp ở base khác, nên mọi phép so sánh **PHẢI** làm với `base + RVA`, **tuyệt đối không**
> so thẳng giá trị tĩnh. Đây là lỗi đã mắc thật — xem mục 3 bên dưới.

---

## 1. Hai công tắc cũ xử lý thực thể không dùng mạng

**Vấn đề:** entity không cần gửi cho client **vẫn** chiếm edict trong dải 0–2047 — dải mà
giao thức 11 bit dành cho thứ **phải gửi**. Đó là lãng phí thuần.

Không gỡ được edict của entity **đang sống** (`DetachEdict()` là `private`, chỉ destructor
gọi được). Nên chỉ còn hai đường:

| công tắc | làm gì | được | mất |
|---|---|---|---|
| `nonetkill = 1` | **XOÁ HẲN** sau khi map nạp xong. Duyệt `gEntList`, `UTIL_Remove` lớp khớp, rồi `CleanupDeleteList()` để trả edict ngay | trả slot về dải 0–2047 **vĩnh viễn** | **mất luôn chức năng** của entity đó |
| `nonethigh = 1` | **ĐẨY** lên dải 2048–4095 thay vì xoá | không mất chức năng | 🛑 **CẦN `bigarray=1` VÀ `snapshot=1`** — tức thuộc **hướng 4096 đã cấm** |

> 🛑 `nonethigh` **từng gây crash** trong phép A/B sạch nhất của dự án. Bật lại là **cố ý
> chấp nhận rủi ro đó** để đo lại.
>
> ⚠️ **KHÔNG bật cả hai cùng lúc** — chúng mâu thuẫn. Nếu bật cả hai, `nonetkill` thắng và
> `nonethigh` bị bỏ qua (có cảnh báo trong log).

Cả hai đều đọc danh sách lớp từ file (quy tắc khớp: dòng kết thúc `_` = khớp **tiền tố**,
còn lại = khớp **chính xác**).

**Cả hai đều đã bị thay thế bởi `noedict`** — xem [04-nonetkill.md](04-nonetkill.md) và
[07-het-huong.md](07-het-huong.md).

---

## 2. `noedict` — khiến một số lớp KHÔNG BAO GIỜ được cấp edict

> 🛑 **ĐÂY KHÔNG PHẢI HƯỚNG 4096.** Không dùng `bigarray`/`snapshot`/`pinmax`/`pinglobals`/
> `markfree`. Không vá một byte nào của `engine.dll`.
>
> Nếu ai đó sửa hàm này mà thấy mình **cần bật một trong năm công tắc đó** ⇒ **ĐÃ ĐI SAI
> ĐƯỜNG, dừng lại.**

### Cơ chế (đã xác minh trên binary)

`CBaseEntity::PostConstructor` @ `0x10055620` (RVA `0x55620`) là nơi **quyết định**:

```asm
mov  eax, [esi+0x138]      ; m_iEFlags
shr  edx, 9
test dl, 1                 ; bit 9 = EFL_SERVER_ONLY
je   <nhánh CẤP EDICT>     ; = 0 -> AddNetworkableEntity, dải 0-2047
mov  ecx, gEntList
call AddNonNetworkableEntity   ; = 1 -> dải 2049-4095, KHÔNG EDICT
```

Ta chỉ cần **bật bit 9 TRƯỚC** khi hàm gốc chạy.

### Điểm móc

`PostConstructor` là **hàm ảo**, ở vtable **slot 29** (`+0x74`). Mỗi lớp có vtable **riêng**,
nên thay slot 29 của riêng vtable lớp mục tiêu ⇒ **chỉ tác động đúng lớp đó**. Không detour
byte, không đụng tới lớp khác.

Factory `Create` của mọi lớp có dạng:

```asm
push <sizeof>                    ; operator new
call operator new
push 0                           ; <- bServerOnly = FALSE (chỗ Valve nói tới)
call <ctor>
mov  dword ptr [esi], <VTABLE>   ; <- ta tìm con số này
...
call [vtable+0x74]               ; PostConstructor
```

### 🛑 Cấm đưa vào danh sách

- lớp **SOLID** hoặc **CÓ DI CHUYỂN**: `IVEngineServer::SolidMoved` / `TriggerMoved` đều
  nhận `edict_t*`. Không edict ⇒ **không cập nhật phân vùng không gian**.
- **mọi lớp `trigger_*`** (cùng lý do)
- lớp có **ServerClass RIÊNG** (có `DT_` riêng) — client cần dùng lại chúng.

**An toàn đã kiểm cho:** `infodecal` (`StaticDecal` dùng chỉ số **BỀ MẶT**, không phải chỉ số
của chính nó) và họ `light` (`LightStyle` **không kèm chỉ số entity nào**).

---

## 3. `noedict`: cổng an toàn 3 — điều kiện 1 (SendTable riêng)

Đây là điều kiện lọc **mạnh nhất** trong 6 điều kiện, và là điều kiện **DUY NHẤT máy kiểm
được thay người**. Trước đây nó nằm trong ghi chú của `noedict.txt`, ai không đọc thì thêm
bừa vào và hỏng khi chạy.

`GetServerClass()` = **vtable slot 9**. Thân hàm là `mov eax, imm32 ; ret`:

```
B8 <imm32> C3

imm32 == 0x107D78A8  ->  ServerClass CBaseEntity / DT_BaseEntity  ->  CHO PHÉP
imm32 != 0x107D78A8  ->  lớp có SendTable riêng                   ->  TỪ CHỐI
```

Đã quét **24 lớp** bằng cách này, hiệu chuẩn với **8 giá trị biết chắc**, khớp **100%**.

Bốn lớp gốc (`infodecal` / `light` / `light_spot` / `path_track`) đều trả `0x107D78A8`.

**Ví dụ bị từ chối:** `spotlight_end` (`CSpotlightEnd`), `beam` (`CBeam`), `env_sprite`
(`CSprite`), `light_dynamic` (`DT_DynamicLight`), cả họ `trigger_*` (`CBaseTrigger`).

### ⚠️ Bài học: `imm32` là địa chỉ trong ẢNH TĨNH

Lúc chạy module nạp ở base khác và **con trỏ đọc từ vtable ĐÃ ĐƯỢC ĐỔI THEO BASE**, nên phép
so **PHẢI** là với `base + DT_BASEENTITY_RVA`, **tuyệt đối không** so thẳng giá trị tĩnh.

> So thẳng đã làm **cả bốn lớp đang chạy bị TỪ CHỐI âm thầm** và `noedict` **tắt hoàn toàn** —
> log ghi *"đã sửa 0 vtable / 4 lớp yêu cầu"*, trong khi cả bốn đều trả đúng
> `0x540378A8 = 0x53860000 (base thật) + 0x7D78A8`.
>
> Cùng loại lỗi với vụ chữ ký `mapclear` bị từ chối vì prologue chưa có mặt nạ
> (xem [02-mapclear.md](02-mapclear.md) mục 5).

---

## 4. `freegate` — bỏ thời gian chờ 1 giây

Yêu cầu engine **bỏ** thời gian chờ 1 giây trước khi một edict vừa giải phóng được cấp phát
lại.

`ED_Alloc` từ chối tái sử dụng một edict cho tới **1 giây** sau khi nó được giải phóng. Mà
một lần wipe giải phóng **hàng trăm** entity rồi tạo lại chúng trong **CÙNG MỘT FRAME**, nên
không cái nào đủ điều kiện, và engine buộc phải nối thêm edict mới — đó chính là thứ làm cạn
kiệt một map đang ở mức 2012/2047.

`IVEngineServer::AllowImmediateEdictReuse()` là **câu trả lời của chính Valve** cho việc này:

> *"Tells the engine we can immediately re-use all edict indices even though we may not have
> waited enough time"* — `eiface.h:345`

Convar đi kèm `sv_useexplicitdelete` — **mặc định BẬT** — làm engine báo cho client biết
entity cũ đã biến mất **TRƯỚC** khi chỉ số của nó được tái dùng, và đó đúng là thứ mà thời
gian chờ kia đang bảo vệ.

> 📌 Hướng này đánh **đúng vào cơ chế hỏng thật sự**. Phân tách chỉ bao giờ cũng **thêm biên
> độ**; còn cái này **xoá bỏ NHU CẦU phải có biên độ**.

### Chi tiết vòng lặp `ED_Alloc`

`ED_Alloc` chỉ nhận một edict đã giải phóng khi:

```asm
comiss  2.0f, freetime[i]      ; freetime < 2.0 (giải phóng đầu map)
ja      lấy_nó
fsub    freetime[i]            ; curtime - freetime
fcompi  1.0
jae     lấy_nó                 ; hoặc đã qua 1 GIÂY   <-- vá ở đây
```

🟢 **Đã đo thực tế:** `num_edicts=2012` với **906–918 edict ĐANG TRỐNG** mà engine vẫn báo
*"ED_Alloc: no free edicts"*. Đây đúng là lỗi engine Source 2009 mà tác giả CEF mô tả:
*"running out of edicts when you have 1000 free"*.

Đổi **một byte** `73` → `EB` (`jae` → `jmp`) làm mọi edict trống đều dùng lại được ngay.
Đích nhảy giữ nguyên, không đổi độ dài lệnh, không trampoline.

**Định vị bằng quét chữ ký**, không dùng RVA cứng. RVA đối chiếu: `engine.dll` `0x1E022A`.

### 🛑 Chế độ vá byte vô điều kiện LÀM HỎNG việc chuyển vật phẩm (30/08/2026)

Lý lẽ “an toàn nhờ `sv_useexplicitdelete`” **đúng ở mức snapshot, sai ở mức frame**. Hai sự
kiện trong cùng một frame **không có snapshot nào ở giữa** để lệnh xoá tường minh đi qua.

Engine L4D2 **chỉ** cho chuyển tay `weapon_pain_pills` và `weapon_adrenaline`. Các plugin như
**Gear Transfer** mở rộng ra bảy loại khác bằng cách **huỷ rồi tạo lại** vật phẩm:

```sourcepawn
RemoveEdict(item);                      // -> ED_Free, freetime = GetTime()
item = CreateAndEquip(target, type);    // -> ED_Alloc, CÙNG MỘT FRAME
```

Vá byte vô điều kiện trả lại **đúng chỉ số vừa giải phóng** ⇒ client không bao giờ thấy ranh
giới xoá/tạo ⇒ **ghost weapon**. Changelog của chính plugin đó đã ghi hai triệu chứng này:
v2.16 *“ghost weapon attached between players legs”*, v2.19 *“‘Invalid edict’ error when creating
items to give”*.

### Ba chế độ

| `freegate` | làm gì |
|---|---|
| `0` | tắt — giữ nguyên cổng 1 giây của engine |
| **`1`** | **có danh sách (mặc định).** Móc `IVEngineServer::RemoveEdict` (**vtable slot 23**). Sau khi hàm gốc chạy: lớp **không** trong `freekeep.txt` ⇒ đặt `freetime[i] = 0.0` ⇒ `ED_Alloc` **nhánh thứ nhất** lấy ngay. Lớp **có** trong danh sách ⇒ để nguyên `GetTime()` ⇒ giữ cách ly 1 giây |
| `2` | vá byte vô điều kiện (chế độ cũ, giữ để đối chứng) |

`ED_Free` có **đúng một lối vào** (1 `call rel32`, 0 tham chiếu dữ liệu) nên một móc ở slot 23
**phủ 100%** mọi lần giải phóng edict.

> 🔑 **Không làm hẹp biên độ lúc wipe:** `wipeclear` gọi `AllowImmediateEdictReuse()`
> (**vtable slot 95**) ngay sau `CleanupDeleteList()`. Hàm đó đặt `freetime = 0.0` cho **mọi**
> edict đang trống — kể cả những cái vừa bị giữ cách ly. Danh sách chỉ có tác dụng **lúc chơi
> thường**.

Hồ sơ đầy đủ: `tools/freegate-hong-gear-transfer.md` (kho phát triển).

> ⚠️ **`freegate` là cơ chế ÍT ĐƯỢC NGHIỆM THU NHẤT trong bốn cơ chế — CHƯA được kiểm
> tra và nghiệm thu đầy đủ.** Nó chưa bao giờ chạy dài ngày trên máy chủ đông người, và phép đo
> A/B từng dùng để biện minh cho nó có trước khi `wipeclear` có hình dạng như hiện nay.
> Ngược lại, lỗi chuyển vật phẩm của chế độ `2` thì **đã xác minh được** — lần được từ đầu
> đến cuối qua `RemoveEdict` → `ED_Free` → `ED_Alloc` bằng dịch ngược.
>
> Xem [06-dia-chi.md](06-dia-chi.md) mục 3.

---

## 5. `wipeclear` — dọn thực thể ở đầu `RestartRound`

Dọn thực thể ở đầu `CTerrorGameRules::RestartRound` (**vtable slot 178**), **trước** vòng
hồi sinh player.

### Ba trạng thái — không phải hai

Để mỗi bước thử chỉ đổi **MỘT** thứ:

| giá trị | nghĩa |
|---|---|
| `0` | **TẮT HOÀN TOÀN.** Không móc vtable, không nghe sự kiện. No-op thật sự, dùng làm **mốc đối chiếu** |
| `1` | **CHỈ QUAN SÁT.** Móc vtable + nghe sự kiện, log đầy đủ mốc thời gian và số slot, nhưng **KHÔNG xoá một entity nào**. Rủi ro gần bằng không |
| `2` | **DỌN THẬT.** Làm nửa "dọn" của `CleanUpMap` ngay đầu `RestartRound` |

Mặc định `0`. Đổi trong `patches.txt`, **không cần build lại**.

### Cơ chế đầy đủ

`CTerrorGameRules::CleanUpMap()` (RVA `0x2DDB10`) **đã tự làm đúng việc này**:

```
UTIL_Remove(mọi thứ ngoài preserve list)
  -> CleanupDeleteList() -> AllowImmediateEdictReuse()
  -> MapEntity_ParseAllEntities()
```

**Vấn đề là nó chạy MUỘN.** Trình tự thật (đã kiểm bằng capstone trên `server.dll` của chính
server này, 9.130.288 byte, ImageBase `0x10000000`):

```
CDirector::Restart          0x2700D0
  m_bRestarting = 1         0x27045F
  RestartRound()            0x2704C4   <- vtable slot 178
    VÒNG HỒI SINH PLAYER    0x2E0794..0x2E08A3   <== tiêu edict Ở ĐÂY
    FIRE round_start_pre_entity        0x2E08CE
    CleanUpMap()            0x2E08DF   <== game mới dọn Ở ĐÂY
  m_bRestarting = 0         0x2705DF
```

Mọi thứ trước `0x2E08DF` chạy khi map **vẫn giữ đủ 2012 entity / 35 slot trống**.

Khối này làm nửa "dọn" của `CleanUpMap` ngay đầu `RestartRound` rồi để game chạy tiếp bình
thường — `CleanUpMap` sẽ thấy gần như không còn gì để xoá, và `MapEntity_ParseAllEntities`
vẫn dựng lại đầy đủ từ entity lump.

> 📌 **QUAN TRỌNG — đây vừa là BẢN VÁ vừa là PHÉP ĐO.**
>
> Log *"slot trống trước → sau"* trả lời luôn câu hỏi còn treo:
>
> | kết quả | kết luận |
> |---|---|
> | +~1100 slot và **hết crash** | lỗ nằm **TRƯỚC** `CleanUpMap`, bản vá **đúng** |
> | +~1100 slot mà **vẫn crash** | lỗ nằm **SAU** khi dựng lại xong; bài toán trở về "map thật sự cần 2012/2047, không có lãng phí để thu hồi" |

Giữ nguyên tập **"preserve" CỦA CHÍNH GAME** (đọc runtime từ RVA `0x7ACE40`) nên ngữ nghĩa
y hệt `CleanUpMap` — **chỉ khác THỜI ĐIỂM**. Đó là lựa chọn có ý: **đổi một biến duy nhất**.

### Nghe sự kiện — chỉ để chẩn đoán

Ban đầu định dùng `mission_lost` làm cổng chặn. **ĐÃ BỎ** (phương án A). Lý do, xác minh
trên binary chứ không phải suy đoán:

`mission_lost` bắn **DUY NHẤT** tại `0x10269096`, trong hàm `0x10268CA0`. Hàm đó chỉ push
bốn chuỗi: `trigger_finale`, `finale_trigger`, `FinaleLost`, `mission_lost` ⇒ đây là **đường
THUA FINALE**. 11 vị trí push `mission_lost` còn lại đều là `AddListener(+0x0C)` hoặc so
chuỗi. `c6m1_riverbank` không phải finale ⇒ cổng sẽ không bao giờ mở.

Vẫn giữ listener vì nó trả lời một câu còn treo: thực tế `mission_lost` có bắn không, và bắn
trước hay sau `RestartRound`.

**Cờ MỘT-LẦN, KHÔNG dùng cửa sổ thời gian.**

Ban đầu dùng cửa sổ 5,0 s. **SAI:** đo thật tế cho thấy `mission_lost` bắn lúc `t=63.47` còn
`RestartRound` chạy lúc `t=70.50` — cách **7,03 s**, **vượt cửa sổ 5 s** ⇒ cổng sẽ trượt luôn
cả wipe thật.

Khoảng cách này do Director quyết định (màn hình thua, đếm ngược...), **không có giá trị nào
an toàn để đoán**. Dùng cờ một-lần thì không phải đoán:

```
mission_lost  -> bật cờ
RestartRound  -> có cờ thì dọn, rồi TẮT cờ ngay
nạp map mới   -> tắt cờ (tránh cờ cũ sót lại)
```

### ⚠️ Cổng chặn — KHÔI PHỤC 07/08 sau khi đo thật tế

Đã từng **bỏ** cổng này, dựa trên suy luận từ disassembly rằng `mission_lost` *"chỉ bắn khi
thua finale"* (hàm `0x10268CA0` có push `trigger_finale` / `FinaleLost`).

**SUY LUẬN ĐÓ SAI** — đo thật tế trên `c6m1_riverbank` (**KHÔNG** phải finale) cho thấy
`mission_lost` **VẪN BẮN**, lúc `t=63.47`.

> Lại dính cái bẫy: **suy từ chuỗi nằm gần nhau**.

**Hậu quả khi không có cổng** (log 07/08, `wipeclear=2`): `RestartRound` được gọi ngay lúc
`t=1.00` **KHI MAP VỪA NẠP** (vòng chơi đầu tiên, không phải wipe). Bản vá đã xoá **1155
entity** của map ngay tại đó, và slot trống **sau** `RestartRound` vẫn ở 1462 (nền là 474)
⇒ map **KHÔNG được dựng lại**. **Phá map.**

⇒ **Chỉ dọn khi CÓ `mission_lost` đang chờ.** Cờ một-lần, không cửa sổ giờ.

### 🛑 Cảnh báo: plugin SourceMod và `mission_lost`

`wipeclear` huỷ entity **sớm hơn** bình thường — `CleanupDeleteList()` chạy ngay trong thân
hook, trước `RestartRound` gốc. Preserve list của game chỉ có **38 lớp**, nên phần lớn entity
của map bị xoá ở bước này.

Plugin nào còn giữ tham chiếu tới một entity vừa bị xoá sẽ cầm tham chiếu **treo** ⇒ server
có thể **sập**. Nguy hiểm nhất là những tham chiếu **không kiểm serial** — con trỏ thô, hoặc
thứ do chính engine giữ.

**Cách chữa nằm ở phía plugin:** dọn tham chiếu của chính mình trên **`mission_lost`** — nó
bắn **trước** `RestartRound` vài frame, đó là cửa sổ để dọn. Dọn sau là muộn.

**Vì sao không dời việc dọn ra sau `RestartRound`:** `RestartRound` tạo entity mới trong khi
entity cũ vẫn sống — đỉnh edict là *cũ + mới*. `wipeclear` tồn tại để giải phóng chỗ **trước**
đỉnh đó. Dời đi là quay lại đúng lỗi `ED_Alloc` mà nó sinh ra để chữa.

Không sửa được plugin thì đặt `wipeclear=1` (chỉ quan sát) hoặc `0`.

### `wipekeep.txt` — danh sách giữ bổ sung

Preserve list của game (`0x7ACE40`) là thứ **game dùng**. Nhưng có những lớp game sẵn sàng
xoá mà **xoá sớm** lại sinh lỗi phía client. Ca đầu tiên gặp: giữ lại thực thể của người
chơi gây **lỗi MẤT BÓNG**.

Nên cần một danh sách **GIỮ THÊM**, sửa được bằng file, không phải build lại:

```
dòng kết thúc bằng '_'  ->  khớp TIỀN TỐ cả họ   (ví dụ "weapon_")
còn lại                 ->  khớp CHÍNH XÁC tên lớp
```

Đặt ở `left4dead2/addons/edictbudget/wipekeep.txt`. Thiếu file = không giữ thêm gì.

> ⚠️ **Để TRỐNG mới đúng.** Ở wipe, entity bị xoá sẽ **được dựng lại** từ entity lump, nên
> giữ thêm chỉ làm **hẹp biên độ**. Ngược hẳn với `mapkeep.txt` — xem
> [02-mapclear.md](02-mapclear.md).

---

## 6. `swap` — đổi một lớp entity thành lớp RẺ HƠN

### Bài toán

`point_spotlight` khi spawn **TỰ TẠO THÊM** `spotlight_end` + `beam` ⇒ **3 edict** cho mỗi
dòng trong lump BSP.

`beam_spotlight` làm việc tương tự nhưng **VẼ HOÀN TOÀN PHÍA CLIENT**, không sinh entity con
⇒ **1 edict**.

> Chính tác giả `the_hive` đã dùng **cả hai lớp** trong cùng chiến dịch (`m1` có 2
> `beam_spotlight`, `m5` có 21, `m4` có 312 `point_spotlight`).

🟢 Đổi được: `m4` 937 → 313, `m3` 240 → 80, `m5` 41 → 33. **Tổng 792 edict.**

> **KHÁC HẲN `noedict`:** đây **KHÔNG** phải gỡ mạng. Client vẫn nhận entity, vẫn vẽ tia
> sáng. Chỉ là một lớp rẻ hơn. Nên **không dính 6 điều kiện nào hết**.

### Chỗ móc — vì sao chỗ này sạch

`CreateEntityByName` (`0x101196B0`) không tự tạo gì, nó gọi qua
`EntityFactoryDictionary()->vtable[1]`:

```asm
101196E7  call 0x1020CA70 ; mov eax,[edx+4] ; call eax
```

Quét cả `.text`: **562 chỗ** gọi `0x1020CA70`, trong đó:

| slot | số chỗ | hàm |
|---|---|---|
| 0 | 558 | `InstallFactory` |
| 4 | 1 | `GetCannonicalName` |
| **1** | **3** | **`Create`** — `CreateEntityByName` + 2 nhánh của bộ phân tích lump BSP |

> ⇒ Vá **MỘT** con trỏ vtable phủ **cả lúc nạp lump lẫn lúc chơi**.

`0x1020CA70` là địa chỉ plugin **đã dùng sẵn** trong `ResolveClassVtable`. Dùng
SourceHook-style vtable swap, **KHÔNG detour byte**.

### Ánh xạ keyvalue (đọc datamap của cả hai lớp)

| khoá | kết quả |
|---|---|
| `SpotlightLength` / `SpotlightWidth` / `HDRColorScale` | **TRÙNG TÊN TUYỆT ĐỐI** |
| input `LightOn` / `LightOff`, output `OnLightOn` | trùng tên |
| cùng `baseMap = CBaseEntity` | mọi khoá kế thừa giống hệt |
| **`HaloScale`** | ❌ **MẤT** — `client.dll` ghi cứng `halo = 60.0` tại `0x1006CC80` |

⇒ map nào đặt `HaloScale 10` (ví dụ `the_hive_m4`) sẽ thấy **halo TO GẤP 6**.

43/517 `point_spotlight` trên 50 map vốn đã đặt 60 = đúng mặc định, không đổi gì.

### `spawnflags`

Bit 0 (bật sẵn) và bit 1 (không đèn động) **GIỐNG HỆT** giữa hai lớp.

Quét **517** `point_spotlight` trên 45 map gốc + 5 map hive: chỉ từng là **2 hoặc 3**, chưa
cái nào đặt bit 2/3/6 ⇒ **rủi ro bật nhầm xoay/nofog = 0**.

### `m_iClassname`

Từ lump sẽ ghi đè lại thành `"point_spotlight"`. Đã chứng minh `server.dll` **không có chỗ
nào tra cứu chuỗi đó** ngoài `InstallFactory` ⇒ **vô hại**, và giữ **tương thích ngược** cho
plugin SourceMod đang lọc theo tên lớp.

### Chưa làm (cố ý, để test dần)

Tự so datamap của hai lớp lúc khởi động để báo khoá nào bị mất.

`GetDataDescMap()` = vtable **slot 11** (`+0x2C`), cùng khuôn `B8 imm32 C3` như slot 9.
`datamap_t` 24 byte `{dataDesc, nFields, className, baseMap}`; `typedescription_t` 60 byte,
tên keyvalue ở `+0x10`.

Chưa viết vì đây là **đọc con trỏ chưa kiểm chứng** trên bản này — thêm sau, khi cơ chế đổi
lớp đã chạy ổn.

### Đặt lại bộ đếm tại `LevelInit`

Bộ đếm `swap` về 0 tại **`LevelInit`**, không phải ở `SwapReport()` (`ServerActivate`).

**15/08:** log cho thấy có lần báo *"gặp 392"* = 312 (`m4`) + 80 (`m3`), tức **MỘT báo cáo
gộp HAI lần nạp map** — `ServerActivate` không chạy đúng nhịp với mỗi lần nạp. Cũng thế với
*"gặp 82"* (80 + 2).

Chỉ **sai con số trong log**, không sai việc đổi lớp (`đổi` luôn bằng `gặp`), nhưng đọc log
để suy ra map nào thì bị nhầm.

`LevelInit` chạy **đúng một lần** cho mỗi map và **TRƯỚC** khi lump được phân tích, nên đặt
lại ở đây mới khớp.
