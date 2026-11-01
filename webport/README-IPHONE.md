# Zelda: Flame & Lantern — Safari/iPhone browser port

This is a **real source-level WebAssembly port** of the custom modified `zelda3` engine from your provided ZIP. It includes the Dark World prologue, lantern, extra torches, ray-based lighting, music, custom dialogue, and sprite resources from your modified data file.

**Status: build-ready source, not a precompiled live game.** This workspace has no Emscripten compiler or access to a cloud build account, so the .wasm, .data and .js runtime files are **not included**. A successful GitHub Actions run compiles them automatically. The web build has **not been tested on a physical iPhone** yet.

## Publish the playable version without Xcode, a Mac, or a local compiler

1. Create a **private** GitHub repository (or a public repository only if you are comfortable sharing the engine/data files). GitHub Pages visibility and availability depend on the repository and your plan.
2. Upload all the **contents** of this package to the repository's root (`src/`, `snes/`, `web/`, `site/`, `scripts/`, `third_party/`, `.github/` etc.). Hidden `.github` must be included.
3. Rename your default branch to `main` or adapt `.github/workflows/publish.yml` to the branch name.
4. Open GitHub repository **Settings → Pages** and choose **GitHub Actions** as the build/deployment source.
5. Open **Actions → Build and publish iPhone web game** and run the workflow (or push a change to `main`). The run also produces a downloadable **flame-lantern-iphone-website** artifact containing the compiled browser game, whether or not Pages succeeds.
6. When it succeeds, open the Pages URL shown under the `github-pages` environment, e.g. `https://YOURNAME.github.io/YOUR-REPO/`, **in Safari on your iPhone**.
7. In Safari tap **Share → Add to Home Screen** to launch as a full-screen web app. Rotate to landscape for the best layout.

The Emscripten build runs on GitHub's Linux runner. Xcode and Apple signing are not involved.

## Local build option

Install [emsdk](https://emscripten.org/docs/getting_started/downloads.html), activate it, then run `bash scripts/build_web.sh`. Serve the `site/` folder from an **HTTP(S)** server (do not open the HTML using `file://`, which cannot load `.wasm/.data` reliably):

```bash
python3 -m http.server -d site 8000
```

Then open `http://localhost:8000` on your computer, or deploy `site/` to any static HTTPS host. Safari on iPhone needs a publicly reachable HTTPS URL to use the offline PWA behavior.

## Controls

- Touch D-pad: move; A: interact / dialogue; B: sword; X: map; Y: equipped item. START: inventory. SELECT: save menu. L/R: shoulder buttons.
- Bluetooth controller: standard gamepads are supported via browser Gamepad API, **when exposed by Safari**. Xbox-style A (bottom) maps to SNES B; Xbox-style B (right) maps to SNES A.
- Settings gear: Save snapshot, Load snapshot, lighting on/off, Deep/Brighter Night, pause, and export/import a save backup.
- Saves are stored inside IndexedDB (within this website's browser storage) and synced approximately every 25 seconds. Use Export save backup before clearing Safari data or changing website origin.
- Game speed is nominally capped at 60 simulated FPS. Lighting ray count has been reduced from 1024 to 512 to help mobile performance; lighting still simulates occlusion and keeps Link's lantern. Real-world phone performance is unmeasured.

## Technical notes

- The custom native main loop was adapted into an Emscripten animation frame callback.
- SDL2 renders onto the HTML canvas. Browser controls call exported C functions, rather than attempting to emulate arbitrary DOM keyboard events.
- The `.dat` asset file is bundled with the WASM app as an Emscripten virtual filesystem. Browser saves mount `/saves` via IDBFS and sync before `main()` reads save data.
- Source files from the provided modified game are under the upstream zelda3 license; see `LICENSE.txt`. The `.dat` assets were supplied by the user; decide whether/how to publish them yourself.
- The package is **not a fully compiled, ready-to-open local HTML file**. It needs the GitHub Actions or local Emscripten build step once. Gameboy/SNES generic web emulators won't preserve this modified C engine's visual effects.

## Private alternative: no public game website

If you prefer not to post the asset file on a public-facing website, use a **private GitHub repository** and download the built `flame-lantern-iphone-website` artifact from Actions. Extract its contents into a new folder on your PC, start a simple HTTP server (`python -m http.server 8000 --bind 0.0.0.0`), then visit `http://YOUR_PC_LAN_IP:8000` in Safari while connected to the same Wi-Fi. This needs a PC running to play; iOS offline install and some browser controller features may require HTTPS. Do not share the site or game data publicly without the necessary rights.
