#!/bin/bash

# test1.bin: {1,2,3,4}
printf "\x01\x02\x03\x04" > test1.bin

# test2.bin: два 4-байтовых числа
# 0x01020304 и 0x10111213 (little-endian)
printf "\x04\x03\x02\x01" > test2.bin
printf "\x13\x12\x11\x10" >> test2.bin

# test3.bin: три 4-байтовых числа
# 0xF0F0F0F0, 0x00FF00FF, 0xFFFF0000 (little-endian)
printf "\xF0\xF0\xF0\xF0" > test3.bin
printf "\xFF\x00\xFF\x00" >> test3.bin
printf "\x00\x00\xFF\xFF" >> test3.bin

echo "Binary test files created:"
ls -l test1.bin test2.bin test3.bin
