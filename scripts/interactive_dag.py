#!/usr/bin/env python
"""Post-process scheduler DAG SVGs into interactive HTML files.

Reads perm_*.svg from wave_output/, injects JavaScript for click-to-trace
interactivity, and writes standalone HTML files to wave_output/interactive/.

Usage:
    .venv/Scripts/python.exe scripts/interactive_dag.py
"""
import sys
import os
import re
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

WAVE_DIR = Path("wave_output")
OUT_DIR = WAVE_DIR / "interactive"

PANZOOM_CODE = r"""
(function() {
  const viewport = document.getElementById('viewport');
  const tg = document.getElementById('transform-group');
  const svgEl = tg.querySelector('svg');

  let scale = 1;
  let tx = 0, ty = 0;
  let isPanning = false;
  let startX, startY, startTx, startTy;

  const MIN_SCALE = 0.05;
  const MAX_SCALE = 8;
  const ZOOM_SPEED = 0.002;

  function applyTransform() {
    tg.style.transform = 'translate(' + tx + 'px,' + ty + 'px) scale(' + scale + ')';
  }

  function fitToScreen() {
    const vw = viewport.clientWidth;
    const vh = viewport.clientHeight;
    // Get SVG natural size from width/height attributes (in pt)
    let sw = svgEl.width.baseVal.value;
    let sh = svgEl.height.baseVal.value;
    if (!sw || !sh) {
      const vb = svgEl.viewBox.baseVal;
      sw = vb.width; sh = vb.height;
    }
    scale = Math.min(vw / sw, vh / sh, 1) * 0.95;
    tx = (vw - sw * scale) / 2;
    ty = (vh - sh * scale) / 2;
    applyTransform();
  }

  // Wheel zoom: zoom toward cursor
  viewport.addEventListener('wheel', function(ev) {
    ev.preventDefault();
    const rect = viewport.getBoundingClientRect();
    const mx = ev.clientX - rect.left;
    const my = ev.clientY - rect.top;

    const delta = -ev.deltaY * ZOOM_SPEED;
    const factor = Math.exp(delta);
    const newScale = Math.max(MIN_SCALE, Math.min(MAX_SCALE, scale * factor));
    const r = newScale / scale;

    tx = mx - r * (mx - tx);
    ty = my - r * (my - ty);
    scale = newScale;
    applyTransform();
  }, {passive: false});

  // Pan: middle mouse, or left mouse on background (not on nodes)
  viewport.addEventListener('mousedown', function(ev) {
    // Middle button always pans; left button pans if not on a node
    if (ev.button === 1 || (ev.button === 0 && !ev.target.closest('g.node'))) {
      isPanning = true;
      startX = ev.clientX;
      startY = ev.clientY;
      startTx = tx;
      startTy = ty;
      viewport.classList.add('panning');
      ev.preventDefault();
    }
  });

  window.addEventListener('mousemove', function(ev) {
    if (!isPanning) return;
    tx = startTx + (ev.clientX - startX);
    ty = startTy + (ev.clientY - startY);
    applyTransform();
  });

  window.addEventListener('mouseup', function(ev) {
    if (isPanning) {
      isPanning = false;
      viewport.classList.remove('panning');
    }
  });

  // Home key = fit to screen
  document.addEventListener('keydown', function(ev) {
    if (ev.key === 'Home') { fitToScreen(); ev.preventDefault(); }
  });

  // Remove SVG fixed width/height so it sizes naturally, keep viewBox
  svgEl.removeAttribute('width');
  svgEl.removeAttribute('height');
  // Re-apply natural size from viewBox
  const vb = svgEl.viewBox.baseVal;
  if (vb && vb.width && vb.height) {
    svgEl.setAttribute('width', vb.width);
    svgEl.setAttribute('height', vb.height);
  }

  // Initial fit
  fitToScreen();
  window.addEventListener('resize', fitToScreen);
})();
"""

