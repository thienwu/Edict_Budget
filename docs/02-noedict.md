# noedict

*Tiếng Việt · [English](02-noedict.en.md)*

Không cấp edict cho những lớp entity không cần gửi về client.

---

## Cơ chế

Engine có cờ `EFL_SERVER_ONLY` (bit 9 của `m_iEFlags`, `[this+0x138]`). Khi tạo entity,
`CBaseEntity::PostConstructor` (RVA `0x55620`) xem cờ này: bật thì entity vào dải 2049–4095
và **không có edict**.

Plugin thay **vtable slot 29** (`PostConstructor`) của mỗi lớp trong `noedict.txt` bằng một
hàm nhỏ: bật cờ rồi gọi hàm gốc. Không sửa byte nào của engine.

### Phần không mạng: ô 2049–4095, tối đa 2047

Lịch sử: dự án bắt đầu bằng việc **nâng giới hạn edict lên 4096** (`bigarray`, `snapshot`… trong
`engine.dll`). Cách đó không ổn định: `num_edicts` lên 2060, entity có mạng tràn lên trên 2047
(client giải mã sai), vòng hồi sinh lúc thua bị hỏng. Trong lúc điều tra mới thấy các ô ≥ 2048
chỉ dùng được cho entity **không** gửi về client — và game đã có sẵn chỗ cho loại entity đó.
Từ đây dự án chuyển sang **cắt giảm** thay vì nâng trần; `noedict` là kết quả. Chi tiết hướng
4096: [05-huong-da-bo.md](05-huong-da-bo.md).

Bảng entity của `server.dll` (`CBaseEntityList`) vốn có **4096 ô**. Hàm khởi tạo của nó
(`0x100B7890`) đưa sẵn các ô **2049–4095** vào danh sách dành cho entity không mạng:

```
100B78B8  mov ebx, 0x1000         ; 4096 ô, mỗi ô 0x10 byte
100B78DF  lea eax, [edi+0x801C]   ; bắt đầu từ ô 2049
100B78E5  mov esi, 0x7FF          ; 2047 ô
```

Đoạn này nằm trong `server.dll` gốc (md5 trùng bản Steam; plugin không sửa hàm này). Entity
không mạng chỉ lấy ô từ danh sách đó, nên **không bao giờ nằm ở ô 0–2048**. Ô 0–2047 là của
entity có mạng — ô của chúng chính là số edict; `RemoveEntity` (`0x100B7970`) cũng chỉ trả về
danh sách không mạng những ô `≥ 0x800`. Số đo khớp: entity `logic_script` bị cắt mạng có tham chiếu
`-2117514541` = `0x81C94AD3`, tức chỉ số `0xAD3` = 2771.

Giới hạn: **tổng số entity không mạng cùng sống tối đa 2047** — gồm các lớp vốn server-only của
game và các lớp do `noedict` cắt. Hết ô thì `AddNonNetworkableEntity` (`0x100B7BD0`) in
`CBaseEntityList::AddNonNetworkableEntity: no free slots!` và trả handle không hợp lệ; máy chủ
không dừng, nhưng entity đó không có chỗ trong bảng.

Đếm trên 231 nguồn bản đồ với `noedict.txt` hiện tại, coi như mọi dòng lump cùng sống một lúc:
nhiều nhất `anemoia_kitty` 1486 (10 của game + 1476 do `noedict`). Chưa có lần nào log in dòng
`no free slots`. Con số này chưa tính entity tạo thêm lúc chơi — thêm lớp mới thì nhớ cộng.

Entity không có edict vẫn:
- được tạo, chạy `Spawn()` / `Activate()`, nhận và bắn input/output;
- tìm thấy được theo tên hoặc theo lớp qua danh sách entity của máy chủ (handle 12 bit, 0–4095).

Nhưng:
- client không nhận được nó;
- lệnh `ent_text`, `ent_dump`… không thấy nó;
- hàm nào chỉ xét entity có edict (`[entity+0x28] != 0`) sẽ bỏ qua nó — xem điều kiện 7.

---

## noedict.txt

