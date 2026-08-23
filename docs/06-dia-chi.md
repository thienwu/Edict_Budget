# Địa chỉ dịch ngược đã xác minh — bảng tra cho từng tính năng

*Tiếng Việt (bản chính) · [English](06-dia-chi.en.md)*

Tài liệu này gom **mọi địa chỉ mà plugin thực sự dùng**, kèm **cách đã xác minh** và
**cách suy lại nếu Valve cập nhật game**. Mục đích: người khác dùng lại được, và kiểm
lại được, mà không phải đọc hết 2600 dòng mã nguồn.

> **Luật của dự án:** cái gì không xác minh được thì ghi thẳng là **KHÔNG XÁC ĐỊNH**,
> không đoán. Ba mức chứng cứ dùng xuyên suốt:
> **🟡 ĐỌC ĐƯỢC** (tài liệu/SDK — mới là giả thuyết) ·
> **🟠 XÁC MINH BINARY** (đã dịch ngược, có địa chỉ) ·
> **🟢 ĐO ĐƯỢC** (đã chạy trên máy chủ thật).

---

## 0. Nhị phân tham chiếu

**Phiên bản game đã xác minh: `2.2.4.3` build `10097`** (Left 4 Dead 2 Dedicated Server).

Mọi RVA dưới đây tính theo **ImageBase `0x10000000`**. Cộng thêm base thật lúc chạy.

| file | kích thước | md5 |
|---|---|---|
| `server.dll` | 9.130.288 | `533888fbb4e5ed534b172470613a3017` |
| `engine.dll` | 4.817.712 | `a16cd381409bab749909d5000c2302d8` |
| `client.dll` | 8.305.664 | `21565d29a23caeabe3d0ffc6156c3e5c` |

Lấy từ thư mục **máy chủ chuyên dụng**:
```
<Steam>/Left 4 Dead 2 Dedicated Server/left4dead2/bin/server.dll
<Steam>/Left 4 Dead 2 Dedicated Server/bin/engine.dll
<Steam>/Left 4 Dead 2 Dedicated Server/left4dead2/bin/client.dll
```

🔑 `client.dll` của máy chủ chuyên dụng **trùng khít** với `client.dll` của bản game
thật — đọc bản nào cũng ra cùng kết quả.

> **Nếu md5 của bạn khác:** đừng dùng bảng RVA. Đọc mục 6 — suy lại bằng neo nội dung.

---

## 1. `noedict` — không cấp edict cho lớp không dùng mạng

Cơ chế: bật **bit 9 của `m_iEFlags`** (`EFL_SERVER_ONLY`) **trước khi**
`CBaseEntity::PostConstructor` chạy. Engine thấy cờ đó thì gọi
`AddNonNetworkableEntity` (dải 2049–4095) thay vì `AddNetworkableEntity` (dải 0–2047).

| thứ | giá trị | loại | mức |
|---|---|---|---|
| `m_iEFlags` | `[this + 0x138]` | offset trường | 🟠 |
| `EFL_SERVER_ONLY` | `1 << 9` | hằng số | 🟡 SDK |
| `CBaseEntity::PostConstructor` | **vtable slot 29** · RVA `0x055620` | hàm ảo | 🟠 (bình chọn đa số trên **539 lớp**) |
| `CBaseEntity::GetServerClass` | **vtable slot 9** | hàm ảo | 🟠 |
| `DT_BaseEntity` (ServerClass) | RVA `0x7D78A8` | dữ liệu | 🟠 (**229 lớp** trả về nó) |
| `CEntityFactoryDictionary::FindFactory` | **vtable slot 3** | hàm ảo | 🟠 |
| `IEntityFactory::Create` | **vtable slot 0** | hàm ảo | 🟠 |
| `UpdateTransmitState` | **vtable slot 21** | hàm ảo | 🟠 |
| `ObjectCaps` | **vtable slot 40** · bit `0x2` = `FCAP_ACROSS_TRANSITION` | hàm ảo | 🟠 |

**Cách plugin tìm vtable của một lớp** (`ResolveClassVtable`, `sample_mm.cpp:1585`):
`GetEntityFactoryDictionary` (RVA `0x20CA70`) → `FindFactory(classname)` → `Create` →
quét thân hàm tìm lệnh ghi con trỏ vtable. Nếu `Create` chỉ là vỏ bọc gọi hàm khác thì
đi theo `call rel32` rồi quét tiếp.

