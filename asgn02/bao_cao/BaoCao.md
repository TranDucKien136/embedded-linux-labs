BÁO CÁO ASSIGNMENT 02 – SENSOR MONITORING SYSTEM
Môn học: Embedded Linux Systems (LOS201)
Sinh viên: Trần Đức Kiên
MSSV: SE183502
Môi trường: QEMU vexpress-a9 (ARMv7), Linux 5.15; host x86_64 cho Valgrind/perf
Ngày: 07/11/2026
TÓM TẮT VÀ MỤC TIÊU
Tóm tắt
Assignment 02 xây dựng một hệ thống giám sát cảm biến (Sensor Monitoring System) trên Embedded Linux. Phần kernel cung cấp driver mô phỏng cảm biến nhiệt độ và độ ẩm qua thiết bị ký tự /dev/sms_sensor, đồng thời xuất thống kê qua /proc/sms_stats. Phần userspace là ứng dụng sms_app gồm bốn luồng: Sensor, Processor, Alert và Logger.

Sensor Thread đọc dữ liệu theo chu kỳ, phân tích dòng CSV và đưa mẫu vào circular buffer. Processor Thread lấy mẫu, tính giá trị trung bình và phát hiện điều kiện bất thường. Khi phát hiện bất thường, Processor gửi thông điệp qua POSIX message queue để Alert Thread xử lý. Logger Thread nhận thông điệp log qua queue riêng và ghi vào /tmp/sms.log.

Báo cáo tổng hợp kết quả từ log chạy QEMU, /proc, GDB, Valgrind, strace, perf và phép đo jitter. Những số liệu được giữ theo output thực tế. Báo cáo không chèn mã nguồn giả định; phần mô tả triển khai tập trung vào hành vi quan sát được và thiết kế đã thực hiện.
Mục tiêu
- Xây dựng driver mô phỏng cảm biến và giao diện userspace thông qua device file, ioctl và procfs.
- Tổ chức ứng dụng thành nhiều POSIX thread, trao đổi dữ liệu qua circular buffer và message queue.
- Đảm bảo đóng tài nguyên, dừng thread có trật tự và kiểm tra memory leak.
- Sử dụng GDB, Valgrind, strace và perf để quan sát hoạt động và phân tích hiệu năng.
- Đo jitter của Sensor Thread, so sánh SCHED_OTHER với SCHED_FIFO và đánh giá giới hạn của kết quả trong QEMU.
MÔI TRƯỜNG VÀ CẤU TRÚC HỆ THỐNG
Môi trường thực nghiệm
| Thành phần | Cấu hình / giá trị |

|---|---|

| Target | QEMU vexpress-a9, ARMv7-A Cortex-A9 |

| Kernel | Linux 5.15, PREEMPT, HZ=1000, HIGH_RES_TIMERS |

| Driver | sms_sensor_driver.ko; device /dev/sms_sensor |

| Ứng dụng ARM | /root/sms_app |

| Ứng dụng host | sms_app_x86 |

| Chu kỳ mặc định | 500 ms |

| Công cụ debug/profile | GDB, Valgrind 3.26.0, strace, perf |

| Giả lập cảm biến trên host | FIFO /tmp/fake_sensor |
Cấu trúc chức năng
Hệ thống được chia thành hai lớp. Lớp kernel chịu trách nhiệm tạo dữ liệu cảm biến, giới hạn khoảng cách giữa hai lần đọc, đếm số lần đọc và cung cấp thống kê. Lớp userspace chịu trách nhiệm lấy mẫu, xử lý dữ liệu, phát hiện cảnh báo, ghi log và quản lý vòng đời các thread.

Việc tách ứng dụng thành nhiều file theo chức năng giúp giới hạn trách nhiệm của từng thành phần. Makefile hỗ trợ build target ARM và build host x86. Bản ARM được chạy trong QEMU; bản host được sử dụng cho các phép thử phụ thuộc công cụ như Valgrind và perf.
SƠ ĐỒ KIẾN TRÚC VÀ LUỒNG DỮ LIỆU
Sơ đồ kiến trúc
                 KERNEL SPACE
       +--------------------------------+
       | sms_sensor_driver.ko          |
       | random sensor + interval       |
       | read_count + proc statistics  |
       +---------------+----------------+
                       |
              /dev/sms_sensor
                       |
                       v
                 USER SPACE
       +--------------------------------+
       | Sensor Thread                  |
       | open / read / ioctl            |
       | parse timestamp,T,H            |
       +---------------+----------------+
                       |
                       v
       +--------------------------------+
       | Shared Circular Buffer        |
       | mutex + condition variable     |
       +---------------+----------------+
                       |
                       v
       +--------------------------------+
       | Processor Thread               |
       | average + threshold detection  |
       +-----------+--------------------+
                   |
            +------+------+
            |             |
       /sms_alarm     /sms_log
            |             |
            v             v
       +----------+   +-----------+
       | Alert    |   | Logger    |
       | Thread   |   | Thread    |
       +----------+   +-----+-----+
                            |
                            v
                       /tmp/sms.log

       /proc/sms_stats: driver statistics
