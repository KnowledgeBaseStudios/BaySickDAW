# Manual 2 assembler - control-cluster edition (2026-08-13 second rebuild).
# Chapters live at Manuals/src-m2/<group-slug>/<CODE>.html and are hand-written
# prose. Two media tags are expanded:
#   <figure data-views="CODE">            - the figure's Manual 1 view set
#   <figure data-cluster="Label|1,2,3">   - a CLOSE-UP of the controls whose
#       dot numbers are listed. The crop rect is computed from those dots'
#       master-percent coordinates (bounding box + padding) unless Jeff has
#       reshaped it - overrides live in M2CROPS keyed "CODE:idx". Dots render
#       inside the crop and scroll to the control's paragraph on click.
# Dot coordinates are the SAME master-percent set Manual 1 uses, so one nudge
# pass fixes both manuals.
import re, os, sys, html, json, struct, importlib.util

ROOT = r"C:\Users\jeffm\Documents\BaySickDAW"
REG  = os.path.join(ROOT, "Plans & Specs", "System Reference", "Callout Registry.md")
MAN  = os.path.join(ROOT, "Manuals")
SRCD = os.path.join(MAN, "src-m2")
OUT  = os.path.join(MAN, "manual-2.html")
FIGD = os.path.join(MAN, "figures")

spec = importlib.util.spec_from_file_location(
    "mc", os.path.join(MAN, "assets", "marker-coords.py"))
mc = importlib.util.module_from_spec(spec); spec.loader.exec_module(mc)
C, CROPS = mc.C, mc.CROPS
M2CROPS = getattr(mc, "M2CROPS", {})

reg = open(REG, encoding="utf-8").read()
FIGS, ORDER = {}, []
for m in re.finditer(r'^\| (Shell|Instrument|Mixing & Effects) \| (\d+) \| `([A-Z\-]+)` '
                     r'\| (.*?) \| (Main|Sub) \| (.*?) \| (.*?) \| (.*?) \|\s*$', reg, re.M):
    g, o, code, name, kind, par, files, view = m.groups()
    FIGS[code] = dict(group=g, order=int(o), name=name.strip(), kind=kind,
                      parents=re.findall(r'`([A-Z\-]+)`', par),
                      files=re.findall(r'`([^`]+)`', files) or [files.strip().strip('`')])
    ORDER.append(code)
assert len(ORDER) == 91
GROUPS = ["Shell", "Instrument", "Mixing & Effects"]
GSLUG = {"Shell": "shell", "Instrument": "instrument", "Mixing & Effects": "mixing-effects"}

def png_size(fn):
    with open(os.path.join(FIGD, fn), 'rb') as f:
        return struct.unpack('>II', f.read(33)[16:24])

m2crop_data = {}
warns = []

def crop_html(code, fn, rect, dots, label, key):
    W, H = png_size(fn)
    rx, ry, rw, rh = rect
    ar = (rw / 100.0 * W) / (rh / 100.0 * H)
    px = rw / 100.0 * W
    scale = 1.0
    if px < 260: scale = min(2.0, 260.0 / px)      # tiny clusters render enlarged
    style = 'width:%.0fpx;aspect-ratio:%.4f' % (px * scale, ar)
    imgst = ('position:absolute;width:%.3f%%;left:-%.3f%%;top:-%.3f%%'
             % (10000.0 / rw, rx / rw * 100, ry / rh * 100))
    dot_html = []
    for n in dots:
        pt = C.get(code, {}).get(n)
        if pt is None:
            warns.append("%s cluster dot %d has no coordinate" % (code, n))
            continue
        x, y = pt
        dot_html.append('<button type="button" class="marker" data-goto="%s-%d" '
                        'style="left:%.2f%%;top:%.2f%%">%d</button>'
                        % (code, n, (x - rx) / rw * 100, (y - ry) / rh * 100, n))
    cap = ('<div class="viewlabel">%s</div>' % html.escape(label)) if label else ''
    return ('%s<div class="shot cropview m2c" data-screen="%s" data-m2key="%s" '
            'data-file="%s" data-rect="%g,%g,%g,%g" data-dims="%d,%d" style="%s">'
            '<img src="figures/%s" alt="" style="%s">%s</div>'
            % (cap, code, key, html.escape(fn), rx, ry, rw, rh, W, H, style,
               html.escape(fn), imgst, "".join(dot_html)))