> ⚠️ **Vá theo VTABLE, không theo CLASSNAME.** Có **20 nhóm** classname dùng chung một
> vtable. Bật một tên có thể kéo theo cả nhóm. Ví dụ đã biết:
> `info_teleport_destination` dùng chung vtable với `info_player_start` và `info_landmark`.
>
> ⚠️ **8 lớp giải sai vtable** ở bản hiện tại (trả về vtable của lớp khác):
> `point_commentary_viewpoint`, `env_soundscape_proxy`, `env_soundscape_triggerable`,
> `prop_vehicle_driveable`, `player`, `weapon_first_aid_kit`, `weapon_defibrillator`,
> `env_fire_trail`. **Không được thêm chúng vào `noedict.txt`.**

### Địa chỉ của từng lớp đã bật

| lớp | chứng cứ then chốt | mức |
|---|---|---|
| `infodecal` | `StaticDecal()` = `IVEngineServer + 0xA4`, tham số là chỉ số **bề mặt** bị dán, không phải chỉ số của nó | 🟢 |
| `light`, `light_spot` | `LightStyle(style, pattern)` = `IVEngineServer + 0xA0`, **không kèm chỉ số entity nào** (6 chỗ gọi) | 🟢 |
| `path_track` | `SetSolid(SOLID_NONE)`, không model, không ai trỏ tới qua SendProp | 🟢 |
| `func_areaportal` | `UpdateTransmitState` slot 21 = RVA `0x0DA8F0`, **3 lệnh**: `push 0x10` (`FL_EDICT_DONTSEND`) ; `call SetTransmitState` ; `ret` → **vô điều kiện**. `Spawn` `0x0DA8A0` **không** gọi `SetSolid`. Engine gọi `SetAreaPortalState` (vtable `+0xF4`) với `m_portalNumber [+0x42C]` và `m_state [+0x438]` = **hai số nguyên** | 🟢 |
| `info_zombie_spawn` | 0/86 có `model` ⇒ `GetModelIndex()` trả 0 ⇒ `je 0x056A84` = DONTSEND. Director tìm bằng `FindEntityByClassname` RVA `0x0B47F0` — hàm này duyệt `CEntInfo` (`[esi+0x10004]`), so `[esi+0x74]` = `m_iClassname`, **không đọc `[this+0x28]` lần nào** ⇒ entity không edict **vẫn tìm thấy được** | 🟢 |
| `func_nav_blocker` | **ĐANG TẮT.** `Spawn` `0x48C80E`: `push 0x20` (`EF_NODRAW`) ; `call AddEffects`. Bộ tiêu thụ `0x48BD58` duyệt **bảng riêng** `0x7C31A4` bằng con trỏ thẳng, đọc `[esi+0x42C]/[+0x438]/[+0x444]`, **không dùng phân vùng không gian** | 🟠 |

> ⚠️ **Dương tính giả đã mắc:** `func_nav_blocker` đọc `[esi+0x28]` **6 lần**, nhưng `esi`
> là **cấu trúc lưới nav**, không phải `this`. `[reg+0x28]` **không** là bằng chứng truy
> cập edict.

---

## 2. `swap` — đổi lớp entity sang lớp rẻ hơn lúc tạo

| thứ | giá trị | mức |
|---|---|---|
| `GetEntityFactoryDictionary` | RVA `0x20CA70` | 🟠 |
| `CEntityFactoryDictionary::Create` | **vtable slot 1** | 🟢 |

Móc slot 1 rồi ghi đè classname ngay lúc tạo. Nằm **dưới mọi đường tạo entity** — kể cả
phân tích lump, `CreateEntityByName` lúc chơi, và VScript.

**Cặp duy nhất dùng được:** `point_spotlight` → `beam_spotlight`.

Lý do: `CPointSpotlight::SpotlightCreate` (RVA `0x18E5xx`) tự tạo thêm `spotlight_end` +
`beam` ⇒ **1 dòng lump = 3 edict**. `beam_spotlight` vẽ hoàn toàn phía client ⇒ **1 edict**.

```
🟢 đo được: sống 1954 -> 1330   giảm đúng 624   chỗ thở 93 -> 718 slot
```

Giá phải trả: `client.dll` ghi cứng `HaloScale = 60.0` ⇒ quầng sáng to gấp 6.

> Quét **557 lớp** của `server.dll` đối chiếu **16 map** của ba chiến dịch: **đúng một
> cặp**. Xem [07-het-huong.md](07-het-huong.md).

---

## 3. `freegate` — bỏ thời gian chờ 1 giây trước khi tái dùng edict

| thứ | giá trị | mức |
|---|---|---|
| điểm vá | `engine.dll` RVA `0x1E022A` — `jae` → `jmp` (**một byte**) | 🟢 |
| chữ ký neo | `D9 E8 D9 C9 DF F1 DD D8 73` (`fld1 ; fxch st(1) ; fcompi st(1) ; fstp st(0) ; jae`) | 🟠 |
| bảng "vừa giải phóng" | `sv + 0x104` (`sv + 0x180` có nhưng **không dùng**) | 🟠 |
| kích thước xoá bảng | hằng `0x2000` ghi cứng trong hàm xoá | 🟠 |