Luồng dữ liệu chính bắt đầu từ driver. Sensor Thread mở thiết bị, cấu hình khoảng đọc bằng ioctl và đọc từng dòng CSV. Mỗi mẫu được chuyển vào circular buffer. Processor Thread chờ dữ liệu mới, lấy mẫu và thực hiện tính toán. Các cảnh báo được gửi qua queue /sms_alarm; thông tin vận hành được gửi qua /sms_log để Logger Thread ghi xuống file.

/proc/sms_stats là kênh quan sát riêng của driver, không nằm trong đường truyền dữ liệu của circular buffer. Người vận hành có thể đọc node này trước, trong và sau khi ứng dụng chạy để đối chiếu read_count và giá trị cảm biến cuối cùng.
KERNEL DRIVER VÀ GIAO DIỆN TÍCH HỢP
Chức năng driver
Kernel module sms_sensor_driver tạo thiết bị /dev/sms_sensor. Mỗi lần userspace đọc thiết bị, driver trả về một dòng CSV gồm timestamp theo mili giây, nhiệt độ và độ ẩm. Dữ liệu được sinh ngẫu nhiên trong kernel bằng get_random_u32(). Giá trị nhiệt độ nằm trong khoảng mô phỏng 15,0–45,0 °C; độ ẩm nằm trong khoảng 30,0–90,0 %.

Kernel không sử dụng kiểu số thực như userspace. Giá trị cảm biến được biểu diễn theo đơn vị phần mười để định dạng kết quả với một chữ số thập phân. Cách biểu diễn này tránh phụ thuộc phép toán floating-point trong kernel.
Ioctl và giới hạn chu kỳ
Ứng dụng sử dụng ioctl SMS_SET_INTERVAL để cấu hình khoảng thời gian tối thiểu giữa hai lần đọc. Giá trị interval được truyền từ tham số -i của sms_app; miền hợp lệ của driver là 1 đến 10000 ms. Mặc định ứng dụng sử dụng chu kỳ 500 ms.

Driver duy trì read_count, last_temperature, last_humidity và uptime. Các giá trị được công bố qua /proc/sms_stats để kiểm chứng hoạt động của driver mà không cần đọc trực tiếp cấu trúc nội bộ kernel.
Tích hợp và xử lý lỗi
Sensor Thread phải xử lý các tình huống đọc bị gián đoạn (EINTR), thiết bị tạm thời chưa sẵn sàng (EAGAIN) và kết thúc dữ liệu (EOF). Khi shutdown, file descriptor được đóng. Nếu lỗi nghiêm trọng xảy ra trong sensor thread, trạng thái lỗi được truyền về luồng điều khiển chính để chương trình không báo kết thúc thành công một cách sai lệch.

Kết quả /proc xác nhận read_count tăng từ 0 trước khi chạy lên 60 ở khoảng giữa phiên chạy và 120 sau khi ứng dụng kết thúc. Điều này phù hợp với chu kỳ 500 ms trong khoảng 60 giây.
THIẾT KẾ ĐA LUỒNG VÀ IPC
Vai trò của bốn thread
| Thread | Trách nhiệm | Đầu vào / đầu ra |

|---|---|---|

| Sensor Thread | Đọc driver theo chu kỳ, parse CSV, đẩy mẫu | /dev/sms_sensor → circular buffer |

| Processor Thread | Lấy mẫu, tính trung bình, kiểm tra ngưỡng | circular buffer → alarm/log queues |

| Alert Thread | Nhận và xử lý cảnh báo | /sms_alarm → thông báo/log |

| Logger Thread | Nhận thông điệp và ghi file | /sms_log → /tmp/sms.log |
Circular buffer
Circular buffer được chọn thay cho linked list vì số lượng phần tử có giới hạn và thao tác push/pop lặp lại theo chu kỳ. Bộ đệm dùng vùng nhớ cố định, tránh cấp phát và giải phóng node cho từng mẫu. Head, tail và size được cập nhật trong vùng đồng bộ; mutex bảo vệ trạng thái dùng chung và condition variable cho phép Processor Thread chờ khi buffer rỗng.
POSIX message queues
Hai queue tách biệt được sử dụng cho hai loại thông điệp. /sms_alarm truyền cấu trúc cảnh báo từ Processor đến Alert. /sms_log truyền nội dung log để Logger ghi file. Việc dùng queue giúp tách thời điểm phát sinh sự kiện khỏi thời điểm xử lý hoặc ghi log.
Quyết định thiết kế
- Circular buffer phù hợp với luồng mẫu tuần tự, kích thước cố định và chi phí thao tác thấp.
- POSIX message queue phù hợp với thông điệp cảnh báo/log có ranh giới message rõ ràng.
- Logger Thread riêng tránh để thao tác file làm chậm Sensor Thread hoặc Processor Thread.
- Tham số -i, -t, -h, -d, -v, -r và -D cho phép điều chỉnh chu kỳ, ngưỡng, thời lượng, chế độ RT và đường dẫn thiết bị khi chạy.
ĐỒNG BỘ, SHUTDOWN VÀ STATE MACHINE
Dữ liệu dùng chung và đồng bộ
| Dữ liệu | Thread sử dụng | Cơ chế |

