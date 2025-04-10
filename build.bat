windres -coff -o resource.o resource.rc
gcc -o main main.c resource.o -lgdi32 -luxtheme -lcomctl32 -mwindows && strip main.exe