Plugin **không** dùng RVA cứng — nó quét chữ ký. RVA chỉ ghi để đối chiếu.

> ⚠️ **Chưa nghiệm thu dài hạn.** Có nghi ngờ (chưa chứng minh) rằng chạy lâu thì gây
> nghẽn hệ thống, và làm sai lệch số entity ở máy chủ ≥ 4 người chơi. Gói xuất bản để
> `freegate=1` vì nó đã qua phép đo đối chứng; nếu máy chủ bạn đông người và thấy tickrate
> lạ thì **đặt về 0 trước tiên**.

---

## 4. `wipeclear` — dọn thực thể ở đầu vòng hồi sinh

| thứ | giá trị | mức |
|---|---|---|
| `CTerrorGameRules::RestartRound` | **vtable slot 178** · RVA `0x2E0650` | 🟢 |
| `g_pGameRules` | RVA `0x7F7F6C` | 🟠 |
| `gEntList` | RVA `0x7E0760` | 🟠 |
| `g_fInCleanupDelete` | RVA `0x7E0730` | 🟠 |
| `CleanupDeleteList` | RVA `0x0B5D10` (`__cdecl`) | 🟠 |
| `NextEnt` | RVA `0x0B4270` (`__cdecl`) | 🟠 |
| `UTIL_Remove` | RVA `0x2071E0` (`__cdecl`) | 🟠 |
| `s_PreserveEnts` | RVA `0x7ACE40` — mảng **38 mục**, `[0]` = `ai_network`, `[33]` = `predicted_viewmodel` | 🟠 |

**Hai cổng an toàn** trước khi móc: prologue phải khớp, **và** slot 178 phải đang trỏ
đúng hàm đó. Không khớp ⇒ **bỏ qua, không vá**.

---

## 5. `mapclear` — dọn ở chuyển màn

| thứ | giá trị | mức |
|---|---|---|
| `CServerGameDLL::PrepChangelevel` | **vtable slot 38** · RVA `0x2B8140` | 🟢 |
| neo chuỗi | `"Preparing player entities for changelevel"` — `server.dll` `0x687718`, **1 xref** | 🟠 |
| `ObjectCaps` | vtable slot 40, bit `0x2` = `FCAP_ACROSS_TRANSITION` | 🟠 |

> ⚠️ **Bài học đắt nhất của dự án:** xoá một entity **mang sang màn sau** ở chuyển màn
> **làm sập máy chủ**. Quy tắc "xoá ít đi" mà ai cũng nghĩ ra đầu tiên là **quy tắc sai**.
> Đọc [02-mapclear.md](02-mapclear.md) trước khi bật.

---

## 6. Chuỗi neo khác + cách suy lại sau khi Valve cập nhật

Khi RVA chết, **nội dung** và **cấu trúc** vẫn sống. Kịch bản `tools/doi-offset.py`
(trong repo phát triển) suy lại các neo chính mà **không dùng một RVA ghi cứng nào**:

| neo | cách suy | kết quả trên bản này |
|---|---|---|
| `s_PreserveEnts` | tìm chuỗi `"ai_network"`, duyệt mọi xref, nhận ra mảng nào có `[33]` = `"predicted_viewmodel"` | `0x107ACE40`, 38 mục |
| `PostConstructor` | **bình chọn đa số** slot 29 trên mọi vtable entity | `0x10055620` (539 lớp) |
| `DT_BaseEntity` | slot 9 có thân `mov eax,imm32 ; ret`; `imm` phổ biến nhất | `0x107D78A8` (229 lớp) |
| `PrepChangelevel` | chuỗi `"Preparing player entities for changelevel"` | `server.dll 0x10687718`, 1 xref |
| `CreateEntityByName` | chuỗi `"CreateEntityByName( %s, %d ) - CreateEdict failed."` | `server.dll 0x10619988`, 1 xref |
| `ED_Alloc` | chuỗi `"ED_Alloc: no free edicts"` | **`engine.dll`** `0x10395800`, 1 xref |
| trần temp entity máy chủ | chuỗi `"sv_multiplayer_maxtempentities"` | `engine.dll 0x10357998` |
| hồ temp entity của client (500 ô) | chuỗi `"Overflow %d temporary ents!"` | **`client.dll`** `0x1059B01C` |

