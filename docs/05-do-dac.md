# Đo đạc — log, kiểm kê, bẫy

*Tiếng Việt (bản chính) · [English](05-do-dac.en.md)*

Các cơ chế **chỉ để quan sát**. Chúng không đụng vào entity nào.

---

## 1. Ghi log ra file riêng

`META_LOG` chỉ đẩy ra console của máy chủ. Nếu máy chủ không bật ghi `console.log` thì
**mọi số liệu đo được đều mất**. Nên toàn bộ log của plugin ghi thẳng vào file riêng:

```
left4dead2/addons/edictbudget/edictbudget.log
```

Mỗi dòng có **dấu thời gian**. File mở ở chế độ **nối tiếp**, không ghi đè.

`fflush` sau **mỗi dòng**, để nếu máy chủ chết đột ngột vẫn còn đủ log đến phút cuối —
**đúng lúc cần nhất**.

**Công tắc:** `logconsole = 1` thì in ra **cả** console (mặc định `0`).

---

## 2. Đếm số lần cấp phát trong MỘT frame

Mẫu cuối cùng trước khi chết luôn là `num_edicts=2012` với **~904 slot trống** — trạng
thái mà `ED_Alloc` **không thể** báo lỗi (2012 < 2048 nên nó cấp mới).

Để tới được nhánh lỗi thì trong khoảng giữa hai lần lấy mẫu (< 0,25 s) phải có **~940 lần
cấp phát**: 36 lần đẩy `num_edicts` lên 2048, cộng 904 lần chiếm hết slot trống.

> Nếu đúng, **wipe là một đợt bùng nổ ~940 entity CÙNG LÚC** và map thật sự vượt 2048 ở
> đỉnh — lúc đó **mọi hướng "tái sử dụng slot" đều vô nghĩa** vì không còn gì để tái sử dụng.

---

## 3. `loadprobe` — ghi số edict trong N frame đầu

Ghi số edict trong **N frame đầu** sau khi nạp map, để bắt **đỉnh tạm thời**. `0` = tắt.

### Vì sao cần

**Mốc cơ sở** ghi tại `ServerActivate`, lúc đó nhiều entity **chưa spawn xong**.

Ví dụ: `point_spotlight` tạo `spotlight_end` + `beam` trong `Activate()`/`Think()`, tức là
**SAU** `ServerActivate`. Đó là lý do `m4` ghi `num_edicts=1463` lúc đó trong khi đếm từ
lump ra **2067** — phần chênh xuất hiện ở **mấy frame kế tiếp**.

Ngoài ra ~35 lớp `weapon_*_spawn` tạo entity thật rồi `UTIL_Remove` chính nó; `UTIL_Remove`
**hoãn đến cuối frame** nên mỗi cái chiếm **2 edict cùng lúc** trong frame nạp.

Cả hai giả thuyết **chỉ kiểm được bằng cách lấy mẫu TỪNG FRAME**.

---

## 4. Kiểm kê mọi lớp map tạo ra

Kiểm kê **mọi** lớp mà map tạo ra, dù ta có đụng chạm nó hay không.

> Chọn danh sách cho phép **bằng trực giác thì KHÔNG ăn thua**: tập thận trọng
> `logic_`/`math_`/`ai_` chỉ giải phóng được **10 ô** trên `c1m1_hotel`, vì L4D2 đặt phần
> lớn logic của map trong **VScript** chứ không phải trong entity.

Muốn chọn có ích thì phải biết map **thực sự sinh ra những gì** và **bao nhiêu cái**, sắp
theo số lượng, để những nhóm phía máy chủ lớn nhất lộ ra ngay.

---

## 5. `trap` — bẫy tại chính nhánh lỗi của `ED_Alloc`

Mọi phép đo đặt tại `IVEngineServer::CreateEdict` đều **mù**: bộ đếm burst không thấy frame
nào có ≥ 32 lần cấp phát, và hook **chưa bao giờ được gọi** cho lần thất bại. Nghĩa là
`ED_Alloc` được gọi từ **đường nội bộ** của engine.

Chỗ duy nhất còn nhìn được là **chính nhánh lỗi**:

```asm
1E0247  85 DB              test ebx, ebx
1E0249  0F 88 84 00 00 00  js   1E02D3      ; -> báo "no free edicts"
1E024F  ...                                 ; -> tái sử dụng ebx
```

Tám byte đó được thay bằng một `JMP` 5 byte tới stub của ta + 3 `NOP`. Stub ghi log rồi
dựng lại đúng hai nhánh gốc.

> Đây là **đường lạnh** — chỉ chạy khi engine sắp chết — nên rủi ro **thấp hơn hẳn**
> detour trên đường nóng.

`ebx` = **chỉ số edict trống CUỐI CÙNG** mà vòng quét nhìn thấy (`-1` = không thấy cái nào).
Đó chính là con số cần biết: engine có **thật sự** không thấy slot trống nào không, trong
khi ta đếm được ~912.

---

## 6. Kiểm kê tại thời điểm hết edict

Kiểm kê **tại đúng thời điểm này**: cái gì đang chiếm 2048 slot?