- Mỗi dòng **một tên lớp, khớp chính xác**. Dòng bắt đầu bằng `#` là ghi chú.
- **Không có khớp tiền tố** ở file này (dòng `light_` không khớp gì cả). Quy tắc "kết thúc
  bằng `_`" chỉ áp cho `wipekeep.txt`.
- Tối đa 1024 dòng (từ `v2.0.2`; bản cũ dừng im lặng ở dòng 32).

Mỗi khối lớp trong file có:
- **TRẠNG THÁI**: đã kiểm chứng / đã test / mới, mặc định tắt / biết lỗi, tắt;
- **BẢN ĐỒ ĐỂ KIỂM CHỨNG**: map có lớp đó và số cái trên từng map.

### Vá theo vtable, không theo tên

Nhiều tên lớp dùng chung một vtable. Bật một tên là cắt **mọi** tên dùng chung vtable đó.
Ví dụ:

| bật | cũng bị cắt |
|---|---|
| `light` | `light_spot`, `light_directional`, `light_glspot` |
| `info_landmark` | `info_player_start`, `info_teleport_destination`, `info_player_logo`, `info_hang_lighting`, `logic_proximity` |
| `info_ambient_mob_end` | `info_ambient_mob_start` |

Khối nào trong `noedict.txt` có lớp bị kéo theo thì cũng liệt kê bản đồ của lớp đó.

---

## Plugin tự kiểm những gì lúc chạy

Trước khi vá một lớp, plugin kiểm:

1. Phần đầu hàm `PostConstructor` đúng như mong đợi (kiểm một lần cho cả plugin).
2. Slot 29 của vtable còn trỏ đúng `PostConstructor` — nếu đã bị vá (vtable dùng chung với
   lớp đứng trước) hoặc lớp tự ghi đè thì bỏ qua.
3. **Điều kiện 1**: `GetServerClass` (slot 9) trả về `DT_BaseEntity` — lớp có SendTable riêng
   thì từ chối.

Không qua thì bỏ qua lớp đó và ghi log lý do; máy chủ chạy tiếp.

**Chỉ điều kiện 1 được máy kiểm.** Điều kiện 2–8 bên dưới là trách nhiệm của người thêm lớp.