|---|---|---|

| Circular buffer: head/tail/size/data | Sensor ghi, Processor đọc | Mutex + condition variable |

| Cờ running | Main và các thread | Cờ điều khiển vòng đời; thay đổi theo quy trình shutdown |

| Thông điệp cảnh báo | Processor gửi, Alert nhận | POSIX message queue |

| Thông điệp log | Processor/Alert gửi, Logger nhận | POSIX message queue |

| File log | Logger ghi | Một thread chuyên trách ghi file |
State machine
        +---------+
        |  INIT   |
        +----+----+
             |
      khởi tạo thành công
             v
        +---------+
        | RUNNING |
        +----+----+
             |
   SIGINT / hết thời lượng /
   lỗi sensor / lỗi runtime
             v
        +---------+
        | SHUTDOWN|
        +----+----+
             |
   join thread + flush log
   close fd + unlink queues
             v
        +---------+
        |  EXIT   |
        +---------+
Tránh deadlock
Circular buffer sử dụng một mutex thống nhất cho thao tác thay đổi head, tail và size. Condition variable được chờ cùng mutex theo quy tắc pthread, nhờ đó thao tác kiểm tra điều kiện và chuyển sang trạng thái chờ không tạo cửa sổ race giữa kiểm tra và ngủ. Khi shutdown, cờ running được hạ và condition variable được broadcast để thread đang chờ buffer thức dậy, kiểm tra điều kiện dừng rồi thoát.

Với POSIX queue, việc shutdown cần bảo đảm thread nhận không bị mắc kẹt vô thời hạn. Thiết kế dùng thông điệp dừng hoặc cơ chế timeout phù hợp để đánh thức Alert/Logger. Trình tự join phải theo luồng dữ liệu: dừng Sensor trước, sau đó cho Processor xử lý phần dữ liệu còn lại, rồi dừng các thread tiêu thụ queue và cuối cùng giải phóng queue. Không nên đóng queue khi thread khác vẫn đang sử dụng.

Các nguyên tắc trên làm giảm nguy cơ deadlock: không giữ mutex circular buffer trong lúc thực hiện thao tác I/O hoặc gửi queue; không chờ join một thread trong khi vẫn giữ lock mà thread đó cần; shutdown phải đánh thức mọi điểm chờ.
KẾT QUẢ CHẠY ỨNG DỤNG VÀ DRIVER
Kiểm thử phiên chạy 60 giây
| Chỉ số | Kết quả ghi nhận |

|---|---|

| Uptime ứng dụng | 60 giây |

| Samples produced | 120 |

| Samples consumed | 120 |

| Buffer overflows | 0 |

| Samples remaining | 0 |

| TEMP_HIGH | 22 cảnh báo |

| HUMID_HIGH | 15 cảnh báo |

| TEMP_SPIKE | 79 cảnh báo |

Trong lần chạy mẫu, ứng dụng tạo 120 mẫu và Processor tiêu thụ đủ 120 mẫu. Không có buffer overflow và không còn mẫu tồn đọng khi kết thúc. Điều này cho thấy tốc độ tiêu thụ đáp ứng tốc độ sinh mẫu trong điều kiện thử nghiệm.
Đối chiếu /proc/sms_stats
| Thời điểm | read_count | Nhiệt độ cuối | Độ ẩm cuối | Driver uptime |

|---|---|---|---|---|

| Trước chạy | 0 | 0,0 °C | 0,0 % | 15 s |

| Khoảng T=30 s | 60 | 32,5 °C | 61,5 % | 46 s |

| Sau khi kết thúc | 120 | 19,3 °C | 88,0 % | 76 s |

read_count tăng đúng 120 lần sau phiên chạy 60 giây với chu kỳ 500 ms. Số lần đọc trong driver khớp với số sample được ứng dụng tạo ra. Đây là bằng chứng trực tiếp cho thấy đường giao tiếp userspace–driver hoạt động trong phiên kiểm thử.

