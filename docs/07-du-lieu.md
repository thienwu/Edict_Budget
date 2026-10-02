# Dữ liệu phân loại lớp

File `data/phanloai_entity_l4d2.json`: toàn bộ 557 tên lớp đăng ký trong `server.dll`.

| trường | nghĩa |
|---|---|
| `ten` | tên lớp |
| `vt` | địa chỉ vtable (RVA + `0x10000000`) |
| `dk1` | `UNGVIEN` (229) = qua điều kiện 1 · `CAM` (320) = có SendTable riêng · `?` (8) = không đọc được |
| `pripyat03` | số entity của lớp đó trên `ch04_pripyat03` |

Lưu ý:

- `UNGVIEN` chỉ có nghĩa là **qua điều kiện 1**. Phần lớn số đó trượt các điều kiện khác. Không
  dùng cột này làm danh sách để thêm vào `noedict.txt`.
- `vt` dùng để biết lớp nào **dùng chung vtable** (bật một là cắt tất cả).
- Số đếm trên bản đồ của các lớp đang dùng nằm trong chính `noedict.txt` (mục BẢN ĐỒ ĐỂ KIỂM
  CHỨNG), đã tính cả bản ghi đè `update/maps/*.lmp` và bản đồ workshop.
