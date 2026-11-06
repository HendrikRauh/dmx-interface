# %%
import itertools
import json
import os
import struct
import subprocess
from pathlib import Path

try:
    from ocp_vscode import show

    in_vscode = True
except ImportError:

    def show(*args, **kwargs):
        pass

    in_vscode = False

from build123d import (
    Align,
    Box,
    BuildPart,
    BuildSketch,
    Color,
    Compound,
    Cone,
    Cylinder,
    Keep,
    Location,
    Locations,
    Mode,
    Plane,
    RegularPolygon,
    add,
    export_gltf,
    export_step,
    export_stl,
    extrude,
    import_step,
    offset,
)

SCRIPT_DIR = Path(__file__).resolve().parent  # assets/case/src
CASE_ROOT = SCRIPT_DIR.parent  # assets/case
PARTS_DIR = CASE_ROOT / "parts"  # assets/case/parts
OUTPUT_DIR = CASE_ROOT / "output"  # assets/case/output

# %% --- Parameters (all mm, case centered at origin: x=long, y=wide, z=up)

## Variant toggles — every combination of these gets its own export set.
## Each value is (file token, docs description): the token names the files
## (ChaosDMX_bottom_<token>.stl, ChaosDMX_<token>.step) and both end up in
## manifest.json, which drives the docs downloads table. Descriptions are
## self-contained ("solid lid", not "none"), so any combination reads
## unambiguously. Add a toggle = add one entry here; exports, manifest and
## docs table pick it up.
TOGGLES = {
    "xlr_screws": {
        True: ("xlrScrewed", "screwed XLR (M3)"),
        False: ("xlrGlued", "glued XLR"),
    },  # M3 clearance holes in the +X wall
    "vent": {True: ("vent", "vent"), False: ("noVent", "solid")},
}
DEFAULT_VARIANT = {"xlr_screws": True, "vent": True}

## Shell
wall = 2
box_size = (110, 65, 40)
lid_t = 2
inner_top = box_size[2] / 2 - lid_t  # 18: split plane / lid underside
floor_z = -box_size[2] / 2 + wall  # -18: cavity floor

## XLR connectors (NC3FD/NC3MD-LX, rear mount, D cutout, latch up)
xlr_bore_r = 12.0  # D-nose passes a plain Ø24 hole
xlr_y0 = 15.5  # bore centers at +/- this on the +X face
xlr_thread = ((9.5, 12.0), (-9.5, -12.0))  # M3 threads, local (x, y) of the flange
xlr_clear_r = 3.4 / 2  # M3 clearance in the wall
xlr_roll = -90.0  # about the local bore axis before placement: latch points up
xlr_body_bottom_z = -11.8  # body cylinder underside (D23.6 about the bore axis)

## W5500 (RJ45 through the -Y long wall); STEP local (0,0,z) is the PCB top face,
## local +x toward the jack face (proud of the wall), local +y across the board.
w5500_pos = (-23.0, -29.4, -14.0)
w5500_rot = (0, 0, 90)  # local +x -> case +y, local +y -> case -x
w5500_pcb_t = 1.6  # PCB thickness; bottom (standoff seat) at pos.z - pcb_t
w5500_pilot_d = 2.1  # M2.5 self-tap
w5500_holes_near = ((1.5, 1.5), (1.5, 26.4))  # local (x, y), jack-end pair
w5500_rib_y = 24.5  # case y of the far-edge support rib (local x=53.9)

## S2 Mini (USB-C through the -Y long wall, next to the RJ45)
s2_pos = (-11.0, -29.5, -15.0)  # case pos of STEP local (0, 0, pcb bottom nominal)
s2_pilot_d = 1.7  # M2 self-tap
s2_holes = ((2.5, 30.45), (22.9, 30.45))  # local (x, y)
usb_slot_w, usb_slot_h = 9.6, 4.4  # through the -Y wall
usb_local_x, usb_local_z0, usb_local_z1 = 12.7, 0.9, 3.9  # shell extent

## Button (12 mm panel module, M12 thread, nut inside)
button_hole_d = 12.2
button_pos = (s2_pos[0] + usb_local_x, -box_size[1] / 2, 2.0)  # -Y face, above USB

