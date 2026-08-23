# edictbudget

<https://github.com/thienwu/Edict_Budget>

*Tiếng Việt (bản chính) · [English](README.en.md)*

Plugin Metamod:Source cho **Left 4 Dead 2 dedicated server**, ngăn server chết vì:

```
Engine Error
ED_Alloc: no free edicts
```

Không cần SourceMod. Không mở rộng giới hạn entity của engine.

---

## 📌 Trạng thái — 23/08/2026: **đã đạt giới hạn**

Dự án tuyên bố dừng việc tìm thêm chỗ cắt. Bốn cơ chế đang chạy đã lấy **hết những gì lấy
được mà không phải trả giá bằng thứ người chơi nhìn thấy hoặc chạm vào**.

Ba mặt trận đã đóng, có bằng chứng:

| hướng | kết quả |
|---|---|
| `noedict` | trạng thái chỉ sống sót ngoài entity qua **đúng ba cửa** (`LightStyle`, `StaticDecal`, `EmitAmbientSound`). Hai cửa đầu **đã dùng**, cửa ba **bị cấm** vì entity tự nhét `entindex` vào gói tin. **Không có cửa thứ tư.** |
| `swap` | quét **108 × 549** cặp lớp ⇒ **đúng một cặp** dùng được |
| `nonetkill` / `killent` | **tập rỗng** (chứng minh bằng phản chứng), và `killent` **bị bác bỏ** vì bộ điều kiện chưa bao giờ hỏi entity **có va chạm không** — xem dưới |

Cái lớn nhất còn nhìn thấy được — `phys_bone_follower` ≈ **587 edict** — **cấm vĩnh viễn**,
và chính tài liệu Valve chứng thực lý do.

📖 Hai tài liệu quan trọng nhất nếu bạn muốn **dùng lại** hoặc **kiểm lại** công trình này:

- 🔑 [**docs/06-dia-chi.md**](docs/06-dia-chi.md) — **toàn bộ địa chỉ dịch ngược đã xác
  minh** cho từng tính năng: RVA, số hiệu vtable slot, chuỗi neo, phiên bản game và md5
  của nhị phân tham chiếu, cùng **cách suy lại tất cả** sau khi Valve cập nhật.
- 🛑 [**docs/07-het-huong.md**](docs/07-het-huong.md) — mọi hướng đã tìm, đã đo, đã bác bỏ,
  và **những gì còn chưa chắc chắn**.

---

## ⚠️ Giới hạn — đọc trước khi cài

**Plugin này không ngăn được hoàn toàn `no free edicts`.**

Nó làm bốn việc: **thu hồi edict đúng lúc**, **cho phép tái dùng slot vừa giải phóng**, **không cấp edict cho những lớp thực sự không dùng mạng**, và **đổi lớp entity sang lớp rẻ hơn**. Nếu bản thân map cần nhiều hơn 2048 entity **có mạng** cùng lúc thì không cứu được.

### Nâng giới hạn thì làm được — nhưng nó không giải quyết vấn đề

Phải tách rõ hai chuyện thường bị gộp làm một:

| | |
|---|---|
| Nâng số edict lên **4096 hoặc cao hơn** | ✅ **làm được** — mã có sẵn trong repo, mặc định tắt |
| Đặt entity **có mạng** ở chỉ số ≥ 2048 | ❌ **không thể**, và sẽ không bao giờ làm được bằng cách vá server |

Lý do vế thứ hai: chỉ số entity được mã hoá trong gói tin bằng **trường 11 bit** (tối đa 2047). Đó là **định dạng gói tin**, nằm ở cả hai đầu dây. Server không có cách nào làm client hiểu được chỉ số 2048 — client sẽ giải mã ra một chỉ số khác hẳn. Muốn sửa thì phải sửa `client.dll` của từng người chơi.

⇒ Chỗ trống ở dải 2048-4095 **chỉ chứa được entity không có mạng**. Mà engine **đã có sẵn** cơ chế cho việc đó — `EFL_SERVER_ONLY` — không cần vá byte nào. Đó chính là cơ chế `noedict` dùng.

Nên nâng trần không sai về kỹ thuật, nó chỉ **không giải quyết được bài toán**. Và đo thực tế còn cho thấy nó có hại: bật `bigarray`+`snapshot` mà thiếu `pinmax`/`pinglobals` thì `num_edicts` leo lên **2060**, entity **ngẫu nhiên** tràn lên trên 2047 — mất ổn định ngay. Nhóm công tắc đó cũng **làm hỏng vòng hồi sinh lúc wipe**, tức phá luôn `wipeclear`.

Chi tiết từng byte ghi trong `src/sample_mm.cpp`, khối đầu file.

### Ví dụ một map suýt không cứu được — và cách nó được cứu

```
312 point_spotlight + 312 spotlight_end + 312 beam = 936 edict (45,7%)
chi rieng cho hieu ung anh sang. Ca ba lop deu PHAI co mang.
```

Đoạn này trong bản trước ghi *"cách chữa duy nhất là người làm map giảm bớt hiệu ứng"*.
**Không còn đúng nữa.**

