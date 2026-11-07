# BÁO CÁO ASSIGNMENT 01 -- ADVANCED DEVICE DRIVER

  Thông tin         Nội dung
  ----------------- ---------------------------------------------
  Môn học           Embedded Linux Systems (LOS201)
  Sinh viên         Trần Đức Kiên
  MSSV              SE183502
  Môi trường        QEMU vexpress-a9 (ARMv7), Linux kernel 5.15
  Cross-compiler    arm-linux-gnueabihf-gcc 15.2.0
  Ngày hoàn thiện   06/11/2026

# 1. Sơ đồ kiến trúc

## 1.1. Sơ đồ tổng quát

``` text
                    USER SPACE
       +--------------------------------------+
       | Task 1 application                   |
       | open/read/write/ioctl/close          |
       +-------------------+------------------+
                           |
       +-------------------v------------------+
       | Task 2 application                   |
       | open/read/write + đọc procfs         |
       +-------------------+------------------+
                           |
===========================|============================
                 SYSTEM CALL / VFS
          open / read / write / ioctl / close
                           |
              +------------+-------------+
              |                          |
       +------v---------+         +------v---------+
       | asgn1_driver   |         | counter_driver |
       | character dev  |         | misc device    |
       | major 244      |         | major 10       |
       | /dev/asgn1     |         | minor 125      |
       +------+---------+         | /dev/counter   |
              |                   +------+---------+
       +------v---------+                |
       | file_operations|         +------v---------+
       | open/read/write|         | file_operations|
       | release/ioctl  |         | open/read/write|
       +------+---------+         | release        |
              |                   +------+---------+
       +------v---------+                |
       | Buffer, mode,  |         +------v---------+
       | counters, mutex|         | atomic counter,|
       +----------------+         | statistics     |
                                  +------+---------+
                                         |
                                  +------v---------+
                                  | procfs         |
                                  | /proc/counter_ |
                                  | info           |
                                  +----------------+
```

Sơ đồ thể hiện ứng dụng userspace gọi system call. VFS dựa vào file
descriptor và thông tin thiết bị để chuyển yêu cầu đến callback trong
`file_operations`. Task 1 truy cập character device `/dev/asgn1`; Task 2
truy cập misc device `/dev/counter`. Thông tin thống kê của Task 2 được
xuất riêng qua `/proc/counter_info`.

Với `read()` và `write()`, VFS gọi callback tương ứng. Task 1 đọc/ghi
buffer nội bộ và áp dụng chế độ hiện tại. Task 2 đọc giá trị counter
hoặc ghi giá trị mới. Với `ioctl()`, VFS gọi `unlocked_ioctl`; driver
kiểm tra command, xử lý và truyền dữ liệu giữa kernel/userspace bằng các
API user-copy phù hợp.

## 1.2. Luồng ioctl của Task 1

``` text
Userspace ioctl(fd, command, argument)
                 |
                 v
              VFS
                 |
                 v
       file_operations
                 |
                 v
          unlocked_ioctl
                 |
           kiểm tra command
        /       |       |       \
       v        v       v        v
    RESET    GET_STATS SET_MODE GET_VERSION
              |         |          |
              v         v          v
          copy_to__  copy_from_  copy_to_
            user       user       user
                 \      |       /
                  \     |      /
                   trả kết quả
                    userspace
```

`GET_STATS` và `GET_VERSION` trả dữ liệu từ kernel về userspace bằng
`copy_to_user()`. `SET_MODE` nhận giá trị mode bằng `copy_from_user()`.
Command không hợp lệ phải trả `ENOTTY`, phù hợp với kết quả kiểm thử.

