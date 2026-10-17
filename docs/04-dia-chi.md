# Địa chỉ

*Tiếng Việt · [English](04-dia-chi.en.md)*

Các địa chỉ plugin dùng, để kiểm lại hoặc suy lại khi game cập nhật.

---

## Nhị phân tham chiếu

Left 4 Dead 2 Dedicated Server `2.2.4.3`, build `10097`. RVA tính theo ImageBase
`0x10000000`; lúc chạy phải cộng base thật của module.

| file | kích thước | md5 |
|---|---|---|
| `left4dead2/bin/server.dll` | 9 130 288 | `533888fbb4e5ed534b172470613a3017` |
| `bin/engine.dll` | 4 817 712 | `a16cd381409bab749909d5000c2302d8` |
| `left4dead2/bin/client.dll` | 8 305 664 | `21565d29a23caeabe3d0ffc6156c3e5c` |

md5 khác thì đừng dùng RVA trong bảng — xem mục **Suy lại sau khi cập nhật**.

"vtable slot N" = hàm ảo thứ N (offset `N × 4`).

---

## noedict (`server.dll`)

| thứ | địa chỉ |
|---|---|
| `m_iEFlags` | `[this+0x138]`, `EFL_SERVER_ONLY` = bit 9 |
| `CBaseEntity::PostConstructor` | vtable slot 29, RVA `0x55620` |
| `GetServerClass` | vtable slot 9, thân `mov eax, imm32 ; ret` |
| ServerClass của `DT_BaseEntity` | RVA `0x7D78A8` |
| `UpdateTransmitState` | vtable slot 21 (mặc định RVA `0x56A40`) |
| `ObjectCaps` | vtable slot 40, bit `0x2` = `FCAP_ACROSS_TRANSITION` |
| `GetEntityFactoryDictionary` | RVA `0x20CA70` |
| dictionary `FindFactory` / `Create` | vtable slot 3 / slot 1 |
| factory `Create` | vtable slot 0 |
| `CreateEntityByName(name, forceIndex)` | RVA `0x1196B0` |
| danh sách entity (`gEntList`) | RVA `0x7E0760` |
| `CBaseEntityList` khởi tạo | RVA `0xB7890` — 4096 ô; ô 2049–4095 vào danh sách không mạng |
| `AddNonNetworkableEntity` | RVA `0xB7BD0` — hết ô: `Warning(...no free slots!)`, trả handle `0xFFFFFFFF` |
| `RemoveEntity` (bảng entity) | RVA `0xB7970` — ô ≥ 2048 trả về danh sách không mạng |
| `FindEntityByClassname` | RVA `0xB47F0` — **không** xét edict |
| `FindEntityByClassnameNearest` | RVA `0xB5370` — bỏ qua entity có `[ent+0x28] == 0` (không edict) |

Cách plugin tìm vtable một lớp: `GetEntityFactoryDictionary` → `FindFactory(tên)` → `Create`
→ tìm lệnh `mov dword ptr [reg], imm32` (ghi con trỏ vtable). Không thấy thì lần theo các lệnh
`call rel32` trong 0x30 byte đầu của `Create`. Dừng quét ở hai byte `0xCC` liền nhau (`v2.0.1`);
mọi con trỏ suy ra phải nằm trong `server.dll` (`v2.0.2`).

---

## swap (`server.dll`)

| thứ | địa chỉ |
|---|---|
| dictionary `Create` | vtable slot 1 của đối tượng trả về từ `GetEntityFactoryDictionary` |

Cả `.text` chỉ có 3 chỗ gọi slot 1: `CreateEntityByName` và 2 nhánh của bộ đọc entity lump.

---

## spotlight (`server.dll`, `client.dll`)

| thứ | địa chỉ |
|---|---|
| `MapEntity_ParseEntity` (cdecl: `CBaseEntity*&`, `const char*`, `IMapEntityFilter*` → `const char*`) | `0x101198F0`; đúng 2 lệnh `call`: `0x1011A6DD` (`MapEntity_ParseAllEntities` — nạp map và `CleanUpMap`), `0x101912B2` (`CPointTemplate::CreateInstance`) |
| `CPointSpotlight::SpotlightCreate` (thiscall) | `0x1018E3D0`; đúng 3 lệnh `call`: `0x1018F1A2` (trong `CreateEfficientSpotlight` `0x1018F160`), `0x1018ECB3`, `0x1018ED08` |
| `CPointSpotlight::Activate` — không cha thì chọn "efficient" | `0x1018F3F0` |
| constructor `CPointSpotlight` — `HaloScale` mặc định 60.0 ở `+0x46C` | `0x1018DC20` |
| vtable `point_spotlight` / `spotlight_end` | `0x10630E64` / `0x10643B54`; slot 29 = `PostConstructor` `0x10055620` |
| `CEntityFactoryDictionary` vtable / `Create` gốc | `0x10654F48` / `0x10206A40` (`GetEntityFactoryDictionary` `0x1020CA70`) |
| `m_iEFlags` / `EFL_SERVER_ONLY` | `+0x138` / bit 9 |
| `client.dll`: `C_BeamSpotLight` gán quầng cố định 60 | `0x1006CAD0` |
| `client.dll`: `C_SpotlightEnd::ClientThink` — màu × alpha/255 | `0x1009E7A0` |

