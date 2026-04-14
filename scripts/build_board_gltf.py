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

from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_FACE
from OCP.BRep import BRep_Tool
from OCP.TopLoc import TopLoc_Location
from OCP.TopoDS import TopoDS
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire, BRepBuilderAPI_MakeFace
from OCP.BRepPrimAPI import BRepPrimAPI_MakePrism
from OCP.gp import gp_Pnt, gp_Vec
# XDE for STEP color extraction
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.XCAFApp import XCAFApp_Application
from OCP.XCAFDoc import XCAFDoc_DocumentTool, XCAFDoc_ColorSurf, XCAFDoc_ColorGen
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.Quantity import Quantity_Color
from OCP.TDF import TDF_LabelSequence

sys.stdout.reconfigure(encoding='utf-8')

JSON_PATH = "assets/board_traces.json"
CHIPS_DIR = "assets/3dchips"
OUT_PATH = "assets/board_5150.glb"

MIL_TO_MM = 0.0254
PCB_THICKNESS = 1.6
COPPER_THICKNESS = 0.035

# --- Patch trimesh to deduplicate shared textures in glTF export ---
import trimesh.exchange.gltf as _gltf
_orig_append_image = _gltf._append_image
_image_index_cache = {}

def _dedup_append_image(img, tree, buffer_items, extension_webp):
    key = id(img)
    if key in _image_index_cache:
        return _image_index_cache[key]
    idx = _orig_append_image(img, tree, buffer_items, extension_webp)
    _image_index_cache[key] = idx
    return idx

_gltf._append_image = _dedup_append_image

# --- PBR Textures ---
from PIL import Image as _PILImage

def _load_pbr(directory):
    """Load PBR textures from a directory. Returns (normal, roughness, ao) PIL images."""
    normal = ao = None
    mr = None  # metallicRoughness (glTF: G=roughness, B=metallic)
    try:
        normal = _PILImage.open(os.path.join(directory, "A23DTEX_Normal.jpg"))
    except (OSError, IOError):
        pass
    try:
        rough_img = _PILImage.open(os.path.join(directory, "A23DTEX_Roughness.jpg"))
        rough_arr = np.array(rough_img.convert('L'))
        # Check for separate metallic texture
        metal_arr = np.zeros_like(rough_arr)
        try:
            metal_img = _PILImage.open(os.path.join(directory, "A23DTEX_Metallic.jpg"))
            metal_arr = np.array(metal_img.convert('L'))
        except (OSError, IOError):
            pass
        # Pack into glTF metallicRoughness: G=roughness B=metallic
        mr_arr = np.zeros((*rough_arr.shape, 3), dtype=np.uint8)
        mr_arr[:, :, 1] = rough_arr
        mr_arr[:, :, 2] = metal_arr
        mr = _PILImage.fromarray(mr_arr)
    except (OSError, IOError):
        pass
    try:
        ao = _PILImage.open(os.path.join(directory, "A23DTEX_Ambient Occlusion.jpg"))
    except (OSError, IOError):
        pass
    albedo = None
    try:
        albedo = _PILImage.open(os.path.join(directory, "A23DTEX_Albedo.jpg"))
    except (OSError, IOError):
        pass
    return normal, mr, ao, albedo

_PBR_IC_NORMAL, _PBR_IC_MR, _PBR_IC_AO, _PBR_IC_ALBEDO = _load_pbr("assets/pbr/ic")
_PBR_PCB_NORMAL, _PBR_PCB_MR, _PBR_PCB_AO, _PBR_PCB_ALBEDO = _load_pbr("assets/pbr/pcb")
_PBR_PLASTIC_NORMAL, _PBR_PLASTIC_MR, _PBR_PLASTIC_AO, _PBR_PLASTIC_ALBEDO = _load_pbr("assets/pbr/plastic_comp")
_PBR_LEAD_NORMAL, _PBR_LEAD_MR, _PBR_LEAD_AO, _PBR_LEAD_ALBEDO = _load_pbr("assets/pbr/leads")

