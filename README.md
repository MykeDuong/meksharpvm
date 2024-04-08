# Mek# Programming Language

Mek# is a dynamic-typing programming language that blends object-oriented and functional programming paradigms. It supports classes, inheritance, closures, higher-order functions, first-class functions, and standard language constructs like control flows, loops, and recursion. Mek# adopts a C-style syntax, providing a familiar yet powerful toolset for developers.

## Features

- **Dynamic Typing:** Flexibility in variable and function typing.
- **Object-Oriented Programming:** Includes classes, inheritance (using the `<:` symbol), and the `super` keyword for accessing superclass methods.
- **Functional Programming:** Supports closures, higher-order functions, and first-class functions.
- **C-Style Syntax:** Familiar syntax for defining variables (`var`), functions (`fun`), and classes (`class`).
- **Scoping:** Local scope for functions and classes to manage namespace and lifecycle.
- **REPL & Source File Execution:** Interactive and script-based execution modes.

## Installation

Mek# can be run in a REPL mode or by executing source files. It is compatible with UNIX systems and can be installed or compiled directly from the source.

- To install Mek# on a UNIX machine, navigate to the source directory and run:

```bash
make install
```

- To compile the program without installing:

```bash
make
```

## Usage

To execute a Mek# program, use the `mkv` command followed by the `.mks` source file:

mkv filename.mks

For interactive coding sessions, run the program without arguments to enter the REPL mode.

## Technical Details

Mek# is implemented using a bytecode VM. The source code is compiled into Mek# custom bytecodes, which are then interpreted by a virtual machine (VM) written in C. This approach offers a balance between performance and flexibility, allowing for efficient execution of Mek# programs.

## Getting Started

To start with Mek#, create a `.mks` file with your code. Here is a simple example:

```mek#
class MyClass {
    init() {
        this.x = "Hello from Mek#";
    }

    sayHello() {
        print(this.x);
    }
}

// Create an object
var obj = MyClass();

// Create a closure
var closure = obj.sayHello;
closure();

// Using inheritance
class ExtendedClass < MyClass {
    sayBye() {
        print("Goodbye from Mek#");
    }
}

var child = ExtendedClass();
child.sayBye();
```

This example demonstrates class definition, object instantiation, method invocation, and inheritance in Mek#.

Conclusion
Mek# offers a dynamic and robust platform for software development, combining the best of OOP and functional programming. Whether you prefer interactive sessions or developing extensive applications, Mek# provides the necessary tools and flexibility to bring your ideas to life.


