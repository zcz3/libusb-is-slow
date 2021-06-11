
rm -f enumerate.exe

x86_64-w64-mingw32-gcc -o enumerate.exe enumerate.c -lsetupapi -L. -lusb-1.0
