#include <condition_variable>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>
#include <deque>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>

#include <sys/syscall.h>
#include <unistd.h>

const int QUEUE_SIZE = 512;

struct ThreadInfo {
    std::deque<std::string> traces;
    int fd;
    std::counting_semaphore<QUEUE_SIZE> number_of_queueing_portions{0};
    std::counting_semaphore<QUEUE_SIZE> number_of_empty_positions{QUEUE_SIZE};
    std::mutex buffer_manipulation;
    volatile bool done;
};

void readTraces(ThreadInfo *threadInfo);

void consumeTraces(ThreadInfo *threadInfo);

int main(int argc, char *argv[]) {

    if (argc != 3) {
        std::cout << "Call the program as ./trace-ipc-app OUTPUT_BASE_NAME NUM_THREADS\n";
        exit(1);
    }

    std::string output_base_name = argv[1];
    int numThreads = std::stoi(argv[2]);

    std::cout << "Reading " << output_base_name << ", number of threads are " << numThreads << "\n";

    std::vector<ThreadInfo> threadData(numThreads);
    std::vector<std::thread> readerThreads;
    std::vector<std::thread> consumerThreads;

    for (int i = 0; i < numThreads; i++) {
        std::string outputFileName = output_base_name + "_" + std::to_string(i);
        std::cout << "Reading " << outputFileName << "\n";
        int fd = open(outputFileName.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cerr << "couldn't read file with name outputFileName!\n";
            return -1; /* no point in continuing */
        }

        threadData[i].fd = fd;
        threadData[i].done = false;

        std::thread readerThread(readTraces, &(threadData[i]));
        readerThreads.push_back(std::move(readerThread));

        std::thread consumerThread(consumeTraces, &(threadData[i]));
        consumerThreads.push_back(std::move(consumerThread));
    }

    for (int i = 0; i < numThreads; i++) {
        readerThreads[i].join();
        consumerThreads[i].join();
    }

    return 0;
}

void readTraces(ThreadInfo *threadInfo) {
    // std::cout << "Entered to the readTraces thread function..\n";

    while (1) {
        char buffer[64];
        ssize_t count = read(threadInfo->fd, &buffer, 64);

        if (0 == count) {
            strcpy(buffer, "END");
        } else {
            std::cout << "Read " << count << " bytes, values is " << buffer;
        }

        threadInfo->number_of_empty_positions.acquire();
        {
            std::lock_guard<std::mutex> g(threadInfo->buffer_manipulation);
            threadInfo->traces.push_back(buffer);
        }
        threadInfo->number_of_queueing_portions.release();

        if (0 == count) {
            break;
        }
    }

    close(threadInfo->fd);    /* close pipe from read end */
    threadInfo->done = true;
    // std::cout << "readTraces Done!\n";
}

void consumeTraces(ThreadInfo *threadInfo) {
    // std::cout << "Entered to the consumeTraces thread function..\n";

    while(!(threadInfo->done)){
        threadInfo->number_of_queueing_portions.acquire();
        std::string trace;
        {
            std::lock_guard<std::mutex> g(threadInfo->buffer_manipulation);
            trace = threadInfo->traces.front();
            threadInfo->traces.pop_front();
        }
        threadInfo->number_of_empty_positions.release();

        // Process the data
        std::cout << "Trace is consumed: " << trace << "\n";

        if(trace == "END"){
            break;
        }
    }

    // Read traces must be done, process the last portions of the traces
    while(!(threadInfo->traces.empty())){
        std::string trace;
        trace = threadInfo->traces.front();
        threadInfo->traces.pop_front();

        // Process the data
        std::cout << "Trace is consumed: " << trace << "\n";
    }

    

    // std::cout << "consumeTraces Done!\n";
}