# 2. So sánh ba loại Device Driver

  --------------------------------------------------------------------------------------
  Tiêu chí          Character Device                Block Device      Network Device
  ----------------- ------------------------------- ----------------- ------------------
  Giao diện kernel  `file_operations`:              Block layer và    `net_device`,
                    open/read/write/release/ioctl   các thao tác I/O  `net_device_ops`
                                                    theo block        

  Đơn vị dữ liệu    Luồng byte                      Block/sector có   Packet/frame
                                                    địa chỉ           

  Buffering         Driver hoặc subsystem quản lý   Block layer và    Network stack,
                                                    cache             packet queue

  Node trong `/dev` Thường có, ví dụ `/dev/ttyS0`   Thường có, ví dụ  Thường không truy
                                                    `/dev/mmcblk0`    cập như file
                                                                      `/dev`; giao diện
                                                                      là network
                                                                      interface

  Ví dụ nhúng       UART, GPIO, cảm biến đơn giản   eMMC, SD card,    Ethernet, Wi-Fi,
                                                    SSD               CAN
  --------------------------------------------------------------------------------------

**Tình huống 1 -- Cảm biến UART:** Dữ liệu được truyền tuần tự theo
luồng byte nên character device phù hợp. Ứng dụng dùng `read()` và
`write()` để trao đổi dữ liệu, không cần tổ chức block có địa chỉ.

**Tình huống 2 -- Bộ nhớ eMMC/SD:** Thiết bị lưu trữ cần truy cập theo
sector, hỗ trợ truy cập ngẫu nhiên và làm việc với filesystem. Block
device phù hợp hơn character device.

**Tình huống 3 -- Ethernet:** Bộ điều khiển Ethernet truyền nhận frame
và phải tích hợp với network stack. Vì vậy driver cần dùng network
subsystem và `net_device`.

# 3. Phân tích thiết kế hai driver

## 3.1. `register_chrdev()` và `misc_register()`

Task 1 dùng `register_chrdev()` để đăng ký `asgn1_driver`. Log thực tế
xác nhận major number là **244**, node thiết bị là `/dev/asgn1`.

Ưu điểm: chủ động quản lý major number và xây dựng character device
riêng. Nhược điểm: cần quan tâm việc chọn/quản lý major number và tạo
node thiết bị.

Task 2 dùng `misc_register()`. Log ghi nhận major number **10**, minor
number **125**, node `/dev/counter`.

Ưu điểm: đơn giản hóa đăng ký các character device nhỏ; major number
misc được dùng chung và kernel cấp minor number. Nhược điểm: phù hợp với
thiết bị misc đơn giản, không phải lựa chọn mặc định cho mọi driver
character phức tạp.

Trong bài này, Task 1 có buffer, hai chế độ và nhiều ioctl nên đăng ký
character device riêng là hợp lý. Task 2 chỉ cung cấp counter đơn giản
nên `misc_register()` giúp mã đăng ký gọn hơn.

## 3.2. Vì sao Task 2 dùng `atomic_t`?

Nếu dùng `int` thường và cập nhật counter theo kiểu đọc--sửa--ghi mà
không có đồng bộ, hai process có thể làm mất cập nhật:

1.  Counter ban đầu bằng 10.
2.  Process A đọc 10.
3.  Process B cũng đọc 10.
4.  A tính 11 và ghi 11.
5.  B tính 11 và ghi 11.
6.  Kết quả cuối là 11, trong khi hai lần tăng đáng lẽ phải cho kết quả
    12.

Đây là race condition dạng lost update. `atomic_t` cung cấp thao tác
nguyên tử như `atomic_read()` và `atomic_inc()`, giúp cập nhật một biến
số nguyên không bị ghi đè theo cách trên. Lưu ý `atomic_t` chỉ bảo vệ
thao tác trên biến đó; nếu nhiều biến phải được cập nhật nhất quán cùng
nhau thì vẫn cần mutex hoặc cơ chế đồng bộ phù hợp.

## 3.3. ioctl magic number

Các macro `_IO`, `_IOR`, `_IOW`, `_IOWR` mã hóa thông tin của command:

-   `DIR`: hướng truyền dữ liệu.
-   `SIZE`: kích thước kiểu dữ liệu.
-   `TYPE`: magic number nhận diện nhóm ioctl.
-   `NR`: số thứ tự command.

