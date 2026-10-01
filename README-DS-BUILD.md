# Qu33ph DS build (v3.4)

Push to GitHub and the "Build Qu33ph DS" action makes `qu33ph.nds` (download it from the
run's Artifacts). Everything the arcade games need is packed INSIDE that one .nds:

- `source/mini.pak` and `source/ball.pak` hold Mini Qu33ph's and Qu33ph-Ball's art and sound.
- The build copies every `.pak` into the cartridge's file system (NitroFS), then checks the
  finished .nds really contains them. If the SDK's own rules didn't pack them, it repacks the
  .nds with `ndstool`; if they still aren't in there, the build FAILS (red X) instead of
  handing you a .nds whose arcade games can't load.
- Fallback for unusual loaders: the game also looks for the .pak files on the SD card in
  `/qu33ph/`, the SD root, or `/NDS/`.

The time on the .nds inside the artifact zip is GitHub's build time in UTC (4 hours ahead of
US Eastern daylight time), so a fresh build made at 3:03 PM Eastern shows 7:03 PM.

Rebuilding the packs from the website's files (only needed if the art or sounds change):
`python3 tools/make_mini.py <website folder>`, `tools/make_ball.py`, `tools/make_main.py`.