`point_spotlight` khi spawn **tự tạo thêm** `spotlight_end` + `beam` — 312 dòng trong
lump hoá thành 936 edict. `beam_spotlight` làm cùng việc nhưng vẽ hoàn toàn phía client
⇒ **1 edict**. Cơ chế `swap` đổi lớp ngay lúc tạo:

```
song 1954 -> 1330   giam dung 624   cho tho 93 -> 718 slot
```

Client vẫn nhận entity, vẫn vẽ tia sáng. Cái giá duy nhất là quầng sáng to gấp 6 vì
`client.dll` ghi cứng `HaloScale = 60.0`.

**Nhưng đây là ngoại lệ, không phải quy luật.** Quét 557 lớp của `server.dll`, đối chiếu
với 16 map của ba chiến dịch đã đo, cho ra **đúng một cặp thay thế** như vậy. Lý do: hầu
hết lớp đã ở hệ số 1 sẵn, không còn gì để cắt.

`env_sprite` là ví dụ rõ nhất — lớp đông nhất đã đo được (**2539 cái**, riêng `the_hive`
đã **2280**) — mà **không có cặp thay thế nào**, vì nó đã ở hệ số 1 và xuống 0 là bất
khả thi.

Với những lớp đó thì câu cũ vẫn đúng: **cách chữa nằm ở người làm map.**

### Map nào thì cứu được

Loại nặng vì thứ **không cần gửi xuống client** — decal, đèn tĩnh, mốc toạ độ. Trên một map như vậy: từ chết ở 2048 edict xuống còn `num_edicts=1178`.

**Kết luận thẳng:** đây là công cụ nới biên độ, không phải công cụ nâng trần.

---

## ⚠️ Phạm vi đã kiểm chứng — đọc kỹ

Đây là chỗ dễ hiểu nhầm nhất. **Số liệu tĩnh trải rộng, nhưng số liệu chạy thật thì hẹp.**

### Đã chạy thật trên máy chủ

| map | đã đo gì |
|---|---|
| `the_hive_m3` | `noedict`, `swap`, `loadprobe`, `mapclear` quan sát |
| `the_hive_m4` | như trên, cộng bản kiểm kê lúc hết edict |
| `the_hive_m5` | `swap` có giới hạn |
| `c1m1_hotel` | dùng làm map đối chứng |
| `ch04_pripyat03` | `noedict` (lịch sử — map khởi nguồn của dự án) |

**Về cơ bản là một chiến dịch tuỳ chỉnh, cộng một map gốc làm đối chứng.**

### Chỉ phân tích tĩnh, CHƯA chạy thật

Đã đọc lump và tính chi phí edict bằng `tools/bsp_cost.py` cho **đúng ba chiến dịch**:

```
the_hive     5 map
anemoia      6 map   (thu muc "backroom")
chernobyl    5 map
------------------
            16 map
```

**Đó là đọc file, không phải chạy server.**

⚠️ **Con số "16 map" xuất hiện nhiều lần trong tài liệu này KHÔNG phải một mẫu rộng.**
Nó chính là ba chiến dịch trên, không hơn. Ba chiến dịch do cùng một kiểu tác giả
cộng đồng làm, nặng về trang trí. Kết luận rút ra từ đó **có thể không đúng** với map
gốc của Valve, map finale, map Versus, hay map của tác giả khác.

### Vì vậy

- **Chưa test trên nhiều map có độ phức tạp khác nhau.** Không có dữ liệu chạy thật
  cho map finale, map có sự kiện lớn, map Versus/Scavenge/Survival, map dùng VScript
  nặng, hay map của các tác giả khác.
- **Chưa test ở các chế độ chơi khác Coop.** `CleanUpMap()` chạy mỗi lần khởi động
  lại ván (Versus, Survival ván 2 trở đi) — đường đó chưa ai chạm tới.
- **Chưa test với đông người chơi.** Hầu hết phép đo có 1–4 người.
- Sai số công thức đo được là **2–6%, luôn thiên về cao**. Dùng làm cận trên và bảng
  xếp hạng, **không dùng làm phán quyết** map nào sẽ chết.
- **Ba lớp `noedict` thêm ngày 21–22/08 chưa nghiệm thu dài hạn.** Đã biết là chúng chạy
  được và không mất gì nhìn thấy được; **chưa** biết chúng hành xử ra sao trên nhiều chiến
  dịch có độ phức tạp khác nhau, hay sau nhiều tuần vận hành. `func_nav_blocker` để **tắt**
  và chưa chạy lần nào. Chi tiết: [docs/07-het-huong.md](docs/07-het-huong.md) mục 9.

Ai dùng plugin này nên bật `mapclear=1` và `heartbeat=300` (chỉ ghi log, không đụng
entity nào) chạy vài ngày trước, đọc log, rồi mới bật các cơ chế can thiệp.

## Vấn đề

Server L4D2 đông người chơi, map cộng đồng nặng, chơi được một lúc là chết. Log chỉ có đúng một dòng `ED_Alloc: no free edicts` rồi tiến trình biến mất.

