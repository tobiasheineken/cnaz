# cnaz
**cnaz** is an alterative interpreter for [naz](https://github.com/sporeball/naz), written in C.
The primary objective for this interpreter is to use an unbounded integer type for the unlimited mode `naz -u`,
instead of the floating point based number type used in the [javascript interpreter](https://github.com/sporeball/naz).

## Usage
After cloning the repository you can compile the interpreter with
```
$ cc -std=gnu99 -o interpreter interpreter.c nazlib.c
```

and run the interpreter with
```
$ ./interpreter filename.naz
```

This reposity is currently only written for UNIX and only tested on Debian / Ubuntu.

## Differences
Currently, cnzar can only run a subset of naz programms.
The following list shows all the additional restriction for naz programs to be executed by cnaz:

- The only supported read operation is `1r`. Reading a different character from stdin is not supported
- Comments and spaces cannot be placed between the number and the operand. `1r 1o` is fine, `1 r1o` is not.
- Special opcode blocks (`1xNf`; `2xNv`; `3xNvMg` ; `3xNvMe` and `3xNvMl`) cannot be interrupted by spaces or comments
- Error checking is less strict, for example loading an undefined variable will not (necessarily) result in an exception
- No command line flags are supported
