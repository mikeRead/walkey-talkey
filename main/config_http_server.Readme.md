# Config HTTP Server

## Useful Information for Humans

- `config_http_server.c` starts the device-hosted HTTP server, brings up the local Wi-Fi access point, serves the tiny config web UI, and exposes the REST endpoints for config export, validation, save, and reset.
- The web UI is intentionally small and talks only to the firmware REST endpoints.
- Startup is intentionally delayed by app orchestration in `main.c` so Wi-Fi bring-up does not disturb the display/touch stack during early boot.
- Reset restores the built-in firmware JSON as the active external config source before reloading runtime state.
- Error responses should preserve structured parse/storage details because the portal is also the main troubleshooting surface for end users.

## Useful Information for AI

- Keep the HTTP layer thin and delegate JSON/persistence logic to `config_api_service.c`.
- Keep successful responses canonical so browser/UI clients converge on one JSON shape.
- Treat SoftAP and HTTP startup as transport/bootstrap concerns; the service layer should remain reusable if the config API is later exposed another way.
