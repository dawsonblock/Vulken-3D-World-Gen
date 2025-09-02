# Contributing

Thanks for your interest in contributing!

## Workflow
- Fork/branch off `main` or a working branch.
- Build locally using CMake Presets (`default`, `headless`, `windows-default`).
- Ensure tests pass (ctest with presets) and run lint/format.
- Open a PR; CI must be green.

## Style
- C++20; follow `.clang-format`.
- Prefer modern CMake targets (find_package + :: targets).
- Use presets; avoid hardcoding toolchain.

## Commit messages
- Short imperative summary; include scope when useful (e.g., CI:, Build:, Docs:).

## Reporting issues
- Include OS, compiler, preset used, and logs.