Thử ngày 02/10/2026: đưa cả 465 lớp chưa server-only vào `noedict.txt`. Plugin làm đúng như dự
đoán (172 lớp bị cắt, 293 bị chặn). Nhưng trong 172 lớp bị cắt có **52 lớp trượt các điều kiện
khác** — tức nếu ai thêm chúng vào thì plugin vẫn cắt. Danh sách:
[03-loi-da-biet.md](03-loi-da-biet.md#4-lớp-lọt-qua-bộ-tự-kiểm).

### Lớp bị giải nhầm vtable

Plugin tìm vtable bằng cách đọc mã máy của hàm tạo lớp. Với một số lớp, cách này trả về vtable
của **lớp cha**. Không thêm các lớp sau vào `noedict.txt`:

`point_commentary_viewpoint`, `env_soundscape_proxy`, `env_soundscape_triggerable`,
`prop_vehicle_driveable`, `player`, `weapon_first_aid_kit`, `weapon_defibrillator`,
`env_fire_trail`

Đặc biệt `env_soundscape_proxy` / `env_soundscape_triggerable` trả về vtable của
`env_soundscape`, và vtable đó **qua cả 3 bước tự kiểm** — plugin sẽ cắt nhầm lớp khác mà không
báo gì. (Hiện hai lớp này đã server-only sẵn nên không ai cần thêm.)

---

## Điều kiện trước khi thêm một lớp

| # | điều kiện | ví dụ lớp trượt |
|---|---|---|
| 1 | không có SendTable riêng (`GetServerClass` trả `DT_BaseEntity`) — **plugin tự kiểm** | `env_sprite`, `beam`, `light_dynamic` |
| 2 | không đặc, không di chuyển — engine cập nhật va chạm bằng `edict_t*` | mọi `trigger_*`, `func_wall`, `func_clip_vphysics` |
| 3 | không tự đưa chỉ số của chính nó vào gói tin | `ambient_generic` (`EmitAmbientSound(entindex)`) |
| 4 | chưa server-only sẵn | — |
| 5 | không bị một entity có mạng trỏ tới qua handle | `point_spotlight` (`spotlight_end` gọi `SetOwnerEntity`) |
| 6 | không được gửi về client — không model, hoặc có `EF_NODRAW` và không có entity con | `info_target`, `point_viewcontrol`, `func_illusionary` |
| 7 | không bị tìm bằng hàm chỉ xét entity có edict | `info_ambient_mob_start` (`FindEntityByClassnameNearest` `0x100B5370`) |
| 8 | không có plugin tạo lớp đó lúc chạy — hoặc plugin đã xử lý số âm, xem dưới | `logic_script`, `env_shake` với một số plugin |

Không chắc một điều kiện thì không thêm.

---

## Plugin SourceMod

Entity không có edict thì SourceMod không có chỉ số dương cho nó. `CreateEntityByName()` trả về
**tham chiếu**, là một **số âm** (vd `-2117514541`).

| native | với số âm |
|---|---|
| `EntIndexToEntRef()` | lỗi `Invalid entity index` — đoạn mã phía sau dừng |
| `RemoveEdict()` | không có edict để xoá — có thể sập |

Các native nhận tham chiếu (`IsValidEntity`, `DispatchSpawn`, `AcceptEntityInput`,
`RemoveEntity`, …) vẫn dùng được với số âm đó.

Đã gặp thật:
- `logic_script` + `l4d2_script_cmd_swap`: 35 lỗi, lệnh `script` im lặng không chạy.
- `env_shake` + `l4d_plane_crash`: dùng cả `EntIndexToEntRef()` lẫn `RemoveEdict()`.

Cách sửa trong plugin:

```sourcepawn
// Số âm đã là tham chiếu - không đưa qua EntIndexToEntRef nữa.
stock int SafeRef(int entity)
{
    if (entity < 0)
        return entity;
    if (entity == 0 || !IsValidEntity(entity))
        return INVALID_ENT_REFERENCE;
    return EntIndexToEntRef(entity);
}

// RemoveEntity đi qua danh sách entity của máy chủ, dùng được cả khi không có edict.
stock void SafeRemove(int entity)
{
    if (entity != INVALID_ENT_REFERENCE && IsValidEntity(entity))
        RemoveEntity(entity);
}
```

Thay mọi `EntIndexToEntRef(x)` bằng `SafeRef(x)` và `RemoveEdict(x)` bằng `SafeRemove(x)`.

Không sửa được plugin thì đừng cắt lớp mà nó tạo. Tìm trong file `.sp` (file `.smx` đã nén,
tìm chuỗi sẽ không thấy) các chỗ `CreateEntityByName("<tên lớp>")`.

---

## Cách kiểm một lớp

1. Bật **một** lớp mỗi lần.
2. Khởi động máy chủ, mở `addons/edictbudget/edictbudget.log`. Phải có:

```
NOEDICT: '<lớp>' vtable=... slot29 -> thunk (OK)
NOEDICT: da sua N vtable / M lop yeu cau
```

   Dòng `khong tim duoc vtable, BO QUA` hoặc `TRUOT DIEU KIEN 1, TU CHOI` nghĩa là lớp
   **không** được cắt.
3. Nạp một map trong danh sách **BẢN ĐỒ ĐỂ KIỂM CHỨNG** của lớp đó. So số entity sống /
   `num_edicts` lúc `round_start` khi bật và khi tắt.
4. Chơi qua đoạn có lớp đó, xem có gì mất (âm thanh, hiệu ứng, sự kiện, đường đi của bot).

### Đếm entity: tính cả file `.lmp`

Thư mục `update/maps/` có các file `<map>_l_0.lmp`, `_h_0.lmp`, `_s_0.lmp`. Chúng **thay** entity
lump của file `.bsp` (thư mục `update` đứng đầu SearchPaths), và có thể khác `.bsp` — vd
`c12m3_bridge` có 49 `func_nav_blocker` trong `.bsp` nhưng 51 trong `.lmp`. Danh sách bản đồ trong
`noedict.txt` đã tính cả `.lmp`. Chưa xác minh engine dùng file nào cho chế độ nào (có vẻ
`l` = Coop, `h` = Versus, `s` = Survival).
