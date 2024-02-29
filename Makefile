all: main

libexample.a: example.o
	ar rcs libexample.a example.o

example.o: example.c
	g++ -c example.c -o example.o -I/usr/include/opencv4

main: main.go libexample.a uploads
	go mod tidy
	go build main.go

uploads: 
	mkdir -p uploads

clean:
	rm -f main libexample.a example.o go.sum
	go clean
