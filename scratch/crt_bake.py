# CRT composite bake pipeline -- canonical, reproducible.
# Runs inside Blender 5.1 with assets/ibm5150_monitor_studio_v10_baked.blend
# (or any later version of the staged scene). Produces assets/crt_composite/:
#
#   plate.png / plate_linear.exr  beauty plate, screen unpowered (3200x2400)
#   warp.exr                      tube-face UV map: R=u G=v B=coverage (3200x2400 f32)
#   gather.exr                    per-pixel mirror-traced screen UV for the case
#   resp_white.exr                linear response to full-white screen (1600x1200)
#   gb_rough/gb_normal/gb_position.exr  PBR G-buffer via render passes + Rough AOV
#
# Usage (Blender MCP or CLI):
#   blender -b assets\ibm5150_monitor_studio_v10_baked.blend -P scratch\crt_bake.py
#   or exec sections via the MCP bridge; each stage restores all scene state.
#
# Conventions: screen UV v=0 is BOTTOM (Blender). Visible window measured
# u 0.008-0.970, v 0.079-0.956. Multilayer EXRs from File Output nodes are
# read back with Blender's bundled OpenImageIO, not bpy.data.images.

import bpy
import os
import json

D = bpy.data
S = bpy.context.scene
VL = bpy.context.view_layer
OUT = bpy.path.abspath("//crt_composite") if bpy.data.filepath else r"c:\dev\bench\assets\crt_composite"

SCREEN = "Plane.021"     # phosphor face (emission)
GLASS = "Plane.011"      # CRT glass overlay
LED = "Sphere.002"
PANELS = ("Panel_Key", "Panel_Fill", "Panel_Rim", "Panel_Top")
MONITOR_COLLECTION = "Monitor"


class Stage:
    """Setup/teardown guard: hides objects, kills lights, swaps materials."""

    def __init__(self, dark=True, glass=True, led=False):
        self.dark, self.glass, self.led = dark, glass, led
        self.slots = {}

    def __enter__(self):
        S_ = bpy.context.scene
        bg = S_.world.node_tree.nodes.get("Background")
        self.bg = bg.inputs[1].default_value
        if self.dark:
            bg.inputs[1].default_value = 0.0
            for nm in PANELS:
                D.objects[nm].hide_render = True
        D.objects[GLASS].hide_render = not self.glass
        D.objects[LED].hide_render = not self.led
        self.res_pct = S_.render.resolution_percentage
        return self

    def swap(self, obname, mat):
        ob = D.objects[obname]
        self.slots.setdefault(obname, [
            sl.material.name if sl.material else None for sl in ob.material_slots])
        for sl in ob.material_slots:
            sl.material = mat

    def __exit__(self, *exc):
        S_ = bpy.context.scene
        for obname, mats in self.slots.items():
            ob = D.objects[obname]
            for sl, mname in zip(ob.material_slots, mats):
                sl.material = D.materials[mname] if mname else None
        S_.world.node_tree.nodes.get("Background").inputs[1].default_value = self.bg
        for nm in PANELS:
            D.objects[nm].hide_render = False
        D.objects[GLASS].hide_render = False
        D.objects[LED].hide_render = False
        S_.render.resolution_percentage = self.res_pct
        return False


def render_to(path, pct=200, samples=128, denoise=True, exr=False, depth='16'):
    S.render.resolution_percentage = pct
    S.cycles.samples = samples
    S.cycles.use_denoising = denoise
    fmt = S.render.image_settings
    fmt.file_format = 'OPEN_EXR' if exr else 'PNG'
    fmt.color_depth = depth if exr else '16'
    fmt.color_mode = 'RGBA'
    S.render.filepath = path
    bpy.ops.render.render(write_still=True)


def mat_named(name, builder):
    m = D.materials.get(name)
    if m is None:
        m = D.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        builder(nt)
    return m


def _dark(nt):
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    b.inputs["Base Color"].default_value = (0.03, 0.033, 0.031, 1)
    b.inputs["Roughness"].default_value = 0.45
    nt.links.new(b.outputs["BSDF"], out.inputs["Surface"])


def _warp(nt):
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    uv = nt.nodes.new("ShaderNodeUVMap")
    uv.uv_map = "UVMap"
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    cmb = nt.nodes.new("ShaderNodeCombineXYZ")
    cmb.inputs["Z"].default_value = 1.0
    nt.links.new(uv.outputs["UV"], sep.inputs["Vector"])
    nt.links.new(sep.outputs["X"], cmb.inputs["X"])
    nt.links.new(sep.outputs["Y"], cmb.inputs["Y"])
    nt.links.new(cmb.outputs["Vector"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])


def _white(nt):
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (1, 1, 1, 1)
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])


def _mirror(nt):
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    g = nt.nodes.new("ShaderNodeBsdfGlossy")
    g.inputs["Roughness"].default_value = 0.0
    nt.links.new(g.outputs["BSDF"], out.inputs["Surface"])


