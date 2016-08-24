#! /bin/bash

~/llvm-build/bin/clang++ -std=c++1z -c -O3 -o- -Wall -Wextra -m32 -march=i386 -ggdb -S $1 > $1.x86.asm
cat $1.x86.asm | ~/x86-to-6502-build/x86-to-6502 > $1.6502.asm
cat $1.6502.asm | sed -e "/^\t\..*$/d" > $1.asm
~/Downloads/TMPx_v1.1.0-STYLE/linux-x86_64/tmpx $1.asm