## MAX485 slide-in mounts on the +Y inner wall (two modules, stacked)
max485_w, max485_h = 21.0, 12.0  # PCB width (along case x), height (along z)
max485_x0 = -9.5  # left edge of the modules
max485_z = (8.5, -8.5)  # module centers
max485_depth = 6.0  # rails protrude this far from the wall
max485_gap = 12.4  # rail-to-rail opening (PCB 12 + clearance)
max485_rail_t = 2.0

## Lid fasteners: M3 heat-set inserts in bosses, screws from the top
lid_screws = ((-18.0, 26.0), (-18.0, -26.0), (21.0, 26.0), (21.0, -26.0))
boss_d = 9.0
insert_d = 4.0  # M3 standard heat-set pilot
insert_depth = 5.5
lid_hole_d = 3.4
lid_cs_head_d = 6.5  # M3 countersunk head
lid_cs_depth = 1.55  # 90 deg: (6.5 - 3.4) / 2

## Lid vent grid
hex_af = 5.0  # hexagon across flats
hex_pitch = 6.6
vent_bounds = (-42.0, 42.0, -20.0, 20.0)  # x0, x1, y0, y1


# %% --- Helpers


def standoff(x, y, top_z, base_z, outer_d, pilot_d, pilot_depth):
    with Locations((x, y, base_z)):
        Cylinder(
            outer_d / 2, top_z - base_z, align=(Align.CENTER, Align.CENTER, Align.MIN)
        )
        Cylinder(
            pilot_d / 2,
            pilot_depth + 1.0,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )


def to_case_w5500(lx, ly):
    """W5500 local (x, y) -> case (x, y) for w5500_pos / w5500_rot."""
    # Rz(+90): (lx, ly) -> (-ly, lx), then translate
    return (w5500_pos[0] - ly, w5500_pos[1] + lx)


VARIANTS = [
    dict(zip(TOGGLES, combo, strict=True))
    for combo in itertools.product(*TOGGLES.values())
]


def variant_suffix(variant):
    """File-name tokens of one toggle combination, in TOGGLES order."""
    return "_".join(TOGGLES[key][variant[key]][0] for key in TOGGLES)


def variant_description(variant):
    """Human-readable variant summary for the docs downloads table."""
    return " · ".join(TOGGLES[key][variant[key]][1] for key in TOGGLES)


def variant_files(suffix):
    """Export file names of one variant — the single naming source."""
    return {
        "box": f"ChaosDMX_bottom_{suffix}.stl",
        "lid": f"ChaosDMX_top_{suffix}.stl",
        "step": f"ChaosDMX_{suffix}.step",
        "glb": f"ChaosDMX_{suffix}.glb",
    }


# %% --- Import hardware for placement / interference checks

print("Importing STEP parts...")
esp = import_step(f"{PARTS_DIR}/S2 Mini Board_no_hdr.step")
w5500 = import_step(f"{PARTS_DIR}/w5500.step")
xlr_fd = import_step(f"{PARTS_DIR}/3-D NC3FD-LX.stp")
xlr_md = import_step(f"{PARTS_DIR}/3-D NC3MD-LX.stp")

inner_x = box_size[0] / 2 - wall  # 53

# XLR: roll about the bore axis (latch up), then Rot(Y,-90) sends the nose
# (local z<0) out the +X wall. Net: local (x,y,z) -> case (-z, y0 - x, y).
xlr_roll_at = Location((0, 0, 0), (0, 0, xlr_roll))
xlr_fd_at = Location((inner_x, xlr_y0, 0), (0, -90, 0)) * xlr_roll_at * xlr_fd
xlr_md_at = Location((inner_x, -xlr_y0, 0), (0, -90, 0)) * xlr_roll_at * xlr_md

# S2 Mini: identity; USB edge (local y=0) faces the -Y wall
esp_at = Location(s2_pos) * esp

# W5500: Rz(+90) as documented above
w5500_at = Location(w5500_pos, w5500_rot) * w5500

# Component colors for the GLB preview, all Catppuccin Mocha (the vendor
# STEPs carry none except one stray LIGHTGOLDENROD2 XLR). The case below is
# Mocha teal/sky already.
esp_at.color = Color("#fab387")  # S2 Mini: peach
w5500_at.color = Color("#cba6f7")  # W5500 (Ethernet): mauve
xlr_fd_at.color = Color("#f38ba8")  # XLR front: red
xlr_md_at.color = Color("#89b4fa")  # XLR rear: blue

