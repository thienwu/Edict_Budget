# Hướng đã bỏ

Các công tắc dưới đây vẫn còn trong mã để đối chiếu. Trong `patches.txt` chúng phải là `0`.

---

## Nâng trần lên 4096

Đây là hướng **đầu tiên** của dự án. Nó không giữ được ổn định, và dự án chuyển sang cắt giảm
(`noedict`, `swap`, `wipeclear`).

Công tắc: `bigarray`, `snapshot`, `pinmax`, `pinglobals`, `markfree`, `freetime`,
`indexbounds`, `forcedindex`, `detour`.

Nâng số edict của engine lên 4096 thì **làm được** (`bigarray` sửa hằng `0x800` trong
`SV_AllocateEdicts`). Nhưng nó không giải quyết bài toán:

- Chỉ số entity trong gói tin chỉ có 11 bit (0–2047). Entity có gửi về client mà nằm ở chỉ số
  ≥ 2048 thì client giải mã sai. Không sửa được từ phía máy chủ.
- Chỗ trống ở dải 2048–4095 chỉ dùng được cho entity không gửi về client — và engine đã có sẵn
  đường cho việc đó: `EFL_SERVER_ONLY`, chính là `noedict`.

Đo được: bật `bigarray` + `snapshot` mà thiếu `pinmax` / `pinglobals` thì `num_edicts` lên 2060,
entity có mạng tràn lên trên 2047. Nhóm này còn làm hỏng vòng hồi sinh lúc thua (phá
`wipeclear`).

Câu tự kiểm khi sửa mã: *"Việc này có cần `bigarray` không?"* — có thì dừng.

---

## nonetkill — đổi tên lớp trong entity lump

Công tắc `nonetkill`, danh sách `nonetkill.txt` (mặc định không có file = không làm gì).

Ở `LevelInit`, đổi ký tự đầu của tên lớp trong entity lump thành `~` (`infodecal` →
`~nfodecal`). Engine không biết lớp đó nên **không tạo** entity — không tốn edict.

Vì sao không dùng: entity không được tạo thì `Spawn()` / `Activate()` cũng không chạy. Với
`infodecal` và họ `light`, toàn bộ việc của chúng nằm ở đó (dán decal, đặt trạng thái đèn).
Thử thật trên `ch04_pripyat03`: ánh sáng sai. Decal cũng không được dán, vì
`CDecal::Activate` (nơi gọi `StaticDecal`) không chạy. `noedict` làm được cùng việc mà không
mất gì, vì entity vẫn được tạo.

Còn giữ trong mã vì có thể có ích với lớp không có tác dụng gì lúc spawn. Nếu dùng:

- chỉ thêm lớp mà `Spawn()` / `Activate()` không làm gì cần giữ;
- không bao giờ xoá khối hay đổi độ dài chuỗi trong lump — khối không mở bằng `{` hoặc thiếu
  khoá `classname` làm engine gọi `Error` và dừng máy chủ. Plugin chỉ đổi một ký tự nên độ dài
  giữ nguyên.

`nonethigh` (đẩy entity lên dải cao thay vì không tạo) chỉ chạy khi có `bigarray` + `snapshot`,
tức thuộc nhóm 4096 ở trên. Đã gây sập khi thử.

---

## reuse

Bản đầu của việc tái dùng edict ngay (gọi `AllowImmediateEdictReuse`). Đã thay bằng `freegate`,
mà `freegate` cũng gần như không có tác dụng. Giữ `0`.

---

## mapclear = 2

Dọn entity lúc chuyển màn. Xoá entity được mang sang màn sau thì máy chủ sập; ở mức an toàn thì
chỉ xoá được vài cái. Giữ `mapclear=1` (chỉ ghi log) và `mapclearcarry=0`.

---

## Không đụng tới họ `phys` và `prop_physics`

- `phys_bone_follower` là phần va chạm của model. Bỏ nó thì người chơi đi xuyên qua vật. Tài
  liệu Valve (khoá `DisableBoneFollowers`) ghi rõ: bỏ bone follower để tiết kiệm edict thì mô hình
  va chạm không còn hoạt động.
- `prop_physics` / `prop_physics_multiplayer`: `client.dll` tự tạo một phần prop này từ entity
  lump của chính nó. Can thiệp phía máy chủ làm hai bên lệch nhau.

---

## Không có cách cho `env_sprite`

Lớp đông nhất trên các map tuỳ chỉnh đã đo (2539 cái trên `the_hive`, `anemoia`, `chernobyl`).

- Không cắt mạng được: có SendTable riêng (`CSprite`), client cần dữ liệu đó để vẽ.
- Không đổi lớp được: 1 dòng lump đã là 1 edict, không có lớp nào rẻ hơn mà vẫn vẽ được.

Chỉ người làm map giảm được.

---

## swap: chỉ có một cặp

Đã dò mọi lớp tự tạo thêm entity con lúc spawn. Chỉ `point_spotlight` có cặp thay thế
(`beam_spotlight`). Các lớp khác hoặc đã ở mức 1 edict, hoặc không có lớp tương đương.