Cách hiểu thông thường là *"map dùng quá nhiều entity, phải nâng trần 2048 lên 4096"*. Hướng đó **sai và nguy hiểm** — chỉ số entity trong giao thức Source rộng **11 bit** (tối đa 2047), nên entity có mạng nằm ở chỉ số ≥2048 thì client giải mã ra rác.

Plugin này đi hướng khác: **không nâng trần, mà giảm nhu cầu.**

---

## Chẩn đoán gốc

Điều làm dự án này mất hai ngày đầu là một hiểu nhầm:

> `num_edicts` **không phải** số entity đang dùng. Nó là **mốc nước cao** của bộ cấp phát, và **không bao giờ giảm**.

Số entity đang sống = `num_edicts` − (số ô mang cờ `FL_EDICT_FREE`).

Vì nhầm chỗ này, mọi phép đo trước đó đều đọc sai. Sau khi tách bạch hai đại lượng, ba nguyên nhân thật lộ ra — và mỗi cái cần một cách chữa riêng.

---

## Bốn cơ chế

### 1. `wipeclear` — dọn khi đội survivor thua

Khi cả đội gục/chết hết (*wipe*), engine **có** dọn map — bằng `CleanUpMap` — nhưng **dọn quá muộn**: vòng hồi sinh người chơi chạy trước và ăn hết số edict còn lại.

Plugin móc **vtable slot 178** của `CTerrorGameRules` (`RestartRound`) và chạy **đúng phần dọn của `CleanUpMap`, nhưng sớm hơn** — trước vòng hồi sinh. Dùng nguyên preserve list 38 lớp của game, không tự chế tập giữ.

Cổng mở một lần từ sự kiện `mission_lost`, đặt lại mỗi khi nạp map.

```
Do duoc tren c6m1_riverbank:
  WIPE #1 t= 52.63  -> go 1133, giu 205, giai phong 924 slot
  WIPE #2 t=102.93  -> go 1101, giu 199, giai phong 892 slot
  WIPE #3 t=149.33  -> go 1129, giu 229, giai phong 918 slot
  num_edicts: 2012 -> 2030 -> 2030, on dinh. Truoc do: chet ngay wipe dau tien.
```

> ⚠️ Tập giữ phải **tối thiểu**. Đã thử thêm tiền tố `weapon_` (giữ ~190 entity vũ khí) và kết quả là **crash sớm hơn hẳn**: 2041 sống/7 trống, thay vì 1042 sống/1006 trống. Ở wipe, entity bị xoá **được dựng lại** từ entity lump — giữ thêm chỉ làm hẹp biên độ.

### 2. `freegate` — cho phép tái dùng slot vừa giải phóng

Dọn xong vẫn chết. Bảng chẩn đoán cho con số vô lý:

```
*** ED_ALLOC SAP BAO LOI *** num_edicts=2048 | plugin dem duoc 999 slot trong
```

**Chết trong khi có 999 edict trống.** Nguyên nhân nằm trong chính vòng lặp `ED_Alloc` của `engine.dll`:

```asm
101E01F4  test cl, 1                        ; FL_EDICT_FREE ?
101E01F7  je   101E022C                     ; khong free -> bo qua
101E0201  comiss xmm0, [esi*4+0x106B3A58]   ; so 2.0f voi freetime[i]
101E0209  mov  ebx, esi                     ; ghi nho slot free vua thay
101E020B  ja   101E0295                     ; freetime < 2.0 -> LAY (dau map)
101E0216  call sv.GetTime()
101E021B  fsub [esi*4+0x106B3A58]           ; curtime - freetime
101E022A  jae  101E0295                     ; >= 1.0 GIAY -> LAY   <-- CHO
101E022C  ...                               ; chua du -> BO QUA
```

Engine **từ chối tái dùng** một edict trong **1 giây** sau khi nó được giải phóng. Mà wipe xoá rồi tạo lại hàng trăm entity trong **cùng một khoảnh khắc** — không cái nào qua nổi cổng đó.

Đây chính là lỗi engine Source 2009 mà tác giả CEF mô tả: *"running out of edicts when you have 1000 free"*.

`freegate` đổi **một byte** tại `0x101E022A`: `jae` → `jmp`. Đích nhảy giữ nguyên, không đổi độ dài lệnh. Định vị bằng **quét chữ ký** nên tự tìm lại được.

An toàn nhờ cơ chế sẵn có của Valve: `sv_useexplicitdelete` (mặc định bật) gửi lệnh xoá tường minh xuống client khi một chỉ số bị tái dùng sớm — đó là thứ Valve thiết kế **thay cho** thời gian chờ này.

```
Phep so co doi chung - cung tinh huong num_edicts=2048, ~999 slot trong:
  freegate=0 -> CHET ngay
  freegate=1 -> choi tiep binh thuong, num_edicts=2048 voi 946 slot duoc tai dung
```

### 3. `noedict` — khiến entity không dùng mạng **không lấy edict**

Map nặng chết **ngay lúc nạp**, chưa kịp chơi. Bảng kiểm kê `ch04_pripyat03`:

