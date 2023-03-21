#!/bin/bash -l

NUM_THREADS=16
FIFO_OUTPUT_PATH_BASE=./example

# Set your PIN_ROOT
# export PIN_ROOT=/PATH/TO/YOUR_PIN/pin-3.xxx

export PIN_ROOT=/home/alisemi/graph-frameworks/pin-3.25-98650-g8f6168173-gcc-linux/

cd pin-tool
make
cd ..
make

for (( i=0; i<$NUM_THREADS; i++ ))
do
  mkfifo "$FIFO_OUTPUT_PATH_BASE"_"$i" -m 0666
done

./trace-ipc-app.out $FIFO_OUTPUT_PATH_BASE $NUM_THREADS > output_reader.txt &
$PIN_ROOT/pin -t pin-tool/obj-intel64/trace-ipc.so -o $FIFO_OUTPUT_PATH_BASE -t $NUM_THREADS -- ./sample-app-to-trace.out $NUM_THREADS

for (( i=0; i<$NUM_THREADS; i++ ))
do
  unlink "$FIFO_OUTPUT_PATH_BASE"_"$i"
done