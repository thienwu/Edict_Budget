# CEF — đã gỡ khỏi kế hoạch

*Tiếng Việt (bản chính) · [English](04-cef.en.md)*

Ghi lại để không ai thêm lại nhầm. Chốt ngày **07/08**.

---

## Ý định ban đầu

Đưa CEF vào chính plugin này, vì CEF gốc (`mmscef-code`) **vốn là Metamod plugin** chứ
không phải SourceMod extension — nên nhìn qua thì tưởng chép sang là chạy.

## Quyết định

**Người dùng chốt: KHÔNG chép CEF vào đây.** Mã nguồn CEF gốc chỉ để **tham khảo**; nó
**không hỗ trợ đầy đủ L4D2** — cần thiết kế lại nếu muốn có cơ chế này.

## Lý do kỹ thuật

CEF gốc dùng `PEntityOfEntIndex` để tìm slot trống. Trên L4D2, Valve **đã bỏ hàm đó** khỏi
`IVEngineServer`, nên `engine_wrappers.h` thay bằng một phép tính con trỏ thuần —
**luôn khác `NULL`** ⇒ vòng lặp chạy tới `maxEntities` rồi thoát.

> Tức là **CEF gốc là một no-op trên L4D2**. Nó "ổn định" vì nó **không làm gì cả**.
> Chép nguyên xi sang đây là chép một thứ không chạy.

## Nếu về sau vẫn cần cơ chế này

Phải **thiết kế lại** cho L4D2:

1. Dùng `edict_t::IsFree()` thật, **không** dùng `PEntityOfEntIndex`.
2. **Đo trước.** Hiện chưa có số liệu nào cho thấy có đọng nguy hiểm lúc chơi thường.
   Đo được 07/08: slot cao nhất từng dùng = **682/2048**, luôn dư **~950 chỗ**. Mọi đợt
   bùng đo được đều nằm ở **nhánh wipe**, và `wipeclear` đã xử lý.
3. Nhớ rủi ro đã ghi: **tác giả CEF tự cảnh báo** *"PROBABLY UNSTABLE... random crashing"*,
   và crash `sourcemod+0x13b63` xuất hiện **đúng khi ép chỉ số**.

---

## Cập nhật — CEF ≡ `freegate`

Phần việc thực sự có giá trị của CEF — **cho phép tái dùng slot vừa được giải phóng** —
đã có trong plugin dưới tên `freegate` (một byte tại `engine.dll` RVA `0x1E022A`).
Xem [01-co-che.md](01-co-che.md).

Và câu hỏi mở lớn nhất từng dùng để biện minh cho CEF — *"entity có tích tụ trong lúc chơi
không?"* — **đã đo được là KHÔNG**: 7 phiên chơi dài, cả 7 đều kết thúc **thấp hơn** lúc bắt
đầu (trung bình −114); đỉnh 1375/2048; **0 lần `ED_ALLOC`** trong 105 phiên.

⇒ **Hạng mục này đóng.** Xem [07-het-huong.md](07-het-huong.md) mục 7.