Các cảnh báo được ghi nhận gồm TEMP_HIGH, HUMID_HIGH và TEMP_SPIKE. Trong log, ngưỡng minh họa tương ứng là nhiệt độ trên 40,0 °C, độ ẩm trên 80,0 % và độ tăng nhiệt độ tức thời trên 5,0 °C. Các cảnh báo là kết quả của dữ liệu ngẫu nhiên mô phỏng, không phải số đo từ cảm biến vật lý.
QUẢN LÝ BỘ NHỚ: VALGRIND
Kịch bản kiểm thử
Valgrind Memcheck được chạy trên bản host x86_64 của ứng dụng với FIFO cảm biến giả lập tại /tmp/fake_sensor. Đây là lựa chọn phù hợp vì Valgrind kiểm tra tiến trình userspace trên host; phép thử này không phải phép đo trực tiếp module kernel ARM.

| Chỉ tiêu | Kết quả |

|---|---|

| Valgrind | 3.26.0 |

| Lệnh | ./sms_app_x86 -D /tmp/fake_sensor |

| Heap allocations | 23 |

| Heap frees | 23 |

| Tổng bytes đã cấp phát | 174.079 bytes |

| In use at exit | 0 bytes in 0 blocks |

| Heap blocks còn lại | 0 |

| ERROR SUMMARY | 0 errors from 0 contexts |
Phân tích
Tổng số lần cấp phát bằng tổng số lần giải phóng. Tại thời điểm kết thúc, không còn block heap nào được Memcheck ghi nhận là đang sử dụng. ERROR SUMMARY bằng 0 cho thấy trong kịch bản chạy này không có lỗi truy cập bộ nhớ hoặc lỗi Memcheck được báo cáo.

Kết luận phù hợp là: không phát hiện memory leak trong lần kiểm thử Valgrind đã thực hiện. Kết quả không chứng minh tuyệt đối rằng mọi đường chạy đều không có lỗi; nó phụ thuộc vào đường thực thi, dữ liệu đầu vào và thời gian kiểm thử.
Lỗi uninitialised bytes và bài học
Trong quá trình phát triển đã xuất hiện cảnh báo Valgrind liên quan đến byte chưa khởi tạo được truyền trong mq_timedsend. Nguyên nhân là cấu trúc message có thể chứa byte đệm (padding) hoặc phần tử mảng chưa được ghi hết, trong khi toàn bộ kích thước cấu trúc được gửi qua queue. Cách khắc phục là khởi tạo toàn bộ cấu trúc message về 0 trước khi gán các trường và gửi. Kết quả Valgrind hiện tại sau khi sửa cho thấy 0 lỗi.
PHÂN TÍCH BẢN ĐỒ BỘ NHỚ
Kết quả /proc/PID/maps
| Vùng | Dải địa chỉ | Quyền | Ý nghĩa |

|---|---|---|---|

| Text | 00010000–00082000 | r-xp | Mã máy của sms_app, chỉ đọc và thực thi |

| Read-only data | 0009d000–000a0000 | r--p | Hằng số và dữ liệu chỉ đọc |

| Data/BSS | 000a0000–000a2000 | rw-p | Dữ liệu toàn cục/static có thể ghi |

| Heap | 000a2000–000c6000 | rw-p | Vùng cấp phát động |

| Anonymous mappings | 74c00000–76f9b000 | rw-p / ---p | Các vùng ánh xạ ẩn danh; có thể gồm vùng stack thread và vùng runtime |

| Main stack | 7e880000–7e8a1000 | rw-p | Stack tiến trình chính |

| sigpage/vvar/vdso | 7efc2000–7efc5000 | r-xp / r--p | Vùng hỗ trợ signal và truy cập thông tin kernel/userspace |

| vectors | ffff0000–ffff1000 | r-xp | Vùng vector trên ARM |

Ứng dụng ARM được build static nên maps không hiển thị các shared library userspace thông thường. Các vùng anonymous có quyền rw-p và các vùng ---p xen kẽ là dấu hiệu của các vùng nhớ được runtime hoặc pthread cấp phát; không nên gán tên stack cho từng vùng nếu maps không cung cấp nhãn cụ thể.
Memory footprint theo thời gian
| Thời điểm | VmSize từ status | VmRSS từ status | Rss từ smaps_rollup |

|---|---|---|---|

| T=5 s | 35.596 kB | 4 kB | 544 kB |

| T=30 s | 35.596 kB | 4 kB | 544 kB |

| T=55 s | 35.596 kB | 4 kB | 544 kB |

