# Bike Pressure Real-time Tracker

This STM32 project aims to provide a mobile bicycle tire pressure tracker based solely on a bike-mounted accelerometer.

# Running the project

1. Build

```bash
    cmake -DCMAKE_TOOLCHAIN_FILE="cmake/gcc-arm-none-eabi.cmake" -DCMAKE_BUILD_TYPE=Debug -S . -B build -G Ninja

    cmake --build build
```
2. Flash

```bash
    STM32_Programmer_CLI -c port=SWD -d build/BikeTirePressureMeas.elf -v -hardRst
```

