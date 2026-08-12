# %%
import os
from pathlib import Path

try:
    from ocp_vscode import show

    in_vscode = True
except ImportError:
    in_vscode = False

from build123d import (
    Align,
    Axis,
    Locations,
    Box,
    Cylinder,
    BuildPart,
    Color,
    Compound,
    export_step,
    import_step,
    export_stl,
    Keep,
    Mode,
    offset,
    Plane,
    GeomType,
)

SCRIPT_DIR = Path(__file__).resolve().parent  # assets/case/src
CASE_ROOT = SCRIPT_DIR.parent  # assets/case
PARTS_DIR = CASE_ROOT / "parts"  # assets/case/parts
OUTPUT_DIR = CASE_ROOT / "output"  # assets/case/output

## Case Wallthickness in mm
wall_thickness = 2

xlr_main_diameter = 12
xlr_screw_diameter = 3.5
xlr_count = 2

## Base size of the case (length, width, height)
box_size = (110, 65, 40)

# %%
print("Importing STEP file for S2 Mini Board...")
esp = import_step(f"{PARTS_DIR}/S2 Mini Board_no_hdr.step")
print("STEP file imported successfully.")
# %%

with BuildPart() as main_body:
    Box(*box_size)
    offset(amount=-wall_thickness, mode=Mode.SUBTRACT)

    # TODO: use the shipped center things for easy alignment
    with Locations(
        (box_size[0] / 2, box_size[1] / 4, 0),
        (box_size[0] / 2, -1 * box_size[1] / 4, 0),
    ): 
        Cylinder(
            xlr_main_diameter,
            wall_thickness * 2,
            rotation=(0, 90, 0),
            mode=Mode.SUBTRACT,
        )

    esp.position = (
        0,
        -main_body.faces().sort_by(Axis.Y)[-2].center().Y,
        -main_body.faces().sort_by(Axis.Z)[-2].center().Z,
    )

    target_radius = 1.6
    hole_faces = esp.faces().filter_by(GeomType.CYLINDER)
    mounting_holes = [f for f in hole_faces if abs(f.radius - target_radius) < 0.3]
    mounting_holes_pos = [f.center() for f in mounting_holes]
    
    
    support_top_z = esp.bounding_box().min.Z 
    support_height = support_top_z - -main_body.faces().sort_by(Axis.Z)[-2].center().Z

    for x, y, z in mounting_holes_pos:
        with Locations((x, y, z)):
            Cylinder(
                target_radius * 2,
                support_height,
            )

    # Get the Z coordinate of the inner top face
    inner_top_z = main_body.faces().sort_by(Axis.Z)[-2].center().Z

    # Plane used to split the case into bottom and lid
    split_plane = Plane.XY.offset(inner_top_z)

    bottom, top = main_body.part.split(split_plane, Keep.ALL)

bottom.label = "Case Bottom"
top.label = "Case Lid"
bottom.color = Color("#94e2d5", 0.75)
top.color = Color("#74c7ec", 0.75)

if in_vscode and "VSCODE_CWD" in os.environ:
    show(bottom, top, esp)

# %%

print("Exporting models...")

os.makedirs(OUTPUT_DIR, exist_ok=True)

export_stl(bottom, str(OUTPUT_DIR / "case_bottom.stl"))
export_stl(top, str(OUTPUT_DIR / "case_top.stl"))

combined = Compound(children=[bottom, top])
export_step(combined, str(OUTPUT_DIR / "case.step"))

print("Done. Files generated: STL (bottom/top) and STEP (combined)")
