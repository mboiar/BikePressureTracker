# Bike Pressure Real-time Tracker

This STM32 project aims to provide a mobile bicycle tire pressure tracker based solely on a bike-mounted accelerometer.

## Requirements

### Software

- C99/C++11 Development Tools
- CMake 3.22
- Python3
- uv
- STM32CubeProgrammer

### Hardware

- STM32F4xx Blackpill
- HC-05 Bluetooth module
- ADXL345 3-axis accelerometer
- Powerbank with a low-power mode
- USB-C cable

## Running the project

### Target (STM32) - Experiment

1. Build

```bash
    cmake -DCMAKE_TOOLCHAIN_FILE="cmake/gcc-arm-none-eabi.cmake" -DCMAKE_BUILD_TYPE=Debug -S . -B build -G Ninja

    cmake --build build
```
2. Flash

```bash
    STM32_Programmer_CLI -c port=SWD -d build/BikeTirePressureMeas.elf -v -hardRst
```

### Host (PC) - Visualization

Install the Python dependencies with [uv](https://docs.astral.sh/uv/):

```powershell
    uv sync
```

Parse experiment data and display dominant frequencies:

```powershell
    uv run python utils/parser.py <path/to/data>
```