**Vẫn phải làm tay:** `RestartRound` (lấy vtable `g_pGameRules` rồi đọc slot 178, đối
chiếu prologue) và ba hàm `__cdecl` không có chuỗi riêng (`CleanupDeleteList`, `NextEnt`,
`UTIL_Remove` — lần theo chỗ **gọi** từ hàm đã định vị được).

### ⚠️ Hai cái bẫy đã mắc thật — đừng mắc lại

1. **Chuỗi nằm ở module nào.** `"ED_Alloc: no free edicts"` ở **`engine.dll`**, không phải
   `server.dll`. Tìm nhầm module ra 0 kết quả rồi tưởng là mất.
2. **Mảng dữ liệu không nằm trong `.text`.** `s_PreserveEnts` ở `.rdata`. Quét xref mà
   giới hạn trong `.text` thì **không bao giờ thấy**.

### ⚠️ Bẫy thứ ba: cách mã hoá lệnh gọi ảo

Trong nhị phân này, lệnh gọi engine/gọi ảo được sinh ra dạng:

```asm
mov  edx, [eax+0x4C]
call edx
```

**không phải** `call [reg+disp]` và **không phải** `call rel32`.

⇒ Quét tìm `E8` (call rel32) hoặc `FF /2` sẽ ra **0 kết quả** và dẫn tới kết luận sai
*"không ai gọi hàm này"*. Đã mắc **hai lần**: một lần với `SetOwnerEntity`, một lần với
`LightStyle`. Bộ quét đúng phải bám theo `mov reg,[reg+disp]` rồi tìm `call reg` ngay sau.

---

## 7. Địa chỉ phía `client.dll` (dùng để bác bỏ giả thuyết)

| thứ | RVA | dùng để chứng minh gì | mức |
|---|---|---|---|
| `C_Sprite` constructor | `0x19D6C0` | **0** lệnh ghi địa chỉ tuyệt đối ⇒ không đăng ký toàn cục ⇒ client **không** tự giữ `env_sprite` | 🟠 |
| `CSprite::Spawn` (server) | `0x1E0140` | chỉ gọi `UTIL_Remove`, **không** chạm `g_pEngineServer` | 🟠 |
| `CSprite::Activate` (server) | `0x065CD0` | chỉ `FindEntityByName` + 2 hàm nội bộ | 🟠 |
| hồ temp entity của client | 500 ô (`"Overflow %d temporary ents!"`) | bác bỏ hướng đổi `env_sprite` sang temp entity (cần 639–730 ô) | 🟠 |

---

## 8. Ba cửa "tài sản vs thể hiện"

Nguyên lý rút ra: **trạng thái chỉ sống sót qua việc xoá entity nếu nó được ghi vào một
kho NẰM NGOÀI entity.** L4D2 có **đúng ba cửa** như vậy:

| cửa | `IVEngineServer` offset | số chỗ gọi | trạng thái |
|---|---|---|---|
| `LightStyle` | `+0xA0` | 6 | ✅ đã khai thác (`light`, `light_spot`) |
| `StaticDecal` | `+0xA4` | 1 | ✅ đã khai thác (`infodecal`) |
| `EmitAmbientSound` | `+0x70` | 3 | ❌ **cấm** — tham số đầu là `entindex` của chính nó (vi phạm ĐK3) |

Không có cửa thứ tư. Đây là lý do `noedict` **đã hết đường mở rộng** — xem
[07-het-huong.md](07-het-huong.md).

---

## 9. Cách tự kiểm lại

Plugin **tự kiểm lúc chạy** và ghi vào `edictbudget.log`. Đọc log là cách nhanh nhất để
biết địa chỉ còn đúng không:

```
[EdictBudget] NOEDICT: 'func_areaportal' vtable=64C48B8C slot29 -> thunk (OK)
[EdictBudget] MAPCLEAR: da moc vtable slot 38 (0x102B8140 @ ...)
[EdictBudget] WIPECLEAR: da moc vtable slot 178 (RestartRound @ ...)
[EdictBudget] SWAP: da moc dictionary slot 1 (Create) @...
```

Dòng **"slot29 -> thunk (OK)"** phải xuất hiện **một lần cho mỗi vtable** được vá, và
tổng ở cuối phải khớp số lớp đã bật.

> ⚠️ **Lỗi im lặng đã từng xảy ra:** bản DLL cũ không giải được vtable thì ghi
> *"khong tim duoc vtable, BO QUA"* rồi **chạy tiếp như bình thường**. Suốt **6 phiên**
> `info_zombie_spawn` không hề được bật mà không ai biết. Đã sửa (`ResolveClassVtable`,
> `sample_mm.cpp:1585`) — nếu đường `call rel32` không cho kết quả thì **thử tiếp**,
> không trả `NULL` ngay. **Luôn đối chiếu con số tổng ở cuối log.**
