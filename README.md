# Lab 3: Dynamic Memory Manager Module

## Purpose

The purpose of this assignment is to help you understand how dynamic memory management works in C. It also will give you more opportunity to use the GNU/Unix programming tools.

## Important Dates

| Date | Description |
|:---  |:--- |
| Tuesday, October 15, 18:30 | Memory Lab session 1 |
| Tuesday, October 22, 18:30 | Memory Lab session 2 |
| Friday, November 1, 21:00 | Submission deadline |

## Overview

### Hand-out

Please read this file (README.md) carefully. Additionally, the PPT file presented during the lab session is included in the distribution archive for students. However, please remember that the PPT file is based on the README.md file, and it is important to understand the contents of the README.md file.

The assignment starts by extracting the provided assign3.tar.gz file. It is recommended that you link this folder to your GitHub repository (for better readability of the README file and keep your code safe).

### Submission

As in lab 1 and lab 2, create a directory named after your student number, zip it up, and submit it. And, be sure to follow the following directory structure:

```
202400000_assign3
  |-heapmgr1.c
  |-heapmgr2.c (optional)
  |-chunk.c
  |-chunk.h 
  `-readme
```

### Implementing the Lab on an M1/M2 Mac or custom Unix/Linux

While it is possible to implement the lab on an ARM-based Mac, MacOS is not 100% compatible with Linux and thus requires a number of changes to the code (extra header files, etc.) We recommend installing an ARM-based Linux virtual machine such as [Ubuntu Server for ARM](https://ubuntu.com/download/server/arm). On custom Intel-compatible Linux systems, you should be able to implement the lab without any modifications.

Note that we evaluate your submissions in the provided Bacchus VMs. If you not use the Bacchus VMs to develop your code, **we strongly recommend testing your submission in the Bacchus VMs** to make sure it runs correctly.


## Your task

You are given the interface of a `heapmgr` (heap manager) module in a file named `heapmgr.h`. It declares two functions:
```
void *heapmgr_malloc(size_t ui_bytes);
void  heapmgr_free(void *pv_bytes);
```
You also are given three implementations of the heapmgr module:

* `heapmgrgnu.c` is an implementation that simply calls the GNU `malloc()` and `free()` functions provided with our lab machine development environment.
* `heapmgrkr.c` is the Kernighan and Ritchie (K&R) implementation, with small modifications for the sake of simplicity.
* `heapmgrbase.c` is an implementation that you will find useful as a baseline for your implementations.

Your task is to create a implementation (or two implementations) of the `heapmgr` module. Your implementation, **`heapmgr1.c`**, should enhance `heapmgrbase.c` so it is reasonably efficient. To do that it should use a single doubly-linked list and chunks that contains headers and footers (as described in lectures, and in precepts). Unlike the K&R implementation, the baseline code uses a **non-circular** list, and your code should implement a **doubly-linked** list. (You can implement either **circular** or **non-circular**)

If designed properly, `heapmgr1.c` will be reasonably efficient in most cases. However, `heapmgr1.c` is subject to poor worst-case behavior. An additional implementation, **`heapmgr2.c`**, should enhance `heapmgr1.c` so the worst-case behavior is not poor. To do that it should use **multiple doubly-linked lists**, alias **bins** (as described in lectures, and in precepts). Note that implementing `heapmgr2.c` is **not mandatory**, and you can get full marks without the implementation of `heapmgr2.c`; Instead, you will get **extra credit** if you implement `heapmgr2.c`.

Your `heapmgr` implementations should not call the standard `malloc()`, `free()`, `calloc()`, or `realloc()` functions.

Your `heapmgr` implementations should thoroughly validate function parameters by calling the standard `assert()` macro.

Your `heapmgr` implementations should check invariants by:
* Defining a thorough `check_heap_validity()` function, and
* Calling `assert(check_heap_validity())` at the leading and trailing edges of the `heapmgr_malloc()` and `heapmgr_free()` functions.

Your `heapmgr` implements the strategies for good memory utilization:
* heapmgr_malloc() should check blocks from the free list before allocate new memory
* If free block is bigger then requested, heapmgr_malloc() should divide the free block and keep leftover block in the free list
* heapmgr_free() should check lower/upper neighbor and coalesce if possible

If you ignore memory utilization, you won't get points no matter how fast your implementation is.


## Logistics

### Given codes

#### /reference
* `heapmgr.h`, `heapmgrgnu.c`, `heapmgrkr.c`, and `heapmgrbase.c`: as described above.
* `chunkbase.h` and `chunkbase.c`: a Chunk module used by `heapmgrbase.c`.

#### /src
* `chunk.h` and `chunk.c`: a Chunk module that you may use in both implementations of your `heapmgr` module.
* You may modify `chunk.h` and `chunk.c` and use them, or leave them as is and not use them.

#### /test
* `testheapmgr.c`: a client program that tests the `heapmgr` module, and reports timing and memory usage statistics.
* `testheap1`, `testheap2` and `testheapimp`: bash shell scripts that automate testing. The `testheap1` script assumes the existence of four executable files named `testheapmgrgnu`, `testheapmgrkr`, `testheapmgrbase`, `testheapmgr1`. The `testheap2` script assumes the existence of five executable files named `testheapmgrgnu`, `testheapmgrkr`, `testheapmgrbase`, `testheapmgr1` and `testheapmgr2`.
* `Makefile` : makefiles that can build executable files. Assuming that you have all the appropriate files, `Makefile` makes five executable files(`testheapmgrgnu`, `testheapmgrkr`, `testheapmgrbase`, `testheapmgr1` and `testheapmgr2`).

Note that `testheap1`, `testheap2` and `testheapimp` are used as executables, so you need to enable the file execution bit like `$ chmod u+x testheap1` before using them.

### Perform a test

The `testheapmgr` program requires three command-line arguments. The first should be any one of seven strings, as shown in the following table, indicating which of seven tests the program should run:

| Argument | Test Performed |
|:---          |:---  |
| `LIFO_fixed` | LIFO with fixed size chunks |
| `FIFO_fixed` | FIFO with fixed size chunks |
| `LIFO_random` | LIFO with random size chunks |
| `FIFO_random` | FIFO with random size chunks |
| `random_fixed` | Random order with fixed size chunks |
| `random_random` | Random order with random size chunks |
| `worst` | Worst case order for a heap manager implemented using a single linked list |

The second command-line argument is the number of calls of `heapmgr_malloc()` and `heapmgr_free()` that the program should execute. The third command-line argument is the (maximum) size, in bytes, of each memory chunk that the program should allocate and free.

Immediately before termination testheapmgr prints to stdout an indication of how much CPU time and heap memory it consumed. See the `testheapmgr.c` file for more details.

When testing, set the product of the number of calls (second command line argument) and size in bytes (third command line argument) to less than $10^9$. In all tests evaluating the implementation on the Bacchus server, the product of the number of calls (second command line argument) and size in bytes (third command line argument) is guaranteed to be less than or equal to $10^9$.

### Make testheapmgr

To test your `heapmgr` implementations, you should move your files in same directory and build two programs using these `gcc800` commands:
```
gcc800 -std=gnu99 testheapmgr.c heapmgr1.c chunk.c -o testheapmgr1
gcc800 -std=gnu99 testheapmgr.c heapmgr2.c chunk.c -o testheapmgr2
```
To collect timing statistics, you should move your files in same directory and build five programs using these `gcc800` commands:
```
gcc800 -O3 -D NDEBUG -std=gnu99 testheapmgr.c heapmgrgnu.c -o testheapmgrgnu
gcc800 -O3 -D NDEBUG -std=gnu99 testheapmgr.c heapmgrkr.c -o testheapmgrkr
gcc800 -O3 -D NDEBUG -std=gnu99 testheapmgr.c heapmgrbase.c chunkbase.c -o testheapmgrbase
gcc800 -O3 -D NDEBUG -std=gnu99 testheapmgr.c heapmgr1.c chunk.c -o testheapmgr1
gcc800 -O3 -D NDEBUG -std=gnu99 testheapmgr.c heapmgr2.c chunk.c -o testheapmgr2
```
The `-O3` (that's uppercase "oh", followed by the number "3") argument commands gcc to optimize the machine language code that it produces. When given the `-O3` argument, `gcc` spends more time compiling your code so, subsequently, the computer spends less time executing your code. The `-D NDEBUG` argument commands gcc to define the `NDEBUG` macro, just as if the preprocessor directive `#define NDEBUG` appeared in the specified .c file(s). Defining the `NDEBUG` macro disables the calls of the `assert` macro within the `heapmgr` implementations. Doing so also disables code within `testheapmgr.c` that performs (very time consuming) checks of memory contents.

