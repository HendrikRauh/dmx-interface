import os

try:
    from ocp_vscode import show

    in_vscode = True
except ImportError:
    in_vscode = False

from build123d import (
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
)

## Case Wallthickness in mm
wall_thickness = 2

## Base size of the case (length, width, height)
box_size = (110, 65, 40)

with BuildPart() as main_body:
    Box(*box_size)
    offset(amount=-wall_thickness, mode=Mode.SUBTRACT)

    with Locations((box_size[0]/2, box_size[1]/4, 0), (box_size[0]/2, -1*box_size[1]/4, 0)):
        Cylinder(12, wall_thickness*2, rotation=(0,90,0), mode=Mode.SUBTRACT)

    # Get the Z coordinate of the inner top face
    inner_top_z = main_body.faces().sort_by(Axis.Z)[-2].center().Z

    # Plane used to split the case into bottom and lid
    split_plane = Plane.XY.offset(inner_top_z)

    bottom, top = main_body.part.split(split_plane, Keep.ALL)

esp = import_step("assets/case/parts/S2 Mini Board_no_hdr.step")

bottom = bottom + esp.translate((-5,-30,0))

bottom.label = "Case Bottom"
top.label = "Case Lid"
bottom.color = Color("#94e2d5")
top.color = Color("#74c7ec", 0.75)

if in_vscode and "VSCODE_CWD" in os.environ:
    show(bottom, top)

print("Exporting models...")

out_dir = "assets/case/output"
os.makedirs(out_dir, exist_ok=True)

export_stl(bottom, f"{out_dir}/case_bottom.stl")
export_stl(top, f"{out_dir}/case_top.stl")

combined = Compound(children=[bottom, top])
export_step(combined, f"{out_dir}/case.step")

print("Done. Files generated: STL (bottom/top) and STEP (combined)")