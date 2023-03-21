#
# A simple makefile
#


all: trace-ipc-app sample-app-to-trace

trace-ipc-app: trace-ipc-app.cpp 
	g++ -O3 -Wall -std=c++20 -lpthread -o ./trace-ipc-app.out ./trace-ipc-app.cpp

sample-app-to-trace: sample-app-to-trace.cpp 
	g++ -O2 -Wall -std=c++20 -lpthread -o ./sample-app-to-trace.out ./sample-app-to-trace.cpp


test-pin: trace-ipc-app.cpp
	$(PIN_ROOT)/pin -t pin-tool/obj-intel64/trace-ipc.so -- ./trace-ipc-app.out 

clean: 
	rm -rf ./*.out