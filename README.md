g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp $(pkg-config --cflags --libs x11) -o build/debug/mew