print("Placement bboxes:")
for name, p in [
    ("xlr_fd", xlr_fd_at),
    ("xlr_md", xlr_md_at),
    ("esp", esp_at),
    ("w5500", w5500_at),
]:
    bb = p.bounding_box()
    print(
        f"  {name:8s} x[{bb.min.X:7.2f},{bb.max.X:7.2f}] "
        f"y[{bb.min.Y:7.2f},{bb.max.Y:7.2f}] z[{bb.min.Z:7.2f},{bb.max.Z:7.2f}]"
    )

# %% --- Build one variant (bottom + lid) for one TOGGLES combination


def build_case(xlr_screws: bool, vent: bool):
    """Build and label case bottom + lid for one toggle combination.

    The bottom always gets the plain Ø24 XLR bores; `xlr_screws` adds the
    M3 clearance holes through the +X wall, `vent` the lid + bottom vent grids.
    """
    with BuildPart() as main_body:
        Box(*box_size)
        offset(amount=-wall, mode=Mode.SUBTRACT)

        # XLR: bores always; M3 clearance holes only for xlrScrewed (the ears
        # stay inside the cavity after the roll, so no wall relief either way)
        for y0 in (xlr_y0, -xlr_y0):
            with Locations((box_size[0] / 2, y0, 0)):
                Cylinder(
                    xlr_bore_r, wall * 2 + 1, rotation=(0, 90, 0), mode=Mode.SUBTRACT
                )
            if not xlr_screws:
                continue
            for lx, ly in xlr_thread:
                # rolled local (lx, ly) -> case (y0 - lx, ly)
                with Locations((box_size[0] / 2, y0 - lx, ly)):
                    Cylinder(
                        xlr_clear_r,
                        wall * 2 + 1,
                        rotation=(0, 90, 0),
                        mode=Mode.SUBTRACT,
                    )

        # XLR body support ribs (under the body cylinder bottom edge at z=-11.8)
        for y0 in (xlr_y0, -xlr_y0):
            with Locations((38.5, y0 + 0.1, floor_z)):
                Box(
                    22.0,
                    8.0,
                    xlr_body_bottom_z - floor_z,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                )

        # Button hole (-Y face, above the USB slot)
        with Locations(button_pos):
            Cylinder(
                button_hole_d / 2,
                wall * 2 + 1,
                rotation=(90, 0, 0),
                mode=Mode.SUBTRACT,
            )

        # W5500: standoffs at the jack-end holes (seat = PCB bottom = pos.z - pcb_t),
        # far-edge support rib at the same height
        w5500_seat_z = w5500_pos[2] - w5500_pcb_t
        for lx, ly in w5500_holes_near:
            cx, cy = to_case_w5500(lx, ly)
            standoff(cx, cy, w5500_seat_z, floor_z, 8.0, w5500_pilot_d, 4.0)
        # rib under the far edge (local x=53.9 -> case y=24.5), board x span
        bx0, _ = to_case_w5500(0, 0)
        bx1, _ = to_case_w5500(0, 27.9)
        with Locations(((bx0 + bx1) / 2, w5500_rib_y, floor_z)):
            Box(
                abs(bx0 - bx1),
                3.0,
                w5500_seat_z - floor_z,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
            )

        # W5500 RJ45 cutout through the -Y wall
        # jack body local y 5.5..22.2 (measured clash: reaches case x=-28.38),
        # z local 0..13.7 above the PCB top
        with Locations(
            (
                w5500_pos[0] - (5.5 + 22.2) / 2,  # x center
                -box_size[1] / 2,  # y: through the -Y wall
                w5500_pos[2] + (0 + 13.7) / 2,  # z center of the jack body
            )
        ):
            Box(
                (22.2 - 5.5) + 0.8,
                wall * 2 + 1,
                13.7 + 0.8,
                mode=Mode.SUBTRACT,
            )

        # S2 Mini: standoffs at the mount holes (seat 0.1 below the nominal PCB
        # bottom — the STEP hole-region material reaches z-0.05)
        s2_seat_z = s2_pos[2] - 0.1
        for hx, hy in s2_holes:
            standoff(
                s2_pos[0] + hx,
                s2_pos[1] + hy,
                s2_seat_z,
                floor_z,
                7.0,
                s2_pilot_d,
                3.5,
            )
        # rib under the USB edge, 0.5 below the seat (clears the shell pegs at -0.4)
        with Locations((s2_pos[0] + 13.2, s2_pos[1] + 1.5, floor_z)):
            Box(
                26.0,
                3.0,
                s2_pos[2] - 0.5 - floor_z,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
            )

        # USB-C cutout through the -Y wall
        with Locations(
            (
                s2_pos[0] + usb_local_x,
                -box_size[1] / 2,
                s2_pos[2] + (usb_local_z0 + usb_local_z1) / 2,
            )
        ):
            Box(
                usb_slot_w,
                wall * 2 + 1,
                (usb_local_z1 - usb_local_z0) + 0.5,
                mode=Mode.SUBTRACT,
            )

        # MAX485 slide-in frames on the +Y inner wall (two, stacked)
        wall_y_in = box_size[1] / 2 - wall  # 30.5
        rail_y = wall_y_in - max485_depth / 2
        rail_len = max485_w + 4.0  # entry overhang on the +x side
        rail_cx = max485_x0 - 1.0 + rail_len / 2
        for zc in max485_z:
            with Locations((rail_cx, rail_y, zc - max485_gap / 2 - max485_rail_t)):
                Box(
                    rail_len,
                    max485_depth,
                    max485_rail_t,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                )
            with Locations((rail_cx, rail_y, zc + max485_gap / 2)):
                Box(
                    rail_len,
                    max485_depth,
                    max485_rail_t,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                )
            # end stop (-x side)
            with Locations((max485_x0 - 2.0, rail_y, zc)):
                Box(2.0, max485_depth, max485_h + 4.0)

        # Lid screw bosses with insert holes from the top
        for bx, by in lid_screws:
            standoff(
                bx, by, inner_top, floor_z, boss_d, 0.0, 0.0
            )  # solid boss, no pilot
            with Locations((bx, by, inner_top - insert_depth)):
                Cylinder(
                    insert_d / 2,
                    insert_depth + 0.5,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                    mode=Mode.SUBTRACT,
                )

        # Split into bottom and lid (Keep.BOTH -> (above plane, below plane))
        above, below = main_body.part.split(Plane.XY.offset(inner_top), Keep.BOTH)

    print(
        f"bottom z: {below.bounding_box().min.Z:.2f}..{below.bounding_box().max.Z:.2f}"
    )
    print(
        f"lid    z: {above.bounding_box().min.Z:.2f}..{above.bounding_box().max.Z:.2f}"
    )

    # Lid: screw holes, countersinks, vent grid
    with BuildPart() as lid_build:
        add(above)

        for bx, by in lid_screws:
            with Locations((bx, by, inner_top + lid_t)):
                Cylinder(
                    lid_hole_d / 2,
                    lid_t * 2,
                    rotation=(180, 0, 0),
                    mode=Mode.SUBTRACT,
                )
                # 90 deg countersink from the top face down
                with Locations((0, 0, -lid_cs_depth)):
                    Cone(
                        lid_cs_head_d / 2,
                        lid_hole_d / 2,
                        lid_cs_depth + 0.2,
                        rotation=(180, 0, 0),
                        mode=Mode.SUBTRACT,
                    )

        if vent:
            hex_r = (hex_af / 2) / 0.866_025_4  # circumradius from across-flats
            x_lo, x_hi, y_lo, y_hi = vent_bounds
            dy = hex_pitch * 0.866_025_4
            max_row = int((y_hi - y_lo) / (2 * dy))
            vent_locs = []
            for iy in range(-max_row, max_row + 1):
                y = iy * dy
                if not (y_lo <= y <= y_hi):
                    continue
                if iy % 2 == 0:
                    max_col = int((x_hi - x_lo) / (2 * hex_pitch))
                    xs = [j * hex_pitch for j in range(-max_col, max_col + 1)]
                else:
                    max_col = int((x_hi - x_lo) / (2 * hex_pitch))
                    xs = [(j + 0.5) * hex_pitch for j in range(-max_col, max_col)]
                for x in xs:
                    if x_lo <= x <= x_hi and y_lo <= y <= y_hi:
                        vent_locs.append((x, y))
            print(f"vent (lid): {len(vent_locs)} hexes")
            if vent_locs:
                with (
                    BuildSketch(Plane.XY.offset(inner_top + lid_t + 0.1)),
                    Locations(*vent_locs),
                ):
                    RegularPolygon(hex_r, 6)
                extrude(amount=-(lid_t + 0.2), mode=Mode.SUBTRACT)

    # Bottom vents through the floor, coupled to the same `vent` toggle.
    # Keep-out around the S2 Mini standoffs so the holes never eat a boss.
    if vent:
        hex_r = (hex_af / 2) / 0.866_025_4
        bx_lo, bx_hi, by_lo, by_hi = -30.0, 30.0, -12.0, 12.0
        keepouts = [(s2_pos[0] + hx, s2_pos[1] + hy) for hx, hy in s2_holes]
        dy = hex_pitch * 0.866_025_4
        max_row = int((by_hi - by_lo) / (2 * dy))
        bottom_vent_locs = []
        for iy in range(-max_row, max_row + 1):
            y = iy * dy
            if not (by_lo <= y <= by_hi):
                continue
            if iy % 2 == 0:
                max_col = int((bx_hi - bx_lo) / (2 * hex_pitch))
                xs = [j * hex_pitch for j in range(-max_col, max_col + 1)]
            else:
                max_col = int((bx_hi - bx_lo) / (2 * hex_pitch))
                xs = [(j + 0.5) * hex_pitch for j in range(-max_col, max_col)]
            for x in xs:
                if not (bx_lo <= x <= bx_hi and by_lo <= y <= by_hi):
                    continue
                if any(
                    ((x - kx) ** 2 + (y - ky) ** 2) ** 0.5 < 8.0 for kx, ky in keepouts
                ):
                    continue
                bottom_vent_locs.append((x, y))
        print(f"vent (bottom): {len(bottom_vent_locs)} hexes")
        if bottom_vent_locs:
            with BuildPart() as bottom_build:
                add(below)
                with (
                    BuildSketch(Plane.XY.offset(-box_size[2] / 2)),
                    Locations(*bottom_vent_locs),
                ):
                    RegularPolygon(hex_r, 6)
                extrude(amount=wall + 0.2, mode=Mode.SUBTRACT)
            below = bottom_build.part

    below.label = "Case Bottom"
    below.color = Color("#94e2d5")
    lid = lid_build.part
    lid.label = "Case Lid"
    lid.color = Color("#74c7ec")
    return below, lid