def expand_media(src, code):
    idx = [0]
    def rep_cluster(m):
        label, nums = m.group(1), [int(x) for x in m.group(2).split(",") if x.strip()]
        key = "%s:%d" % (code, idx[0]); idx[0] += 1
        fn = FIGS[code]['files'][0]
        pts = [C.get(code, {}).get(n) for n in nums]
        pts = [p for p in pts if p]
        if key in M2CROPS:
            rect = tuple(M2CROPS[key])
        elif pts:
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            x0, x1 = max(0.0, min(xs) - 4.5), min(100.0, max(xs) + 4.5)
            y0, y1 = max(0.0, min(ys) - 6.0), min(100.0, max(ys) + 6.0)
            if x1 - x0 < 14: cx = (x0 + x1) / 2; x0, x1 = max(0.0, cx - 7), min(100.0, cx + 7)
            if y1 - y0 < 10: cy = (y0 + y1) / 2; y0, y1 = max(0.0, cy - 5), min(100.0, cy + 5)
            rect = (round(x0, 2), round(y0, 2), round(x1 - x0, 2), round(y1 - y0, 2))
        else:
            rect = (0.0, 0.0, 100.0, 100.0)
        m2crop_data[key] = [fn, list(rect)]
        return crop_html(code, fn, rect, nums, label, key)
    src = re.sub(r'<figure data-cluster="([^|"]*)\|([\d,\s]*)"\s*/?>(?:</figure>)?', rep_cluster, src)

    def rep_views(m):
        c = m.group(1)
        views = CROPS.get(c, [(None, None, None)])
        out = []
        for vi, (fn, rect, vlabel) in enumerate(views):
            if fn is None:
                fn = FIGS[c]['files'][0] if c in FIGS else None
            if fn is None: continue
            W, H = png_size(fn)
            rx, ry, rw, rh = rect if rect else (0, 0, 100, 100)
            if rect:
                ar = (rw / 100.0 * W) / (rh / 100.0 * H)
                style = 'width:%.0fpx;aspect-ratio:%.4f' % (rw / 100.0 * W, ar)
                imgst = ('position:absolute;width:%.3f%%;left:-%.3f%%;top:-%.3f%%'
                         % (10000.0 / rw, rx / rw * 100, ry / rh * 100))
                cls = 'shot cropview'
            else:
                style, imgst, cls = '', '', 'shot'
            cap = ('<div class="viewlabel">%s</div>' % html.escape(vlabel)) if vlabel else ''
            out.append('%s<div class="%s" data-screen="%s" data-rect="%g,%g,%g,%g" style="%s">'
                       '<img src="figures/%s" alt="" style="%s"></div>'
                       % (cap, cls, c, rx, ry, rw, rh, style, html.escape(fn), imgst))
        return "\n".join(out)
    return re.sub(r'<figure data-views="([A-Z\-]+)"\s*/?>(?:</figure>)?', rep_views, src)

parts, covered = [], 0
nav = []
for g in GROUPS:
    nav.append('<h4>%s</h4>' % html.escape(g))
    parts.append('<h2 class="chapter" id="g-%s">%s</h2>' % (GSLUG[g], html.escape(g)))
    for code in sorted([c for c in ORDER if FIGS[c]['group'] == g],
                       key=lambda c: FIGS[c]['order']):
        f = FIGS[code]
        nav.append('<a href="#%s"><span>%s</span></a>' % (code, html.escape(f['name'])))
        chap = os.path.join(SRCD, GSLUG[g], code + ".html")
        parts.append('<section class="m2fig" id="%s"><h3>%s</h3>'
                     '<a class="backref" href="manual-1.html#%s">See it on the full screen</a>'
                     % (code, html.escape(f['name']), code))
        if os.path.exists(chap):
            parts.append(expand_media(open(chap, encoding="utf-8").read(), code))
            covered += 1
        else:
            parts.append('<p class="pending">This chapter is being written.</p>')
        parts.append('</section>')

