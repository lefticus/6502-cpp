avr-gcc $1 -Wall -Wextra -c -o- -S -O3 -I ~/avr-libstdcpp/include/ -std=c++20 | tee $1.asm | ./avr-to-6502 | tee $1.asm && xa -O PETSCREEN  -M $1.prg
