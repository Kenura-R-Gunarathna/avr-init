# AVR-Init

A lightweight, professional-grade project initializer for bare-metal AVR development (ATmega328P). 

This tool automates the creation of a structured C project, complete with an optimized Makefile, hardware drivers (millis), and Git initialization.

## Features

- **Standardized Structure:** Creates `src/`, `src/drivers/`, and `build/` automatically.
- **Optimized Makefile:** 
  - Size optimization using `-Os` and `--gc-sections`.
  - Generates diagnostic files (`.lss` assembly, `.sym` symbols).
  - Built-in support for **USBasp** programming and Fuse management.
- **Embedded Drivers:** Includes a `millis()` driver for non-blocking timing.
- **Git Ready:** Automatically initializes a repository and sets up `.gitignore`.

## Installation

1. Clone this repository or copy the `avr-init.sh` script.
2. Move the script to your local bin and make it executable:
   ```bash
   mkdir -p ~/.local/bin
   cp avr-init.sh ~/.local/bin/avr-init
   chmod +x ~/.local/bin/avr-init
   ```
3. (Optional) Add `~/.local/bin` to your PATH in `~/.bashrc`:
   ```bash
   export PATH="$HOME/.local/bin:$PATH"
   ```

## Usage

Create a new project in the current directory:
```bash
avr-init
```

Or create a new folder and initialize it:
```bash
avr-init my_new_project
```

### Build Commands
- `make`: Compile the project and show memory usage.
- `make program`: Flash the `.hex` file to the chip via USBasp.
- `make fuse`: Set the ATmega328P fuses (1MHz internal).
- `make clean`: Remove build artifacts.

## Requirements
- `avr-gcc`
- `avrdude`
- `avr-libc`
- `git`

---
Developed for PH_3032 Embedded Systems.
