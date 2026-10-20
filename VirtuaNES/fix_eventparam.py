# -*- coding: utf-8 -*-
#
# 通用修复：把 Emu.EventParam / Emu.EventParam2 调用里
#   「指针 -> (LONG) / (INT)」的截断改成不截断的 (LONG_PTR)。
#
# 用法：
#   python fix_eventparam.py MainFrame.cpp              # 就地改（自动备份 .bak）
#   python fix_eventparam.py MainFrame.cpp out.cpp      # 输出到新文件
#   python fix_eventparam.py *.cpp                      # 批量
#
# 只动 EventParam / EventParam2 这两类调用，其他 (LONG)/(INT) 一律不碰，
# 因此对小整数参数（比如声道号、磁盘命令号）不会误伤。
import sys, re, shutil, os

CALL = re.compile(r'(EventParam2?\s*\()([^\n;]*)', re.S)

def fix_line(line):
    m = CALL.search(line)
    if not m:
        return line, False
    head, args = m.group(1), m.group(2)
    new_args = re.sub(r'\((LONG|INT)\)', '(LONG_PTR)', args)
    if new_args == args:
        return line, False
    return line[:m.start()] + head + new_args + line[m.end():], True

def process(src, dst):
    with open(src, 'rb') as f:
        text = f.read().decode('latin-1')   # 字节级无损，保留 Shift-JIS 注释
    lines = text.split('\n')
    changed = []
    for i, L in enumerate(lines):
        new, ok = fix_line(L)
        if ok:
            lines[i] = new
            changed.append((i + 1, L.strip(), new.strip()))
    if src == dst and changed:
        shutil.copyfile(src, src + '.bak')
    with open(dst, 'wb') as f:
        f.write('\n'.join(lines).encode('latin-1'))
    return changed

total = 0
for path in sys.argv[1:]:
    dst = path
    if len(sys.argv) == 3 and sys.argv[2] != sys.argv[1]:
        dst = sys.argv[2]
    ch = process(path, dst)
    total += len(ch)
    print('=== %s : %d 处 ===' % (path, len(ch)))
    for ln, o, n in ch:
        print('%6d\n   旧: %s\n   新: %s' % (ln, o, n))
print('\n共修改 %d 处' % total)