```
853 infodecal   <- 42% toan bo 2048 edict
215 func_brush
134 prop_physics_multiplayer
...
```

`infodecal` chỉ dán một vết decal lên tường rồi thôi. Nó **không cần** được gửi xuống client — `StaticDecal()` mang theo chỉ số **bề mặt bị dán**, không phải chỉ số của chính nó.

Engine đã có sẵn hạng công dân cho loại này. `CBaseEntity::PostConstructor` quyết định:

```asm
10055620:
  mov  eax, [esi+0x138]     ; m_iEFlags
  shr  edx, 9
  test dl, 1                ; bit 9 = EFL_SERVER_ONLY
  je   <nhanh CAP EDICT>    ; = 0 -> AddNetworkableEntity, dai 0-2047
  mov  ecx, gEntList
  call AddNonNetworkableEntity   ; = 1 -> dai 2049-4095, KHONG EDICT
```

Dải 2049-4095 (**2047 ô**) là **thiết kế gốc của engine**, không phải chỗ vá vào. Tràn nó chỉ in cảnh báo rồi trả handle không hợp lệ — **không giết server**, khác hẳn `ED_Alloc`.

Plugin thay **vtable slot 29** (`+0x74` = `PostConstructor`) của riêng `CLight` và `CDecal`, bật bit 9 rồi gọi hàm gốc. Không vá byte, không đụng `engine.dll`.

```
ch04_pripyat03:
  truoc: CHET o num_edicts=2048, 0 slot trong
  sau:   nap duoc, num_edicts=1178, du ~870 slot
  ~1041 entity moi lan nap duoc danh dau EFL_SERVER_ONLY
  Kiem bang mat: khong mat decal, anh sang dung.
```

#### Danh sách lớp hiện tại

Sáu lớp đang bật trong `noedict.txt`, cộng một lớp **để tắt**:

| lớp | số cái / 17 map | trạng thái |
|---|---|---|
| `infodecal` | 853 riêng `ch04_pripyat03` | 🟢 chạy từ đầu |
| `light`, `light_spot` | — | 🟢 chạy từ đầu |
| `path_track` | 25 trên `the_hive_m4` | 🟢 chạy từ đầu |
| `func_areaportal` | 179 | 🟢 chạy thật từ 21/08 |
| `info_zombie_spawn` | 86 | 🟢 chạy thật từ 22/08 |
| `func_nav_blocker` | 64 | ⏸️ **để tắt** — bỏ `#` để bật, và **phải test riêng một mình** |

> ⚠️ **Ba lớp cuối chưa nghiệm thu dài hạn.** Đã biết là chúng **chạy được** và **không mất
> gì nhìn thấy được**; **chưa** biết chúng hành xử ra sao trên nhiều chiến dịch có độ phức
> tạp khác nhau, hay sau nhiều tuần vận hành. `func_nav_blocker` có triệu chứng **không
> nhìn thấy được** — phải quan sát **hành vi AI**. Chi tiết ở
> [docs/07-het-huong.md](docs/07-het-huong.md) mục 9.

> 🛑 **Vá theo VTABLE, không theo CLASSNAME.** Có **20 nhóm** classname dùng chung một
> vtable — bật một tên có thể **kéo theo cả nhóm**. Và **8 lớp** hiện giải sai vtable, liệt
> kê ở [docs/06-dia-chi.md](docs/06-dia-chi.md). **Đừng thêm lớp mới** trước khi đọc đủ sáu
> điều kiện ghi trong chính `noedict.txt`.

---

### 4. `swap` — đổi lớp entity sang lớp rẻ hơn

Khác hẳn ba cơ chế trên: **không gỡ mạng, không xoá.** Client vẫn nhận entity và vẫn
vẽ bình thường — chỉ là đổi sang một lớp làm cùng việc nhưng tốn ít edict hơn.

`point_spotlight` khi spawn **tự tạo thêm** `spotlight_end` + `beam` ⇒ **3 edict** cho
mỗi dòng trong lump. `beam_spotlight` vẽ hoàn toàn phía client, không sinh entity con
⇒ **1 edict**. Cả hai đều là lớp gốc của L4D2, và Valve dùng cả hai trong map chính
thức của họ (tối đa 77 và 94 cái).

Móc `CEntityFactoryDictionary::Create` — vtable slot 1 của dictionary lấy từ hàm
`0x1020CA70` — rồi thay chuỗi classname trước khi gọi hàm gốc. Quét cả `.text`: 562 chỗ
gọi hàm đó, **chỉ đúng 3 chỗ dùng slot 1** (`CreateEntityByName` + 2 nhánh của bộ phân
tích lump BSP). Vá **một con trỏ vtable** phủ cả lúc nạp map lẫn lúc chơi.

```
Do duoc, khop tuyet doi voi du doan:
  the_hive_m4   song 1954 -> 1330   (312 den, giam 624 = 312 x 2)
                cho tho 93 -> 718 slot
  the_hive_m3   song 1591 -> 1431   (80 den,  giam 160)
```

**Bảng đổi ở `swap.txt`**, một dòng một cặp, thêm cặp mới không cần build lại.

