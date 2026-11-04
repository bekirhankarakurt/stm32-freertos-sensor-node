# Contributing

Thanks for your interest in improving this project.

## Workflow

1. Fork the repository and create a topic branch.
2. Make focused commits with imperative subject lines ("Add SPI timeout handling").
3. Run the checks below locally; all of them run in CI.
4. Open a pull request using the template and describe how you tested the change.

## Local checks

```bash
cmake --preset host && cmake --build --preset host && ctest --preset host
```

```bash
cmake --preset host-asan && cmake --build --preset host-asan && ctest --preset host-asan
```

```bash
cmake --preset firmware && cmake --build --preset firmware
```

```bash
scripts/static_analysis.sh build/host build/firmware
```

## Coding rules

- C11 for drivers, bootloader and portable core code; C++17 without exceptions, RTTI or heap use for application logic.
- MISRA-inspired style: fixed-width types, explicit casts, braces on every block, no recursion, bounded loops and waits.
- The build must stay warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (`SN_WERROR=ON` in presets).
- Hardware-independent code belongs in `common/` or `app/` and needs host unit tests.
- Public functions are documented with Doxygen comments.
- MISRA C:2012 addon findings are reported, not gated. Accepted deviations: rule 15.5 (early returns for error
  handling), rule 2.5 (protocol constants kept in public headers) and rule 11.5/11.4 at the register and flash
  driver boundary.
