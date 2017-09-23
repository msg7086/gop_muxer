default: gop_muxer.exe

gop_muxer.exe: gop_muxer.o
	g++ -static -static-libgcc -static-libstdc++ -o $@ $< -llsmash

gop_muxer.o: gop_muxer.cpp gop_muxer.h
	g++ -O2 --std=c++11 -c gop_muxer.cpp