def bake_plate():
    """Additive light decomposition: plate (LED geometry present, unlit)
    + LED-only emission layer. Runtime: plate*bench_light + led*power."""
    led_mat = D.materials["Monitor LED"]
    def set_led(s):
        for n in led_mat.node_tree.nodes:
            if n.type == 'EMISSION':
                n.inputs["Strength"].default_value = s
    with Stage(dark=False, glass=True, led=True) as st:
        st.swap(SCREEN, mat_named("CRT_Off", _dark))
        S.view_settings.view_transform = 'Standard'
        set_led(0.0)
        render_to(os.path.join(OUT, "plate_leddark.png"), samples=192)
    with Stage(dark=True, glass=True, led=True) as st:
        st.swap(SCREEN, mat_named("CRT_Off", _dark))
        set_led(40.0)
        render_to(os.path.join(OUT, "led_layer.png"), samples=192)
    set_led(8.0)   # artist's original strength


def bake_warp():
    with Stage(dark=True, glass=False) as st:
        st.swap(SCREEN, mat_named("WarpEmit", _warp))
        render_to(os.path.join(OUT, "warp.exr"), samples=48, denoise=False,
                  exr=True, depth='32')


def bake_white():
    with Stage(dark=True, glass=True) as st:
        st.swap(SCREEN, mat_named("WhiteEmit", _white))
        render_to(os.path.join(OUT, "resp_white.exr"), pct=100, samples=256, exr=True)


def bake_gather():
    with Stage(dark=True, glass=False) as st:
        st.swap(SCREEN, mat_named("WarpEmit", _warp))
        mir = mat_named("BakeMirror", _mirror)
        for ob in D.collections[MONITOR_COLLECTION].objects:
            if ob.type == 'MESH' and ob.name not in (SCREEN, GLASS):
                if not ob.material_slots:
                    ob.data.materials.append(mir)   # removed by hand if ever needed
                else:
                    st.swap(ob.name, mir)
        S.cycles.max_bounces = 6
        S.cycles.glossy_bounces = 4
        render_to(os.path.join(OUT, "gather.exr"), samples=32, denoise=False,
                  exr=True, depth='32')


def bake_gbuffer():
    """Render passes + Rough AOV -> gb_rough / gb_normal / gb_position EXRs."""
    if "Rough" not in [a.name for a in VL.aovs]:
        a = VL.aovs.add()
        a.name = "Rough"
        a.type = 'VALUE'
    VL.use_pass_normal = True
    VL.use_pass_position = True

    taps = []
    for ob in D.collections[MONITOR_COLLECTION].objects:
        if ob.type != 'MESH':
            continue
        for sl in ob.material_slots:
            m = sl.material
            if not (m and m.use_nodes):
                continue
            nt = m.node_tree
            if any(n.type == 'OUTPUT_AOV' for n in nt.nodes):
                continue
            aov = nt.nodes.new('ShaderNodeOutputAOV')
            aov.aov_name = "Rough"
            b = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if b is not None and b.inputs["Roughness"].is_linked:
                nt.links.new(b.inputs["Roughness"].links[0].from_socket,
                             aov.inputs["Value"])
            elif b is not None:
                aov.inputs["Value"].default_value = float(b.inputs["Roughness"].default_value)
            else:
                aov.inputs["Value"].default_value = 0.5
            taps.append((m.name, aov.name))

    grp = D.node_groups.get("CRT_GBuffer")
    if grp is None:
        grp = D.node_groups.new("CRT_GBuffer", 'CompositorNodeTree')
    for n in list(grp.nodes):
        grp.nodes.remove(n)
    rl = grp.nodes.new('CompositorNodeRLayers')
    for fname, sock, stype in (("gb_rough", "Rough", 'FLOAT'),
                                ("gb_normal", "Normal", 'VECTOR'),
                                ("gb_position", "Position", 'VECTOR')):
        fo = grp.nodes.new('CompositorNodeOutputFile')
        fo.directory = OUT
        fo.file_name = fname
        fo.file_output_items.clear()
        it = fo.file_output_items.new(stype, "data")
        it.override_node_format = True
        it.format.file_format = 'OPEN_EXR'
        it.format.color_depth = '32'
        grp.links.new(rl.outputs[sock], fo.inputs["data"])
    S.compositing_node_group = grp
    S.render.use_compositing = True

    with Stage(dark=False, glass=False):
        S.render.resolution_percentage = 100
        S.cycles.samples = 64
        S.cycles.use_denoising = False
        bpy.ops.render.render(write_still=False)

    for mname, nodename in taps:
        nt = D.materials[mname].node_tree
        node = nt.nodes.get(nodename)
        if node:
            nt.nodes.remove(node)
    S.compositing_node_group = None


