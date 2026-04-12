"""Build a GLTF of the IBM 5150 motherboard from BRD data + STEP components.

Reads board_traces.json for geometry, loads STEP files from assets/3dchips/,
places components at BRD coordinates, and exports a .glb scene with
separate named objects per component.

Usage: .venv/Scripts/python.exe scripts/build_board_gltf.py
"""
import json, math, sys, os, time
import numpy as np
import trimesh
from trimesh.visual.material import PBRMaterial

from OCP.STEPControl import STEPControl_Reader
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_FACE
from OCP.BRep import BRep_Tool
from OCP.TopLoc import TopLoc_Location
from OCP.TopoDS import TopoDS
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire, BRepBuilderAPI_MakeFace
from OCP.BRepPrimAPI import BRepPrimAPI_MakePrism
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib
from OCP.gp import gp_Pnt, gp_Vec, gp_Ax1, gp_Dir, gp_Trsf
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform

sys.stdout.reconfigure(encoding='utf-8')

JSON_PATH = "assets/board_traces.json"
CHIPS_DIR = "assets/3dchips"
OUT_PATH = "assets/board_5150.glb"

MIL_TO_MM = 0.0254
PCB_THICKNESS = 1.6
COPPER_THICKNESS = 0.035

# --- Materials ---
MAT_PCB = PBRMaterial(
    name="PCB_Board",
    baseColorFactor=[0x00/255, 0x2d/255, 0x04/255, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.6,
)
MAT_TRACE = PBRMaterial(
    name="Trace",
    baseColorFactor=[63/255, 126/255, 91/255, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.4,
)
MAT_VIA = PBRMaterial(
    name="Via_Tin",
    baseColorFactor=[0.75, 0.75, 0.78, 1.0],
    metallicFactor=1.0,
    roughnessFactor=0.30,
)


def apply_material(mesh, material):
    """Apply a PBR material to a trimesh."""
    mesh.visual = trimesh.visual.TextureVisuals(material=material)

FOOTPRINT_STEP = {
    "DIP-8__300":           "DIP-8_W7.62mm.step",
    "DIP-14__300":          "DIP-14_W7.62mm.step",
    "DIP-16__300":          "DIP-16_W7.62mm.step",
    "DIP-18__300":          "DIP-18_W7.62mm.step",
    "DIP-20__300":          "DIP-20_W7.62mm.step",
    "DIP-24__600":          "DIP-24_W15.24mm.step",
    "DIP-28__600":          "DIP-28_W15.24mm.step",
    "DIP-40__600":          "DIP-40_W15.24mm.step",
    "62":                   "7-5530843-0.step",
    "62PinEdgeIOConnector": "7-5530843-0.step",
    "5PINDIN":              "User Library-DIN-5.STEP",
    "5PINDIN2":             "User Library-DIN-5.STEP",
    "R5":                   "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal.step",
    "C1-1":                 "C_Disc_D3.0mm_W1.6mm_P2.50mm.step",
    "CP8":                  "C_Disc_D7.5mm_W2.5mm_P5.00mm.step",
    "VC":                   "Crystal_HC49-U_Vertical.step",
    "PIN_ARRAY_2X1":        "PinHeader_1x02_P2.54mm_Vertical.step",
    "PIN_ARRAY_2X2":        "PinHeader_2x02_P2.54mm_Vertical.step",
    "PIN_ARRAY_4x1":        "PinHeader_1x04_P2.54mm_Vertical.step",
    "D5":                   "D_DO-35_SOD27_P10.16mm_Horizontal.step",
    "G5V-2DPDT":            "Relay_DPDT_Omron_G5V-2.step",
    "POWER_CON":            "User Library-6 way 0_1inch pitch molex header.step",
    "VR":                   "C_Trimmer_Murata_TZB4-B.step",
    "PE-21712":             "TD1_PE21712_delay.step",
    "TD2":                  "TD2_SIP3_delay.step",
    "HOLE":                 None,
}

# Per-footprint pre-transform: (rot_x_deg, rot_y_deg, rot_z_deg, z_offset_mm)
# Applied BEFORE the BRD orient rotation.
# rot_z: aligns STEP pin axis with BRD pad axis (+90 for KiCad DIPs whose pins run along Y)
# rot_x/rot_y: for connectors that need to face a board edge instead of pointing up
# z_offset: aligns STEP board surface with Z=0
STEP_PRE_TRANSFORM = {
    # KiCad DIPs: pins along Y in STEP, pads along X in BRD -> +90 Z
    "DIP-8__300":   (0, 0, 90, 0),
    "DIP-14__300":  (0, 0, 90, 0),
    "DIP-16__300":  (0, 0, 90, 0),
    "DIP-18__300":  (0, 0, 90, 0),
    "DIP-20__300":  (0, 0, 90, 0),
    "DIP-24__600":  (0, 0, 90, 0),
    "DIP-28__600":  (0, 0, 90, 0),
    "DIP-40__600":  (0, 0, 90, 0),
    # Pin headers: same convention as DIPs
    # Crystal: STEP body along X, BRD pins along Y -> +90
    "VC": (0, 0, 90, 0),
    "PIN_ARRAY_2X1": (0, 0, 90, 0),
    "PIN_ARRAY_2X2": (0, 0, 90, 0),
    "PIN_ARRAY_4x1": (0, 0, 90, 0),
    # ISA slot: body below Z=0, shift up
    "62":                   (0, 0, 0, 15.5),
    "62PinEdgeIOConnector": (0, 0, 0, 15.5),
    # DIN connectors: pins exit at Y=-10.4 in STEP. After +90 X-rot, pins go to Z=-10.4.
    # Z offset +10.4 lifts pin bases to board surface (Z=0), body sits above.
    "5PINDIN":  (90, 0, 0, 10.4),
    "5PINDIN2": (90, 0, 0, 10.4),
    # Power connector: pins exit at Y=-4.5 in STEP. After +90 X-rot, pins go to Z=-4.5.
    # Z offset +4.5 lifts pin bases to board surface.
    "POWER_CON": (90, 0, 0, 4.5),
    # Relay: DIP-8 body, pins along Y like KiCad DIPs -> +90
    "G5V-2DPDT": (0, 0, 90, 0),
}

REF_STEP = {
    "SW1": "206-8.step",
    "SW2": "206-8.step",
    "RN1": "BO_4116R.step",
    "RN2": "BO_4116R.step",
    "RN3": "BO_4116R.step",
    "RN4": "BO_4116R.step",
}

# Base rotation for ref-overridden components (same logic as STEP_BASE_ROTATION)
# Per-ref pre-transforms (overrides STEP_PRE_TRANSFORM)
# Socketed components: ref -> socket STEP file
# Socket is placed at board level; IC is elevated by socket height (~4mm)
SOCKET_HEIGHT = 4.01  # mm, from socket STEP Z-max
SOCKETED_REFS = {}
# CPU (U3) -- socketed
SOCKETED_REFS["U3"] = "DIP-40_W15.24mm_Socket.step"
# FPU (XU4) -- empty socket only, no IC
SOCKETED_REFS["XU4"] = "DIP-40_W15.24mm_Socket.step"
# ROMs U28-U33 (U28 is empty socket only)
for _r in ["U28", "U29", "U30", "U31", "U32", "U33"]:
    SOCKETED_REFS[_r] = "DIP-24_W15.24mm_Socket.step"
# DRAMs: banks 1-3 socketed (U53-U61, U69-U77, U85-U93)
# Bank 0 (U37-U45) is soldered directly to the board (first 64KB)
for _bank in [(53,61), (69,77), (85,93)]:
    for _n in range(_bank[0], _bank[1]+1):
        SOCKETED_REFS[f"U{_n}"] = "DIP-16_W7.62mm_Socket.step"

# Empty sockets: socket placed but no IC on top
EMPTY_SOCKETS = {"XU4", "U28"}

# Not populated at all: no IC, no socket, bare pads only
SKIP_REFS = {"U100", "U101"}

# U95 (75477) is socketed
SOCKETED_REFS["U95"] = "DIP-8_W7.62mm_Socket.step"

REF_PRE_TRANSFORM = {
    # 206-8.step: long axis already along X, no Z-rotation needed. Z offset for body.
    "SW1": (0, 0, 0, 8.2),
    "SW2": (0, 0, 0, 8.2),
    # BO_4116R.step: long axis along Y, needs +90 like KiCad DIPs
    "RN1": (0, 0, 90, 0),
    "RN2": (0, 0, 90, 0),
    "RN3": (0, 0, 90, 0),
    "RN4": (0, 0, 90, 0),
}

# Cache: filename -> (OCC shape, bb_center_x, bb_center_y)
_step_cache = {}


def load_step(filename):
    """Load STEP file, return raw OCC shape. Cached."""
    if filename in _step_cache:
        return _step_cache[filename]
    path = os.path.join(CHIPS_DIR, filename)
    if not os.path.exists(path):
        print(f"  WARNING: missing {path}")
        _step_cache[filename] = None
        return None
    reader = STEPControl_Reader()
    reader.ReadFile(path)
    reader.TransferRoots()
    shape = reader.OneShape()
    _step_cache[filename] = shape
    return shape


def get_bb_center_xy(shape):
    """Return (cx, cy) of shape's bounding box in XY."""
    box = Bnd_Box()
    BRepBndLib.Add_s(shape, box)
    xmin, ymin, zmin, xmax, ymax, zmax = box.Get()
    return ((xmin + xmax) / 2, (ymin + ymax) / 2)


def occ_shape_to_trimesh(shape, linear_deflection=0.1, angular_deflection=0.5):
    """Tessellate an OCC shape to a trimesh.Trimesh."""
    BRepMesh_IncrementalMesh(shape, linear_deflection, False, angular_deflection, True)

    all_verts = []
    all_faces = []
    offset = 0

    explorer = TopExp_Explorer(shape, TopAbs_FACE)
    while explorer.More():
        face = TopoDS.Face_s(explorer.Current())
        loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(face, loc)
        if tri is not None:
            trsf = loc.Transformation()
            n_nodes = tri.NbNodes()
            n_tri = tri.NbTriangles()

            for i in range(1, n_nodes + 1):
                p = tri.Node(i).Transformed(trsf)
                all_verts.append([p.X(), p.Y(), p.Z()])

            for i in range(1, n_tri + 1):
                i1, i2, i3 = tri.Triangle(i).Get()
                all_faces.append([i1 - 1 + offset, i2 - 1 + offset, i3 - 1 + offset])

            offset += n_nodes
        explorer.Next()

    if not all_verts:
        return None
    return trimesh.Trimesh(vertices=np.array(all_verts), faces=np.array(all_faces))


def pad_centroid_mm(pads):
    """Compute centroid of pad positions (in mils), return (cx_mm, cy_mm)."""
    if not pads:
        return (0.0, 0.0)
    xs = [p["x"] for p in pads]
    ys = [p["y"] for p in pads]
    return (sum(xs) / len(xs) * MIL_TO_MM, sum(ys) / len(ys) * MIL_TO_MM)


def build_board_outline(outline_segs, bounds):
    """Build PCB slab. Returns trimesh."""
    segs = list(outline_segs)
    if not segs:
        # Fallback: rectangle from bounds
        b = bounds
        segs = [
            {"x1": b["x_min"], "y1": b["y_min"], "x2": b["x_max"], "y2": b["y_min"]},
            {"x1": b["x_max"], "y1": b["y_min"], "x2": b["x_max"], "y2": b["y_max"]},
            {"x1": b["x_max"], "y1": b["y_max"], "x2": b["x_min"], "y2": b["y_max"]},
            {"x1": b["x_min"], "y1": b["y_max"], "x2": b["x_min"], "y2": b["y_min"]},
        ]

    # Chain segments end-to-end
    ordered = [segs.pop(0)]
    while segs:
        ex, ey = ordered[-1]["x2"], ordered[-1]["y2"]
        found = False
        for i, s in enumerate(segs):
            if abs(s["x1"] - ex) < 1 and abs(s["y1"] - ey) < 1:
                ordered.append(segs.pop(i)); found = True; break
            if abs(s["x2"] - ex) < 1 and abs(s["y2"] - ey) < 1:
                segs[i] = {"x1": s["x2"], "y1": s["y2"], "x2": s["x1"], "y2": s["y1"]}
                ordered.append(segs.pop(i)); found = True; break
        if not found:
            break

    edges = []
    for s in ordered:
        p1 = gp_Pnt(s["x1"] * MIL_TO_MM, s["y1"] * MIL_TO_MM, 0)
        p2 = gp_Pnt(s["x2"] * MIL_TO_MM, s["y2"] * MIL_TO_MM, 0)
        edges.append(BRepBuilderAPI_MakeEdge(p1, p2).Edge())

    wire_builder = BRepBuilderAPI_MakeWire()
    for e in edges:
        wire_builder.Add(e)

    face = BRepBuilderAPI_MakeFace(wire_builder.Wire()).Face()
    prism = BRepPrimAPI_MakePrism(face, gp_Vec(0, 0, -PCB_THICKNESS)).Shape()
    return occ_shape_to_trimesh(prism, linear_deflection=0.5)


def build_traces(traces, layer_filter=None):
    """Build trace segments as thin ribbons. Returns trimesh."""
    verts = []
    faces = []
    for tr in traces:
        if layer_filter is not None and tr.get("layer", 0) != layer_filter:
            continue
        x1, y1 = tr["x1"] * MIL_TO_MM, tr["y1"] * MIL_TO_MM
        x2, y2 = tr["x2"] * MIL_TO_MM, tr["y2"] * MIL_TO_MM
        w = tr["w"] * MIL_TO_MM * 0.5
        dx, dy = x2 - x1, y2 - y1
        length = math.sqrt(dx*dx + dy*dy)
        if length < 0.001:
            continue
        nx, ny = -dy/length*w, dx/length*w
        z_top, z_bot = COPPER_THICKNESS, 0.0
        i = len(verts)
        verts.extend([
            [x1+nx,y1+ny,z_top],[x1-nx,y1-ny,z_top],
            [x2-nx,y2-ny,z_top],[x2+nx,y2+ny,z_top],
            [x1+nx,y1+ny,z_bot],[x1-nx,y1-ny,z_bot],
            [x2-nx,y2-ny,z_bot],[x2+nx,y2+ny,z_bot],
        ])
        faces.extend([
            [i,i+1,i+2],[i,i+2,i+3],
            [i+4,i+6,i+5],[i+4,i+7,i+6],
            [i,i+3,i+7],[i,i+7,i+4],
            [i+1,i+5,i+6],[i+1,i+6,i+2],
            [i,i+4,i+5],[i,i+5,i+1],
            [i+3,i+2,i+6],[i+3,i+6,i+7],
        ])
    if not verts:
        return None
    return trimesh.Trimesh(vertices=np.array(verts, dtype=np.float64),
                           faces=np.array(faces, dtype=np.int64))


def build_vias(vias_data):
    """Build vias as cylinders."""
    meshes = []
    for via in vias_data:
        x, y = via["x"] * MIL_TO_MM, via["y"] * MIL_TO_MM
        r = via["dia"] * MIL_TO_MM * 0.5
        cyl = trimesh.creation.cylinder(radius=r, height=PCB_THICKNESS + COPPER_THICKNESS*2, sections=8)
        cyl.apply_translation([x, y, -PCB_THICKNESS/2 + COPPER_THICKNESS])
        meshes.append(cyl)
    return trimesh.util.concatenate(meshes) if meshes else None


def main():
    t0 = time.time()

    print(f"Loading {JSON_PATH}...")
    with open(JSON_PATH, 'r') as f:
        data = json.load(f)

    bounds = data["bounds"]
    outline = data["board_outline"]
    traces = data["traces"]
    vias_data = data["vias"]
    components = data["components"]
    print(f"  {len(traces)} traces, {len(vias_data)} vias, {len(components)} components")

    # Compute board center for origin centering (mils -> mm)
    # KiCad Y-axis points downward; we flip to Blender Y-up.
    board_cx = (bounds["x_min"] + bounds["x_max"]) / 2 * MIL_TO_MM
    board_cy = (bounds["y_min"] + bounds["y_max"]) / 2 * MIL_TO_MM
    print(f"  Board center: ({board_cx:.1f}, {board_cy:.1f}) mm -- will offset to origin")

    def brd_to_blender(x_mil, y_mil):
        """Convert BRD mils to Blender mm with Y-flip and centering."""
        return (x_mil * MIL_TO_MM - board_cx,
                -(y_mil * MIL_TO_MM - board_cy))  # negate Y

    # Build scene with separate named meshes
    scene = trimesh.Scene()

    # a) Board slab from bounds (proper solid box)
    print("Building PCB slab...")
    bx0, by0 = brd_to_blender(bounds["x_min"], bounds["y_max"])  # TL -> -X,-Y
    bx1, by1 = brd_to_blender(bounds["x_max"], bounds["y_min"])  # BR -> +X,+Y
    board_mesh = trimesh.creation.box(
        extents=[bx1 - bx0, by1 - by0, PCB_THICKNESS],
        transform=trimesh.transformations.translation_matrix([
            (bx0 + bx1) / 2, (by0 + by1) / 2, -PCB_THICKNESS / 2
        ])
    )
    # Cut mounting holes
    for comp in components:
        if comp["footprint"] == "HOLE":
            hx, hy = brd_to_blender(comp["x"], comp["y"])
            hole = trimesh.creation.cylinder(radius=1.6, height=PCB_THICKNESS + 1,
                                              sections=16)
            hole.apply_translation([hx, hy, -PCB_THICKNESS / 2])
            board_mesh = board_mesh.difference(hole)
    apply_material(board_mesh, MAT_PCB)
    scene.add_geometry(board_mesh, node_name="PCB_Board")
    print(f"  Board: {len(board_mesh.faces)} triangles")

    # b) Traces (offset by board center)
    print("Building top traces (layer 15 = front copper)...")
    top_traces = build_traces(traces, layer_filter=15)
    if top_traces:
        top_traces.apply_translation([-board_cx, -board_cy, 0])
        top_traces.apply_transform(np.diag([1, -1, 1, 1]))  # flip Y
        apply_material(top_traces, MAT_TRACE)
        scene.add_geometry(top_traces, node_name="Traces_Top")
        print(f"  Top traces: {len(top_traces.faces)} triangles")

    print("Building bottom traces (layer 0 = back copper)...")
    bot_traces = build_traces(traces, layer_filter=0)
    if bot_traces:
        bot_traces.apply_translation([-board_cx, -board_cy, -PCB_THICKNESS - COPPER_THICKNESS])
        bot_traces.apply_transform(np.diag([1, -1, 1, 1]))  # flip Y
        apply_material(bot_traces, MAT_TRACE)
        scene.add_geometry(bot_traces, node_name="Traces_Bottom")
        print(f"  Bottom traces: {len(bot_traces.faces)} triangles")

    print("Building vias...")
    via_mesh = build_vias(vias_data)
    if via_mesh:
        via_mesh.apply_translation([-board_cx, -board_cy, 0])
        via_mesh.apply_transform(np.diag([1, -1, 1, 1]))  # flip Y
        apply_material(via_mesh, MAT_VIA)
        scene.add_geometry(via_mesh, node_name="Vias")
        print(f"  Vias: {len(via_mesh.faces)} triangles")

    # c) Components
    print("Placing components...")
    placed = 0
    skipped = 0
    for comp in components:
        ref = comp["ref"]
        fp = comp["footprint"]

        if ref in SKIP_REFS:
            skipped += 1
            continue

        step_file = REF_STEP.get(ref) or FOOTPRINT_STEP.get(fp)
        if step_file is None:
            skipped += 1
            continue

        raw_shape = load_step(step_file)
        if raw_shape is None:
            skipped += 1
            continue

        # Get pre-transform (per-ref overrides per-footprint)
        pre = REF_PRE_TRANSFORM.get(ref, STEP_PRE_TRANSFORM.get(fp, (0, 0, 0, 0)))
        pre_rx, pre_ry, pre_rz, z_offset = pre

        # Step 1: Apply pre-rotations to the raw shape around its own BB center,
        #         THEN compute the new BB center for pad alignment.
        step_cx, step_cy = get_bb_center_xy(raw_shape)
        has_pre_rot = any(abs(a) > 0.01 for a in (pre_rx, pre_ry, pre_rz))

        if has_pre_rot:
            # Center at origin, rotate, then get new BB center
            pre_trsf = gp_Trsf()
            pre_trsf.SetTranslation(gp_Vec(-step_cx, -step_cy, 0))
            for axis_dir, angle in [
                (gp_Dir(1, 0, 0), pre_rx),
                (gp_Dir(0, 1, 0), pre_ry),
                (gp_Dir(0, 0, 1), pre_rz),
            ]:
                if abs(angle) > 0.01:
                    rot = gp_Trsf()
                    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), axis_dir), math.radians(angle))
                    pre_trsf = rot.Multiplied(pre_trsf)
            pre_rotated = BRepBuilderAPI_Transform(raw_shape, pre_trsf, True).Shape()
            # New BB center after rotation
            step_cx, step_cy = get_bb_center_xy(pre_rotated)
            occ_shape = pre_rotated
        else:
            occ_shape = raw_shape

        # BRD position and orient (with per-ref overrides for BRD errors)
        comp_x_mil, comp_y_mil = comp["x"], comp["y"]
        # DIN connectors: BRD error -- manual correction from board inspection.
        if ref in ("J6", "J7"):
            comp_x_mil = comp["x"] - 7.0 / MIL_TO_MM    # -7mm in X
            comp_y_mil = comp["y"] - 1.0 / MIL_TO_MM     # -1mm in Y
        brd_x, brd_y = brd_to_blender(comp_x_mil, comp_y_mil)
        brd_orient = -(comp["orient"] / 10.0)

        # Pad centroid in local coords (mils -> mm, Y-flipped)
        pad_cx_mm, pad_cy_mm = pad_centroid_mm(comp["pads"])
        pad_cy_mm = -pad_cy_mm

        # Step 2: Align BB center to pad centroid, apply Z offset, BRD orient, board position
        trsf = gp_Trsf()
        trsf.SetTranslation(gp_Vec(pad_cx_mm - step_cx, pad_cy_mm - step_cy, z_offset))

        if abs(brd_orient) > 0.01:
            rot = gp_Trsf()
            rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                            math.radians(brd_orient))
            trsf = rot.Multiplied(trsf)

        # Final translation to board position
        final = gp_Trsf()
        final.SetTranslation(gp_Vec(brd_x, brd_y, 0))
        trsf = final.Multiplied(trsf)

        transformed = BRepBuilderAPI_Transform(occ_shape, trsf, True).Shape()

        # If socketed, place the socket first at board level, then elevate the IC
        socket_file = SOCKETED_REFS.get(ref)
        if socket_file:
            socket_shape = load_step(socket_file)
            if socket_shape:
                # Socket uses same footprint as IC -> same pre-transform & alignment
                sock_pre = STEP_PRE_TRANSFORM.get(fp, (0, 0, 0, 0))
                sock_rx, sock_ry, sock_rz, _ = sock_pre
                has_sock_rot = any(abs(a) > 0.01 for a in (sock_rx, sock_ry, sock_rz))

                if has_sock_rot:
                    sock_cx, sock_cy = get_bb_center_xy(socket_shape)
                    sock_trsf = gp_Trsf()
                    sock_trsf.SetTranslation(gp_Vec(-sock_cx, -sock_cy, 0))
                    for axis_dir, angle in [
                        (gp_Dir(1, 0, 0), sock_rx),
                        (gp_Dir(0, 1, 0), sock_ry),
                        (gp_Dir(0, 0, 1), sock_rz),
                    ]:
                        if abs(angle) > 0.01:
                            rot = gp_Trsf()
                            rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), axis_dir), math.radians(angle))
                            sock_trsf = rot.Multiplied(sock_trsf)
                    socket_shape = BRepBuilderAPI_Transform(socket_shape, sock_trsf, True).Shape()

                sock_cx2, sock_cy2 = get_bb_center_xy(socket_shape)
                sock_align = gp_Trsf()
                sock_align.SetTranslation(gp_Vec(pad_cx_mm - sock_cx2, pad_cy_mm - sock_cy2, 0))
                if abs(brd_orient) > 0.01:
                    rot = gp_Trsf()
                    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), math.radians(brd_orient))
                    sock_align = rot.Multiplied(sock_align)
                sock_final = gp_Trsf()
                sock_final.SetTranslation(gp_Vec(brd_x, brd_y, 0))
                sock_align = sock_final.Multiplied(sock_align)

                sock_transformed = BRepBuilderAPI_Transform(socket_shape, sock_align, True).Shape()
                sock_mesh = occ_shape_to_trimesh(sock_transformed, linear_deflection=0.2)
                if sock_mesh and len(sock_mesh.faces) > 0:
                    scene.add_geometry(sock_mesh, node_name=f"{ref}_Socket")

            # Empty socket: place socket only, skip the IC
            if ref in EMPTY_SOCKETS:
                placed += 1
                continue

            # Elevate the IC by socket height
            elevate = gp_Trsf()
            elevate.SetTranslation(gp_Vec(0, 0, SOCKET_HEIGHT))
            trsf = elevate.Multiplied(trsf)
            transformed = BRepBuilderAPI_Transform(occ_shape, trsf, True).Shape()

        mesh = occ_shape_to_trimesh(transformed, linear_deflection=0.2)
        if mesh and len(mesh.faces) > 0:
            node_name = f"{ref}_{comp['value']}"
            scene.add_geometry(mesh, node_name=node_name)
            placed += 1

    print(f"  Placed: {placed}, Skipped: {skipped}")

    # Export
    total_faces = sum(len(g.faces) for g in scene.geometry.values())
    print(f"  Total: {len(scene.geometry)} objects, {total_faces} triangles")

    print(f"Exporting {OUT_PATH}...")
    scene.export(OUT_PATH, file_type='glb')

    dt = time.time() - t0
    size_mb = os.path.getsize(OUT_PATH) / (1024 * 1024)
    print(f"Done in {dt:.1f}s. Output: {OUT_PATH} ({size_mb:.1f} MB)")


if __name__ == '__main__':
    main()
