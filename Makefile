all: test pipe execlp myecho

test: main.cpp
	g++ main.cpp -o test

pipe: pipe.cpp
	g++ pipe.cpp -o pipe

execve: execlp.cpp
	g++ execlp.cpp -o execlp

myecho: myecho.cpp
	g++ myecho.cpp -o myecho