Plugin tự đếm mọi lệnh `call` tới hai hàm đích lúc nạp (quét toàn `.text`); số khác 2 / 3, hoặc có
`jmp`, thì module tự tắt.

## hopbao (`server.dll`)

| thứ | địa chỉ |
|---|---|
| `CCollisionProperty::MarkSurroundingBoundsDirty` (thiscall), dài 0x64 byte, 12 lời gọi trong 9 hàm | `0x10093A80` |
| chỗ vá: `mov eax,[ecx+0x28]` · `test eax,eax` · `je` = `8B 41 28 85 C0 74 55` | `0x10093A86` → `E9 <stub> 90 90` |
| đường có edict tiếp ở / `pop esi; ret` | `0x10093A8D` / `0x10093AE2` |
| neo tự định vị: `or dword [ecx+0x138],0x4000` = `81 89 38 01 00 00 00 40 00 00`, duy nhất trong `.text` | `0x10093A9D` = đầu hàm + 0x1D |
| 2 địa chỉ tuyệt đối trong hàm, bỏ qua khi so mẫu | `gpGlobals` `0x107F7690` (+0x0F), danh sách partition `0x107DCA70` (+0x3A) |
| `CCollisionProperty::WorldSpaceSurroundingBounds` — chỉ tính lại khi bit 14 bật | `0x100935F0` |
| `CFuncNavBlocker::BlockNav` / `UnblockNav` | `0x1048BCC0` / `0x1048C470` |
| `m_iEFlags` / `EFL_SERVER_ONLY` / `EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS` | `+0x138` / bit 9 (`0x200`) / bit 14 (`0x4000`) |

Stub 48 byte (log in đủ byte lúc nạp, để dịch ngược kiểm):

```
8B 41 28                         mov  eax,[ecx+0x28]
85 C0                            test eax,eax
0F 85 rel32                      jnz  0x10093A8D
F7 81 38 01 00 00 00 02 00 00    test dword [ecx+0x138],0x200
0F 84 rel32                      jz   0x10093AE2
FF 05 addr32                     inc  dword [g_HopBaoDem]
81 89 38 01 00 00 00 40 00 00    or   dword [ecx+0x138],0x4000
E9 rel32                         jmp  0x10093AE2
```

---

## wipeclear (`server.dll`)

| thứ | địa chỉ |
|---|---|
| `CTerrorGameRules::RestartRound` | vtable slot 178, RVA `0x2E0650` |
| `g_pGameRules` | RVA `0x7F7F6C` |
| `UTIL_Remove` | RVA `0x2071E0` |
| `CleanupDeleteList` | RVA `0xB5D10` |
| `NextEnt` | RVA `0xB4270` |
| `g_fInCleanupDelete` | RVA `0x7E0730` |
| preserve list của game | RVA `0x7ACE40`, 38 mục, `[0]` = `ai_network`, `[33]` = `predicted_viewmodel` |
| `IVEngineServer::AllowImmediateEdictReuse` | vtable slot 95 |

Trước khi móc, plugin kiểm phần đầu hàm và kiểm slot 178 còn trỏ đúng hàm đó.

---

## mapclear (`server.dll`)

| thứ | địa chỉ |
|---|---|
| `PrepChangelevel` | vtable slot 38, RVA `0x2B8140` |
| chuỗi neo | `"Preparing player entities for changelevel"` (1 chỗ dùng) |

Phần đầu hàm này có một địa chỉ tuyệt đối; so khớp phải bỏ qua 4 byte đó.

---

## freegate, trap (`engine.dll`)

