# -*- coding: utf-8 -*-
#
# 按 error.txt 里记录的行号，把 MainFrame.cpp 中「指针 -> LONG/INT」的截断
# 统一改成 LONG_PTR。用法（PowerShell / CMD）：
#     python fix_mainframe.py MainFrame.cpp MainFrame.cpp
#
# 只改 error.txt 记录的那 24 行，改前自动备份为 .bak，并打印改动供核对。
import sys, re, shutil

# error.txt 中 MainFrame.cpp 的 C4311 行号
LINES = [1236,1256,1429,2082,2160,2170,2183,2284,2287,2290,2293,2296,2299,2302,
         2349,2352,2395,2398,2469,2479,2505,2508,2615,2623]

src, dst = sys.argv[1], sys.argv[2]
shutil.copyfile(src, src + '.bak')

with open(src, 'rb') as f:
    data = f.read()
text = data.decode('latin-1')          # 字节级无损，保留 Shift-JIS 注释
lines = text.split('\n')

changed = []
for ln in LINES:
    i = ln - 1
    if i >= len(lines):
        print('!! 行号越界:', ln); continue
    old = lines[i]
    # (LONG)xxx / (INT)xxx  --->  (LONG_PTR)xxx
    new = re.sub(r'\((LONG|INT)\)', '(LONG_PTR)', old)
    if new != old:
        lines[i] = new
        changed.append((ln, old.strip(), new.strip()))

with open(dst, 'wb') as f:
    f.write('\n'.join(lines).encode('latin-1'))

print('修改 %d 行：' % len(changed))
for ln, o, n in changed:
    print('%5d\n   旧: %s\n   新: %s' % (ln, o, n))
if not changed:
    print('（无改动，请确认行号是否仍匹配）')