> ⚠️ Cái giá: `client.dll` ghi cứng kích thước halo = 60.0, còn map thường đặt
> `HaloScale 10` — nên **quầng sáng to gấp 6**. Không mất tia sáng, chỉ to hơn.

> **Bảng này đã quét hết phần quét được.** 557 lớp của `server.dll`, đối chiếu với 16
> map của ba chiến dịch đã đo, cho ra **đúng một cặp dùng được**. Lý do: hầu hết lớp đã
> ở hệ số 1 sẵn, không còn gì để cắt.
>
> `env_sprite` — **2539 cái**, đông nhất trong ba chiến dịch đó — không có cặp thay thế: nó đã ở
> hệ số 1, và xuống 0 là bất khả thi, vì muốn tốn **0 edict phía máy chủ mà vẫn được
> vẽ** thì client phải tự dựng entity từ lump của nó, mà `client.dll` chỉ làm việc đó
> cho **đúng hai lớp** `prop_physics` / `prop_physics_multiplayer`.

## Cài đặt

Xem `configs/`. Chép `addons/` vào `left4dead2/`, khởi động lại, kiểm `meta list`.

Mọi công tắc nằm trong `patches.txt` — **sửa file rồi khởi động lại là đủ, không cần build lại**.

### Chạy quan sát trước (khuyến nghị)

Máy chủ của bạn có map khác, số người chơi khác, chế độ chơi khác. Nên chạy vài ngày
ở mức ít can thiệp nhất trước:

```
noedict=1     manh nhat, mat mat bang 0 - bat ngay tu dau
freegate=1    doi 1 byte engine, da do doi chung
wipeclear=2   da do qua 5 lan wipe lien tiep
trap=1        chi ghi log khi sap chet
mapclear=1    CHI QUAN SAT, khong xoa gi
heartbeat=300 ghi so lieu moi 5 phut
swap=0        TAT luc dau
```

Đọc `edictbudget.log` vài ngày, xem số entity thật của map mình, rồi mới bật `swap=2`.

### Toàn bộ công tắc

| công tắc | mặc định | nghĩa |
|---|---|---|
| `noedict` | 1 | đặt `EFL_SERVER_ONLY` cho lớp trong `noedict.txt` ⇒ không tốn edict |
| `freegate` | 1 | bỏ thời gian chờ 1 giây trước khi tái dùng edict |
| `wipeclear` | 2 | dọn entity lúc đội thua. `0` tắt · `1` quan sát · `2` dọn thật |
| `swap` | 2 | đổi lớp theo `swap.txt`. `0` tắt · `1` quan sát · `2` đổi thật |
| `swapmax` | 0 | trần số lần đổi. `0` = không giới hạn |
| `mapclear` | 1 | chuyển màn. **Để `1`** — mức `2` chưa chứng minh được lợi ích |
| `mapclearcarry` | 0 | **để `0`**. Bật `1` = xoá entity mang sang = **sập** |
| `mapclearmax` | 100 | trần số lượt xoá của `mapclear=2` |
| `trap` | 1 | in bản kiểm kê đầy đủ khi sắp hết edict |
| `heartbeat` | 300 | giây giữa hai lần ghi số liệu. `0` tắt |
| `loadprobe` | 8 | số frame lấy mẫu sau khi nạp map |
| `logconsole` | 0 | `1` = in cả ra console máy chủ |
| `stage.txt` | 1 | `0` = plugin nạp nhưng **nằm im hoàn toàn** |

Nhóm `bigarray` / `snapshot` / `pinmax` / `pinglobals` / `markfree` **để nguyên `0`** —
xem mục "Nâng trần lên 4096" bên dưới.

---

## Nguyên tắc thiết kế

**Hỏng thì tự tắt, không hỏng bừa.** Trước khi móc vào game, plugin kiểm:

- chữ ký prologue của hàm đích
- slot vtable có đang trỏ đúng hàm đó không (bắt được trường hợp plugin khác đã móc)
- preserve list đọc ra có đúng không (`[0]=="ai_network"`, `[33]=="predicted_viewmodel"`)

Bất kỳ kiểm tra nào thất bại thì tính năng đó **tự tắt và ghi log lý do**. Server vẫn chạy.

**Mỗi công tắc độc lập.** Tắt một cái không ảnh hưởng cái khác.

**Chế độ quan sát.** `wipeclear=1` và `mapclear=1` chỉ đếm và ghi log, không xoá gì — dùng để lấy số trước khi cho phép can thiệp.

---

## Những hướng đã thử và loại bỏ

Phần này có lẽ hữu ích hơn phần trên, vì nó tiết kiệm thời gian cho người đi sau.

### Nâng trần lên 4096 — **có hại**

Vá `SV_AllocateEdicts` cấp 4096 edict. Kết quả: `num_edicts` leo lên **2060**, entity **ngẫu nhiên** (có mạng) trèo lên chỉ số >2047, client giải mã sai. Và nhóm công tắc này **làm hỏng vòng hồi sinh lúc wipe** — tức phá luôn thứ đang hoạt động.