| thứ | địa chỉ |
|---|---|
| `ED_Alloc` | RVA `0x1E0170` (`IVEngineServer::CreateEdict`, slot 22) |
| `ED_Free` | RVA `0x1DFF60` (`IVEngineServer::RemoveEdict`, slot 23, lối vào duy nhất) |
| bảng `freetime` | `engine + 0x6B3A58` (plugin suy từ mã máy của `ED_Free`) |
| điểm vá `freegate=2` | RVA `0x1E022A`, `jae` → `jmp` (định vị bằng chữ ký `D9 E8 D9 C9 DF F1 DD D8 73`) |
| nhánh lỗi của `ED_Alloc` (`trap`) | RVA `0x1E0247` `test ebx,ebx` / `0x1E0249` `js` |

---

## Suy lại sau khi cập nhật

| thứ | cách tìm |
|---|---|
| `PostConstructor` | hàm xuất hiện nhiều nhất ở slot 29 của các vtable entity |
| `MarkSurroundingBoundsDirty` (`hopbao`) | lệnh `or dword [ecx+0x138],0x4000` duy nhất trong `.text`; đầu hàm = lệnh đó − 0x1D. Plugin tự tìm |
| `DT_BaseEntity` | giá trị `imm32` xuất hiện nhiều nhất trong thân slot 9 (`B8 imm32 C3`) |
| preserve list | chuỗi `"ai_network"`; mảng nào có `[33]` = `"predicted_viewmodel"` |
| `PrepChangelevel` | chuỗi `"Preparing player entities for changelevel"` |
| `CreateEntityByName` | chuỗi `"CreateEntityByName( %s, %d ) - CreateEdict failed."` |
| `ED_Alloc` | chuỗi `"ED_Alloc: no free edicts"` — nằm trong **`engine.dll`** |

`RestartRound` và các hàm `__cdecl` không có chuỗi riêng (`CleanupDeleteList`, `NextEnt`,
`UTIL_Remove`) phải lần theo chỗ gọi từ hàm đã biết.

Ba lỗi hay gặp khi tìm:

1. Tìm chuỗi nhầm module (`"ED_Alloc: ..."` ở `engine.dll`, không phải `server.dll`).
2. Mảng dữ liệu nằm ở `.rdata`; tìm tham chiếu chỉ trong `.text` sẽ không thấy.
3. Lệnh gọi hàm ảo trong bản build này có dạng `mov edx, [eax+N]` rồi `call edx`, không phải
   `call [reg+N]` hay `call rel32`. Chỉ tìm `E8` sẽ kết luận sai là "không ai gọi".

---

## Kiểm lúc chạy

Plugin ghi vào `addons/edictbudget/edictbudget.log` mỗi lần cài một tính năng, ví dụ:

```
NOEDICT: server.dll base=67A90000 size=0x982000 (chan bien cho bo giai vtable)
NOEDICT: 'func_areaportal' vtable=680940A4 slot29 -> thunk (OK)
NOEDICT: da sua N vtable / M lop yeu cau
```

Lấy địa chỉ trong log trừ base rồi cộng `0x10000000` là ra địa chỉ tĩnh để so với bảng
(ví dụ trên: `0x680940A4 − 0x67A90000 + 0x10000000 = 0x106040A4`, vtable của `func_areaportal`).
`N` nhỏ hơn `M` là bình thường khi có lớp dùng chung vtable (vd `light` / `light_spot`).

`hopbao` tự tìm hàm (neo duy nhất + so đủ 0x64 byte) rồi in sẵn RVA và byte thật để đối chiếu:

```
HOPBAO: da va MarkSurroundingBoundsDirty @6D1B3A80 (RVA 0x93A80), stub @1A660000 (48 byte) - ...
HOPBAO: stub = 8B 41 28 85 C0 0F 85 82 3A B5 52 F7 81 38 01 00 00 00 02 00 00 0F 84 C7 3A B5 52 FF 05 88 9E FF 6C 81 89 38 01 00 00 00 40 00 00 E9 B2 3A B5 52
HOPBAO: dau+6 = E9 75 C5 4A AD 90 90 (goc 8B 41 28 85 C0 74 55)
```

`0x93A80` + `0x10000000` = `0x10093A80`, khớp bảng ở mục hopbao. Dịch ngược dòng `stub` phải ra
đúng 8 lệnh ở mục đó; tính theo địa chỉ trong log, ba rel32 trỏ về đầu hàm + `0x0D`
(`0x6D1B3A8D`), + `0x62` (`0x6D1B3AE2`) hai lần, và `E9` ở dòng `dau+6` trỏ tới stub (`0x1A660000`).
Không có ba dòng này mà có dòng `HOPBAO: ... BO QUA` thì plugin đã không vá (bản `server.dll` khác
mẫu) — xem mục **Suy lại sau khi cập nhật**.
