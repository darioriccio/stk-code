# SC4 Relay mission data

`snowtuxpeak_mission_v1.json` is authored development data, **not locally qualified**. It is deliberately labelled `development_unqualified`. Public SC4 readiness must remain closed until a matching runtime qualification has been obtained.

The six authored groups use the native lower-center 3D route, separate 6.6 m between the two 2 m spheres, and place their centers 4.15 m along the quad normal. Activation uses the last native node at least 70 m before the target node; expiry uses the last node at least 6 m before it. The two positions are newly authored simultaneous objects, not the overlapping lateral alternatives in the planning JSON. Native graph distances, hashes and forward direction are validated at held-world preparation. Neither geometry nor straight-line calculations prove unobstructed visibility in Snow Tux Peak.

All 12 native lap indices are allowed; actual session laps bound allocation. Three confirmation stations at nodes 31/63/80 are narrative only. No track, mesh, collision or native pickup is changed. Sphere graphics are procedural Relay-owned primitives with matching selectable geometry.

Source: existing Snow Tux Peak driveline XML, whose exact concatenated SHA-256 is recorded in the manifest. New data and procedural graphic definitions: Immersive Relay contributors, GPL-3.0-or-later. Existing track assets keep their original licenses.

## Native result presentation

`mission_results_it_v1.png` is the final Italian transparent type atlas: 2048×800 RGBA, five 160-pixel rows in the immutable outcome order. Native rank and secured N/3 are drawn from the frozen record; absent official rank uses a dash. The same atlas reaches both projection shaders. `tools/immersive/scintilla/create_mission_result_atlas.py` regenerates it offline with Pillow and an explicitly selectable installed font. It uses Arial Bold on the authoring Mac; no font file is bundled. The newly authored wording, composition, procedural graphics and generator are GPL-3.0-or-later. Existing typeface software stays with its owner.

The atlas is loaded only while the world is held. Texture-cache removal invalidates its pointer before destruction; a successful held resource preparation restores readiness without rewriting cargo, timers or the final result. Native result textures use fragment unit 15, restoring its previous binding after either projection draw. The six active cargo/beacon HUD circles and terminal result plate have matching picking masks; the terminal plate is never interactive.

## Local Italian introduction

[`intro_it_v1/README.md`](intro_it_v1/README.md) describes the final original 2D panels, the C1/C2/C3plus clips and WebVTT subtitles, their CC-BY-4.0 attribution and offline authoring. Each variant is exactly 40 s / 1200 frames / 30 fps at 1920×1080. The raw manifest and every selected asset are hashed at held-world preparation. The host uses the existing front-surface composition and actual media-end callback; receipt completion never opens gameplay by itself.