VmSize ổn định ở 35.596 kB trong cả ba thời điểm. Giá trị Rss từ smaps_rollup cũng ổn định ở 544 kB, không có xu hướng tăng theo thời gian. Tuy nhiên, trường VmRSS trong bản ghi /proc/PID/status lại là 4 kB ở cả ba mốc, không tương thích với Rss=544 kB trong smaps_rollup. Báo cáo giữ nguyên cả hai số liệu và đánh dấu sự khác biệt này thay vì tự ý thay thế. Khi đánh giá resident memory, cần kiểm tra lại cách thu thập và nguồn trường RSS.
GDB: DEBUG MULTI-THREAD
Kịch bản debug
GDB được sử dụng trên bản host sms_app_x86 để quan sát hoạt động đa luồng. Breakpoint được đặt tại processor_thread.c:94, ngay tại điều kiện kiểm tra nhiệt độ so với ngưỡng. Đây là điểm phù hợp để theo dõi nhánh xử lý bất thường và xác định Processor Thread đang xử lý mẫu nào.

Log ghi nhận breakpoint tại địa chỉ 0x4116. Khi chương trình dừng, GDB hiển thị Thread 4 đang thực thi processor_thread_func tại dòng 94. Danh sách thread có tiến trình chính và bốn thread ứng dụng. Các thread khác đang nằm trong syscall hoặc điểm chờ, phù hợp với mô hình một thread đọc theo chu kỳ và các thread khác chờ buffer/queue.
Quan sát từ session
- GDB bật hỗ trợ thread debugging thông qua libthread_db.
- Có bốn thread mới được tạo sau khi chương trình khởi động.
- Thread 4 dừng tại điều kiện kiểm tra ngưỡng trong Processor Thread.
- Một thread khác có stack trace đi qua clock_nanosleep, phù hợp với việc chờ chu kỳ.
- Các lệnh xem thread và backtrace giúp phân biệt thread đang xử lý với thread đang chờ.
Giới hạn và cải tiến
Bản build tối ưu -O2 có thể làm biến bị tối ưu hoặc biểu thức bị inline, khiến việc thay đổi biến trực tiếp bằng GDB không phản ánh như mong đợi. Khi cần thử giá trị giả lập, nên tạo bản debug -O0 -g riêng và giữ bản build tối ưu để đo hiệu năng. Session đã cung cấp bằng chứng dừng tại logic kiểm tra ngưỡng; không nên khẳng định đã sửa biến runtime hoặc kích hoạt đủ mọi nhánh nếu log không thể hiện điều đó.
STRACE: PHÂN TÍCH SYSTEM CALL
Kết quả strace -c
Tổng cộng 1.394 system call được thống kê, trong đó 10 lần trả về lỗi. Tổng thời gian nằm trong các system call là 1,826175 giây; thời gian trung bình trên mỗi call theo bảng là 1.310 micro giây.

| System call | Calls | Errors | Seconds | % time | Vai trò |

|---|---|---|---|---|---|

| clock_nanosleep | 91 | 0 | 0,707068 | 38,72% | Chờ theo chu kỳ |

| mq_timedreceive | 168 | 0 | 0,594613 | 32,56% | Nhận message có timeout |

| futex | 197 | 5 | 0,330085 | 18,08% | Đồng bộ pthread/condition |

| clock_gettime64 | 437 | 0 | 0,060843 | 3,33% | Đọc thời gian monotonic |

| mq_timedsend | 168 | 0 | 0,050444 | 2,76% | Gửi message queue |

| write | 65 | 0 | 0,023871 | 1,31% | Ghi log/terminal |

| read | 60 | 0 | 0,020619 | 1,13% | Đọc dữ liệu cảm biến |

clock_nanosleep chiếm tỷ lệ thời gian cao nhất do Sensor Thread chủ động chờ giữa các chu kỳ. mq_timedreceive đứng thứ hai vì Alert và Logger chờ message. futex phản ánh hoạt động đồng bộ giữa các pthread, bao gồm mutex và condition variable. Đây là pattern phù hợp với một ứng dụng đa luồng thiên về chờ sự kiện và I/O hơn là tính toán CPU liên tục.

Mười lỗi system call được phân bố trong log: futex 5, openat 1, mq_unlink 2, madvise 1 và getrandom 1. Các giá trị này chỉ là số lần syscall trả lỗi theo strace; cần xem errno và thời điểm gọi mới kết luận là lỗi chức năng. Một số trường hợp như thao tác dọn queue không tồn tại hoặc syscall bị ngắt có thể là tình huống được xử lý có chủ đích.

Tổng user time và system time của lần chạy riêng lần lượt là 0,03 giây và 0,10 giây; real time là 30,51 giây. Chênh lệch lớn giữa real time và CPU time cho thấy tiến trình dành phần lớn thời gian chờ chu kỳ, queue hoặc dữ liệu, không phải chạy phép tính liên tục.
PERF: CPU HOTSPOT VÀ ĐỀ XUẤT
Thông tin phiên perf
Bản perf được thu trên sms_app_x86 trên host. Log ghi nhận 49 mẫu của event cpu_atom/cycles/P, event count xấp xỉ 8.043.592 và Total Lost Samples bằng 0. Vì số mẫu không lớn, các tỷ lệ phần trăm cần được xem là dấu hiệu định hướng, không phải phép đo tuyệt đối.

