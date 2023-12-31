[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/PvOa2T4Z)
# Task 9 - Compilers/LLVM

This task involves implementing two LLVM passes and a runtime library that your LLVM pass will invoke.
When developing LLVM passes, you can find a lot of valuable methods already implemented in LLVM.
You are allowed (and encouraged!) to use them instead of re-implementing logic yourself.
Before starting your implementation, it might be helpful to look at the documentation of classes
like `llvm::Instruction`, `llvm::BasicBlock`, `llvm::Function`, and others. Also, look at
`llvm/Transforms/Utils/Local.h`.

## Important Notes

You are **not** allowed to call/use other LLVM passes (e.g. `instcombine`) or depend on external analyses (e.g.
`TargetIRAnalysis`). This also means that using the function `simplifyCFG` is not allowed.

## Task 9.1 - Dead Code Elimination

Your first assignment is to design a pass that eliminates redundant instructions.
We define dead instructions as instructions that write to a register not used by subsequent instructions and have no
observable side effects (e.g., write to memory, return, perform a jump).

There are three test cases for this task that increase in difficulty.

### 9.1.1 Trivially Dead Instructions

Initially, your focus should be on eliminating redundant instructions that neither yield any use nor produce observable
side effects.

#### Example

In the following example, the instruction `%mul` is dead and can be removed.
Consequently, the instruction `%add` becomes dead and can be removed too.

```ll
define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
 %add = add nsw i32 %argc, 42
 %mul = mul nsw i32 %add, 2
 ret i32 0
}
```

Your pass should transform the above code into the following code:

```ll
define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
 ret i32 0
}
```

### 9.1.2 Simplifying Basic Blocks

In the subsequent test case, we expect your pass to remove and streamline irrelevant basic blocks, which surface as a
result of eliminating redundant instructions.

#### Example #1

Conditional branches that always branch to the same block:

```ll
entry:
  ; ...
  br i1 %cmp, label %block.1, label %block.1
```

Can be simplified to:

```ll
entry:
  ; ...
  br label %block.1
```

#### Example #2

Blocks that just contain a single unconditional branch instruction to the next block:

```ll
entry:
  ; ...
  br i1 %cmp, label %block.1, label %block.2

block.1:
  br label %block.2
  
block.2:
  ; ...
```

Can be simplified to:

```ll
entry:
  ; ...
  br i1 %cmp, label %block.2, label %block.2
  
; block.1 has been removed
  
block.2:
  ; ...
```

After doing this optimization, you can now transform the conditional branch instruction to an unconditional branch.

### 9.1.3 Removing Loads/Stores to Stack Slots

The third test case is to remove useless loads/stores from/to stack slots.
You can walk over the function and check if a stores are never read again, i.e. there is no load from this stack slot
in the same function and the stack slot is not passed to another function.
For simplification, you do not need to check if the load/store is volatile.

#### Example



```ll
define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
 %x = alloca i32
 store i32 10, i32* %x
 ret i32 0
```

Since the stored value is never read again, we can remove the store instruction. Additionally, we can then remove the
alloca instruction, since it is not used anymore.

```ll
define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
 ret i32 0
}
```

## Task 9.2 - Memory Safety

For the second task, you construct a basic version of AddressSanitizer.
You should implement a pass that runs over the program, and inserts calls to a runtime library that checks if a memory
access is valid.

The idea is that the runtime library keeps track of allocated memory and checks if a memory access is within the bounds
of the allocated memory.
If it is not, the runtime library should print an error message and abort the program.
You can check out
the [AddressSanitizer paper](https://www.usenix.org/system/files/conference/atc12/atc12-final39.pdf).

We have provided you with a basic skeleton for the runtime library, but you can adapt it to your preference and needs.
Allocations should be padded on the left and right with a few bytes to detect out-of-bounds accesses close to
the allocated memory.
You only need to detect overflows within 16 bytes of the allocated memory (as ASan does).

If an invalid memory access is detected, you should print an error message to `stderr` that contains the following
substring (we check for this in the tests):

```
Illegal memory access
```

We again have three different test cases:

### 9.2.1 Heap out of Bounds

The first test case checks that out-of-bounds access to the heap is detected.
We will only check out-of-bounds access within a few bytes of the allocated memory.

```c
int main() {
  int *x = malloc(16 * sizeof(int));
  x[15] = 0; // in bounds, should still work
  x[16] = 0; // out of bounds, should be detected
  x[-1] = 0; // out of bounds, should also be detected
  return 0;
}
```

### 9.2.2 Heap use after free

This test case checks that your pass can detect a use-after-free error.

```c
int main() {
  int *x = malloc(16 * sizeof(int));
  free(x);
  x[0] = 0; // use after free, should be detected
  return 0;
}
```

### 9.2.3 Stack out of bounds

This test case ensures your pass can detect out-of-bounds access to a stack slot.

```c
int main() {
  int x[16];
  x[15] = 0; // in bounds, should still work
  x[16] = 0; // out of bounds, should be detected
  x[-1] = 0; // out of bounds, should also be detected
  return 0;
}
```

### 9.2.4 Performance

You can first start with a naive implementation that doesn't concern itself with performance and memory usage.
Once you have a working implementation, you can improve its performance and memory usage.
This final test checks that your application can handle a large number and size of allocations with a memory limit.

One approach you could try is implementing a compact shadow memory. See
the [AddressSanitizer paper](https://www.usenix.org/system/files/conference/atc12/atc12-final39.pdf) for inspiration.

## Deliverables

- `tasks/dead-code-elimination/build/libDeadCodeElimination.so`
- `tasks/memory-safety/build/libMemorySafety.so`
- `tasks/memory-safety/build/libMemorySafetyRuntime.a`

## The build system

The LLVM passes can only be implemented in C++, not C or Rust. You may implement the runtime for task 9.2 C, C++,
or Rust. Update the [Makefile](./tasks/dead-code-elimination/Makefile) accordingly.

You must have LLVM 16, ninja, make, and CMake installed to build this on your local system.

The assignments will run `make dce` (dead-code-elimination) and `make memory-safety`.
Make sure that this produces the correct files.

## Tests

The tests run the subtests located in the `tests/` folder.
Each test program is a python3 script and can be run individually, i.e.:

```console
python3 ./tests/memory-safety/test-malloc.py
```

The dce tests are run with [llvm-lit](https://llvm.org/docs/CommandGuide/lit.html) and can be run individually, i.e.:

```console
pip install lit
lit -v ./tests/dce
```

For convenience, our Makefile also comes with a `check` target, which will run all tests in serial:

```console
$ make check
```

To run a pass manually, you can do so with the following command:
Note: on macOS, replace `.so` with `.dylib`.

```console
opt -load-pass-plugin ./tasks/dead-code-elimination/build/libDeadCodeElimination.so -passes=dead-code-elimination -S input.ll
```
