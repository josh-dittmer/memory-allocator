gcc -g -fPIC -c mms.cpp
gcc -g -o libmms.so -shared mms.o
gcc -L. -o mmc mmc.cpp -lmms -Wl,-rpath=./
gcc -L. -o test test.cpp -lmms -Wl,-rpath=./
gcc -L. -o test2 test2.cpp -lmms -Wl,-rpath=./