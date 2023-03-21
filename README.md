# pin-attach-detach
A simple tool directing the memory traces from pin into named pipes.

Usually, people write traces into a file and use those trace files to design their simulators. However, this requires huge disk space. This tool instead writes the trace output onto named pipes in FIFO manner so that a simulator get can those traces in real-time.

The pin tool at pin-tool/trace-ipc.cpp file instruments a sample application (sample-app-to-trace.cpp) and writes the trace information onto 
named pipes.

Another program (e.g. a simulator, trace-ipc-app.cpp) reads from those pipes in a multi-threaded fashion. The simulator programs is inherently designed as a producer-consumer model to keep memory space low. It reads the traces by a limited size, then it consumes them. For example, traces are stored in memory by a limited size, and then consumed one-by-one in a simulator.

You should first compile the pin-tool:

```
$export PIN_ROOT=/PATH/TO/YOUR_PIN/pin-3.xxx
$cd pin-tool
$make
$cd ..
```

Next, compile the test tool

```
$make
```

Run the sample application with the pin-tool instrumentation, together with the parallel simulation.

```
$./test.sh
```