> Mọi lần kiểm kê trước đây đều đếm **lúc bình yên** và cho ra bức tranh **khác hẳn** —
> chính nó làm cả hai ngày đi sai hướng. Đây là thời điểm **duy nhất có nghĩa**: engine
> vừa xác nhận **không còn một slot trống nào**.

Lần trước bảng này **không in ra được**: tiêu đề được log **sau** vòng lặp, và vòng lặp gọi
hàm ảo `GetClassName()` trên 2048 edict trong lúc engine đang hấp hối nên chạm phải con trỏ
hỏng và chết trước khi kịp in.

Nay đã sửa:
- in **tiêu đề TRƯỚC**
- bọc **SEH** quanh mỗi lần đọc một edict
- in **từng dòng ngay** khi gom xong, không đợi tới cuối

---

## 7. Mâu thuẫn giữa số đo và mã máy

Phép đo trước đó cho ra một kết quả **mâu thuẫn** với mã máy: `num_edicts=2048` với **880
edict** mang cờ `FL_EDICT_FREE`, mà engine vẫn báo *"no free edicts"*.

`ED_Alloc` ghi nhớ **mọi** edict trống nó đi ngang qua (`mov ebx,esi` tại `0x1E0209`) và chỉ
báo lỗi khi `ebx` vẫn còn `-1`; nên với 880 ô trống thì nhánh đó **không thể tới được**.

**Chỉ có MỘT cách để cả hai sự thật cùng đúng: vòng quét KHÔNG HỀ CHẠY.**

Nó bắt đầu từ:
```asm
esi = sv.GetMaxClients() + 1
```
và lệnh tại `0x1E01E8` **nhảy qua cả vòng lặp** khi `esi >= num_edicts`.

> ⇒ Con số quan trọng **KHÔNG PHẢI** là có bao nhiêu ô trống — mà là có bao nhiêu ô trống
> **NẰM TRONG CỬA SỔ** mà engine thực sự nhìn vào.

Gọi chính `GetMaxClients` của engine (RVA `0x134640` trên đối tượng `sv`) để đọc **đúng cái
mà `ED_Alloc` đọc**, thay vì tin vào `gpGlobals`.

Hoá ra `sv.GetMaxClients()` chỉ là một getter một dòng:
```asm
mov eax, [ecx+0x104] ; ret
```
nên đọc thẳng trường đó — không gọi hàm, không rủi ro về quy ước gọi.

> ⚠️ **Đáng lưu ý:** L4DToolZ ghi vào `sv[+0x180]` (`slots_idx 0x60` của nó), một trường
> **khác hẳn**. Hai trường này có đồng ý với nhau không, chính là câu đang hỏi.

---

## 8. Bắt đúng khoảnh khắc `ED_Alloc` bỏ cuộc

Lấy mẫu từ một hook **có tiết chế** thì không bao giờ bắt được cú hỏng: mọi lần lấy mẫu đều
thấy `num_edicts=2012` (dưới trần 2048) với **861 edict trống** nằm trong cửa sổ quét — một
trạng thái mà `ED_Alloc` **chứng minh được** là không thể thất bại.

> Đợt bùng nổ của wipe xảy ra **gọn trong một frame**, tức là **giữa hai lần lấy mẫu**.

Một **POST hook** trên `CreateEdict` nhìn thấy đúng **một** thứ quan trọng: chính lời gọi đã
trả về `NULL`. Ghi lại **toàn bộ** trạng thái ngay tại đó, **không tiết chế**.

---

## 9. `heartbeat` — ghi định kỳ số liệu

Máy chủ chính thức chạy dài ngày cho nhiều dữ liệu hơn test cục bộ. `trap=1` chỉ đo bằng
kiểm kê **lúc chết** — tức chỉ biết **kết quả**, không biết **diễn biến**.

Heartbeat cho biết **lớp nào TĂNG DẦN theo thời gian** — thứ cần để thiết kế cơ chế thu hồi
entity trong lúc chơi.

> **CHỈ GHI LOG.** Không động vào entity nào.

Mỗi lần chạm nhịp:
- một dòng tổng hợp: `sống / num_edicts / trống / biên độ`
- các lớp **CÓ THAY ĐỔI** so với lần trước, sắp theo mức tăng giảm dần
  *(chỉ in thay đổi, không in cả bảng ⇒ log không phình)*

**Công tắc:** `heartbeat` = **số GIÂY** giữa hai lần ghi. `0` = tắt. Khuyến nghị **300**
(5 phút).

---

## 10. Kết quả đã thu được

Heartbeat + kiểm kê đã trả lời được câu hỏi mở lớn nhất của dự án:

> *"Entity có tích tụ trong lúc chơi không?"* — 🟢 **KHÔNG.**

- Nhật ký máy chủ thật qua **7 phiên chơi dài**: cả 7 đều kết thúc **thấp hơn** lúc bắt đầu
  (trung bình **−114**)
- Đỉnh entity sống: **1375/2048**
- **0 lần `ED_ALLOC`** trong **105 phiên**

Xem [07-het-huong.md](07-het-huong.md) mục 7.