# %% --- Build every variant (cartesian product of TOGGLES)

built = {}
for variant in VARIANTS:
    suffix = variant_suffix(variant)
    built[suffix] = build_case(**variant)
    print(f"built variant: {suffix}")

# %% --- Interference check: placed hardware vs case solids

print("Interference check (intersection volume, mm^3):")
ok = True
for suffix, (bottom, _lid) in built.items():
    for name, part in [
        ("xlr_fd", xlr_fd_at),
        ("xlr_md", xlr_md_at),
        ("esp", esp_at),
        ("w5500", w5500_at),
    ]:
        inter = part.intersect(bottom)  # None => no common volume
        if inter is None:
            v = 0.0
        elif hasattr(inter, "volume"):
            v = inter.volume
        else:  # ShapeList of solids
            v = sum(s.volume for s in inter)
        flag = "OK" if v < 1.0 else "CLASH"
        if v >= 1.0:
            ok = False
        print(f"  {suffix:20s} {name:8s} vs bottom: {v:10.3f}  {flag}")

# %% --- Display

default_bottom, default_lid = built[variant_suffix(DEFAULT_VARIANT)]
placed = [default_bottom, default_lid, xlr_fd_at, xlr_md_at, esp_at, w5500_at]
if in_vscode and "VSCODE_CWD" in os.environ:
    show(*placed)