``` text
+----------------------+----------+----------+
| DIR và SIZE          | TYPE     | NR       |
+----------------------+----------+----------+
```

-   `_IO(type, nr)`: không có đối số dữ liệu.
-   `_IOR(type, nr, data_type)`: driver trả dữ liệu về userspace.
-   `_IOW(type, nr, data_type)`: userspace gửi dữ liệu vào driver.
-   `_IOWR(type, nr, data_type)`: truyền hai chiều.

Trong Task 1, `ASGN1_RESET_BUFFER` dùng `_IO`; `ASGN1_GET_STATS` dùng
`_IOR`; `ASGN1_SET_MODE` dùng `_IOW`; `ASGN1_GET_VERSION` dùng `_IOR`.

Giá trị magic number, `NR` và `SIZE` cụ thể phải được đọc từ
`asgn1_ioctl.h`. Tệp header và kết quả `gcc -E` không nằm trong tài liệu
được cung cấp, vì vậy báo cáo không tự suy đoán các giá trị số. Cần đối
chiếu header trước khi nộp nếu giảng viên yêu cầu ghi giá trị cụ thể.

# 4. Kết quả kiểm thử

Hai driver được kiểm thử trên QEMU `vexpress-a9`, Linux kernel 5.15.
Module được biên dịch cho ARMv7 bằng `arm-linux-gnueabihf-gcc`. Sau khi
build initramfs và chạy QEMU, lần lượt thực thi `/root/test_asgn1.sh` và
`/root/test_counter.sh`. Log được lưu trong `test_asgn1_output.txt`,
`test_counter_output.txt` và `dmesg_full.txt`.

## 4.1. Task 1 -- `test_asgn1.sh`

``` text
/ # sh /root/test_asgn1.sh
asgn1_driver: loading out-of-tree module taints kernel.
asgn1: registered char device with major number 244
asgn1: version: asgn1_driver v1.0-SE183502
asgn1: device opened (open_count = 1)
asgn1: ioctl SET_MODE: mode set to 1
Mode set to 1.
asgn1: device closed
=== Echo mode ===
asgn1: device opened (open_count = 2)
asgn1: wrote 12 bytes to device (write_count = 1, mode = 1)
asgn1: device closed
asgn1: device opened (open_count = 3)
asgn1: read 12 bytes from device (read_count = 1)
Hello World
asgn1: device closed
=== Kernel mode ===
asgn1: device opened (open_count = 4)
asgn1: ioctl SET_MODE: mode set to 0
Mode set to 0.
asgn1: device closed
asgn1: device opened (open_count = 5)
asgn1: wrote 12 bytes to device (write_count = 2, mode = 0)
asgn1: device closed
asgn1: device opened (open_count = 6)
asgn1: read 21 bytes from device (read_count = 2)
[KERNEL] Hello World
asgn1: device closed
=== ioctl ===
asgn1: device opened (open_count = 7)
asgn1: ioctl GET_VERSION
Device version: asgn1_driver v1.0-SE183502
asgn1: device closed
asgn1: device opened (open_count = 8)
asgn1: ioctl GET_STATS
Open count: 8
Write count: 2
Read count: 2
Buffer length: 21
Last write size: 12
asgn1: device closed
asgn1: device opened (open_count = 9)
ioctl SET_MODE: Invalid argument
asgn1: device closed
asgn1: device opened (open_count = 10)
ioctl BAD_COMMAND: Inappropriate ioctl for device
Bad command test passed: ioctl returned ENOTTY as expected.
asgn1: device closed
=== Reset ===
asgn1: device opened (open_count = 11)
asgn1: wrote 17 bytes to device (write_count = 3, mode = 0)
asgn1: device closed
asgn1: device opened (open_count = 12)
asgn1: ioctl RESET_BUFFER
Buffer reset successfully.
asgn1: device closed
asgn1: device opened (open_count = 13)
asgn1: device closed
asgn1: device opened (open_count = 14)
asgn1: ioctl GET_STATS
Open count: 14
Write count: 0
Read count: 0
Buffer length: 0
Last write size: 17
asgn1: device closed
=== Unload ===
asgn1: Unloaded
```

  ------------------------------------------------------------------------------------
  Kiểm tra          Kỳ vọng           Kết quả thực tế                Đánh giá
  ----------------- ----------------- ------------------------------ -----------------
  Echo mode         Đọc lại dữ liệu   Ghi/đọc 12 byte `Hello World`  Đạt
                    nguyên bản                                       

  Kernel mode       Thêm `[KERNEL]`   Đọc 21 byte                    Đạt

  `GET_VERSION`     Trả version       `asgn1_driver v1.0-SE183502`   Đạt

  `GET_STATS`       Trả đủ 5 trường   In đủ                          Đạt
                                      open/write/read/buffer/last    
                                      write                          

  `SET_MODE` sai    Từ chối mode      `Invalid argument`             Đạt

  ioctl sai         Trả `ENOTTY`      Script xác nhận `ENOTTY`       Đạt

  Reset             Xóa buffer, reset Buffer length, write count,    Đạt
                    read/write count  read count đều 0               

  Unload            Gỡ module         Có `asgn1: Unloaded`           Đạt
  ------------------------------------------------------------------------------------

