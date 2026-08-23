# docs/

Ghi chép chi tiết tách ra từ `src/sample_mm.cpp`, để mã nguồn chỉ giữ chú thích ngắn kèm
con trỏ tới đây.

Mọi kết luận đều kèm **địa chỉ hàm** và **đoạn lệnh** để kiểm lại được trên bản
`server.dll` / `engine.dll` của chính bạn. Cái gì không xác minh được thì ghi thẳng là
**KHÔNG XÁC ĐỊNH**, không đoán.

| chủ đề | Tiếng Việt | English |
|---|---|---|
| **Tổng quan** — nhiệm vụ, giới hạn, số liệu đo trên 3 chiến dịch, file cấu hình, build | [00-tong-quan.md](00-tong-quan.md) | [00-tong-quan.en.md](00-tong-quan.en.md) |
| **Bốn cơ chế đang chạy** — `noedict`, `freegate`, `wipeclear`, `swap` | [01-co-che.md](01-co-che.md) | [01-co-che.en.md](01-co-che.en.md) |
| **`mapclear`** — và vì sao không bao giờ được xoá cái mang sang màn | [02-mapclear.md](02-mapclear.md) | [02-mapclear.en.md](02-mapclear.en.md) |
| **Hướng 4096** — toàn bộ nhóm công tắc, **đã tắt** | [03-huong-4096.md](03-huong-4096.md) | [03-huong-4096.en.md](03-huong-4096.en.md) |
| **`nonetkill`** — đổi tên classname trong lump, **đã loại bỏ** | [04-nonetkill.md](04-nonetkill.md) | [04-nonetkill.en.md](04-nonetkill.en.md) |
| **CEF** — đã gỡ khỏi kế hoạch | [04-cef.md](04-cef.md) | [04-cef.en.md](04-cef.en.md) |
| **Đo đạc** — log, kiểm kê, bẫy, `heartbeat`, `loadprobe` | [05-do-dac.md](05-do-dac.md) | [05-do-dac.en.md](05-do-dac.en.md) |
| 🔑 **Địa chỉ dịch ngược đã xác minh** — bảng tra cho từng tính năng | [06-dia-chi.md](06-dia-chi.md) | [06-dia-chi.en.md](06-dia-chi.en.md) |
| 🛑 **Hết hướng** — vì sao không còn gì để cắt nữa | [07-het-huong.md](07-het-huong.md) | [07-het-huong.en.md](07-het-huong.en.md) |

## Ghi chú

**Bản tiếng Việt là bản gốc** và được giữ cập nhật nhất. Chỗ nào bản dịch nói khác bản tiếng
Việt thì tin bản tiếng Việt.

**Địa chỉ và mã máy giữ nguyên trong khối `code`** — bảng ASCII và đoạn lệnh assembly chỉ
thẳng hàng ở font đơn cách, và giữ nguyên văn nghĩa là tài liệu không trôi khỏi thứ mà mã
nguồn thực sự nói.

*(Trước bản 23/08/2026 các tài liệu này viết tiếng Việt **không dấu** để khớp với chú thích
trong mã nguồn. Nay đã chuyển sang **có dấu** cho dễ đọc; nội dung kỹ thuật giữ nguyên.)*

## Thứ tự đọc

Bắt đầu từ **`00-tong-quan`** — nó chứa tuyên bố nhiệm vụ, cái giới hạn cứng mà không bản vá
nào gỡ được, và các con số đã đo.

Rồi **`01-co-che`** để biết plugin thực sự làm gì.

**`06-dia-chi`** là thứ đáng đọc nhất nếu bạn muốn **dùng lại** hoặc **kiểm lại** công trình
này: toàn bộ RVA, số hiệu vtable slot, chuỗi neo, và cách suy lại tất cả sau khi Valve cập
nhật game.

**`02-mapclear`** đáng đọc **ngay cả khi bạn không bao giờ bật `mapclear`**, vì nó chứa bài
học đắt nhất của dự án: **xoá một entity mang sang màn ở chuyển màn thì máy chủ sập**, và
quy tắc *"xoá ít đi"* mà ai cũng nghĩ ra đầu tiên là **quy tắc sai**.

**`03-huong-4096`** là lịch sử: nhóm công tắc đó đã tắt và phải giữ tắt. Nó được ghi lại để
không ai suy lại rồi bật lại.

**`04-nonetkill`** và **`04-cef`** là những hướng đã bác bỏ, giữ lại vì cùng lý do đó.

**`07-het-huong`** là kết luận: mọi hướng đã tìm, đã đo, đã bác bỏ — và **những gì còn chưa
chắc chắn** về ba lớp mới thêm gần đây.