| Hàm / vùng | Tỷ lệ Children hoặc Self trong report | Nhận xét |

|---|---|---|

| entry_SYSCALL_64_after_hwframe | 48,43% Children | Nhiều thời gian mẫu nằm trong đường vào kernel |

| do_syscall_64 | 48,43% Children | Chi phí xử lý syscall và chuyển ngữ cảnh |

| __x64_sys_futex | 15,92% Children | Đồng bộ và đánh thức thread |

| cb_push | 14,56% Children; 3,30% Self | Đẩy mẫu và đánh thức Processor qua condition variable |

| __vfscanf_internal | 11,45% Children; 8,15% Self | Phân tích chuỗi cảm biến qua sscanf |

Điểm đáng chú ý là overhead tập trung đáng kể ở đường syscall và đồng bộ. Điều này phù hợp với strace: ứng dụng thực hiện nhiều lần sleep, queue receive/send và futex. cb_push xuất hiện tương đối nổi bật vì thao tác đẩy dữ liệu đồng thời đánh thức thread tiêu thụ. __vfscanf_internal cũng có tỷ lệ đáng kể do parse chuỗi CSV thành số.
Đề xuất tối ưu
- Nếu định dạng dữ liệu cố định, cân nhắc parser chuyên biệt có kiểm tra biên thay cho sscanf tổng quát; cần benchmark lại để xác nhận lợi ích.
- Giảm log đồng bộ trên đường xử lý mẫu; gom log hoặc chỉ ghi khi có sự kiện quan trọng.
- Giữ vùng critical section của circular buffer ngắn; không thực hiện parse, file I/O hoặc gửi message khi đang giữ mutex buffer.
- Đo perf nhiều lần với thời lượng dài hơn và cùng điều kiện chạy; so sánh theo cùng loại event và cùng môi trường.
perf được thu trên host x86_64 trong khi ứng dụng đích chạy ARM/QEMU. Vì vậy, kết quả chỉ dùng để hiểu hotspot của bản host và không đại diện trực tiếp cho chu kỳ CPU trên Cortex-A9.
REAL-TIME: CẤU HÌNH VÀ ĐO JITTER
Cấu hình Sensor Thread
Sensor Thread có hai chế độ. Mặc định sử dụng SCHED_OTHER. Khi bật tùy chọn -r, thread được yêu cầu chạy với SCHED_FIFO priority 50. Chu kỳ được điều khiển bằng clock_nanosleep với CLOCK_MONOTONIC và TIMER_ABSTIME để dùng deadline tuyệt đối, hạn chế sai lệch tích lũy so với việc ngủ tương đối lặp lại.

mlockall(MCL_CURRENT | MCL_FUTURE) được thử khi bật chế độ RT nhằm hạn chế page fault do paging. Lệnh này có thể thất bại nếu giới hạn RLIMIT_MEMLOCK không đủ; vì vậy lỗi được báo cáo nhưng không nên làm ứng dụng thường bị từ chối chạy chỉ vì không có quyền khóa toàn bộ bộ nhớ.
Kết quả jitter sau khi sửa
| Chính sách | Samples | Min µs | Avg µs | Max µs | P99 µs | Overruns |

|---|---|---|---|---|---|---|

| SCHED_OTHER | 1201 | 219 | 289,10 | 669 | 470 | 0 |

| SCHED_FIFO prio 50 | 1201 | 231 | 408,54 | 953 | 798 | 0 |
Histogram
| Khoảng jitter | SCHED_OTHER | SCHED_FIFO |

|---|---|---|

| 0–100 µs | 0 (0,00%) | 0 (0,00%) |

| 100–500 µs | 1199 (99,83%) | 1125 (93,67%) |

| 500–1000 µs | 2 (0,17%) | 76 (6,33%) |

| >1 ms | 0 (0,00%) | 0 (0,00%) |

Trong lần đo này, SCHED_FIFO không cải thiện P99. P99 tăng từ 470 µs lên 798 µs, tức tăng khoảng 69,8%. Giá trị trung bình tăng từ 289,10 µs lên 408,54 µs; Max tăng từ 669 µs lên 953 µs. Cả hai chế độ đều không ghi nhận overrun và toàn bộ mẫu đều dưới 1 ms.

