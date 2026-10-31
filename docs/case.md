---
title: Case
description: 3D-printable enclosure — build123d source, generated print files, hardware and known issues.
slug: case
---

The enclosure is 3D-printable and is modelled with
[build123d](https://build123d.readthedocs.io/en/latest/) — like
[OpenSCAD](https://openscad.org/) but with some advantages. The first revision
we shipped was designed in
[OnShape](https://cad.onshape.com/documents/7363818fd18bf0cbf094790e/w/52455282b39e47fbde5d0e53/e/9bec98aa83a813dc9a4d6ab2)
and still has [known issues](https://github.com/HendrikRauh/dmx-interface/issues/92);
the rework replaces it. Feel free to
[contribute](./development.md#contributing) to it!

## Preview

Drag to rotate, scroll or pinch to zoom. The model is regenerated together with
the print files (`inv case:generate`) and shows the case bottom (teal) and lid
(blue) in their assembled positions. Checkboxes below the view hide the lid or
the bottom (hide the lid to look inside); **Components** shows the placed S2
Mini (peach), W5500 (mauve) and both XLR sockets (red & blue), which are hidden by default.

<model-viewer id="case-viewer" alt="3D model of the case bottom and lid, assembled" camera-controls touch-action="pan-y" auto-rotate rotation-per-second="10deg" interaction-prompt="none" shadow-intensity="0.8">
  <p>
    The interactive 3D view needs a WebGL-capable browser — the print files are
    in the Downloads section below.
  </p>
</model-viewer>

## Downloads

The docs CI regenerates the print files on every deployment, so these links
always serve the current revision. `inv case:generate` builds every
combination of the two case toggles (`TOGGLES` in
`assets/case/src/main_case.py`) and spells it out in the filename:
`xlrScrewed`/`xlrGlued` for the M3 clearance holes behind the XLR sockets,
`vent`/`noVent` for the vent grids (lid + bottom). The table below is rendered at
build time from the generated `assets/case/output/manifest.json`, so it always
matches the files in `assets/case/output/`.

```case-downloads
```

## Generating the print files

The model lives in `assets/case/src/main_case.py` and imports the vendor parts
(XLR sockets, W5500, the bare S2 Mini board) from `assets/case/parts/`. Those
parts are stored with [git-lfs](https://git-lfs.com/), so install it before
cloning the repository. Render the model with:

```sh
inv case:generate
```

This writes one `ChaosDMX_bottom_…stl` / `ChaosDMX_top_…stl` pair plus one
`ChaosDMX_….step` per variant to `assets/case/output/` (git-ignored), the default
variant's GLB for the 3D preview above, and a `manifest.json` listing every
generated file — the Downloads table above is rendered from it. Slice the STL
files as-is, or import a `ChaosDMX_….step` if you want to adapt the design. The
CAD toolchain has a few environment quirks, which are covered in
[development.md](./development.md#cad-environment).

## Print hardware

In addition to the 3D print, the case needs some parts for assembly — which
ones depends on the variant you print.

| Count | Part | Location | Needed when |
| ---: | --- | --- | --- |
| 4x | M3 heat-set insert | Bottom bosses (screwed from the lid) | always |
| 4x | M3 screw | Lid | always |
| 2x | M2x5 screw | S2 Mini | always |
| 2x | M2.5x5 screw | W5500 | always |
| 4x | M3 screw | XLR sockets | `xlrScrewed` variant |
| 4x | M3 nut | XLR sockets (in the connector's own recesses) | `xlrScrewed` variant |
| — | adhesive for the XLR sockets | XLR sockets | `xlrGlued` variant |
