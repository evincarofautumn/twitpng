main : main.cpp
	clang++ main.cpp -o main -std=c++11 -stdlib=libc++ -lpng -lgmpxx -lgmp -Wall -g