Lệnh `echo` thêm ký tự xuống dòng nên `Hello World` được ghi thành 12
byte. Tiền tố `[KERNEL]` dài 9 byte, vì vậy dữ liệu Kernel mode dài 9 +
12 = 21 byte. `Last write size` vẫn là 12 vì trường này ghi nhận kích
thước dữ liệu đầu vào, không phải độ dài sau khi thêm tiền tố.

Sau reset, `Buffer length`, `Write count` và `Read count` bằng 0.
`Last write size` vẫn bằng 17 theo log thực tế; báo cáo giữ nguyên kết
quả quan sát được, không giả định trường này cũng được reset.

## 4.2. Task 2 -- `/proc/counter_info`

Trước khi write:

``` text
counter     : 0
read_total  : 0
write_total : 0
```

Sau `echo 100 > /dev/counter`:

``` text
counter     : 101
read_total  : 4
write_total : 1
```

Sau `echo 0 > /dev/counter`:

``` text
counter     : 1
read_total  : 5
write_total : 2
```

Khi ghi 100, driver đặt counter thành 100 và `write_total` tăng lên 1.
Lần đọc kế tiếp trả về 100 rồi tăng counter lên 101, do đó procfs hiển
thị 101. Khi ghi 0, counter được đặt về 0 và `write_total` tăng lên 2.
Lần đọc sau trả về 0 rồi tăng counter thành 1, nên procfs hiển thị 1.
`read_total` tăng từ 4 lên 5.

  Thời điểm              Counter   `read_total`   `write_total`
  -------------------- --------- -------------- ---------------
  Trước write                  0              0               0
  Sau ghi 100 và đọc         101              4               1
  Sau ghi 0 và đọc             1              5               2

## 4.3. `dmesg`

``` text
counter: unloaded
hw perfevents: enabled with armv7_cortex_a9 PMU driver, 7 counters available
counter_driver: loading out-of-tree module taints kernel.
counter: registered, minor = 125
counter: read 0
counter: read 1
counter: read 2
counter: set counter = 100
counter: read 100
counter: set counter = 0
counter: read 0
counter: unloaded
```

Log xác nhận driver đăng ký thành công với minor 125; các lần đọc trả về
0, 1, 2 và 100; các lần ghi đặt counter thành 100 rồi 0.
`counter: unloaded` xác nhận hàm thoát module đã được gọi. Task 2 không
có ioctl nên không có log ioctl.

