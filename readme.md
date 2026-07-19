# LosTacOS

> [!WARNING]
> This shit is for research purposes only, it was made using 3 steps: hoping, coping, vibe coding.

## Description

LosTacOS is a simple operating system written in C/C++/NASM. It is designed to be a minimalistic operating system that can run on x86_64 architecture. It is not intended to be a full-fledged operating system, but rather a learning tool for those interested in operating system development.

## Features

- **vfs** - virtual file system
- **paging** - memory management
- **heap** - memory management
- **drivers** - keyboard, mouse, framebuffer, (now deprecated) vga, serial, etc.
- **shell** - simple shell
- **userspace** - (in progress)
- **AND MORE**!

## Requirements

- **QEMU** - for running the OS
- **NASM** - for assembling the bootloader
- **GCC** - for compiling the kernel
- **GDB** - for debugging the kernel
- **Make** - for building the OS

## Shortcuts:

```bash
make        # to build
make run    # to run with QEMU
make format # to format the code using clang-tidy
```
