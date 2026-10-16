# SC5 phase 7 software and offline assets

Working directory: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc5-implementation-2026-10-16/stk-code`.

Environment actually used: `/opt/homebrew/opt/python@3.14/bin/python3.14` (3.14.6), Pillow 12.2.0, macOS 26.5.2 arm64; `/opt/homebrew/bin/ffmpeg` and `/opt/homebrew/bin/ffprobe`.

Executed checks, final relevant runs:

- `python3 -m unittest discover -s tools/immersive/scintilla -p 'test_*qualification.py'`: 37 passed.
- `python3 -m unittest discover -s tools/immersive/scintilla -p 'test_collect_mission_measurement.py'`: 10 passed.
- `python3 -m unittest discover -s tools/immersive/scintilla -p 'test_crew_intro.py'`: 3 passed.
- `python3 tools/immersive/CheckRelayProjectionSource.py`: 5 passed.
- `python3 tools/immersive/tests/check_scintilla_crew_render_source.py`: 6 passed.
- `python3 tools/immersive/tests/check_scintilla_mission_hud_source.py`: 18 source/asset checks passed.
- `python3 tools/immersive/tests/check_scintilla_mission_api_source.py`: 28 source checks passed.
- `git diff --check`: passed.

The qualification tests use explicitly synthetic fixtures and cannot emit a native qualification descriptor. Their temporary directories explicitly use the test file's parent on the SSD. Collector temporary directories have the same SSD constraint. Python bytecode is in this SSD worktree. No global HOME, CODEX_HOME or TMPDIR was changed.

Final asset authoring commands (run from the working directory):

```sh
python3 tools/immersive/scintilla/create_mission_intro.py --crew-profile sc5_crew_standard_v1 --output data/immersive_relay/scintilla/intro_crew_standard_it_v1 --work-dir '/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc5-implementation-2026-10-16/intro-authoring/standard'
python3 tools/immersive/scintilla/create_mission_intro.py --crew-profile sc5_crew_party_v1 --output data/immersive_relay/scintilla/intro_crew_party_it_v1 --work-dir '/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc5-implementation-2026-10-16/intro-authoring/party'
```

Final verification commands:

```sh
python3 tools/immersive/scintilla/check_mission_intro_assets.py --manifest data/immersive_relay/scintilla/intro_crew_standard_it_v1/manifest.json --decode --report '/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc5-implementation-2026-10-16/evidence/intro-standard-assets.json'
python3 tools/immersive/scintilla/check_mission_intro_assets.py --manifest data/immersive_relay/scintilla/intro_crew_party_it_v1/manifest.json --decode --report '/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc5-implementation-2026-10-16/evidence/intro-party-assets.json'
```

Both final reports match the final manifests and all six videos decoded completely. Each profile has C1/C2/C3plus, six PNG panels and six VTT cues per variant, 1920x1080 H.264 yuv420p, 30/1 FPS, 1200 frames and 40 seconds. C1/C2 describe the universal kit. The C3plus competence is Cannoniere in Italian. Party has separate content, subtitles, files and hashes. These checks do not establish host playback, native rendering or optical readability.

Final manifest SHA256:

- standard: `35c1c5c45288c06ace693378c2f35ab51be8e52d69ded70c082881e3e7eab633`
- Party: `c657990f1a990174e6fb4b57a412fa48a6a861685d95cdce0eb48baf4d0d525d`

Review corrections include injective physical-slot assignment, real target instance/phase matching, preservation of SC1--4 release/busy/outcomes, Party progress distinct from mature consent, bounded held font atlas and invalidation, diagnostic v3 without gameplay intent, separate report directories per crew profile and exact zone-manifest fingerprint. C++ Evidence tilt range now matches Python/SC1 [-90,90].

Native regression cases written and registered by the coordinator, not compiled or run: `crewHeaderRequiresDistinctClosedV3Contract` and `crewIntroRequiresProfileRulesLayoutAndSixPanels`. Existing native SC4 parser fixtures now contain the complete closed fingerprint. The shader's nominal declared-component count is about 948; target masks use vec4[8] and the shared sampler15. Driver limits, shader compilation/link, native font/readback, build, app startup, native tests, playback, hardware and venue qualification remain unverified. SC6 was not started.

For the R8 font-page readback policy, the Khronos glGetTexImage reference specifies expansion to red with other colour components zero and alpha one: https://wikis.khronos.org/opengl/GLAPI/glGetTexImage. Therefore the atlas uses red coverage for GL_R8 and alpha for RGBA glyph pages, rather than assuming the sampler swizzle affects CPU readback.

Temporary intro segments and ffconcat manifests were removed only after final MP4/PNG/VTT/manifests and decode reports were preserved. All delivered assets, tests and reports remain on the SSD.