doc = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BaySickDAW &middot; Manual 2 &middot; Control Reference</title>
<link rel="stylesheet" href="assets/manual.css">
<link rel="stylesheet" href="assets/atlas.css">
<style>
section.m2fig { scroll-margin-top: 14px; margin-bottom: 40px; }
a.backref { display: inline-block; font-size: 12px; margin: 0 0 10px; }
p.pending { color: var(--text-dim); font-size: 13px; }
.m2c { margin: 14px 0 6px; }
section.m2fig p:target { background: rgba(0,229,255,.10); border-radius: 4px; }
section.m2fig p.flash { background: rgba(0,229,255,.16); border-radius: 4px; }
</style>
</head>
<body>

<header class="masthead">
  <div class="wrap">
    <h1><span class="brand">Manual 2</span> &middot; Control Reference</h1>
    <p>Every control, taught from zero: what it is, what it does, and what you hear when you move it.</p>
    <nav class="manuals">
      <a href="index.html">Contents</a>
      <a href="manual-1.html">1 &middot; Visual Atlas</a>
      <a class="here" href="manual-2.html">2 &middot; Control Reference</a>
      <a href="manual-3.html">3 &middot; Under the Hood</a>
    </nav>
  </div>
</header>

<div class="wrap atlas">
<aside class="figtoc noprint">
  <div class="jump">
    <label for="jumpbox">Search this manual</label>
    <input id="jumpbox" type="text" placeholder="control or screen name" autocomplete="off">
    <div id="jumphint"></div>
  </div>
{nav}
</aside>
<main>
{parts}
</main>
</div>

<div class="nudge noprint" id="nudgebar">
  <button id="nudgetoggle">Enable dot nudging</button>
  <button id="croptoggle">Reshape close-up boxes</button>
  <span id="nudgestate">off</span>
  <button id="nudgecopy" disabled>Copy positions</button>
  <span class="hint">Dots are shared with Manual 1 - nudging here fixes both. One copy carries dots and boxes.</span>
</div>

<script>
const COORDS  = {coords};
const M2CROPS = {m2crops};

