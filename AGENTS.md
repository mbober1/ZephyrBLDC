# AGENTS.md

## Project Overview

This repository contains a Zephyr RTOS-based controller project for small, high-performance BLDC motors. The project prioritizes precise, high-quality control, automatic commissioning, and a broad set of motor-control and diagnostic capabilities.The main application is in `app/`. Helper apps are in `helper_apps/`. Board files are in `boards/`, and local modules are in `modules/lib/`.

## Build targets
- **native** - native_sim target
- **WeAct STM32G431 Core Board** - target for evaluation and development.
- **MicroSpora** - full featured target hardware 

## Build firmware

Use vscode tasks.
- `Build native` for the native_sim target.
- `Build devboard` for the WeAct STM32G431 Core Board target.
- `Build MicroSpora` for the MicroSpora target.

## Debugging

For debugging purposes, you can connect to the target using `west attach` command. It will attach GDB to the running target, but not reset or flash it. Remember to exit gdb when finished.

## Guidelines
- Use built-in VSCode tasks for building the firmware.
- Do not use grep, sed, awk, or other command-line tools. Use only built-in tools.
- Follow Zephyr conventions for Kconfig, devicetree, drivers, and board configuration. Keep board-specific peripheral setup target-specific and control logic portable where practical.
- Prefer the existing Zephyr and Spinner APIs and keep changes consistent with nearby code.
- Preserve existing user changes; inspect a file before editing it and keep changes scoped to the request.
- Do not flash or run motor-control firmware unless the user explicitly asks.