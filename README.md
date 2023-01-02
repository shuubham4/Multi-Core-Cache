# Multi-Core-Cache
Design of a multicore writeback, write no allocate cache using the 4 state MESI protocol

To run the program, execute the following commands at the terminal:
1. make 
2. make sim
3. sim -n 4 -bs 128 -us 8192 -a 1 radix-4c.trace (where .trace can be replaced with relevant trace file. '4c' represents 4 cores. -n represents the no. of cores, -bs represents bloack size in Bytes, -us represents unified cache size in Bytes and - a represents associativity.)
