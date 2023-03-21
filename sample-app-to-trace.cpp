#include <condition_variable>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <iterator>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Simple multithreaded app

int SIZE_PER_THREAD = 1000;


struct ThreadInfo {
    std::vector<int> randArray;
    int sum;
};

void sumValues(ThreadInfo *threadInfo);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cout << "Call the program as ./trace-ipc-app NUM_THREADS\n";
        exit(1);
    }

    int numThreads = std::stoi(argv[1]);

    std::cout << "Sample multi-threaded app, number of threads are " << numThreads << "\n";

    std::vector<ThreadInfo> threadData(numThreads);
    std::vector<std::thread> sampleThreads;

    for (int i = 0; i < numThreads; i++) {
        std::thread consumerThread(sumValues, &(threadData[i]));
        sampleThreads.push_back(std::move(consumerThread));
    }

    int totalSum = 0;
    for (int i = 0; i < numThreads; i++) {
        sampleThreads[i].join();
        totalSum += threadData[i].sum;
    }

    std::cout << "Total sum is " << totalSum << "\n";



    return 0;
}

void sumValues(ThreadInfo *threadInfo) {
    // First create an instance of an engine.
    std::random_device rnd_device;
    // Specify the engine and distribution.
    std::mt19937 mersenne_engine {rnd_device()};  // Generates random integers
    std::uniform_int_distribution<int> dist {1, 52};
    
    auto gen = [&dist, &mersenne_engine](){
                   return dist(mersenne_engine);
               };

    threadInfo->randArray.resize(SIZE_PER_THREAD);

    std::generate(begin(threadInfo->randArray), end(threadInfo->randArray), gen);
    threadInfo->sum = 0;

    for(int i=0; i < SIZE_PER_THREAD; i++){
      threadInfo->sum += threadInfo->randArray[i];
    }
    
}