# %% --- GLB part ids


def _label_glb_parts(path: Path, part_ids: list[str]) -> None:
    """Give the exported parts stable names for the docs viewer.

    The glTF writer emits OCCT's internal names (`=>[0:1:1:2]`) instead of the
    build123d labels, and a part may be a whole subtree (the S2 Mini board
    exports ~300 solids), so visibility is toggled on the subtree's root node.
    Renames the scene root's children — they follow the Compound child order —
    and strips every other internal name. Rewrites only the JSON chunk of the
    binary file; the BIN chunk is copied verbatim, lengths recomputed.
    """
    raw = path.read_bytes()
    magic, version, _total = struct.unpack_from("<4sII", raw)
    if magic != b"glTF" or version != 2:
        raise ValueError(f"{path.name}: not a binary GLB (glTF 2) file")
    json_len, json_type = struct.unpack_from("<II", raw, 12)
    doc = json.loads(raw[20 : 20 + json_len])
    root = doc["nodes"][doc["scenes"][doc["scene"]]["nodes"][0]]
    children = root.get("children", [])
    if len(children) != len(part_ids):
        raise ValueError(
            f"{path.name}: expected {len(part_ids)} parts under the scene root, "
            f"got {len(children)}"
        )
    for node in doc["nodes"]:
        node.pop("name", None)  # internal name, meaningless outside OCCT
    for child_id, part_id in zip(children, part_ids, strict=True):
        node = doc["nodes"][child_id]
        node["name"] = part_id
        if "mesh" in node:  # single-mesh parts name the mesh too (Blender et al.)
            doc["meshes"][node["mesh"]]["name"] = part_id
    payload = json.dumps(doc, separators=(",", ":")).encode()
    payload += b" " * (-len(payload) % 4)  # JSON chunks are space-padded
    tail = raw[20 + json_len :]  # BIN chunk incl. its 8-byte header
    header = struct.pack("<4sII", magic, version, 12 + 8 + len(payload) + len(tail))
    path.write_bytes(
        header + struct.pack("<II", len(payload), json_type) + payload + tail
    )