(function () {
  function rectOf(shot) { return shot.dataset.rect.split(',').map(Number); }

  // Dot click -> the control's paragraph
  document.addEventListener('click', function (e) {
    var b = e.target.closest('.marker[data-goto]');
    if (!b || document.body.classList.contains('nudging')) return;
    var p = document.getElementById(b.dataset.goto);
    if (!p) return;
    p.scrollIntoView({ block: 'center' });
    p.classList.add('flash');
    setTimeout(function () { p.classList.remove('flash'); }, 1400);
  });

  // ── dot nudging (writes MASTER coords - shared with Manual 1) ────────────
  let on = false, drag = null, cropOn = false, cdrag = null;
  const toggle = document.getElementById('nudgetoggle');
  const state  = document.getElementById('nudgestate');
  const copy   = document.getElementById('nudgecopy');
  const ctoggle = document.getElementById('croptoggle');

  toggle.addEventListener('click', function () {
    on = !on;
    document.body.classList.toggle('nudging', on);
    toggle.textContent = on ? 'Disable dot nudging' : 'Enable dot nudging';
    state.textContent = on ? 'ON - drag dots' : (cropOn ? 'ON - drag boxes' : 'off');
    copy.disabled = !(on || cropOn);
  });

  document.addEventListener('pointerdown', function (e) {
    if (!on) return;
    const m = e.target.closest('.marker');
    if (!m) return;
    e.preventDefault();
    drag = { m: m, shot: m.parentElement };
    m.setPointerCapture(e.pointerId);
  });
  document.addEventListener('pointermove', function (e) {
    if (!drag) return;
    const s = drag.shot, r = rectOf(s), br = s.getBoundingClientRect();
    const vx = Math.min(100, Math.max(0, ((e.clientX - br.left) / br.width)  * 100));
    const vy = Math.min(100, Math.max(0, ((e.clientY - br.top)  / br.height) * 100));
    drag.m.style.left = vx.toFixed(2) + '%';
    drag.m.style.top  = vy.toFixed(2) + '%';
    const id = drag.m.dataset.goto, i = id.lastIndexOf('-');
    const code = id.slice(0, i), n = id.slice(i + 1);
    if (!COORDS[code]) COORDS[code] = {};
    COORDS[code][n] = [ +(r[0] + vx / 100 * r[2]).toFixed(2),
                        +(r[1] + vy / 100 * r[3]).toFixed(2) ];
  });
  document.addEventListener('pointerup', function () { drag = null; });

  // ── close-up box reshape (writes M2CROPS overrides) ──────────────────────
  function applyRect(shot, rect) {
    const dims = shot.dataset.dims.split(',').map(Number);
    shot.dataset.rect = rect.join(',');
    const px = rect[2] / 100 * dims[0];
    const scale = px < 260 ? Math.min(2, 260 / px) : 1;
    shot.style.width = (px * scale).toFixed(0) + 'px';
    shot.style.aspectRatio = ((rect[2] / 100 * dims[0]) / (rect[3] / 100 * dims[1])).toFixed(4);
    const img = shot.querySelector('img');
    img.style.width = (10000 / rect[2]) + '%';
    img.style.left  = -(rect[0] / rect[2] * 100) + '%';
    img.style.top   = -(rect[1] / rect[3] * 100) + '%';
  }
  function enterReshape(shot) {
    const key = shot.dataset.m2key;
    if (!key) return;
    const dims = shot.dataset.dims.split(',').map(Number);
    shot.classList.add('reshaping');
    shot.dataset.savedrect = shot.dataset.rect;
    shot.style.width = Math.min(dims[0], 900) + 'px';
    shot.style.aspectRatio = (dims[0] / dims[1]).toFixed(4);
    const img = shot.querySelector('img');
    img.style.width = '100%'; img.style.left = '0'; img.style.top = '0';
    shot.querySelectorAll('.marker').forEach(function (m) { m.style.display = 'none'; });
    const box = document.createElement('div');
    box.className = 'croprect';
    ['nw','ne','sw','se'].forEach(function (c) {
      const h = document.createElement('div');
      h.className = 'handle ' + c; h.dataset.corner = c;
      box.appendChild(h);
    });
    shot.appendChild(box);
    syncBox(shot);
  }
  function syncBox(shot) {
    const r = (M2CROPS[shot.dataset.m2key] ? M2CROPS[shot.dataset.m2key][1]
                                           : shot.dataset.savedrect.split(',').map(Number));
    const box = shot.querySelector('.croprect');
    if (!box) return;
    box.style.left = r[0] + '%'; box.style.top = r[1] + '%';
    box.style.width = r[2] + '%'; box.style.height = r[3] + '%';
  }
  function exitReshape(shot) {
    shot.classList.remove('reshaping');
    const box = shot.querySelector('.croprect');
    if (box) box.remove();
    const key = shot.dataset.m2key;
    const rect = M2CROPS[key] ? M2CROPS[key][1] : shot.dataset.savedrect.split(',').map(Number);
    applyRect(shot, rect);
    const r = rect;
    shot.querySelectorAll('.marker').forEach(function (m) {
      m.style.display = '';
      const id = m.dataset.goto, i = id.lastIndexOf('-');
      const pt = (COORDS[id.slice(0, i)] || {})[id.slice(i + 1)];
      if (pt) {
        m.style.left = ((pt[0] - r[0]) / r[2] * 100).toFixed(2) + '%';
        m.style.top  = ((pt[1] - r[1]) / r[3] * 100).toFixed(2) + '%';
      }
    });
  }
  ctoggle.addEventListener('click', function () {
    cropOn = !cropOn;
    document.body.classList.toggle('cropmode', cropOn);
    ctoggle.textContent = cropOn ? 'Done reshaping' : 'Reshape close-up boxes';
    state.textContent = cropOn ? 'ON - drag boxes' : (on ? 'ON - drag dots' : 'off');
    copy.disabled = !(on || cropOn);
    document.querySelectorAll('.shot.m2c').forEach(cropOn ? enterReshape : exitReshape);
  });
  document.addEventListener('pointerdown', function (e) {
    if (!cropOn) return;
    const box = e.target.closest('.croprect');
    if (!box) return;
    e.preventDefault();
    const shot = box.parentElement;
    const key = shot.dataset.m2key;
    if (!M2CROPS[key]) M2CROPS[key] = [shot.dataset.file, shot.dataset.savedrect.split(',').map(Number)];
    cdrag = { shot: shot, corner: e.target.dataset ? e.target.dataset.corner : null,
              start: M2CROPS[key][1].slice(), sx: e.clientX, sy: e.clientY };
    box.setPointerCapture(e.pointerId);
  });
  document.addEventListener('pointermove', function (e) {
    if (!cdrag) return;
    const shot = cdrag.shot, br = shot.getBoundingClientRect();
    const dx = (e.clientX - cdrag.sx) / br.width * 100;
    const dy = (e.clientY - cdrag.sy) / br.height * 100;
    const s = cdrag.start;
    let r = s.slice();
    if (!cdrag.corner) {
      r[0] = Math.min(100 - s[2], Math.max(0, s[0] + dx));
      r[1] = Math.min(100 - s[3], Math.max(0, s[1] + dy));
    } else {
      if (cdrag.corner.indexOf('w') >= 0) { r[0] = s[0] + dx; r[2] = s[2] - dx; }
      if (cdrag.corner.indexOf('e') >= 0) { r[2] = s[2] + dx; }
      if (cdrag.corner.indexOf('n') >= 0) { r[1] = s[1] + dy; r[3] = s[3] - dy; }
      if (cdrag.corner.indexOf('s') >= 0) { r[3] = s[3] + dy; }
      if (r[2] < 3) r[2] = 3;
      if (r[3] < 2) r[3] = 2;
      r[0] = Math.max(0, Math.min(r[0], 100));
      r[1] = Math.max(0, Math.min(r[1], 100));
      if (r[0] + r[2] > 100) r[2] = 100 - r[0];
      if (r[1] + r[3] > 100) r[3] = 100 - r[1];
    }
    M2CROPS[cdrag.shot.dataset.m2key][1] = r.map(function (v) { return +v.toFixed(2); });
    syncBox(shot);
  });
  document.addEventListener('pointerup', function () { cdrag = null; });

  copy.addEventListener('click', function () {
    const out = JSON.stringify({ coords: COORDS, m2crops: M2CROPS }, null, 1);
    navigator.clipboard.writeText(out).then(
      function () { state.textContent = 'copied to clipboard'; },
      function () {
        const w = window.open('', '_blank');
        w.document.write('<pre>' + out.replace(/</g, '&lt;') + '</pre>');
      });
  });
})();
</script>

<script src="assets/search.js"></script>
</body>
</html>
"""
doc = doc.replace("{nav}", "\n".join(nav)).replace("{parts}", "\n".join(parts))
coord_out = {c: {str(n): [round(x, 2), round(y, 2)] for n, (x, y) in sorted(v.items())}
             for c, v in C.items()}
doc = doc.replace("{coords}", json.dumps(coord_out, separators=(',', ':')))
doc = doc.replace("{m2crops}", json.dumps(m2crop_data, separators=(',', ':')))
open(OUT, "w", encoding="utf-8", newline="").write(doc)

anchors = set(re.findall(r'id="([A-Z][A-Z\-]*-\d+)"', doc))
print("figures        :", len(ORDER))
print("chapters landed:", covered)
print("clusters       :", len(m2crop_data))
print("callout anchors:", len(anchors))
for w in warns: print("WARN:", w)
