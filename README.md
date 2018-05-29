# Cache Simulator


## Introduction
**Cache Simulator** project assignment for better understanding of **Computer Architecture and Design**.

This cache simulator has following characteristics:
- **Split Cache**: I-cache and D-cache
- **Cache Sizes**: 1024, 2048, 4096, 8192, 16384 bytes
- **Block Sizes**: 16 bytes, 64 bytes
- **Associativities**: Direct Mapped, 2-way, 4-way, 8-way
- **Replacement Policies**: LRU(Least Recently Used)
- **Write and Allocate Policy**: Write Back and Write Allocate for D-cache
- **Multi-level Cache**: L1 (I-cache, D-cache), L2 (Unified cache)
- **L2 cache configuration**: 8-way, same block size as L1 cache, 16384 bytes
- L2 cache also uses the **Write Back** and **Write Allocate** policy.
- **Multi-level Cache Policy**: inclusive, exclusive

You can see more information of this project in `CA-Assignment-Cache-2017.pdf`.<br>
You can also see the description of source code in `document.pdf`.<br>
You can see the output table of **cache miss rate** and **memory write count** of `trace1.din` in `trace1-output.xlsx`.<br>

**Professor** - *Dongkun Shin*


## Compilation
```
$ g++ -std=c++11 -o cache cache.cpp
```
`make` command is also available (see `Makefile`):
```
$ make
```

## Execution
```
$ ./cache [your_input_file]
```
- Input files can be `trace1.din`, `trace2.din`, `trace1-short.din`, `trace2-short.din`, or any other files are valid if they are following the forms of given input files.
- For `trace1.din` and `trace2.din`, execution time will be taken about **20 ~ 30 seconds**.
- You can see the output table of **cache miss rate** and **memory write count** for each state of your input file after the execution.