Chỉ số entity trong giao thức là **11 bit**. Không vá được bằng cách nâng trần.

### `nonetkill` — đổi tên classname trong entity lump

Entity có classname engine không nhận sẽ **không được sinh ra**, không tốn gì. Nghe hợp lý.

Nhưng nó **giết luôn `Spawn()`/`Activate()`** — và với `infodecal`/`light` thì **toàn bộ giá trị nằm ở tác dụng phụ lúc spawn**: dán decal, đặt lightstyle. Kết quả: **mất decal, sai ánh sáng**.

Khác biệt sinh tử với `noedict`: `noedict` vẫn tạo entity, vẫn chạy `Spawn()`/`Activate()`, chỉ không cấp edict.

### `killent` — xoá hẳn entity khỏi map: **hướng lớn nhất, và đã bác bỏ**

Đây là hướng **đo được nhiều nhất** của cả dự án: **6200 edict** trên 16 map, riêng
`the_hive_m4` **1227** — trong khi biên độ sống/chết của chính map đó chỉ **122**.

Cơ chế đã dịch ngược xong: trả `false` ở `IMapEntityFilter::ShouldCreateEntity` (**vtable
slot 0**) của cả ba bộ lọc. Entity **không tồn tại**. Kèm một bộ điều kiện tự động năm
tầng để chọn cái nào được xoá.

**Lỗ hổng chí mạng: bộ điều kiện chưa bao giờ hỏi entity có VA CHẠM hay không.**