JS_CODE = r"""
(function() {
  const svg = document.querySelector('svg');
  const nodes = svg.querySelectorAll('g.node');
  const edges = svg.querySelectorAll('g.edge');

  // Build adjacency from edge titles: "n0&#45;&gt;n1" decodes to "n0->n1"
  // But in DOM, .textContent already gives decoded text like "n0->n1"
  const adj = {};      // nodeId -> [{edge, target}]
  const radj = {};     // nodeId -> [{edge, source}]
  const edgeMap = {};  // "src->tgt" -> edgeElement

  function getNodeId(gElem) {
    const t = gElem.querySelector('title');
    return t ? t.textContent.trim() : null;
  }

  function getNodeLabel(gElem) {
    const texts = gElem.querySelectorAll('text');
    return texts.length > 0 ? texts[0].textContent.trim() : '';
  }

  // Map nodeId -> g element
  const nodeById = {};
  nodes.forEach(n => {
    const id = getNodeId(n);
    if (id) {
      nodeById[id] = n;
      adj[id] = [];
      radj[id] = [];
    }
  });

  edges.forEach(e => {
    const t = e.querySelector('title');
    if (!t) return;
    const txt = t.textContent.trim();
    const m = txt.match(/^(\S+)->(\S+)$/);
    if (!m) return;
    const [, src, tgt] = m;
    if (!adj[src]) adj[src] = [];
    if (!radj[tgt]) radj[tgt] = [];
    adj[src].push({edge: e, target: tgt});
    radj[tgt].push({edge: e, source: src});
    edgeMap[src + '->' + tgt] = e;
  });

  // BFS downstream from a node
  function bfsDown(startId) {
    const visited = new Set();
    const visitedEdges = new Set();
    const queue = [startId];
    visited.add(startId);
    while (queue.length > 0) {
      const cur = queue.shift();
      for (const {edge, target} of (adj[cur] || [])) {
        visitedEdges.add(edge);
        if (!visited.has(target)) {
          visited.add(target);
          queue.push(target);
        }
      }
    }
    return {nodes: visited, edges: visitedEdges};
  }

  // BFS to find all nodes/edges on ANY path from src to dst (downstream)
  function findPaths(srcId, dstId) {
    // Forward BFS from src: record distances
    const fwd = new Map();
    fwd.set(srcId, 0);
    let queue = [srcId];
    while (queue.length > 0) {
      const cur = queue.shift();
      const d = fwd.get(cur);
      for (const {target} of (adj[cur] || [])) {
        if (!fwd.has(target)) {
          fwd.set(target, d + 1);
          queue.push(target);
        }
      }
    }
    if (!fwd.has(dstId)) return null; // no path

    // Backward BFS from dst, only through nodes reachable from src
    const bwd = new Map();
    bwd.set(dstId, 0);
    queue = [dstId];
    while (queue.length > 0) {
      const cur = queue.shift();
      const d = bwd.get(cur);
      for (const {source} of (radj[cur] || [])) {
        if (fwd.has(source) && !bwd.has(source)) {
          bwd.set(source, d + 1);
          queue.push(source);
        }
      }
    }

    // Nodes on a shortest path: fwd[n] + bwd[n] == fwd[dst]
    const totalDist = fwd.get(dstId);
    const pathNodes = new Set();
    const pathEdges = new Set();
    for (const [nid] of fwd) {
      if (bwd.has(nid) && fwd.get(nid) + bwd.get(nid) === totalDist) {
        pathNodes.add(nid);
      }
    }
    // Collect edges between path nodes
    for (const nid of pathNodes) {
      for (const {edge, target} of (adj[nid] || [])) {
        if (pathNodes.has(target) && fwd.get(target) === fwd.get(nid) + 1) {
          pathEdges.add(edge);
        }
      }
    }
    return {nodes: pathNodes, edges: pathEdges};
  }

  function setAllOpacity(val) {
    nodes.forEach(n => { n.style.opacity = val; });
    edges.forEach(e => { e.style.opacity = val; });
    // clusters too
    svg.querySelectorAll('g.cluster').forEach(c => { c.style.opacity = val; });
  }

  function resetAll() {
    setAllOpacity('');
    nodes.forEach(n => {
      n.style.filter = '';
      const polys = n.querySelectorAll('polygon, ellipse');
      polys.forEach(p => { p.style.stroke = ''; p.style.strokeWidth = ''; });
    });
    edges.forEach(e => {
      const paths = e.querySelectorAll('path');
      const arrows = e.querySelectorAll('polygon');
      paths.forEach(p => { p.style.stroke = ''; p.style.strokeWidth = ''; });
      arrows.forEach(a => { a.style.stroke = ''; a.style.fill = ''; });
    });
    selected = [];
  }

  function highlightNode(nodeElem, color) {
    nodeElem.style.opacity = '1';
    const polys = nodeElem.querySelectorAll('polygon, ellipse');
    polys.forEach(p => { p.style.stroke = color; p.style.strokeWidth = '2.5'; });
  }

  function highlightEdge(edgeElem, color) {
    edgeElem.style.opacity = '1';
    const paths = edgeElem.querySelectorAll('path');
    const arrows = edgeElem.querySelectorAll('polygon');
    paths.forEach(p => { p.style.stroke = color; p.style.strokeWidth = '2'; });
    arrows.forEach(a => { a.style.stroke = color; a.style.fill = color; });
  }

  let selected = []; // array of nodeIds

  function handleNodeClick(nodeId) {
    if (selected.length === 0) {
      // First click: highlight downstream
      const result = bfsDown(nodeId);
      setAllOpacity('0.12');
      // Show clusters at reduced opacity
      svg.querySelectorAll('g.cluster').forEach(c => { c.style.opacity = '0.35'; });
      result.nodes.forEach(nid => {
        if (nodeById[nid]) {
          nodeById[nid].style.opacity = '1';
          if (nid === nodeId) highlightNode(nodeById[nid], '#e63946');
        }
      });
      result.edges.forEach(e => highlightEdge(e, '#e63946'));
      selected = [nodeId];
    } else if (selected.length === 1) {
      if (selected[0] === nodeId) {
        resetAll();
        return;
      }
      // Second click: show path between first and second, plus downstream of second
      const pathResult = findPaths(selected[0], nodeId);
      const downResult = bfsDown(nodeId);

      setAllOpacity('0.12');
      svg.querySelectorAll('g.cluster').forEach(c => { c.style.opacity = '0.35'; });

      // Path nodes/edges in red
      if (pathResult) {
        pathResult.nodes.forEach(nid => {
          if (nodeById[nid]) {
            nodeById[nid].style.opacity = '1';
            highlightNode(nodeById[nid], '#e63946');
          }
        });
        pathResult.edges.forEach(e => highlightEdge(e, '#e63946'));
      }

      // Downstream of second node in blue (if not already on path)
      downResult.nodes.forEach(nid => {
        if (nodeById[nid]) {
          nodeById[nid].style.opacity = '1';
          if (!pathResult || !pathResult.nodes.has(nid)) {
            highlightNode(nodeById[nid], '#457b9d');
          }
        }
      });
      downResult.edges.forEach(e => {
        if (!pathResult || !pathResult.edges.has(e)) {
          highlightEdge(e, '#457b9d');
        }
      });

      // Always highlight the two selected nodes
      if (nodeById[selected[0]]) highlightNode(nodeById[selected[0]], '#e63946');
      if (nodeById[nodeId]) highlightNode(nodeById[nodeId], '#2a9d8f');

      selected = [selected[0], nodeId];
    } else {
      // Third click resets and starts over
      resetAll();
      handleNodeClick(nodeId);
    }
  }

  // Attach click handlers to nodes
  nodes.forEach(n => {
    n.style.cursor = 'pointer';
    n.addEventListener('click', function(ev) {
      ev.stopPropagation();
      const id = getNodeId(n);
      if (id) handleNodeClick(id);
    });
  });

  // Hover: subtle highlight of direct neighbors
  nodes.forEach(n => {
    n.addEventListener('mouseenter', function() {
      if (selected.length > 0) return; // don't hover when selection active
      const id = getNodeId(n);
      if (!id) return;
      const polys = n.querySelectorAll('polygon, ellipse');
      polys.forEach(p => { p.style.stroke = '#457b9d'; p.style.strokeWidth = '2'; });
      // highlight direct edges
      for (const {edge} of (adj[id] || [])) {
        const paths = edge.querySelectorAll('path');
        paths.forEach(p => { p.style.stroke = '#457b9d'; p.style.strokeWidth = '1.5'; });
      }
      for (const {edge} of (radj[id] || [])) {
        const paths = edge.querySelectorAll('path');
        paths.forEach(p => { p.style.stroke = '#a8dadc'; p.style.strokeWidth = '1.5'; });
      }
    });
    n.addEventListener('mouseleave', function() {
      if (selected.length > 0) return;
      const id = getNodeId(n);
      if (!id) return;
      const polys = n.querySelectorAll('polygon, ellipse');
      polys.forEach(p => { p.style.stroke = ''; p.style.strokeWidth = ''; });
      for (const {edge} of (adj[id] || [])) {
        const paths = edge.querySelectorAll('path');
        paths.forEach(p => { p.style.stroke = ''; p.style.strokeWidth = ''; });
      }
      for (const {edge} of (radj[id] || [])) {
        const paths = edge.querySelectorAll('path');
        paths.forEach(p => { p.style.stroke = ''; p.style.strokeWidth = ''; });
      }
    });
  });

  // Click background or press Escape to reset
  // Use mousedown/mouseup distance to distinguish click from pan
  let mdX = 0, mdY = 0;
  svg.addEventListener('mousedown', function(ev) { mdX = ev.clientX; mdY = ev.clientY; });
  svg.addEventListener('click', function(ev) {
    const dx = ev.clientX - mdX, dy = ev.clientY - mdY;
    if (dx*dx + dy*dy > 25) return; // was a drag, not a click
    if (ev.target === svg || ev.target.closest('g.graph') === ev.target) {
      resetAll();
    }
  });
  document.addEventListener('keydown', function(ev) {
    if (ev.key === 'Escape') resetAll();
  });

  // Legend
  const legend = document.getElementById('legend');
  if (legend) {
    legend.innerHTML =
      '<b>Click</b> a node to highlight downstream. ' +
      '<b>Click a second</b> node to show path (red) + downstream (blue). ' +
      '<b>Escape</b> or click background to reset. ' +
      '<b>Hover</b> for direct neighbors.';
  }
})();
"""

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
  body {{
    margin: 0;
    padding: 0;
    background: #1a1a2e;
    overflow: hidden;
    width: 100vw;
    height: 100vh;
  }}
  #legend {{
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    background: rgba(26,26,46,0.92);
    color: #a8dadc;
    font-family: Consolas, monospace;
    font-size: 13px;
    padding: 6px 16px;
    z-index: 10;
    border-bottom: 1px solid #457b9d;
  }}
  #viewport {{
    position: absolute;
    top: 32px;
    left: 0;
    right: 0;
    bottom: 0;
    overflow: hidden;
    cursor: grab;
  }}
  #viewport.panning {{
    cursor: grabbing;
  }}
  #transform-group {{
    transform-origin: 0 0;
    will-change: transform;
  }}
  svg {{
    display: block;
  }}
  /* Smooth transitions for interactive highlighting */
  g.node, g.edge {{
    transition: opacity 0.18s ease;
  }}
  g.node polygon, g.node ellipse {{
    transition: stroke 0.18s ease, stroke-width 0.18s ease;
  }}
  g.edge path, g.edge polygon {{
    transition: stroke 0.18s ease, stroke-width 0.18s ease, fill 0.18s ease;
  }}
