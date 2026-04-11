# Contributing

## Local Checks

Use the smallest useful loop before flashing:

```bash
gcc -Wall -Wextra -Werror -I./main tests/ptt_state_test.c main/ptt_state.c -o ptt_state_test
./ptt_state_test
idf.py set-target esp32s3
idf.py build
```

On Windows, `.\flash.ps1 -BuildOnly` remains the quickest firmware-only build helper once your ESP-IDF environment is loaded.

## Manual Hardware Smoke Test

After flashing, verify:

- the screen boots to `Idle`
- the secondary line becomes `Mic Ready`
- BOOT press sends one `F13 down`, BOOT release sends one `F13 up`
- touch hold for about `400 ms` sends `F13 down`, release sends `F13 up`, matching `BOOT`
- short touch taps do not send any HID key
- the USB microphone stays enumerated while idle, stays quiet while idle, and carries live audio only during BOOT PTT

## Release Hygiene

- Prefer pinning dependency updates intentionally in `main/idf_component.yml`.
- Generate and commit `dependencies.lock` for release-oriented changes so dependency resolution is reproducible across clones and CI.
- Keep README behavior notes, host test docs, and firmware behavior aligned in the same change.