# %% --- Export

# A clashing build must not leave fresh STLs/STEP/GLB or a manifest behind:
# _ensure_case_outputs (tasks.py) trusts the manifest and would skip
# regeneration of the next local `inv docs`/deploy forever.
if not ok:
    raise SystemExit("interference check failed — not exporting")

print("Exporting models...")
os.makedirs(OUTPUT_DIR, exist_ok=True)
# output/ is generated (git-ignored): drop files from earlier variant sets so
# renamed or removed toggles cannot linger as stale downloads.
for stale in OUTPUT_DIR.iterdir():
    if stale.suffix in {".stl", ".step", ".glb"}:
        stale.unlink()

for variant in VARIANTS:
    suffix = variant_suffix(variant)
    bottom, lid = built[suffix]
    files = variant_files(suffix)
    export_stl(bottom, str(OUTPUT_DIR / files["box"]))
    export_stl(lid, str(OUTPUT_DIR / files["lid"]))
    export_step(Compound(children=[bottom, lid]), str(OUTPUT_DIR / files["step"]))
    print(f"exported {suffix}")

# The print files above stay case-only; the GLB additionally embeds the
# hardware so the docs viewer can show it (Components toggle) — it exists for
# the default variant only.
default_suffix = variant_suffix(DEFAULT_VARIANT)
default_files = variant_files(default_suffix)
default_bottom, default_lid = built[default_suffix]
glb_path = OUTPUT_DIR / default_files["glb"]
# Derived from the manifest name (not re-hardcoded) so a prefix change here
# stays the single source; the .raw.glb never reaches output/ anyway.
raw_glb = OUTPUT_DIR / f"{default_files['glb'][: -len('.glb')]}.raw.glb"
export_gltf(
    Compound(
        children=[default_bottom, default_lid, xlr_fd_at, xlr_md_at, esp_at, w5500_at]
    ),
    str(raw_glb),
    binary=True,
)
# The vendor boards explode the mesh count (~24 MB raw). Quantize and
# simplify down to ~5 MB — no meshopt compression, three's GLTFLoader needs
# setMeshoptDecoder for that and model-viewer does not call it. -kn keeps the
# node names that _label_glb_parts relies on and that the viewer toggles by.
try:
    subprocess.run(
        ["gltfpack", "-kn", "-si", "0.5", "-i", str(raw_glb), "-o", str(glb_path)],
        check=True,
    )
except FileNotFoundError as exc:
    raise SystemExit("gltfpack not found on PATH (run inside `nix develop`)") from exc
raw_glb.unlink()
_label_glb_parts(
    glb_path,
    ["case_bottom", "case_lid", "xlr_fd", "xlr_md", "esp", "w5500"],
)

# Manifest for the docs site: the downloads table (case-downloads fence in
# docs/case.md) and the preview-GLB picker in DocHead.astro read this file.
variant_entries = []
for variant in VARIANTS:
    suffix = variant_suffix(variant)
    files = variant_files(suffix)
    variant_entries.append(
        {
            "description": variant_description(variant),
            "suffix": suffix,
            "box": files["box"],
            "lid": files["lid"],
            "step": files["step"],
        }
    )
with open(OUTPUT_DIR / "manifest.json", "w", encoding="utf-8") as f:
    json.dump(
        {"preview_glb": default_files["glb"], "variants": variant_entries}, f, indent=2
    )
    f.write("\n")

print(f"Done: {len(VARIANTS)} variants + manifest.json -> {OUTPUT_DIR}")