# --- Materials ---
MAT_PCB = PBRMaterial(
    name="PCB_Board",
    baseColorFactor=[0.0, 0.05, 0.016, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.6,
    normalTexture=_PBR_PCB_NORMAL,
    occlusionTexture=_PBR_PCB_AO,
    metallicRoughnessTexture=_PBR_PCB_MR,
)
MAT_TRACE = PBRMaterial(
    name="Trace",
    baseColorFactor=[0.063, 0.250, 0.125, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.4,
)
MAT_VIA = PBRMaterial(
    name="Via_Tin",
    baseColorFactor=[0.75, 0.75, 0.78, 1.0],
    metallicFactor=1.0,
    roughnessFactor=0.30,
)
MAT_IC_BODY = PBRMaterial(
    name="IC_Body",
    baseColorFactor=[0.0, 0.0, 0.0, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.35,
    normalTexture=_PBR_IC_NORMAL,
    occlusionTexture=_PBR_IC_AO,
    metallicRoughnessTexture=_PBR_IC_MR,
)
MAT_LEAD = PBRMaterial(
    name="Lead_Tin",
    baseColorTexture=_PBR_LEAD_ALBEDO,
    metallicFactor=1.0,
    roughnessFactor=0.20,
    normalTexture=_PBR_LEAD_NORMAL,
    metallicRoughnessTexture=_PBR_LEAD_MR,
)
MAT_DIN = PBRMaterial(
    name="DIN_Connector",
    baseColorFactor=[0.0, 0.0, 0.0, 1.0],
    metallicFactor=0.6,
    roughnessFactor=0.35,
    normalTexture=_PBR_PLASTIC_NORMAL,
    metallicRoughnessTexture=_PBR_PLASTIC_MR,
)
MAT_MOLEX_BODY = PBRMaterial(
    name="Molex_White",
    baseColorFactor=[0.92, 0.90, 0.85, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.5,
    normalTexture=_PBR_PLASTIC_NORMAL,
    metallicRoughnessTexture=_PBR_PLASTIC_MR,
)
MAT_DIP_SWITCH = PBRMaterial(
    name="DIP_Switch",
    baseColorFactor=[0.0, 0.45, 0.55, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.4,
    normalTexture=_PBR_PLASTIC_NORMAL,
    metallicRoughnessTexture=_PBR_PLASTIC_MR,
)
MAT_RELAY_BODY = PBRMaterial(
    name="Relay_Body",
    baseColorFactor=[0.85, 0.55, 0.05, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.4,
    normalTexture=_PBR_PLASTIC_NORMAL,
    metallicRoughnessTexture=_PBR_PLASTIC_MR,
)
MAT_RESISTOR_BODY = PBRMaterial(
    name="Resistor_Body",
    baseColorFactor=[0.0, 0.35, 0.35, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.6,
)
MAT_CAP_CERAMIC = PBRMaterial(
    name="Cap_Ceramic",
    baseColorFactor=[0.75, 0.45, 0.05, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.5,
)
MAT_FILM_CAP = PBRMaterial(
    name="Film_Cap",
    baseColorFactor=[0.7, 0.05, 0.02, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.5,
)
MAT_ISA_BODY = PBRMaterial(
    name="ISA_Body",
    baseColorFactor=[0.0, 0.0, 0.0, 1.0],
    metallicFactor=0.0,
    roughnessFactor=0.4,
    normalTexture=_PBR_PLASTIC_NORMAL,
    metallicRoughnessTexture=_PBR_PLASTIC_MR,
)
MAT_ISA_CONTACT = PBRMaterial(
    name="ISA_Contact",
    baseColorFactor=[0.83, 0.69, 0.22, 1.0],
    metallicFactor=1.0,
    roughnessFactor=0.25,
)

# Geometric face classifiers for STEP files without color data.
# Called with (xmin, ymin, zmin, xmax, ymax, zmax) -> synthetic rgb_key string.
def _isa_slot_classifier(xmin, ymin, zmin, xmax, ymax, zmax):
    dx, dy, dz = xmax - xmin, ymax - ymin, zmax - zmin
    cx = (xmin + xmax) / 2
    # End walls: span full Y width (dy > 3) -- always plastic body
    if dy > 3:
        return "isa_body"
    # Curved end pieces beyond pin range -- plastic body
    if abs(cx) > 38.5:
        return "isa_body"
    # Through-hole leads: small faces reaching below Z=-14.5 (pin shanks + bottoms)
    # Exclude wide body walls (dx > 5)
    if zmin < -14.5 and dx < 5:
        return "isa_lead"
    # Gold arch contacts: thin vertical features inside the slot
    if dx < 3 and dz > 2:
        return "isa_contact"
    return "isa_body"


FOOTPRINT_FACE_CLASSIFIER = {
    "62":                   _isa_slot_classifier,
    "62PinEdgeIOConnector": _isa_slot_classifier,
}

# Per-footprint material overrides: {rgb_key -> material}
# rgb_key=None means faces with no STEP color data
# String keys come from geometric classifiers above
FOOTPRINT_MAT_OVERRIDE = {
    "62":                   {"isa_body": MAT_ISA_BODY, "isa_contact": MAT_ISA_CONTACT, "isa_lead": MAT_LEAD},
    "62PinEdgeIOConnector": {"isa_body": MAT_ISA_BODY, "isa_contact": MAT_ISA_CONTACT, "isa_lead": MAT_LEAD},
    "5PINDIN":   {None: MAT_DIN},
    "5PINDIN2":  {None: MAT_DIN},
    "POWER_CON": {(1.0, 1.0, 1.0): MAT_MOLEX_BODY, (0.216, 0.216, 0.216): MAT_LEAD, None: MAT_LEAD},
    "G5V-2DPDT": {(0.019, 0.018, 0.018): MAT_RELAY_BODY},
    # Resistors: tan STEP body -> teal green
    "R5":  {(0.754, 0.464, 0.207): MAT_RESISTOR_BODY},
    # Disc caps: dark red STEP body -> orange ceramic
    "C1-1": {(0.619, 0.152, 0.019): MAT_CAP_CERAMIC},
    "CP8":  {"cap_body": MAT_FILM_CAP, "cap_lead": MAT_LEAD},
}
# Per-ref overrides (take priority over footprint)
REF_MAT_OVERRIDE = {
    "SW1": {None: MAT_DIP_SWITCH},
    "SW2": {None: MAT_DIP_SWITCH},
}


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
    "CP8":                  None,  # parametric axial cap, generated inline
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

# --- XDE STEP loading (extracts per-face colors) ---
_xde_cache = {}  # filename -> (shape, color_tool, doc)  -- doc kept alive!


def load_step_xde(filename):
    """Load STEP with XDE for color data. Returns (shape, color_tool). Cached."""
    if filename in _xde_cache:
        entry = _xde_cache[filename]
        return (entry[0], entry[1])
    path = os.path.join(CHIPS_DIR, filename)
    if not os.path.exists(path):
        print(f"  WARNING: missing {path}")
        _xde_cache[filename] = (None, None, None)
        return (None, None)

    app = XCAFApp_Application.GetApplication_s()
    doc = TDocStd_Document(TCollection_ExtendedString("MDTV-XCAF"))
    app.InitDocument(doc)

    reader = STEPCAFControl_Reader()
    reader.SetColorMode(True)
    reader.ReadFile(path)
    reader.Transfer(doc)

    shape_tool = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    color_tool = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())

    labels = TDF_LabelSequence()
    shape_tool.GetFreeShapes(labels)
    if labels.Length() == 0:
        _xde_cache[filename] = (None, None, None)
        return (None, None)

    shape = shape_tool.GetShape_s(labels.Value(1))
    _xde_cache[filename] = (shape, color_tool, doc)  # doc must stay alive
    return (shape, color_tool)


def tessellate_colored(shape, color_tool, linear_deflection=0.2, angular_deflection=0.5,
                       face_classifier=None):
    """Tessellate shape, grouping faces by STEP color.

    Strategy: tessellate into ONE mesh first so fix_normals() sees the full
    watertight topology, then split by color.  Splitting by color first
    creates non-watertight sub-meshes where fix_normals() picks the wrong
    winding direction.

    face_classifier: optional callable(xmin,ymin,zmin,xmax,ymax,zmax) -> key
        Used when a face has no STEP color to classify by geometry.

    Returns list of (trimesh, key) tuples, one per color group.
    """
    BRepMesh_IncrementalMesh(shape, linear_deflection, False, angular_deflection, True)

    all_verts = []
    all_faces = []
    face_colors = []  # one rgb_key per triangle
    offset = 0

    explorer = TopExp_Explorer(shape, TopAbs_FACE)
    while explorer.More():
        face = TopoDS.Face_s(explorer.Current())

        # Query face color from XDE (faces not in label tree raise)
        rgb_key = None
        if color_tool is not None:
            try:
                c = Quantity_Color()
                if (color_tool.GetColor(face, XCAFDoc_ColorSurf, c) or
                        color_tool.GetColor(face, XCAFDoc_ColorGen, c)):
                    rgb_key = (round(c.Red(), 3), round(c.Green(), 3), round(c.Blue(), 3))
            except Exception:
                pass

        # Geometric fallback for colorless faces
        if rgb_key is None and face_classifier is not None:
            from OCP.Bnd import Bnd_Box
            from OCP.BRepBndLib import BRepBndLib
            box = Bnd_Box()
            BRepBndLib.Add_s(face, box)
            rgb_key = face_classifier(*box.Get())

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
                face_colors.append(rgb_key)

            offset += n_nodes

        explorer.Next()

    if not all_verts:
        return []

    # Fix normals on the combined mesh (watertight = correct winding)
    combined = trimesh.Trimesh(
        vertices=np.array(all_verts), faces=np.array(all_faces))
    combined.fix_normals()

    # Split fixed faces by color group
    color_set = set(face_colors)
    result = []
    for rgb_key in color_set:
        mask = [fc == rgb_key for fc in face_colors]
        sub = combined.submesh([mask], append=True)
        if sub and len(sub.faces) > 0:
            result.append((sub, rgb_key))

    return result


_tess_cache = {}  # filename -> [(trimesh, rgb_key), ...]


def get_colored_meshes(filename, linear_deflection=0.2, face_classifier=None):
    """Get tessellated color-grouped meshes for a STEP file. Cached."""
    cache_key = (filename, face_classifier is not None)
    if cache_key in _tess_cache:
        return _tess_cache[cache_key]

    shape, color_tool = load_step_xde(filename)
    if shape is None:
        _tess_cache[cache_key] = None
        return None

    meshes = tessellate_colored(shape, color_tool, linear_deflection,
                                face_classifier=face_classifier)
    if not meshes:
        _tess_cache[cache_key] = None
        return None

    _tess_cache[cache_key] = meshes
    return meshes


def meshes_bb_center_xy(meshes):
    """Get XY bounding box center of a list of (trimesh, _) tuples."""
    all_v = np.vstack([m.vertices for m, _ in meshes])
    return ((all_v[:, 0].min() + all_v[:, 0].max()) / 2,
            (all_v[:, 1].min() + all_v[:, 1].max()) / 2)


def _translation(x, y, z):
    m = np.eye(4)
    m[0, 3] = x
    m[1, 3] = y
    m[2, 3] = z
    return m


def _rotation(axis, deg):
    """Rotation matrix. axis: 0=X, 1=Y, 2=Z."""
    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    m = np.eye(4)
    if axis == 0:
        m[1, 1] = c; m[1, 2] = -s; m[2, 1] = s; m[2, 2] = c
    elif axis == 1:
        m[0, 0] = c; m[0, 2] = s; m[2, 0] = -s; m[2, 2] = c
    else:
        m[0, 0] = c; m[0, 1] = -s; m[1, 0] = s; m[1, 1] = c
    return m


def _apply_pre_rotation(meshes, pre_rx, pre_ry, pre_rz):
    """Compute pre-rotation matrix and new BB center. Returns (matrix, new_cx, new_cy)."""
    cx, cy = meshes_bb_center_xy(meshes)
    has_rot = any(abs(a) > 0.01 for a in (pre_rx, pre_ry, pre_rz))
    if not has_rot:
        return np.eye(4), cx, cy

    mat = _translation(-cx, -cy, 0)
    for axis, angle in [(0, pre_rx), (1, pre_ry), (2, pre_rz)]:
        if abs(angle) > 0.01:
            mat = _rotation(axis, angle) @ mat

    # Compute new BB center after rotation
    all_v = np.vstack([m.vertices for m, _ in meshes])
    v4 = np.column_stack([all_v, np.ones(len(all_v))])
    v_rot = (mat @ v4.T).T
    new_cx = (v_rot[:, 0].min() + v_rot[:, 0].max()) / 2
    new_cy = (v_rot[:, 1].min() + v_rot[:, 1].max()) / 2
    return mat, new_cx, new_cy


_mat_cache = {}


def rgb_to_material(rgb):
    """Map STEP face color to a PBR material."""
    if rgb is None:
        return None
    if rgb in _mat_cache:
        return _mat_cache[rgb]

    r, g, b = rgb
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    mx = max(r, g, b)
    mn = min(r, g, b)
    sat = (mx - mn) / mx if mx > 0.01 else 0.0

    if sat < 0.15 and lum > 0.35:
        mat = MAT_LEAD
    elif lum < 0.15:
        mat = MAT_IC_BODY
    else:
        mat = PBRMaterial(
            name=f"Color_{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}",
            baseColorFactor=[r, g, b, 1.0],
            metallicFactor=0.0,
            roughnessFactor=0.5,
        )

    _mat_cache[rgb] = mat
    return mat


def place_colored_meshes(scene, meshes, transform, node_name,
                         mat_override=None, label_img=None, band_img=None):
    """Clone cached meshes, apply transform and material, add to scene.

    If label_img is provided, the IC body mesh (MAT_IC_BODY) gets UV-mapped
    with the label texture instead of a solid color.
    If band_img is provided, the resistor body gets UV-mapped with band texture.
    """
    # Resistor body STEP color
    RESISTOR_BODY_RGB = (0.754, 0.464, 0.207)

    for i, (mesh, rgb) in enumerate(meshes):
        m = mesh.copy()
        m.apply_transform(transform)
        # Check override first, then default mapping
        material = None
        if mat_override and rgb in mat_override:
            material = mat_override[rgb]
        else:
            material = rgb_to_material(rgb)

        # Apply band texture to resistor body
        if band_img is not None and rgb == RESISTOR_BODY_RGB:
            verts = m.vertices
            x_min, y_min, z_min = verts.min(axis=0)
            x_max, y_max, z_max = verts.max(axis=0)
            dx = x_max - x_min
            dy = y_max - y_min
            # UV: longest axis = U (band direction), other = V
            if dx > dy:
                u = (verts[:, 0] - x_min) / max(dx, 0.01)
                v = (verts[:, 1] - y_min) / max(dy, 0.01)
            else:
                u = (verts[:, 1] - y_min) / max(dy, 0.01)
                v = (verts[:, 0] - x_min) / max(dx, 0.01)
            uv = np.column_stack([u, v]).astype(np.float32)
            band_mat = PBRMaterial(
                name=f"Bands_{node_name}",
                baseColorTexture=band_img,
                metallicFactor=0.0,
                roughnessFactor=0.6,
            )
            m.visual = trimesh.visual.TextureVisuals(uv=uv, material=band_mat)
            suffix = f"_{i}" if len(meshes) > 1 else ""
            scene.add_geometry(m, node_name=f"{node_name}{suffix}")
            continue

        # Apply label texture to IC body mesh
        is_body = (material is MAT_IC_BODY) if material else False
        if is_body and label_img is not None:
            verts = m.vertices
            normals = m.face_normals
            # UV map: project XY onto [0,1] based on mesh bounding box
            x_min, y_min, z_min = verts.min(axis=0)
            x_max, y_max, z_max = verts.max(axis=0)
            dx = x_max - x_min
            dy = y_max - y_min
            if dx > 0.01 and dy > 0.01:
                u = (verts[:, 0] - x_min) / dx
                v = (verts[:, 1] - y_min) / dy
                uv = np.column_stack([u, v]).astype(np.float32)
                label_mat = PBRMaterial(
                    name=f"Label_{node_name}",
                    baseColorTexture=label_img,
                    metallicFactor=0.0,
                    roughnessFactor=0.35,
                    normalTexture=_PBR_IC_NORMAL,
                    occlusionTexture=_PBR_IC_AO,
                    metallicRoughnessTexture=_PBR_IC_MR,
                )
                m.visual = trimesh.visual.TextureVisuals(uv=uv, material=label_mat)
            elif material:
                apply_material(m, material)
        elif material:
            apply_material(m, material)

        suffix = f"_{i}" if len(meshes) > 1 else ""
        scene.add_geometry(m, node_name=f"{node_name}{suffix}")


_axial_cap_cache = {}

def _bent_lead(x_sign, body_half, pad_half, body_rad, lead_rad, sections=24):
    """Build one bent lead: horizontal from body, 90-degree bend, vertical into board."""
    horiz_len = pad_half - body_half - body_rad * 0.3  # horizontal portion
    vert_len = body_rad + 2.0  # down through board

    # Horizontal segment along X
    horiz = trimesh.creation.cylinder(radius=lead_rad, height=horiz_len, sections=sections)
    horiz.apply_transform(trimesh.transformations.rotation_matrix(math.pi/2, [0, 1, 0]))
    horiz.apply_translation([x_sign * (body_half + horiz_len/2), 0, 0])

    # Bend: quarter-torus approximated by short angled segments
    bend_r = body_rad * 0.3  # bend radius
    bend_segs = []
    n_bend = 8
    bx = x_sign * (body_half + horiz_len)
    for j in range(n_bend):
        a0 = (math.pi / 2) * j / n_bend
        a1 = (math.pi / 2) * (j + 1) / n_bend
        mx = bx + x_sign * bend_r * math.sin((a0 + a1) / 2)
        mz = -bend_r * (1 - math.cos((a0 + a1) / 2))
        seg_len = bend_r * (math.pi / 2) / n_bend
        seg = trimesh.creation.cylinder(radius=lead_rad, height=seg_len, sections=sections)
        angle = (a0 + a1) / 2
        seg.apply_transform(trimesh.transformations.rotation_matrix(angle * x_sign, [0, 1, 0]))
        seg.apply_translation([mx, 0, mz])
        bend_segs.append(seg)

    # Vertical segment going down
    vert = trimesh.creation.cylinder(radius=lead_rad, height=vert_len, sections=sections)
    vert.apply_translation([bx + x_sign * bend_r, 0, -bend_r - vert_len/2])

    parts = [horiz] + bend_segs + [vert]
    return trimesh.util.concatenate(parts)


# Film cap dimensions derived from BRD silkscreen outlines:
# Outline: 800 x 200 mils = 20.3 x 5.1 mm
# Short dimension (200 mils) = body diameter = 5.1mm
# Long dimension (800 mils) = full extent. Body ~65% of that.
FILM_CAP_DIMS = {
    ".01u":   (5.1, 13.0, 0.5),   # body_dia, body_len, lead_dia (mm)
    ".01uF":  (5.1, 13.0, 0.5),
    ".047uF": (5.1, 14.0, 0.5),   # slightly longer body for larger value
    ".047":   (5.1, 14.0, 0.5),
}


def build_axial_cap(pad_span_mm, body_len=16.5, body_rad=3.0, lead_rad=0.3):
    """Build a parametric axial film cap as color-grouped meshes.
    Body centered at origin sitting on Z=0 (board surface), leads bend down.
    Returns list of (trimesh, rgb_key)."""
    key = (pad_span_mm, body_len, body_rad, lead_rad)
    if key in _axial_cap_cache:
        return _axial_cap_cache[key]

    pad_half = pad_span_mm / 2
    body_half = body_len / 2

    # Body cylinder along X, raised so bottom sits on board surface
    body = trimesh.creation.cylinder(radius=body_rad, height=body_len, sections=32)
    body.apply_transform(trimesh.transformations.rotation_matrix(math.pi/2, [0, 1, 0]))
    body.apply_translation([0, 0, body_rad])

    # Bent leads
    left = _bent_lead(-1, body_half, pad_half, body_rad, lead_rad)
    left.apply_translation([0, 0, body_rad])
    right = _bent_lead(+1, body_half, pad_half, body_rad, lead_rad)
    right.apply_translation([0, 0, body_rad])

    leads = trimesh.util.concatenate([left, right])
    body.fix_normals()
    leads.fix_normals()

    result = [(body, "cap_body"), (leads, "cap_lead")]
    _axial_cap_cache[key] = result
    return result


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


def build_traces(traces, layer_filter=None, z_top=None, z_bot=None):
    """Build traces as a clean extruded 2D polygon (no self-intersections).

    Uses Shapely to union all trace ribbons in 2D, then triangulates
    and extrudes into a watertight solid.
    """
    from shapely.geometry import LineString
    from shapely.ops import unary_union

    if z_top is None:
        z_top = COPPER_THICKNESS
    if z_bot is None:
        z_bot = -0.3  # penetrate into board for boolean etch

    # Build 2D polygons from trace segments
    polys = []
    for tr in traces:
        if layer_filter is not None and tr.get("layer", 0) != layer_filter:
            continue
        x1, y1 = tr["x1"] * MIL_TO_MM, tr["y1"] * MIL_TO_MM
        x2, y2 = tr["x2"] * MIL_TO_MM, tr["y2"] * MIL_TO_MM
        w = tr["w"] * MIL_TO_MM / 2
        if abs(x2 - x1) < 0.001 and abs(y2 - y1) < 0.001:
            continue
        line = LineString([(x1, y1), (x2, y2)])
        polys.append(line.buffer(w, cap_style='flat'))

    if not polys:
        return None

    # Union into one clean MultiPolygon
    merged = unary_union(polys)

    # Triangulate and extrude
    return _extrude_polygon(merged, z_top, z_bot)


def _extrude_polygon(poly, z_top, z_bot):
    """Extrude a Shapely polygon/multipolygon into a trimesh solid."""
    from shapely.geometry import MultiPolygon, Polygon
    import shapely

    if poly.is_empty:
        return None

    if isinstance(poly, Polygon):
        polys = [poly]
    elif isinstance(poly, MultiPolygon):
        polys = list(poly.geoms)
    else:
        return None

    all_verts = []
    all_faces = []

    for pg in polys:
        # Get exterior + holes as coordinate arrays
        rings = [np.array(pg.exterior.coords[:-1])]  # drop closing duplicate
        for hole in pg.interiors:
            rings.append(np.array(hole.coords[:-1]))

        # Triangulate the polygon face
        from shapely import get_coordinates
        # Use trimesh's triangulate_polygon which handles holes
        try:
            face_verts, face_faces = trimesh.creation._polygon_to_vertices(pg)
        except Exception:
            # Fallback: simple ear-clip on exterior only
            coords = np.array(pg.exterior.coords[:-1])
            n = len(coords)
            if n < 3:
                continue
            face_verts = coords
            face_faces = [[0, i, i + 1] for i in range(1, n - 1)]
            face_faces = np.array(face_faces)

        n_verts = len(face_verts)
        offset = len(all_verts)

        # Top face vertices
        for v in face_verts:
            all_verts.append([v[0], v[1], z_top])
        # Bottom face vertices
        for v in face_verts:
            all_verts.append([v[0], v[1], z_bot])

        # Top faces
        for f in face_faces:
            all_faces.append([f[0] + offset, f[1] + offset, f[2] + offset])
        # Bottom faces (reversed winding)
        for f in face_faces:
            all_faces.append([f[0] + offset + n_verts, f[2] + offset + n_verts, f[1] + offset + n_verts])

        # Side walls from exterior ring
        ext_coords = np.array(pg.exterior.coords[:-1])
        n_ext = len(ext_coords)
        # Find indices in face_verts that match exterior coords
        # (face_verts may have been reordered by triangulation)
        # Simpler: just build side walls from the exterior ring directly
        side_offset = len(all_verts)
        for v in ext_coords:
            all_verts.append([v[0], v[1], z_top])
        for v in ext_coords:
            all_verts.append([v[0], v[1], z_bot])
        for i in range(n_ext):
            j = (i + 1) % n_ext
            t0 = side_offset + i
            t1 = side_offset + j
            b0 = side_offset + n_ext + i
            b1 = side_offset + n_ext + j
            all_faces.append([t0, t1, b1])
            all_faces.append([t0, b1, b0])

    if not all_verts:
        return None

    mesh = trimesh.Trimesh(vertices=np.array(all_verts, dtype=np.float64),
                           faces=np.array(all_faces, dtype=np.int64))
    return mesh


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

    # a) Board slab
    print("Building PCB slab...")
    bx0, by0 = brd_to_blender(bounds["x_min"], bounds["y_max"])
    bx1, by1 = brd_to_blender(bounds["x_max"], bounds["y_min"])
    board_mesh = trimesh.creation.box(
        extents=[bx1 - bx0, by1 - by0, PCB_THICKNESS],
        transform=trimesh.transformations.translation_matrix([
            (bx0 + bx1) / 2, (by0 + by1) / 2, -PCB_THICKNESS / 2
        ])
    )

    # b) Build traces and etch into board
    print("Building top traces (layer 15)...")
    top_traces = build_traces(traces, layer_filter=15)
    if top_traces:
        top_traces.apply_translation([-board_cx, -board_cy, 0])
        top_traces.apply_transform(np.diag([1, -1, 1, 1]))
        print(f"  Top traces: {len(top_traces.faces)} tris")

    print("Building bottom traces (layer 0)...")
    bot_traces = build_traces(traces, layer_filter=0)
    if bot_traces:
        bot_traces.apply_translation([-board_cx, -board_cy, -PCB_THICKNESS - COPPER_THICKNESS])
        bot_traces.apply_transform(np.diag([1, -1, 1, 1]))
        print(f"  Bottom traces: {len(bot_traces.faces)} tris")

    from shapely.geometry import LineString, Point, box as shapely_box
    from shapely.ops import unary_union

    board_2d = shapely_box(bx0, by0, bx1, by1)

    def build_trace_polys_2d(traces_data, layer):
        polys = []
        for tr in traces_data:
            if tr.get("layer", 0) != layer:
                continue
            x1 = tr["x1"] * MIL_TO_MM - board_cx
            y1 = -(tr["y1"] * MIL_TO_MM - board_cy)
            x2 = tr["x2"] * MIL_TO_MM - board_cx
            y2 = -(tr["y2"] * MIL_TO_MM - board_cy)
            w = tr["w"] * MIL_TO_MM / 2
            if abs(x2 - x1) < 0.001 and abs(y2 - y1) < 0.001:
                continue
            polys.append(LineString([(x1, y1), (x2, y2)]).buffer(w, cap_style='flat'))
        return unary_union(polys) if polys else None

    def extrude_multi(polygon, height, z_offset=0):
        from shapely.geometry import MultiPolygon, Polygon, GeometryCollection
        if polygon is None or polygon.is_empty:
            return None
        if isinstance(polygon, Polygon):
            geoms = [polygon]
        elif isinstance(polygon, (MultiPolygon, GeometryCollection)):
            geoms = [g for g in polygon.geoms if isinstance(g, Polygon) and not g.is_empty]
        else:
            return None
        if not geoms:
            return None
        meshes = [trimesh.creation.extrude_polygon(pg, height=height) for pg in geoms]
        result = trimesh.util.concatenate(meshes)
        if z_offset != 0:
            result.apply_translation([0, 0, z_offset])
        return result

    # 2D trace unions
    print("Building top traces (2D union)...")
    t0 = time.time()
    top_traces_2d = build_trace_polys_2d(traces, layer=15)
    print(f"  Done: {time.time()-t0:.1f}s")

    print("Building bottom traces (2D union)...")
    t0 = time.time()
    bot_traces_2d = build_trace_polys_2d(traces, layer=0)
    print(f"  Done: {time.time()-t0:.1f}s")

    # Mounting holes as 2D circles
    holes_2d = unary_union([Point(*brd_to_blender(c["x"], c["y"])).buffer(1.6)
                            for c in components if c["footprint"] == "HOLE"])

    THIRD = PCB_THICKNESS / 3.0

    board_h = board_2d
    if holes_2d and not holes_2d.is_empty:
        board_h = board_2d.difference(holes_2d)

    # Top layer: board minus top traces + top traces. Z = -THIRD to 0
    print("Top layer...")
    t0 = time.time()
    top_board_2d = board_h
    if top_traces_2d and not top_traces_2d.is_empty:
        top_board_2d = board_h.difference(top_traces_2d)
    m = extrude_multi(top_board_2d, THIRD, z_offset=-THIRD)
    if m:
        apply_material(m, MAT_PCB)
        scene.add_geometry(m, node_name="PCB_Top")
        print(f"  PCB top: {len(m.faces)} tris ({time.time()-t0:.1f}s)")

    if top_traces_2d and not top_traces_2d.is_empty:
        tc = top_traces_2d.intersection(board_h)
        m = extrude_multi(tc, THIRD, z_offset=-THIRD)
        if m:
            apply_material(m, MAT_TRACE)
            scene.add_geometry(m, node_name="Traces_Top")
            print(f"  Top traces: {len(m.faces)} tris")

    # Core: full board, no cutouts. Z = -2*THIRD to -THIRD
    print("Core...")
    t0 = time.time()
    m = extrude_multi(board_h, THIRD, z_offset=-2*THIRD)
    if m:
        apply_material(m, PBRMaterial(name="PCB_Core", **{
            "baseColorFactor": MAT_PCB.baseColorFactor,
            "metallicFactor": MAT_PCB.metallicFactor,
            "roughnessFactor": MAT_PCB.roughnessFactor}))
        scene.add_geometry(m, node_name="PCB_Core")
        print(f"  Core: {len(m.faces)} tris ({time.time()-t0:.1f}s)")

    # Bottom layer: board minus bot traces + bot traces. Z = -PCB to -2*THIRD
    print("Bottom layer...")
    t0 = time.time()
    bot_board_2d = board_h
    if bot_traces_2d and not bot_traces_2d.is_empty:
        bot_board_2d = board_h.difference(bot_traces_2d)
    m = extrude_multi(bot_board_2d, THIRD, z_offset=-PCB_THICKNESS)
    if m:
        m.invert()
        apply_material(m, PBRMaterial(name="PCB_Bot", **{
            "baseColorFactor": MAT_PCB.baseColorFactor,
            "metallicFactor": MAT_PCB.metallicFactor,
            "roughnessFactor": MAT_PCB.roughnessFactor}))
        scene.add_geometry(m, node_name="PCB_Bot")
        print(f"  PCB bot: {len(m.faces)} tris ({time.time()-t0:.1f}s)")

    if bot_traces_2d and not bot_traces_2d.is_empty:
        bc = bot_traces_2d.intersection(board_h)
        m = extrude_multi(bc, THIRD, z_offset=-PCB_THICKNESS)
        if m:
            apply_material(m, MAT_TRACE)
            scene.add_geometry(m, node_name="Traces_Bottom")
            print(f"  Bot traces: {len(m.faces)} tris")

    print("Building vias...")
    via_mesh = build_vias(vias_data)
    if via_mesh:
        via_mesh.apply_translation([-board_cx, -board_cy, 0])
        via_mesh.apply_transform(np.diag([1, -1, 1, 1]))
        apply_material(via_mesh, MAT_VIA)
        scene.add_geometry(via_mesh, node_name="Vias")
        print(f"  Vias: {len(via_mesh.faces)} tris")

    # d) UV-map top layer meshes to board bounds (for silkscreen texture in Blender)
    def apply_board_uv(mesh):
        """Add UVs mapping vertex XY to board-normalized [0,1] coordinates."""
        verts = mesh.vertices
        u = (verts[:, 0] - bx0) / (bx1 - bx0)
        v = (verts[:, 1] - by0) / (by1 - by0)
        uv = np.column_stack([u, v]).astype(np.float32)
        mesh.visual = trimesh.visual.TextureVisuals(uv=uv, material=mesh.visual.material)

    for node_name in ("PCB_Top", "Traces_Top"):
        try:
            _, geom_key = scene.graph[node_name]
            apply_board_uv(scene.geometry[geom_key])
            print(f"  UV mapped {node_name}")
        except (KeyError, ValueError):
            pass

    # e) Components (tessellate once per STEP, clone + matrix-transform per placement)
    # Load IC label textures
    from PIL import Image as PILImage
    LABEL_DIR = "assets/ic_labels_rendered"
    BAND_DIR = "assets/resistor_bands"
    _label_cache = {}
    _band_cache = {}
    def get_label_img(ref):
        if ref not in _label_cache:
            path = os.path.join(LABEL_DIR, f"{ref}.png")
            if os.path.exists(path):
                _label_cache[ref] = PILImage.open(path).convert('RGB')
            else:
                _label_cache[ref] = None
        return _label_cache[ref]
    def get_band_img(ref):
        if ref not in _band_cache:
            path = os.path.join(BAND_DIR, f"{ref}.png")
            if os.path.exists(path):
                _band_cache[ref] = PILImage.open(path).convert('RGB')
            else:
                _band_cache[ref] = None
        return _band_cache[ref]

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

        # Parametric axial cap (CP8) with real Mallory 150M dimensions
        if fp == "CP8":
            pads = comp["pads"]
            if len(pads) >= 2:
                dx = (pads[0]["x"] - pads[1]["x"]) * MIL_TO_MM
                dy = (pads[0]["y"] - pads[1]["y"]) * MIL_TO_MM
                span = math.sqrt(dx*dx + dy*dy)
            else:
                span = 20.3
            dims = FILM_CAP_DIMS.get(comp["value"], (5.1, 13.0, 0.5))
            body_dia, body_len, lead_dia = dims
            meshes = build_axial_cap(span, body_len=body_len,
                                     body_rad=body_dia / 2, lead_rad=lead_dia / 2)
            override = FOOTPRINT_MAT_OVERRIDE.get(fp)
            brd_x, brd_y = brd_to_blender(comp["x"], comp["y"])
            brd_orient = -(comp["orient"] / 10.0)
            mat = np.eye(4)
            if abs(brd_orient) > 0.01:
                mat = _rotation(2, brd_orient) @ mat
            mat = _translation(brd_x, brd_y, 0) @ mat
            place_colored_meshes(scene, meshes, mat, f"{ref}_{comp['value']}",
                                 override)
            placed += 1
            continue

        if step_file is None:
            skipped += 1
            continue

        classifier = FOOTPRINT_FACE_CLASSIFIER.get(fp)
        meshes = get_colored_meshes(step_file, face_classifier=classifier)
        if not meshes:
            skipped += 1
            continue

        # Pre-transform (per-ref overrides per-footprint)
        pre = REF_PRE_TRANSFORM.get(ref, STEP_PRE_TRANSFORM.get(fp, (0, 0, 0, 0)))
        pre_rx, pre_ry, pre_rz, z_offset = pre
        pre_mat, step_cx, step_cy = _apply_pre_rotation(meshes, pre_rx, pre_ry, pre_rz)

        # BRD position and orient
        comp_x_mil, comp_y_mil = comp["x"], comp["y"]
        if ref in ("J6", "J7"):
            comp_x_mil = comp["x"] - 7.0 / MIL_TO_MM
            comp_y_mil = comp["y"] - 1.0 / MIL_TO_MM
        brd_x, brd_y = brd_to_blender(comp_x_mil, comp_y_mil)
        brd_orient = -(comp["orient"] / 10.0)

        pad_cx_mm, pad_cy_mm = pad_centroid_mm(comp["pads"])
        pad_cy_mm = -pad_cy_mm

        # Build transform: align to pad centroid, orient, position
        mat = _translation(pad_cx_mm - step_cx, pad_cy_mm - step_cy, z_offset) @ pre_mat
        if abs(brd_orient) > 0.01:
            mat = _rotation(2, brd_orient) @ mat
        mat = _translation(brd_x, brd_y, 0) @ mat

        # Socket handling
        socket_file = SOCKETED_REFS.get(ref)
        if socket_file:
            sock_meshes = get_colored_meshes(socket_file)
            if sock_meshes:
                sock_pre = STEP_PRE_TRANSFORM.get(fp, (0, 0, 0, 0))
                sock_mat, sock_cx, sock_cy = _apply_pre_rotation(
                    sock_meshes, sock_pre[0], sock_pre[1], sock_pre[2])

                sock_final = _translation(
                    pad_cx_mm - sock_cx, pad_cy_mm - sock_cy, 0) @ sock_mat
                if abs(brd_orient) > 0.01:
                    sock_final = _rotation(2, brd_orient) @ sock_final
                sock_final = _translation(brd_x, brd_y, 0) @ sock_final

                place_colored_meshes(scene, sock_meshes, sock_final, f"{ref}_Socket",
                                    REF_MAT_OVERRIDE.get(ref) or FOOTPRINT_MAT_OVERRIDE.get(fp))

            if ref in EMPTY_SOCKETS:
                placed += 1
                continue

            # Elevate IC above socket
            mat = _translation(0, 0, SOCKET_HEIGHT) @ mat

        override = REF_MAT_OVERRIDE.get(ref) or FOOTPRINT_MAT_OVERRIDE.get(fp)
        label = get_label_img(ref)
        band = get_band_img(ref)
        place_colored_meshes(scene, meshes, mat, f"{ref}_{comp['value']}",
                             override, label_img=label, band_img=band)
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
