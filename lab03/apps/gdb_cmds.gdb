# LAB-03 Phan A – GDB remote debug buggy_app (ARM) trong QEMU
# Chay: gdb-multiarch -q -batch -x gdb_cmds.gdb buggy_app
set pagination off
set confirm off
set trace-commands on
set print pretty on

# ---- Thiet lap va ket noi ----
set architecture arm
set sysroot /home/kien/embedded_lab3/rootfs/initramfs
target remote localhost:1234

# ---- Dat breakpoint ----
break main
break process_data
break create_message
break calculate
info breakpoints

# ---- Den main ----
continue
backtrace
info locals
next
next
next
print arr
print arr[0]@5
print &arr[0]
print &arr[5]
print arr[5]

# ---- Bug 1: Array out-of-bounds trong process_data ----
continue
backtrace
info args
print len
print data[0]@len
print &data[len]
print data[len]
print/x data[len]
print/x __stack_chk_guard
next
next
print i
# QEMU khong ho tro hardware watchpoint cho process trong guest
# ("Could not insert hardware watchpoint") -> dung software watchpoint
set can-use-hw-watchpoints 0
watch i
continue
print i
print sum
continue
print i
continue
print i
continue
print i
continue
print i
print sum
print data[i]
print/x data[i]
delete 5
next
next
info locals
print sum
print 10+20+30+40+50

# ---- Bug 2: Use after free trong create_message ----
continue
backtrace
next
next
print buf
x/s buf
next
print buf
x/s buf
x/24xb buf
next
finish
print $r0
x/s $r0

# ---- Bug 3: Integer overflow trong calculate ----
continue
backtrace
info args
print a
print b
print (long long)a * b
print 2147483647
step
print result
print/x result
print/x (long long)a * b
info registers pc sp lr
info registers
disassemble

# ---- Ket thuc ----
continue