## 4.4. Lỗi gặp phải và cách debug

  ----------------------------------------------------------------------------------------------------------------
  \#                Triệu chứng                                           Nguyên nhân         Debug và cách sửa
  ----------------- ----------------------------------------------------- ------------------- --------------------
  1                 `macro 'min' passed 3 arguments, but takes just 2`    Toán hạng có kiểu   Kiểm tra kiểu ở dòng
                                                                          `size_t` và         lỗi; thay bằng
                                                                          `loff_t` không      `min_t` và chỉ rõ
                                                                          tương thích với     kiểu so sánh phù
                                                                          kiểm tra kiểu của   hợp.
                                                                          macro `min`.        

  2                 `input file 'asgn1_app' is the same as output file`   Dòng khai báo `APP` Dùng `cat -A` để
                                                                          trong Makefile có   thấy ký tự cuối
                                                                          dấu cách thừa cuối  dòng; xóa dấu cách
                                                                          dòng, làm tên file  thừa rồi build lại.
                                                                          truyền vào compiler 
                                                                          sai.                

  3                 Hai lần `mutex_lock()` trong `asgn1_read`             Mutex không đệ quy; Review mọi nhánh
                                                                          khóa lại khi chưa   `return`, cân bằng
                                                                          mở khóa có thể làm  mỗi lần lock với
                                                                          luồng tự deadlock.  unlock. Lỗi được
                                                                                              phát hiện trước khi
                                                                                              chạy QEMU.

  4                 `asgn1_ioctl undeclared here`                         `file_operations`   Đưa định nghĩa lên
                                                                          tham chiếu callback trước hoặc thêm
                                                                          trước khi compiler  prototype trước bảng
                                                                          thấy khai báo.      `file_operations`.
  ----------------------------------------------------------------------------------------------------------------

Lỗi mutex có thể làm `read()` bị treo và các thao tác khác cần cùng
mutex phải chờ. Luồng khóa mutex lần đầu, sau đó gọi khóa lần thứ hai
khi vẫn đang giữ khóa; nó chờ chính mutex do mình giữ và không thể đi
đến lệnh unlock. Lỗi được phát hiện khi review code trước khi kiểm thử
QEMU, do đó không có lần chạy QEMU nào bị treo vì lỗi này. Bài học là
phải rà soát cặp lock/unlock trên tất cả nhánh thực thi, đặc biệt trước
các lệnh return.

# 5. Câu hỏi lý thuyết

## Câu 1. Reference counting của module

Kernel quản lý số tham chiếu để không gỡ module khi vẫn còn thành phần
đang sử dụng mã hoặc dữ liệu của module. Trường `.owner = THIS_MODULE`
trong `file_operations` cho phép kernel giữ tham chiếu module trong thời
gian file operation còn được sử dụng. Khi file được mở, tham chiếu được
giữ; khi file được đóng, tham chiếu được giảm. Các API
`try_module_get()` và `module_put()` cũng được dùng khi cần quản lý tham
chiếu tường minh.

Nếu một process đang mở `/dev/asgn1`, `rmmod asgn1_driver` thường bị từ
chối vì module đang được sử dụng. Sau khi đóng file descriptor, module
mới có thể được gỡ bình thường.

Có thể kiểm tra:

``` sh
exec 3</dev/asgn1
lsmod
rmmod asgn1_driver
exec 3<&-
rmmod asgn1_driver
```

Không nên ép unload khi vẫn còn process sử dụng thiết bị vì có thể dẫn
đến kernel truy cập mã/dữ liệu đã bị giải phóng.

## Câu 2. Vì sao `GET_STATS` dùng `copy_to_user()`?

Kernel space và userspace có không gian địa chỉ khác nhau. Con trỏ do
ứng dụng truyền vào không được mặc định là địa chỉ kernel hợp lệ.
`copy_to_user()` là API dùng để sao chép dữ liệu từ kernel sang vùng nhớ
userspace và trả về số byte chưa sao chép nếu có lỗi.

Không dùng `memcpy()` hoặc dereference trực tiếp con trỏ userspace vì
địa chỉ có thể không được ánh xạ hoặc không có quyền truy cập. Truy cập
trực tiếp có thể gây lỗi bộ nhớ trong kernel. `GET_STATS` tạo bản sao
cấu trúc thống kê trong kernel rồi gọi `copy_to_user()`. Chiều ngược
lại, `SET_MODE` nhận dữ liệu bằng `copy_from_user()`.