Tài liệu Valve, [`prop_dynamic`](https://developer.valvesoftware.com/wiki/Prop_dynamic):

> **Collisions (solid)**: `0` Not solid · `2` Use bounding box · **`6` Use VPhysics (default)**

Một `prop_dynamic` **không ghi khoá `solid`** vẫn là **vật đặc**.

Đo lại trên **60 file BSP gốc của Valve**: **809 `prop_dynamic` ĐẶC** lọt qua cả năm điều
kiện — riêng `left4dead2/maps` là **472/602 = 78%**. Trong đó có **30 lan can cầu**
(`bridge_rail`), **12 tường hầm mộ** (`crypts_wall`), **89 cổng**, **39 rào chắn bê
tông/gỗ**. Xoá 30 lan can cầu ⇒ người chơi **rơi khỏi cầu**.

Ba xác nhận từ bên ngoài:

1. **Valve nói thẳng rằng cắt edict và mất va chạm là cùng một việc.** Khoá
   `DisableBoneFollowers`: *"`phys_bone_followers` **can quickly eat up the edict count**...
   **This will however make the collision model no longer function**."*
2. **SourceMod đã GỠ BỎ** cơ chế sửa lump ở `LevelInit`
   ([PR #1534](https://github.com/alliedmodders/sourcemod/pull/1534)) — *"some maps have
   over 16MB of entity data"*.
3. **15 năm cộng đồng dùng Stripper:Source chưa bao giờ xoá theo LỚP** — họ xoá từng cá
   thể theo `hammerid`, và luôn **sửa nav mesh kèm theo**. Dự án này bị cấm sửa BSP ⇒
   **không bao giờ bù được nav** ⇒ bot kẹt, Director tính sai đường.

Điều kiện tối thiểu nếu ai muốn làm tiếp — cùng phần còn lại đo được (**~4450** thay vì
6200) — ghi ở [docs/07-het-huong.md](docs/07-het-huong.md) mục 4.

### `mapclear` — dọn entity lúc chuyển màn

Đo được **385 entity** đi qua cửa phòng an toàn sang map mới. Nhưng mọi nỗ lực dọn ở điểm móc đó đều **chết câm** (không `ED_Alloc`, không dump):

| Xoá | Nhóm mang sang | Kết quả |
|---|---|---|
| 1497 | bị xoá lẫn | chết |
| 1382 | được bảo vệ đủ | chết |
| **100** | được bảo vệ | **sống** |

Hai lần đầu không tách được "xoá sai thứ" với "xoá quá nhiều". Lần thứ ba cho thấy vấn đề là **số lượng**. Nhưng ở mức an toàn (100) thì chỉ 9 cái thực sự được mang sang — lợi ích không đáng. **Để tắt trong bản phát hành.**

### Gỡ mạng bộ ba đèn — **đóng**, nhưng bài toán được giải bằng đường khác

Ý định ban đầu: đưa `point_spotlight` / `spotlight_end` / `beam` vào `noedict.txt` để
lấy lại 936 edict. **Không làm được**, và có bằng chứng mã máy:

| lớp | hỏng điều kiện | bằng chứng |
|---|---|---|
| `spotlight_end` | 1 | `vtable[9]` → `0x1082377C` = `CSpotlightEnd`, **có SendTable riêng** |
| `beam` | 1 | `vtable[9]` → `0x107DAF94` = `CBeam`, **có SendTable riêng** |
| `point_spotlight` | 5 | `1018E5C9`: `spotlight_end->SetOwnerEntity(point_spotlight)`, mà `m_hOwnerEntity` (+0x20C) là **SendProp** của `DT_BaseEntity` |

Cơ chế đằng sau điều kiện 5, đọc từ `SendProxy_EHandleToInt` @`101CCFE0`:

```asm
and edx, 0xfff     ; chi so 12 bit
shl eax, 0xb       ; serial dich 11 bit
or  eax, edx       ; <- 12 bit nhet vao truong 11 bit => tran sang serial
```

Nhân tiện bác bỏ luôn một nỗi lo cũ: **`CBaseHandle` biểu diễn được chỉ số ≥ 2048**
(`NUM_ENT_ENTRY_BITS = 12`, mask `0xFFF`, tầm 0–4095). **Giới hạn 11 bit là của giao
thức mạng, không phải của handle phía máy chủ.** Đừng lẫn hai thứ.

**Nhưng 936 edict đó vẫn lấy lại được** — bằng `swap`, không phải bằng gỡ mạng. Đổi
`point_spotlight` (hệ số 3) sang `beam_spotlight` (hệ số 1) ngay lúc tạo. Xem mục
cơ chế 4.

### `env_sprite` — **đóng hẳn**, không có đường nào

Lớp đông nhất trong ba chiến dịch đã đo, **2539 cái**:

```
the_hive    2280   m3 730 | m2 639 | m5 439 | m1 236 | m4 236
anemoia      174
chernobyl     85
------------------
16 map      2539   <- day la TAT CA du lieu, khong phai mot mau rong
```

Valve dùng tối đa **162**, trung bình **30** — tức `the_hive_m3` một mình đã gấp **4,5
lần** mức cao nhất của Valve.

- **Gỡ mạng: không.** `vtable[9]` → `0x10823D14` = ServerClass riêng `CSprite`, có 11
  SendProp riêng (`m_flSpriteScale`, `m_nBrightness`, `m_flFrame`…). Client **cần**
  những dữ liệu đó để vẽ.
- **Đổi lớp: không.** Nó **đã ở hệ số 1** — 730 dòng trong lump ra đúng 730 edict lúc
  chạy. Muốn tốn **0 edict phía máy chủ mà vẫn được vẽ** thì client phải tự dựng entity
  từ lump của chính nó, mà `client.dll` chỉ làm việc đó cho **đúng hai lớp**
  `prop_physics` / `prop_physics_multiplayer`
  (`C_PhysPropClientside::ParseAllEntities` @`0x10176950`, đúng hai lệnh `strcmp`).
- `env_glow` là **bí danh tuyệt đối**: cùng vtable, cùng ctor, cùng datamap. Đổi không
  thay đổi một byte nào.

Còn lại duy nhất là xoá khỏi lump — mất hình. Chưa cài.

### `prop_physics` / `prop_physics_multiplayer` — **cấm xoá phía máy chủ**

`client.dll` tự dựng `C_PhysPropClientside` từ lump **của chính nó**, không nhận từ máy
chủ. `CPhysicsProp::Spawn` @`0x101A5F40` và `C_PhysPropClientside::Initialize`
@`0x10176410` là **bản soi gương của nhau**: mỗi prop do đúng **một** bên giữ, quyết
định bởi `m_iPhysicsMode` và cvar `sv_pushaway_clientside_size`.

Xoá phía máy chủ ⇒ mất phần bên đó phụ trách. **Không phải mỏ**: 1132/2983 prop do client
giữ, nhưng xoá chúng khỏi lump tiết kiệm **0 edict** — `DispatchSpawn` trả `-1` khi thấy
`EFL_KILLME` và `CleanupDeleteList()` chạy ngay trong vòng lặp.

---

## Bài học phương pháp

Dự án này mắc đủ loại sai lầm. Cái đắt nhất đều cùng một dạng: **dùng thẳng thông tin chưa xác minh**.

Thang ba bậc, mọi kết luận phải mang nhãn:

| Bậc | Nghĩa | Dùng để |
|---|---|---|
| 🟡 đọc được | từ wiki / SDK / suy luận | đặt giả thuyết |
| 🟠 xác minh binary | đã đọc mã trong binary **của chính L4D2**, có địa chỉ | thiết kế |
| 🟢 đo được | đã chạy server thật, có số trong log | kết luận |

Vài ví dụ về việc nhảy cóc từ 🟡 lên sản xuất:

- *"`mission_lost` chỉ bắn ở finale"* (suy từ chuỗi nằm cạnh nhau trong binary) → dọn 1155 entity lúc `t=1.00` ngay khi nạp map, **phá nát map**
- *"cắt `infodecal` tiết kiệm gần như bằng 0"* → đúng ở trạng thái ổn định, **sai ở lúc nạp map**, nơi cả 853 cái cùng tồn tại
- *"lớp có model thì không xoá được"* → sai; đúng phải là *"có được **gửi** xuống client không"* — model kèm `EF_NODRAW` vẫn an toàn
- `bspfile.h` của SDK khai `lump_t { fileofs, filelen, version, fourCC }` → BSP L4D2 thật là **v21** với thứ tự **đảo**: `{ version, fileofs, filelen, fourCC }`

Và một cái bẫy cụ thể đáng nhớ cho ai làm việc với binary Source:

> Prologue chứa **địa chỉ tuyệt đối** thì **không được so nguyên khối** — bốn byte đó bị trình nạp ghi lại khi module nạp ở base khác. Phải dùng mặt nạ.

---

## Xây dựng

```
build.bat
```

Cần đặt cạnh thư mục dự án:

```
hl2sdk-l4d2/
metamod-source-1.12.0.1225/
```

Biên dịch bằng MSVC 32-bit, `/O2`.

---

## Mục đích

Dự án được thực hiện nhằm **cải thiện việc xử lý thực thể và duy trì thực thể ổn định** trên máy chủ Left 4 Dead 2 — không phải để mở rộng hay lách giới hạn của engine.

Cụ thể là ba việc:

- **thu hồi** edict đúng lúc, thay vì để engine dọn muộn
- **cho phép tái dùng** slot vừa giải phóng, thay vì để nó nằm chờ vô ích
- **không cấp** edict cho những thực thể vốn không dùng tới mạng

Mọi kết luận trong tài liệu này đều kèm số đo từ máy chủ thật hoặc địa chỉ trong binary. Chỗ nào chưa xác minh được thì ghi rõ là chưa, và không được đưa vào bản chạy.

## Ai viết cái này

Dự án là kết quả của **hai phần việc khác nhau, không tách rời được**.

### Ý tưởng, bài toán và định hướng — **thienwu**, người vận hành máy chủ thật

Bài toán đến từ một sự cố thật trên máy chủ đang chạy, không phải từ một bài tập. Người
vận hành quyết định mọi hướng đi lớn — và quan trọng hơn, quyết định **những hướng KHÔNG
được đi**:

- **cấm hướng 4096**, kèm luật tự kiểm gọn một câu: *"Nó có cần `bigarray` không?"*
- **cấm động vào họ `phys` / `prop_physics`** — vì mất vật lý là đi xuyên qua vật thể
- **cấm sửa file BSP** — chỉ được **đọc** để đo
- yêu cầu **CÔNG THỨC CHUNG**: phải áp dụng cho **mọi map**, kể cả map chưa từng thấy, và
  plugin phải **tự kiểm lúc chạy**, không dùng danh sách viết tay
- **cảnh báo trước rằng hướng `killent` nguy hiểm** — dẫn thẳng tới việc tìm ra lỗ hổng
  **va chạm** mà bộ điều kiện tự động đã bỏ sót hoàn toàn

Và họ chạy thử, chụp log, đo trên máy chủ thật, rồi **bác bỏ nhiều kết luận sai của AI**.
Trong mã nguồn có nhiều đoạn ghi thẳng *"SAI, đã sửa"* — đó chính là dấu vết của những
lần bị bác bỏ đó.

### Dịch ngược, viết mã, đo đạc và tài liệu — **Claude (Anthropic)**, chạy trong Claude Code

Đọc ngược `server.dll` / `engine.dll` / `client.dll`, thiết kế và viết toàn bộ mã nguồn,
chạy các phép đo, và viết mọi tài liệu trong repo này.

### Vì sao nói rõ cách chia này

Vì hai lý do:

1. **Ai đọc mã nên biết nó đến từ đâu** để tự quyết định mức độ tin tưởng.
2. **Nhiều kết luận rút ra từ đọc ngược nhị phân, không phải từ tài liệu chính thức.**
   Chúng đều kèm địa chỉ hàm và đoạn lệnh để kiểm lại được. Cái gì không xác minh được
   thì ghi thẳng là *không xác định* thay vì đoán.

Cũng vì thế mà mục **"Phạm vi đã kiểm chứng"** ở đầu tài liệu này quan trọng hơn bình
thường: phần lớn số liệu là phân tích tĩnh, và phần chạy thật chỉ gói trong vài map.

## Giấy phép

**GNU General Public License v3.0**

| file | nội dung |
|---|---|
| `LICENSE` | **toàn văn GPLv3**, tải nguyên bản từ gnu.org, 35.149 byte / 674 dòng, không sửa một chữ |
| `NOTICE` | thông báo bản quyền, tác giả, phạm vi, và cái gì **không** thuộc giấy phép này |

Chọn GPLv3 vì Metamod:Source cũng dùng GPLv3 — plugin liên kết với nó nên đây là lựa
chọn **tương thích**, không phải tuỳ tiện.

Nghĩa là: dùng, sửa, phân phối lại thoải mái; nhưng bản sửa đổi khi phát hành cũng phải
là GPLv3 và **kèm mã nguồn**.

**Không thuộc giấy phép này:** SDK của Valve, nhị phân của game, và Metamod:Source —
cả ba đều không nằm trong kho. Các địa chỉ hàm và đoạn assembly trong chú thích là **mô
tả hành vi** phục vụ tương thích, không phải mã sao chép.

Gói này **không chứa mã của Valve**. `sample_mm.cpp` là mã tự viết; các địa chỉ và đoạn assembly trong chú thích là **mô tả hành vi** của `server.dll`/`engine.dll` phục vụ việc tương thích, không phải mã sao chép. SDK và nhị phân game không nằm trong kho.
