#!/bin/bash
# Kiem tra repo da hoan chinh truoc khi push.
#   ./scripts/check_repo.sh          -> chay tay
#   .git/hooks/pre-push goi script nay; loi (exit 1) se chan git push.
# Kiem tra noi dung DA COMMIT (git ls-files / HEAD), khong phai file rac trong working tree.

cd "$(git rev-parse --show-toplevel)" || exit 1
ERR=0; WARN=0
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$1"; }
fail() { printf '  \033[31mLOI\033[0m  %s\n' "$1"; ERR=$((ERR+1)); }
warn() { printf '  \033[33mCHU Y\033[0m %s\n' "$1"; WARN=$((WARN+1)); }
tracked() { git ls-files --error-unmatch "$1" >/dev/null 2>&1; }

echo "== 1. File bat buoc cua tung lab"
REQ=(
  lab01/configs/kernel.config lab01/configs/busybox.config
  lab01/rootfs/initramfs/init lab01/rootfs/initramfs/etc/inittab lab01/rootfs/initramfs/etc/init.d/rcS
  lab01/logs/partA_build.txt lab01/logs/partB_uboot.txt lab01/logs/partB_kernel_panic.txt
  lab01/logs/partC_boot_smp2.txt
  lab02/driver/lab2_driver.c lab02/driver/Makefile
  lab02/rootfs/initramfs/etc/inittab lab02/rootfs/initramfs/etc/init.d/rcS
  lab02/rootfs/initramfs/usr/bin/test_driver.sh lab02/rootfs/initramfs/usr/bin/test_procfs.sh
  lab02/logs/boot_log.txt lab02/logs/driver_test.txt lab02/logs/procfs_test.txt lab02/logs/mtd_jffs2.txt
  lab02/mtd/run_part_c.sh
  lab03/apps/buggy_app.c lab03/apps/leaky_app.c lab03/apps/leaky_app_fixed.c lab03/apps/perf_target.c
  lab03/apps/Makefile
  lab03/logs/gdb_session.txt lab03/logs/valgrind_report.txt lab03/logs/valgrind_fixed.txt
  lab03/logs/strace_summary.txt lab03/logs/perf_report.txt lab03/logs/perf_stat.txt
  lab03/logs/hist_standard.txt lab03/logs/latency_histogram.png
  asgn01/task1_ioctl/{asgn1_driver.c,asgn1_ioctl.h,Makefile,test_asgn1.sh}
  asgn01/task2_misc/{counter_driver.c,Makefile,test_counter.sh}
  asgn01/logs/{test_asgn1_output.txt,test_counter_output.txt,dmesg_full.txt}
  asgn02/driver/{sms_sensor_driver.c,sms_sensor.h,Makefile}
  asgn02/app/{sms_app.c,Makefile}
  asgn02/app/{sensor,processor,alert,logger}_thread.{c,h}
  asgn02/app/circular_buffer.{c,h}
  asgn02/logs/{sms_sample_run.txt,sms.log,valgrind_sms.txt,strace_sms.txt,perf_sms.txt,jitter_report.txt}
)
miss=0; for f in "${REQ[@]}"; do tracked "$f" || { fail "thieu $f"; miss=1; }; done
[ $miss = 0 ] && ok "du ${#REQ[@]} file bat buoc"

echo "== 2. Bao cao (MD + PDF, PDF khong cu hon MD)"
for n in 01 02 03; do
  md=lab$n/bao_cao/SE183502_Lab${n}_BaoCao.md; pdf=lab$n/bao_cao/SE183502_Lab${n}_BaoCao.pdf
  if ! tracked "$md" || ! tracked "$pdf"; then fail "LAB-$n thieu $( tracked "$md" || echo MD ) $( tracked "$pdf" || echo PDF )"; continue; fi
  [ "$(head -c4 "$pdf")" = "%PDF" ] || fail "$pdf khong phai file PDF hop le"
  if [ "$md" -nt "$pdf" ]; then warn "LAB-$n: MD moi hon PDF – nho xuat lai PDF"; else ok "LAB-$n: co MD + PDF, PDF cap nhat"; fi
  grep -q '^## Mục 1' "$md" && grep -q 'SE183502' "$md" || fail "LAB-$n: bao cao thieu Muc 1 / MSSV"
done

echo "== 3. Khong co file nhi phan / file qua lon"
bad=0
while IFS= read -r f; do
  case "$f" in *.pdf|*.png|*.svg) continue ;; esac
  if head -c4 "$f" | grep -q $'\x7fELF'; then fail "file ELF: $f"; bad=1; fi
done < <(git ls-files)
for f in $(git ls-files | grep -E '\.(ko|o|docx|cpio|cpio\.gz|tar\.gz|tar\.xz|tar\.bz2|data)$|(^|/)(zImage|u-boot)$'); do
  fail "file build/khong nen dua len: $f"; bad=1; done
while IFS= read -r f; do
  s=$(stat -c %s "$f"); [ "$s" -gt 5242880 ] && { fail "file lon hon 5 MB: $f ($((s/1048576)) MB)"; bad=1; }
done < <(git ls-files)
[ $bad = 0 ] && ok "khong co ELF, .ko, .docx, image; moi file < 5 MB ($(git ls-files | wc -l) file)"

echo "== 4. Thong tin nhay cam"
leak=$(git grep -nIE 'gh[pousr]_[A-Za-z0-9]{20,}|github_pat_|BEGIN [A-Z ]*PRIVATE KEY|kien1362004@|AKIA[0-9A-Z]{16}' -- . ':!scripts/check_repo.sh' 2>/dev/null)
if [ -n "$leak" ]; then fail "co the lo thong tin nhay cam:"; echo "$leak" | head -5 | sed 's/^/        /'; else ok "khong thay token, private key, email that"; fi

echo "== 5. Link trong README"
broken=0
for l in $(grep -oE '\]\([^)#]+\)' README.md | sed -E 's/^\]\(|\)$//g' | grep -vE '^https?://'); do
  [ -e "${l%/}" ] || { fail "README link hong: $l"; broken=1; }
done
[ $broken = 0 ] && ok "moi link noi bo trong README deu ton tai"

echo "== 6. Commit va working tree"
bademail=$(git log --format='%ae' | grep -v 'users.noreply.github.com' | sort -u)
[ -n "$bademail" ] && fail "commit dung email khong phai noreply: $bademail" || ok "moi commit dung email noreply"
if [ -n "$(git status --porcelain)" ]; then warn "con thay doi chua commit (se KHONG duoc push):"; git status --short | head -5 | sed 's/^/        /'; else ok "working tree sach"; fi

echo
if [ $ERR -gt 0 ]; then echo "KET QUA: $ERR loi, $WARN chu y – CHUA san sang push"; exit 1; fi
echo "KET QUA: 0 loi, $WARN chu y – san sang push"