</style>
</head>
<body>
<div id="legend">Loading...</div>
<div id="viewport">
<div id="transform-group">
{svg_content}
</div>
</div>
<script>
{panzoom_code}
{js_code}
</script>
</body>
</html>
"""


def strip_xml_header(svg_text: str) -> str:
    """Remove <?xml ...?> and <!DOCTYPE ...> from SVG so it can be inlined."""
    svg_text = re.sub(r'<\?xml[^?]*\?>\s*', '', svg_text)
    svg_text = re.sub(r'<!DOCTYPE[^>]*>\s*', '', svg_text)
    return svg_text.strip()


def process_svg(svg_path: Path, out_dir: Path) -> Path:
    svg_text = svg_path.read_text(encoding='utf-8')
    svg_inline = strip_xml_header(svg_text)

    # Extract perm id for title
    stem = svg_path.stem  # e.g. perm_231314482529216
    title = f"Interactive DAG - {stem}"

    html = HTML_TEMPLATE.format(
        title=title,
        svg_content=svg_inline,
        panzoom_code=PANZOOM_CODE,
        js_code=JS_CODE,
    )

    out_path = out_dir / f"{stem}.html"
    out_path.write_text(html, encoding='utf-8')
    return out_path


def main():
    if not WAVE_DIR.exists():
        print(f"ERROR: {WAVE_DIR} not found", file=sys.stderr)
        sys.exit(1)

    svgs = sorted(WAVE_DIR.glob("perm_*.svg"))
    if not svgs:
        print(f"No perm_*.svg files in {WAVE_DIR}", file=sys.stderr)
        sys.exit(1)

    OUT_DIR.mkdir(exist_ok=True)

    print(f"Processing {len(svgs)} SVGs -> {OUT_DIR}/")
    for svg_path in svgs:
        out = process_svg(svg_path, OUT_DIR)
        print(f"  {svg_path.name} -> {out.name}")

    print("Done.")


if __name__ == '__main__':
    main()
