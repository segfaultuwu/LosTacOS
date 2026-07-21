# LosTacOS

> [!WARNING]
> This shit is for research purposes only, it was made using 3 steps: hoping, coping, vibe coding.
> And no, this is not 100% vibe coded..

## Description

LosTacOS is a simple operating system written in C/C++/NASM. It is designed to be a minimalistic operating system that can run on x86_64 architecture. It is not intended to be a full-fledged operating system, but rather a learning tool for those interested in operating system development.

## Features

- **vfs** - virtual file system
- **paging** - memory management
- **heap** - memory management
- **drivers** - keyboard, mouse, framebuffer, (now deprecated) vga, serial, etc.
- ~~**shell**~~ - (in progress)
- **userspace** - (in progress)
- **AND MORE**!

## Requirements

- **QEMU** - for running the OS
- **NASM** - for assembling the isr and other things
- **GCC** - for compiling the kernel
- **CLANG** - for compiling the user programs
- **GDB** - for debugging the kernel
- **Make** - for building the OS
- **TAR** - for building the rootfs.tar

## Shortcuts:

```bash
make        # to build
make run    # to run with QEMU
make format # to format the code using clang-tidy
```
