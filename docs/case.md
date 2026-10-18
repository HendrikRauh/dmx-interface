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

## Generating the print files

The model lives in `assets/case/src/main_case.py` and imports the vendor parts
(XLR sockets, W5500, the bare S2 Mini board) from `assets/case/parts/`. Those
parts are stored with [git-lfs](https://git-lfs.com/), so install it before
cloning the repository. Render the model with:

```sh
nix develop -c inv generate-case
```

This writes the print files to `assets/case/output/` (git-ignored):
`case_top.stl`, `case_bottom.stl` and `case.step`. Slice the STL files as-is,
or import `case.step` if you want to adapt the design. The CAD toolchain has a
few environment quirks, which are covered in
[development.md](./development.md#cad-environment).

## Print hardware

In addition to the 3D print, the case needs some parts for assembly.

| Count | Part | Location |
| ---: | --- | --- |
| 6x | M2x5 screw | Case lid, ESP32 |
| 2x | M2.5x5 screw | W5500 |
| 4x | M3 screw | XLR sockets |
| 4x | M3 nut | XLR sockets |
