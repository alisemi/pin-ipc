/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <fstream>
#include <iostream>
#include <string>

#include <cassert>
#include <map>

#include <fcntl.h>
#include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "pin.H"

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;

/* ================================================================== */
// Global variables
/* ================================================================== */
int numThreads;

const int BUFFER_SIZE = 64;

int threadCounter = 0;
struct thread_info {
    string pathName;
    NATIVE_FD fd;
};
std::map<int, thread_info> threadMapping;
std::vector<int> bubbles;
NATIVE_FD main_fd;

/* ===================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "simulation_output", "specify file base name");
KNOB<string> KnobNumThreads(KNOB_MODE_WRITEONCE, "pintool", "t", "16", "specify number of threads");

PIN_LOCK pinLock;

/* ===================================================================== */

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage() {
    cerr << "This tool redirects the memory traces to named pipes along with (i.e., non-memory) instructions before the memory request " << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID RecordIp(VOID *ip, THREADID tid) {
    bubbles[static_cast<int>(tid)] += 1;    
}

VOID RecordMemAddrRead(VOID *addr, THREADID tid) {
    string command = std::to_string(bubbles[static_cast<int>(tid)]) + " " +
                        std::to_string(reinterpret_cast<intptr_t>(addr)) + " R\n";
    char buffer[BUFFER_SIZE];
    strcpy(buffer, command.c_str());
    write(threadMapping[static_cast<int>(tid)].fd, &buffer, BUFFER_SIZE);
    bubbles[static_cast<int>(tid)] = 0;
}

VOID RecordMemAddrWrite(VOID *addr, THREADID tid) {
    string command = std::to_string(bubbles[static_cast<int>(tid)]) + " " +
                        std::to_string(reinterpret_cast<intptr_t>(addr)) + " W\n";
    char buffer[BUFFER_SIZE];
    strcpy(buffer, command.c_str());
    write(threadMapping[static_cast<int>(tid)].fd, &buffer, BUFFER_SIZE);
    bubbles[static_cast<int>(tid)] = 0;
}

VOID Instruction(INS ins, VOID *v) {
    if (!INS_IsSyscall(ins)) {

        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordIp, IARG_INST_PTR, IARG_THREAD_ID, IARG_END);

        // send addresses for memory instructions
        if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
            for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); ++i) {
                // pass load address
                if (INS_MemoryOperandIsRead(ins, i)) {
                    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAddrRead, IARG_MEMORYOP_EA, i,
                                             IARG_THREAD_ID, IARG_END);
                }
                // pass store address
                if (INS_MemoryOperandIsWritten(ins, i)) {
                    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemAddrWrite, IARG_MEMORYOP_EA, i,
                                             IARG_THREAD_ID, IARG_END);
                }
            }
        }
    }
}

/*!
 * Increase counter of threads in the application.
 * This function is called for every thread created by the application when it is
 * about to start running (including the root thread).
 * @param[in]   threadIndex     ID assigned by PIN to the new thread
 * @param[in]   ctxt            initial register state for the new thread
 * @param[in]   flags           thread creation flags (OS specific)
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddThreadStartFunction function call
 */
VOID ThreadStart(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v) {
    std::cout << "Thread start called with tid = " << static_cast<int>(threadIndex) << std::endl;
    // assert(threadCounter < numThreads);
    PIN_GetLock(&pinLock, threadIndex);

    string filePath = KnobOutputFile.Value();

    string fifo_fd = filePath + "_" + std::to_string(threadCounter);
    std::cout << fifo_fd << "\n";

    if (threadCounter == 0) {
        // we already opened the pipe
        threadMapping[static_cast<int>(threadIndex)] = {fifo_fd, main_fd};
    } else {
        NATIVE_FD fd;

        OS_RETURN_CODE return_code;
        return_code = OS_OpenFD(fifo_fd.c_str(), OS_FILE_OPEN_TYPE_CREATE | OS_FILE_OPEN_TYPE_WRITE, 0, &fd);

        if (OS_RETURN_CODE_NO_ERROR != return_code) {
            PIN_ExitProcess(1);
        }

        threadMapping[static_cast<int>(threadIndex)] = {fifo_fd.c_str(), fd};
    }
    bubbles.push_back(0);

    threadCounter = (threadCounter+1) % numThreads;

    PIN_ReleaseLock(&pinLock);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v) {
    for (auto it = threadMapping.begin(); it != threadMapping.end(); it++) {
        // close(it->second.fd);                /* close pipe from read end */
        OS_CloseFD(it->second.fd);

        // Unlink done on the enclosing script since mkfifo done there as well
        // unlink(it->second.pathName.c_str()); /* unlink from the underlying file */
    }
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[]) {
    PIN_InitSymbols();

    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // OutFile.open(KnobOutputFile.Value().c_str());

    numThreads = stoi(KnobNumThreads.Value()); // Set this the same as the application threads in the ROI

    std::cout << "Number of threads are " << numThreads << "\n";

    string filePath = KnobOutputFile.Value();
    string fifo_fd = filePath + "_" + std::to_string(threadCounter);
    // mkfifo fails for some reasons in PinCRT, mkfifo should be called exclusively on the command line
    // mkfifo(fifo_fd.c_str(), 0666);                      /* read/write for user/group/others */

    OS_RETURN_CODE return_code;
    return_code = OS_OpenFD(fifo_fd.c_str(), OS_FILE_OPEN_TYPE_CREATE | OS_FILE_OPEN_TYPE_WRITE, 0, &main_fd);

    if (OS_RETURN_CODE_NO_ERROR != return_code) {
        PIN_ExitProcess(1);
    }

    PIN_InitLock(&pinLock);

    // Register function to be called to instrument traces
    INS_AddInstrumentFunction(Instruction, 0);

    // Register function to be called for every thread before it starts running
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
