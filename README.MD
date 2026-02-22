
# XT Operating System

**XT Operating System** is a modern, multitasking, and multiprocessor operating system designed for 64-bit architectures. Developed by **Timur Nazhmetdinov**, the project focuses on minimalism and portability, maintaining a very thin dependency layer.

## Key Features

-   **Multitasking:** Efficient handling of concurrent processes.
    
-   **Multiprocessing (SMP):** Designed to utilize multiple CPU cores.
    
-   **Minimal Dependencies:** Requires only UEFI headers and a `printf` implementation.
    
-   **Architecture-Ready:** Built with future multi-architecture support in mind.
    
-   **64-bit Native:** Engineered strictly for systems with virtual addressing and at least a 64-bit word size.
    

----------

## Roadmap

-   [x] **Bootloader:** Completed and functional.
    
-   [x] **Kernel Memory Manager:** Currently in development.
    
-   [x] **Scheduler:** Refinement for multi-core multitasking.
    
-   [ ] **Multi-architecture Support:** Porting to other 64-bit platforms.
    
-   [ ] **User-space Environment:** Libraries and basic shell.
    

----------

## Prerequisites

Before building the project, ensure you have the following:

1.  **UEFI Headers:** You must have UEFI headers installed on your system.
    
2.  **Configuration:** Specify the path to your UEFI headers in the `config.mk` file.
    

----------

## Building the System

The build process is divided into two main stages:

1.  **Environment Setup:**
    
    Bash
    
    ```
    make setup
    ```
    
2.  **Compilation:**
    
    Bash
    
    ```
    make build
    ```
    

----------

## Running XT OS

To test the operating system, use the pre-configured QEMU command:

Bash

```
make run
```

----------

## License

This project uses a layered licensing model:

-   **GNU GPL:** Applied to the **Kernel** and **System Components**.
    
-   **GNU LGPL:** Applied to **User-space Libraries**.
    

----------

## 👤 Author

**Timur Nazhmetdinov**