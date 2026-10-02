# Lỗi đã biết

*Tiếng Việt · [English](03-loi-da-biet.en.md)*

---

## 1. Plugin SourceMod tạo entity của lớp đã cắt mạng nhận số âm

Lớp nằm trong `noedict.txt` thì mọi entity của nó đều không có edict, kể cả entity do plugin
tạo lúc chạy. `CreateEntityByName()` khi đó trả về một tham chiếu âm. `EntIndexToEntRef()` báo
lỗi `Invalid entity index`; `RemoveEdict()` có thể làm sập.

Đã gặp với `logic_script` (`l4d2_script_cmd_swap`) và `env_shake` (`l4d_plane_crash`).

Cách xử lý và mẫu mã: [02-noedict.md](02-noedict.md#plugin-sourcemod).

---

## 2. `wipeclear` dọn sớm hơn game

Plugin giữ tham chiếu tới entity bị dọn sẽ cầm tham chiếu treo. Dọn tham chiếu ở `mission_lost`,
hoặc ghi lớp đó vào `wipekeep.txt`. Xem [01-co-che.md](01-co-che.md#wipeclear).

---

## 3. `info_ambient_mob_end` làm hỏng tìm điểm ambient mob

`info_ambient_mob_end` dùng chung vtable với `info_ambient_mob_start`, nên bật nó là cắt cả hai.
`info_ambient_mob_start` được tìm bằng `FindEntityByClassnameNearest` (`0x100B5370`), mà hàm này
bỏ qua entity không có edict:

```
100B53C1  cmp dword ptr [esi+0x28], 0   ; không có edict -> bỏ qua
1035EF78  push 'info_ambient_mob_start'
1035EF82  call 100B5370
```

Hệ quả: chỗ gọi tại `0x1035EF78` không bao giờ tìm thấy điểm bắt đầu. Lớp này đang **tắt** trong
`noedict.txt`.

---

## 4. Lớp lọt qua bộ tự kiểm

Plugin chỉ tự kiểm điều kiện 1. Các lớp dưới đây **qua** bước tự kiểm — nghĩa là nếu ai thêm vào
`noedict.txt` thì plugin **vẫn cắt** — nhưng trượt ít nhất một điều kiện khác. **Không thêm.**

Kết quả lấy từ lần thử 02/10/2026 (đưa mọi lớp chưa server-only vào `noedict.txt`; log máy chủ
khớp dự đoán 465/465 lớp). Số theo cột 2 là nhiều nhất trên một bản đồ, tính trên bản đồ chính
thức, bản ghi đè `.lmp` và bản đồ workshop.

Điều kiện xem [02-noedict.md](02-noedict.md#điều-kiện-trước-khi-thêm-một-lớp). `5?`, `6?` = bộ
dò đánh dấu cần đọc tay, chưa phải kết luận.

| lớp | nhiều nhất / map | trượt điều kiện |
|---|---|---|
| `weapon_item_spawn` | 321 | 5? |
| `point_spotlight` | 312 | 6, 5? |
| `ambient_generic` | 170 | 3 |
| `info_particle_target` | 166 | 6 |
| `func_illusionary` | 127 | 6 (có model, không EF_NODRAW) |
| `info_target` | 60 | 6 |
| `func_nav_attribute_region` | 58 | 2 SetSolid[1, 2] |
| `func_clip_vphysics` | 50 | 2 SetSolid[6] |
| `env_player_blocker` | 40 | 2 SetSolid[0, 2] |
| `env_instructor_hint` | 33 | 3 |
| `env_fire` | 32 | 6, 5? |
| `script_clip_vphysics` | 30 | 2 SetSolid[3] |
| `func_wall` | 26 | 2 SetSolid[1], 6 (có model, không EF_NODRAW) |
| `func_wall_toggle` | 24 | 2 SetSolid[1] |
| `info_game_event_proxy` | 17 | 6, 3 |
| `point_viewcontrol` | 16 | 6, 5? |
| `point_deathfall_camera` | 15 | 6 |
| `point_nav_attribute_region` | 14 | 2 SetSolid[1, 2] |
| `filter_activator_team` | 12 | 4 |
| `info_target_instructor_hint` | 9 | 6 |
| `ambient_music` | 8 | 3 |
| `filter_health` | 7 | 4 |
| `filter_activator_name` | 5 | 4 |
| `logic_scene_list_manager` | 5 | 4 |
| `point_viewcontrol_multiplayer` | 5 | 6 |
| `info_ambient_mob_start` | 4 | 7 |
| `point_viewcontrol_survivor` | 4 | 6 |
| `filter_activator_class` | 3 | 4 |
| `filter_activator_model` | 2 | 4 |
| `filter_melee_damage` | 2 | 4 |
| `filter_activator_infected_class` | 1 | 4 |
| `filter_damage_type` | 1 | 4 |
| `func_nav_connection_blocker` | 1 | 2 SetSolid[1] |
| `logic_compare` | 1 | 4 |
| `script_nav_attribute_region` | 1 | 2 SetSolid[3] |
| `ai_changehintgroup` | 0 | CHUA SANG 6  |
| `entity_blocker` | 0 | 2 SetSolid[2] |
| `env_bubbles` | 0 | 6 (có model, không EF_NODRAW) |
| `env_message` | 0 | 3 |
| `escape_route` | 0 | 6? |
| `filter_activator_context` | 0 | 4 |
| `filter_base` | 0 | 4 |
| `func_fish_pool` | 0 | 6 (có model, không EF_NODRAW) |
| `func_vehicleclip` | 0 | 2 SetSolid[1] |
| `func_weight_button` | 0 | 2 SetSolid[6], 6 (có model, không EF_NODRAW) |
| `gibshooter` | 0 | 2 SetSolid[0, 2] |
| `logic_multicompare` | 0 | 4 |
| `point_devshot_camera` | 0 | 6 |
| `point_gamestats_counter` | 0 | 3 |
| `point_playermoveconstraint` | 0 | 6 |
| `simple_physics_brush` | 0 | 2 SetSolid[6], 6 (có model, không EF_NODRAW) |
| `vomit_particle` | 0 | 2 SetSolid[2] |

---

## 5. Lớp bị giải nhầm vtable

8 lớp mà cách tìm vtable của plugin trả về vtable của lớp cha. `env_soundscape_proxy` và
`env_soundscape_triggerable` còn qua cả 3 bước tự kiểm (trỏ tới vtable của `env_soundscape`).
Danh sách: [02-noedict.md](02-noedict.md#lớp-bị-giải-nhầm-vtable).

---

## 6. `freegate=2` làm hỏng chuyển vật phẩm

Plugin chuyển vật phẩm (vd Gear Transfer) huỷ rồi tạo lại vật phẩm trong cùng một frame. Chế độ
`2` trả lại đúng chỉ số vừa giải phóng, client thấy vũ khí "ma". Không dùng `2`.

---

## 7. `mapclear` xoá entity mang sang màn sau thì máy chủ sập

Danh sách mang sang đã lập xong trước điểm móc; xoá entity trong danh sách đó làm engine đọc vào
vùng nhớ đã giải phóng. Đã sập thật với `mapclear=2` và với `mapclearcarry=1`. Giữ `mapclear=1`,
`mapclearcarry=0`.

---

## 8. `swap`: quầng sáng to hơn, entity mang sang màn nhiều hơn

- `client.dll` cố định kích thước quầng sáng của `beam_spotlight` là 60. Map đặt `HaloScale 10`
  cho `point_spotlight` sẽ thấy quầng to gấp 6.
- `beam_spotlight` được mang sang màn sau (`point_spotlight` thì không). Đo trên `the_hive`
  m3 → m4: thêm 48 entity vào danh sách mang sang; trần của engine là 512.

---

## 9. Không phải lỗi của plugin

`c1m3_mall` in nhiều dòng:

```
env_entity_maker maker_mannequin_setup_male-InstanceAutoNN failed to find template @template_mannequin_setup_male.
```

Hai `point_template` `@template_mannequin_setup_male` / `_female` của map có output
`OnEntitySpawned !self,Kill,,1,-1` — tự xoá 1 giây sau lần sinh đầu, trong khi 154
`env_entity_maker` cùng trỏ vào chúng. Maker nào chạy sau thì không còn template. Lỗi có sẵn
trong map.

---

## Đã sửa

| tag | lỗi |
|---|---|
| `v2.0.1` | tìm vtable đọc lấn sang hàm kế tiếp, nhận nhầm vtable của lớp khác (gặp ở `upgrade_spawn` → vtable của `weapon_melee_spawn`) |
| `v2.0.2` | máy chủ sập lúc khởi động khi `noedict.txt` có `beam_spotlight`, `func_breakable`, `logic_choreographed_scene`, `logic_scene_list_manager`, `scene_manager`, `scripted_scene`, `sky_camera`, `weapon_basecsgrenade` hoặc `weapon_hunter_claw` (con trỏ suy ra không được chặn biên) |
| `v2.0.2` | `noedict.txt` chỉ được đọc 32 dòng đầu, phần sau bị bỏ không báo |
