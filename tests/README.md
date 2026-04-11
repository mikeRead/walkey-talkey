# Tests

## Useful Information for Humans

- `ptt_state_test.c` still covers the legacy pure `main/ptt_state.*` helper while the refactor settles.
- `mode_controller_test.c`, `input_router_test.c`, and `action_engine_test.c` cover the new pure-C mode-system modules without depending on ESP-IDF, BSP, LVGL, or TinyUSB.
- `mode_json_loader_test.c` covers the JSON contract and loader-to-runtime mapping, including stable mode ordering, structured modifiers, consumer HID usage actions, and duplicate-key rejection.
- `config_api_service_test.c` covers the whole-document config API service layer, including canonical validation, external save, and restore-to-built-in-default persistence behavior.
- The schema in `config/mode-config.schema.json` is the machine-readable contract, while `docs/mode-system-reference.md` explains the runtime macro semantics those tests are checking.
- Host-side tests still require a desktop compiler such as `gcc` on `PATH`. When that is available, you can run:

```bash
gcc -Wall -Wextra -Werror -I./main tests/ptt_state_test.c main/ptt_state.c -o ptt_state_test
./ptt_state_test
gcc -Wall -Wextra -Werror -I./main tests/mode_controller_test.c main/mode_controller.c main/mode_config.c main/mode_json_loader.c main/mode_hid_tokens.c -o mode_controller_test
./mode_controller_test
gcc -Wall -Wextra -Werror -I./main tests/input_router_test.c main/input_router.c -o input_router_test
./input_router_test
gcc -Wall -Wextra -Werror -I./main tests/action_engine_test.c main/action_engine.c main/mode_controller.c main/mode_config.c main/mode_json_loader.c main/mode_hid_tokens.c -o action_engine_test
./action_engine_test
gcc -Wall -Wextra -Werror -I./main tests/mode_json_loader_test.c main/mode_config.c main/mode_json_loader.c main/mode_hid_tokens.c -o mode_json_loader_test
./mode_json_loader_test
gcc -Wall -Wextra -Werror -I./main tests/config_api_service_test.c main/config_api_service.c main/mode_config.c main/mode_json_loader.c main/mode_hid_tokens.c -o config_api_service_test
./config_api_service_test
```

- On this workstation, the verified ESP-IDF entry point is the PowerShell profile below. It exposes `idf.py`, but a full firmware build still requires the Xtensa toolchain to be installed and available to that environment:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command ". 'C:/Espressif/tools/Microsoft.v5.5.3.PowerShell_profile.ps1'; idf.py build"
```

## Useful Information for AI

- Keep tests in this folder host-buildable and independent from ESP-IDF, BSP, LVGL, and TinyUSB where possible.
- Prefer adding coverage here for pure logic modules such as `mode_controller.*`, `input_router.*`, `action_engine.*`, and `ptt_state.*`.
- Keep JSON contract coverage here too when the parser stays host-buildable, especially when HID token aliases, schema shapes, or validation behavior change.
- Do not duplicate hardware integration checks here; keep those in manual validation docs and firmware smoke tests.
