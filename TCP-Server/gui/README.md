# HomeServer GUI

Static React (CDN) SPA for the HomeServer backend.

## Run

Open `index.html` directly in a browser or serve the folder:

```bash
cd /mnt/festplatte2/ProjectsGithub/ESP32-LoRa-Mesh-Server/TCP-Server/gui
python3 -m http.server 8080
```

Then visit `http://localhost:8080`.

## Notes

- Set the API Base URL (e.g. `http://localhost:8000`).
- If API auth is enabled, enter the `X-API-Key`.