Instead of using upper commands, you can also use Makefile to build programs.
| `make` +  | commands to be executed |
|:---          |:---  |
| `test1` | `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o test/testheapmgr1` |
| `test2` | `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr2.c src/chunk.c -o test/testheapmgr2` |
| `testall` | `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o test/testheapmgr1` <br> `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr2.c src/chunk.c -o test/testheapmgr2` |
| `timegnu` | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrgnu.c -o test/testheapmgrgnu` |
| `timekr` | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrkr.c -o test/testheapmgrkr` |
| `timebase` | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrbase.c reference/chunkbase.c -o test/testheapmgrbase` |
| `time1` | `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o test/testheapmgr1` |
| `time2` | `gcc800 -std=gnu99 test/testheapmgr.c src/heapmgr2.c src/chunk.c -o test/testheapmgr2` |
| `time1all` | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrgnu.c -o test/testheapmgrgnu` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrkr.c -o test/testheapmgrkr` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrbase.c reference/chunkbase.c -o test/testheapmgrbase` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o test/testheapmgr1` |
| `time2all` | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrgnu.c -o test/testheapmgrgnu` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrkr.c -o test/testheapmgrkr` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrbase.c reference/chunkbase.c -o test/testheapmgrbase` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o test/testheapmgr1` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c src/heapmgr2.c src/chunk.c -o test/testheapmgr2` |
| `all` <br> (same as time2all) | `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrgnu.c -o testheapmgrgnu` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrkr.c -o testheapmgrkr` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c reference/heapmgrbase.c reference/chunkbase.c -o test/testheapmgrbase` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c src/heapmgr1.c src/chunk.c -o testheapmgr1` <br> `gcc800 -O3 -D NDEBUG -std=gnu99 test/testheapmgr.c src/heapmgr2.c src/chunk.c -o test/testheapmgr2` |
| `clean` | `rm -f test/testheapmgrgnu test/testheapmgrkr test/testheapmgrbase test/testheapmgr1 test/testheapmgr2` |

You can create additional test programs as you deem necessary. You need not submit your additional test programs.

### Make readme

Create a `readme` text file that contains:

* Your name and student ID.
* The **CPU times and heap memory consumed** by testheapmgr using `heapmgrgnu.c`, `heapmgrkr.c`, `heapmgrbase.c`, `heapmgr1.c`, and `heapmgr2.c`(optional), with all arguments(`LIFO_fixed`, `FIFO_fixed`, `LIFO_random`, `FIFO_random`, `random_fixed`, `random_random`, `worst`), with call count 100000, and with maximum chunk sizes 1000 and 10000. Note that if the CPU time consumed is more than **5 minutes**, `testheapmgr` will **abort execution**. To report the time and memory consumption, it is sufficient to paste the output of the `testheap1` or `testheap2` script into your `readme` file.
* (Optionally) An indication of how much time you spent doing the assignment.
* (Optionally) Your assessment of the assignment.
* (Optionally) Any information that will help us to grade your work in the most favorable light. In particular you should describe all known bugs.

### Submission

Compress the submission files into tar format using the following command (assuming Your ID is 202400000):

If you want to submit only `hapmgr1.c`:
```
mkdir 202400000_assign3
mv heapmgr1.c chunk.c chunk.h readme 202400000_assign3
tar zcf 202400000_assign3.tar.gz 202400000_assign3
```
If you want to submit both `hapmgr1.c` and `heapmgr2.c`:
```
mkdir 202400000_assign3
mv heapmgr1.c heapmgr2.c chunk.c chunk.h readme 202400000_assign3
tar zcf 202400000_assign3.tar.gz 202400000_assign3
```
Upload your submission file (202400000_assign3.tar.gz) to eTL submission page. We do not accept e-mail submission (unless our course eTL page is down).

## Grading

We will grade your work on submit format, quality from the user's point of view, and from the programmer's point of view. From the user's point of view, your module has quality if it behaves as it should. You will get full point if your module is well-designed, and faster then given codes. `heapmgr1` should faster then `heapmgrkr` and `heapmgrbase`, in 6 cases (except worst). `heapmgr2` should faster then `heapmgrkr`, `heapmgrbase`, and `heapmgr1`, in all cases. From the programmer's point of view, your module has quality if it is well styled and thereby simple to maintain. See the specifications of previous assignments for guidelines concerning style. Specifically, function modularity will be a prominent part of your grade.

If you submit `heapmgr2.c`, you can get up to 30% of the score you received for implementing `heapmgr1.c` as extra credit.

You will not get the 30% extra credit just by submitting heapmgr2; If you do not get a perfect score on heapmgr2, extra credit you receive will be reduced.

Since extra credit is proportional to the score you received for implementing `heapmgr1.c`, we recommend that you first fully implement `heapmgr1.c` and then challenge the implementation of `heapmgr2.c`.
