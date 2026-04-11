# Test Plan File

## Useful Information for Humans

- `testplan.md` is a manual validation checklist for the current flashed firmware build.
- It is written for the current built-in mode configuration and current UI behavior, not the full future reference design.
- Fill in results directly as you test the device on hardware and the connected host machine.
- If the file already contains older handwritten results from a previous debug build, keep them as notes but treat the `Expected` sections as the source of truth for the current build.

## Useful Information for AI

- Keep `testplan.md` aligned with the actual shipped firmware behavior in `main/mode_config.c`, `main/main.c`, and `main/ui_status.c`.
- Do not describe unimplemented features as expected behavior.
- If the build changes the mode list, gesture set, or UI labels, update this test plan accordingly.