## Câu 3. MTD và block device

MTD được thiết kế cho flash thô như NAND/NOR, nơi có page, erase block,
giới hạn số lần xóa/ghi và bad block. Block subsystem cung cấp giao diện
sector/block cho HDD, SSD, eMMC và SD card.

  -----------------------------------------------------------------------
  Tiêu chí                MTD                     Block device
  ----------------------- ----------------------- -----------------------
  Thiết bị                NAND/NOR flash thô      HDD, SSD, eMMC, SD

  Đơn vị                  Page và erase block     Sector/block

  Xóa                     Phải xóa theo erase     Controller thường che
                          block                   giấu đặc tính xóa vật
                                                  lý

  Bad block               Có thể phải quản lý     Thường do controller xử
                                                  lý

  Filesystem              JFFS2, UBIFS            ext4, FAT, XFS
  -----------------------------------------------------------------------

Ví dụ, chip NAND flash thô trên bo mạch nhúng dùng để lưu dữ liệu với
JFFS2/UBIFS nên được quản lý qua MTD. eMMC được controller trình bày như
block device thường dùng block subsystem và filesystem như ext4.

## Câu 4. `*ppos` trong `read()` của `counter_driver`

`ppos` trỏ đến vị trí hiện tại trong file mà VFS truyền vào callback
`read()`. Driver dùng vị trí này để biết tiến trình đang đọc đến đâu.
Khi đã trả hết dữ liệu, lần `read()` tiếp theo cần trả 0 để báo EOF.

Nếu driver không kiểm tra hoặc cập nhật `*ppos` và lần nào cũng trả dữ
liệu, chương trình `cat` có thể tiếp tục đọc vô hạn vì không nhận được
EOF. Với thiết bị chỉ trả một giá trị counter cho mỗi lần mở file,
driver có thể dùng vị trí đọc để chỉ trả dữ liệu trong lần đầu; các lần
tiếp theo trả 0.

``` text
cat /dev/counter
       |
       v
     read()
       |
       v
   kiểm tra *ppos
    /       \
   = 0      > 0
   |         |
   v         v
trả dữ liệu  trả 0 (EOF)
và cập nhật
*ppos
```

Khi file được mở lại, file position của file descriptor mới bắt đầu lại
từ đầu.

# 6. Kết luận

Task 1 đã triển khai `asgn1_driver` với major number 244, node
`/dev/asgn1`, thao tác đọc/ghi buffer, hai chế độ Echo/Kernel và bốn
ioctl command. Kiểm thử xác nhận dữ liệu đúng ở hai chế độ, version và
statistics được trả về, command không hợp lệ trả `ENOTTY`, reset xóa
buffer và module unload thành công.

Task 2 đã triển khai `counter_driver` dạng misc device với major 10,
minor 125 và node `/dev/counter`. Driver hỗ trợ đọc/ghi counter;
`/proc/counter_info` cung cấp thống kê. Log kiểm thử và `dmesg` xác nhận
các thao tác đã thực hiện.

Qua assignment, các nội dung được áp dụng gồm kernel module lifecycle,
`file_operations`, giao tiếp kernel/userspace, ioctl, đồng bộ dữ liệu,
atomic operation, procfs và debug lỗi build.

# 7. Tài liệu tham khảo

1.  Tài liệu Assignment 01 -- Embedded Linux Systems.
2.  Linux Kernel Documentation -- Driver API và device drivers.
3.  Linux man-pages: `ioctl(2)`, `open(2)`, `read(2)`, `write(2)`,
    `close(2)`.
4.  Linux kernel header `include/uapi/asm-generic/ioctl.h` -- ioctl
    macros.
5.  Log kiểm thử: `test_asgn1_output.txt`, `test_counter_output.txt`,
    `dmesg_full.txt`.