Một lần đo trước khi sửa driver, dưới tải nền hai tiến trình yes, cho kết quả khác hẳn: SCHED_OTHER có P99 51.616 µs và 44 overrun; SCHED_FIFO có P99 51.962 µs và 41 overrun. Kết quả đó cho thấy jitter lớn trong cấu hình cũ/tải nặng. Không nên trộn hai bộ số liệu: bảng chính sử dụng kết quả sau sửa; bộ cũ được giữ làm bằng chứng quá trình kiểm tra và tối ưu.
ĐÁNH GIÁ REAL-TIME VÀ HƯỚNG CẢI TIẾN
SCHED_FIFO có cải thiện không?
Không trong bộ số liệu chính hiện có. P99 của FIFO cao hơn OTHER khoảng 69,8%. SCHED_FIFO là chính sách ưu tiên real-time, nhưng không đảm bảo mọi phép đo đều tốt hơn. Kết quả còn phụ thuộc tải nền, cấu hình QEMU, vCPU, IRQ, quyền scheduling, thời gian chạy và các thao tác blocking trong thread. Nếu Sensor Thread bị chặn trong read hoặc chịu ảnh hưởng từ driver/virtual device, tăng độ ưu tiên userspace không loại bỏ được toàn bộ nguồn trễ.
Ngưỡng jitter cho chu kỳ 500 ms
Với hệ thống giám sát cảm biến chu kỳ 500 ms, có thể đặt mục tiêu kỹ thuật ban đầu là jitter dưới 1 ms (0,2% chu kỳ) trong điều kiện vận hành đã xác định. Đây là tiêu chí thiết kế tham khảo, không phải giới hạn phổ quát cho mọi hệ thống công nghiệp. Trong lần đo chính, Max của SCHED_OTHER là 669 µs và Max của FIFO là 953 µs; cả hai đều dưới 1 ms và không có overrun. Tuy nhiên, cần kiểm thử thêm dưới tải nặng và trên phần cứng đích trước khi tuyên bố đạt yêu cầu sản phẩm.
Các hướng giảm jitter
- Sử dụng PREEMPT_RT nếu yêu cầu deadline chặt hơn và nền tảng hỗ trợ.
- Cố định CPU affinity cho Sensor Thread và cân nhắc CPU isolation để giảm tranh chấp scheduler.
- Điều chỉnh IRQ affinity, tránh để các IRQ không liên quan làm gián đoạn CPU xử lý cảm biến.
- Giảm thao tác blocking trong Sensor Thread; tách I/O chậm khỏi phần đo thời điểm wake-up nếu kiến trúc cho phép.
- Đo nhiều lượt, ghi nhận Min/Avg/Max/P99 và số overrun trong cùng điều kiện tải.
Khi đánh giá real-time, Max và P99 quan trọng hơn chỉ nhìn Avg vì một số ít lần trễ lớn có thể làm lỡ deadline dù phần lớn chu kỳ vẫn đúng. QEMU hữu ích cho kiểm tra logic nhưng không thay thế phép đo trên phần cứng đích.
THỜI GIAN CHẠY VÀ KIỂM TRA ỔN ĐỊNH
Đo thời gian thực thi
| Lệnh / tham số | Kết quả |

|---|---|

| /root/sms_app -i 500 -d 30 | Chạy với chu kỳ 500 ms trong 30 giây |

| real | 30,51 giây |

| user | 0,03 giây |

| sys | 0,10 giây |

Thời gian thực tế dài hơn thời lượng cấu hình khoảng 0,51 giây. Độ chênh có thể đến từ thời điểm khởi tạo, chờ chu kỳ cuối, shutdown và scheduler. CPU time rất thấp so với real time, củng cố nhận định rằng ứng dụng chủ yếu chờ I/O và đồng bộ.
Kiểm tra dừng ứng dụng
Khi kết thúc, ứng dụng cần dừng Sensor Thread trước để không phát sinh thêm mẫu, đánh thức Processor nếu đang chờ buffer, cho Processor xử lý phần dữ liệu còn lại, rồi dừng Alert và Logger sau khi các message cần thiết đã được gửi. Sau đó join các thread, flush/đóng file, đóng device descriptor và unlink POSIX queues.

