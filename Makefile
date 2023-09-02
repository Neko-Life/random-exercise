all: test pipe execlp myecho one-pipe demo test-pipe one-pipe-ffmpeg test-fifo

test: main.cpp
	g++ main.cpp -o test

test-pipe: main-pipe.cpp
	g++ -g main-pipe.cpp -o test-pipe

test-fifo: main-pipe-fifo.cpp
	g++ -g main-pipe-fifo.cpp -o test-fifo

pipe: pipe.cpp
	g++ pipe.cpp -o pipe

one-pipe: one-pipe.cpp
	g++ one-pipe.cpp -o one-pipe

execve: execlp.cpp
	g++ execlp.cpp -o execlp

myecho: myecho.cpp
	g++ myecho.cpp -o myecho

demo: demo.cpp
	g++ demo.cpp -o demo

one-pipe-ffmpeg: one-pipe-ffmpeg.cpp
	g++ one-pipe-ffmpeg.cpp -o one-pipe-ffmpeg