def bake_moments():
    """Per-lobe transport moments -- the physical glow model (no knobs).

    Three renders, screen emitting: M0=(1,1,1), M1=(u,v,0), M2=(u2,v2,uv).
    Diffuse/Glossy split via Cycles light-path passes; combined per lobe as
    Col*(Dir+Ind) in numpy (NOT in the compositor). Per pixel, per lobe:
      weight  W   = M0                        (RGB, linear)
      mean    mu  = M1 / M0  (per channel: R->u, G->v; tint cancels exactly)
      cov         = M2/M0 - mu^2  (uu, vv, uv; clamp to texel floor)
    Outputs mom_M0/M1/M2.exr (multi-PART EXRs: parts dc,dd,di,gc,gd,gi --
    iterate subimages with OpenImageIO, one part per File Output item).
    """
    for flag in ("diffuse_direct", "diffuse_indirect", "diffuse_color",
                 "glossy_direct", "glossy_indirect", "glossy_color"):
        setattr(VL, "use_pass_" + flag, True)

    def m1(nt):
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        em = nt.nodes.new("ShaderNodeEmission")
        uv = nt.nodes.new("ShaderNodeUVMap"); uv.uv_map = "UVMap"
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        cmb = nt.nodes.new("ShaderNodeCombineXYZ")
        cmb.inputs["Z"].default_value = 0.0
        nt.links.new(uv.outputs["UV"], sep.inputs["Vector"])
        nt.links.new(sep.outputs["X"], cmb.inputs["X"])
        nt.links.new(sep.outputs["Y"], cmb.inputs["Y"])
        nt.links.new(cmb.outputs["Vector"], em.inputs["Color"])
        nt.links.new(em.outputs["Emission"], out.inputs["Surface"])

    def m2(nt):
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        em = nt.nodes.new("ShaderNodeEmission")
        uv = nt.nodes.new("ShaderNodeUVMap"); uv.uv_map = "UVMap"
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(uv.outputs["UV"], sep.inputs["Vector"])
        def mul(a, b):
            n = nt.nodes.new("ShaderNodeMath"); n.operation = 'MULTIPLY'
            nt.links.new(a, n.inputs[0]); nt.links.new(b, n.inputs[1])
            return n.outputs[0]
        cmb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(mul(sep.outputs["X"], sep.outputs["X"]), cmb.inputs["X"])
        nt.links.new(mul(sep.outputs["Y"], sep.outputs["Y"]), cmb.inputs["Y"])
        nt.links.new(mul(sep.outputs["X"], sep.outputs["Y"]), cmb.inputs["Z"])
        nt.links.new(cmb.outputs["Vector"], em.inputs["Color"])
        nt.links.new(em.outputs["Emission"], out.inputs["Surface"])

    grp = D.node_groups.get("CRT_Moments")
    if grp is None:
        grp = D.node_groups.new("CRT_Moments", 'CompositorNodeTree')
    for n in list(grp.nodes):
        grp.nodes.remove(n)
    rl = grp.nodes.new('CompositorNodeRLayers')
    want = {}
    for key in rl.outputs.keys():
        k = key.lower().replace(" ", "")
        lobe = "d" if "diff" in k else ("g" if "gloss" in k else None)
        if lobe is None:
            continue
        # NB: "indirect" contains "dir" -- test "ind" before "dir"
        if "col" in k:
            want[lobe + "c"] = key
        elif "ind" in k:
            want[lobe + "i"] = key
        elif "dir" in k:
            want[lobe + "d"] = key
    fo = grp.nodes.new('CompositorNodeOutputFile')
    fo.directory = OUT
    fo.file_output_items.clear()
    for short, sockname in sorted(want.items()):
        fo.file_output_items.new('RGBA', short)
        grp.links.new(rl.outputs[sockname], fo.inputs[short])
    S.compositing_node_group = grp
    S.render.use_compositing = True

    emitters = (("mom_M0", mat_named("WhiteEmit", _white)),
                ("mom_M1", mat_named("MomentM1", m1)),
                ("mom_M2", mat_named("MomentM2", m2)))
    with Stage(dark=True, glass=True) as st:
        S.render.resolution_percentage = 100
        S.cycles.samples = 512
        S.cycles.use_denoising = False
        for fname, mat in emitters:
            st.swap(SCREEN, mat)
            fo.file_name = fname
            bpy.ops.render.render(write_still=False)
    S.compositing_node_group = None


def read_multilayer(path):
    """bpy can't read multilayer pixels; Blender's bundled OpenImageIO can."""
    import OpenImageIO as oiio
    import numpy as np
    inp = oiio.ImageInput.open(path)
    spec = inp.spec()
    arr = np.array(inp.read_image(format="float")).reshape(
        spec.height, spec.width, spec.nchannels)
    inp.close()
    return arr, list(spec.channelnames)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    bake_plate()
    bake_warp()
    bake_white()
    bake_gather()      # legacy mirror-gather (superseded by bake_moments)
    bake_gbuffer()
    bake_moments()     # the physical glow model: per-lobe weight/mean/covariance
    print("crt_bake: all passes ->", OUT)