Phiên chạy mẫu kết thúc với exit=0, 120 mẫu được tạo và tiêu thụ, không có mẫu tồn đọng. /proc/sms_stats sau khi kết thúc ghi read_count=120. Các kết quả này phù hợp với shutdown bình thường trong lần kiểm thử đã ghi log.
REFLECTION – NHÌN LẠI QUÁ TRÌNH THỰC HIỆN
Khó khăn lớn nhất
Khó khăn lớn nhất trong assignment là phối hợp nhiều cơ chế cùng lúc: driver kernel, thread userspace, circular buffer, message queue và shutdown. Một lỗi ở một thread có thể biểu hiện ở thành phần khác. Ví dụ, nếu Logger không xử lý queue đúng cách, queue có thể đầy và làm thread gửi bị chặn; khi đó quá trình join lúc kết thúc có nguy cơ bị treo. Vì vậy, việc thiết kế thứ tự dừng và cơ chế đánh thức thread quan trọng không kém việc xử lý dữ liệu bình thường.
Bài học từ công cụ debug
Valgrind giúp phát hiện vấn đề mà chương trình vẫn có thể chạy bình thường, như byte chưa khởi tạo trong cấu trúc message. GDB giúp quan sát thread nào đang chạy và thread nào đang chờ. strace cho thấy ứng dụng dành nhiều thời gian cho sleep, queue và futex; perf bổ sung góc nhìn về chi phí syscall, đồng bộ và parse dữ liệu. Mỗi công cụ trả lời một loại câu hỏi khác nhau nên cần kết hợp thay vì dựa vào một kết quả duy nhất.
Nếu thực hiện lại
Nếu làm lại, tôi sẽ xác định giao thức shutdown và quyền sở hữu tài nguyên ngay từ đầu: thread nào tạo queue, thread nào đóng queue, khi nào gửi stop message và thứ tự join ra sao. Tôi cũng sẽ xây dựng các bài test lỗi có chủ đích sớm hơn, gồm đường dẫn thiết bị sai, queue không tạo được, logger không mở được file và sensor bị ngắt. Với real-time, tôi sẽ đo baseline trước, thay đổi từng yếu tố một và giữ nguyên điều kiện thử nghiệm để tránh kết luận sai.
Kỹ năng sẽ sử dụng trong công việc
Các kỹ năng có giá trị nhất đối với công việc Embedded Linux là đọc log kernel, hiểu device file và ioctl, quản lý vòng đời thread, phân tích memory map, dùng GDB/strace/perf và đo latency có phương pháp. Đây là các kỹ năng có thể áp dụng cho driver cảm biến, gateway công nghiệp, thiết bị IoT và hệ thống điều khiển.
Cải tiến muốn phát triển
Một hướng mở rộng là bổ sung giao diện giám sát từ xa bằng MQTT và dashboard. Sensor data và alarm có thể được gửi đến broker; dashboard hiển thị nhiệt độ, độ ẩm, lịch sử và trạng thái cảnh báo. Khi triển khai cần bổ sung xác thực, giới hạn tốc độ gửi, xử lý mất kết nối và cơ chế lưu tạm dữ liệu để tránh làm ảnh hưởng chu kỳ đọc cảm biến.
KẾT LUẬN VÀ TÀI LIỆU THAM KHẢO
Kết luận
Assignment đã xây dựng được mô hình Sensor Monitoring System gồm kernel driver và ứng dụng userspace đa luồng. Driver cung cấp dữ liệu cảm biến qua /dev/sms_sensor và thống kê qua /proc/sms_stats. Ứng dụng sử dụng bốn thread, circular buffer, POSIX message queues và logger riêng. Trong phiên chạy 60 giây, 120 mẫu được tạo và tiêu thụ, không có buffer overflow và read_count tăng đúng 120.

Valgrind ghi nhận 23 allocations, 23 frees, 0 byte còn sử dụng và 0 lỗi. Memory footprint từ smaps_rollup ổn định ở 544 kB RSS tại ba mốc; cần lưu ý trường VmRSS trong status có số liệu không khớp. GDB xác nhận hoạt động của các thread và breakpoint trong Processor Thread. strace cho thấy ứng dụng thiên về chờ chu kỳ/IPC; perf cho thấy chi phí syscall, futex, circular buffer và parse CSV là các khu vực đáng quan sát.

Trong lần đo jitter chính, SCHED_FIFO priority 50 không cho P99 thấp hơn SCHED_OTHER. Kết quả này nhấn mạnh rằng real-time scheduling phải được đánh giá bằng số liệu trong điều kiện xác định, không thể suy luận chỉ từ tên chính sách. QEMU phù hợp kiểm tra chức năng và quy trình, nhưng đánh giá cuối cùng cần thực hiện trên phần cứng đích.
Tài liệu tham khảo
- Tài liệu Assignment 02 – Embedded Linux Systems: Write Embedded Application – Sensor Monitoring System, các mục Task 1 đến Task 5.
- Linux man-pages: pthreads, pthread_mutex_lock, pthread_cond_wait, mq_open, mq_send, mq_receive, clock_nanosleep, sched_setscheduler, mlockall, procfs.
- Linux kernel documentation: character device drivers, procfs, timekeeping và scheduler.
- GNU GDB Manual – debugging programs and threads.
- Valgrind User Manual – Memcheck.
- strace manual page và perf documentation.
- Log thực nghiệm của dự án: sms_sample_run.txt, proc_sms_stats_truoc.txt, proc_sms_stats_giua.txt, proc_sms_stats_sau.txt, memory_footprint.txt, proc_maps_sms.txt, gdb_session.txt, perf_sms.txt, jitter_report.txt, jitter_before_fix_console.txt, dmesg_full.txt.
