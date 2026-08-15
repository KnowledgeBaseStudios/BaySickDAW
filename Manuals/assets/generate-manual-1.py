# Manual 1 generator - 2026-08-13 restructure.
# Reads the Callout Registry's figure tree + per-figure sections, marker
# coordinates and crop views from marker-coords.py, and emits manual-1.html.
# Codes are build-time keys only: nothing user-visible shows them (Jeff's
# ruling - names everywhere; codes live on as anchors and search terms).
import re, os, json, sys, html, struct, importlib.util

ROOT = r"C:\Users\jeffm\Documents\BaySickDAW"
REG  = os.path.join(ROOT, "Plans & Specs", "System Reference", "Callout Registry.md")
MAN  = os.path.join(ROOT, "Manuals")
OUT  = os.path.join(MAN, "manual-1.html")
FIGD = os.path.join(MAN, "figures")

spec = importlib.util.spec_from_file_location(
    "mc", os.path.join(MAN, "assets", "marker-coords.py"))
mc = importlib.util.module_from_spec(spec); spec.loader.exec_module(mc)
C, CROPS = mc.C, mc.CROPS

reg = open(REG, encoding="utf-8").read()

# ── figure tree ─────────────────────────────────────────────────────────────
FIGS, ORDER = {}, []
for m in re.finditer(r'^\| (Shell|Instrument|Mixing & Effects) \| (\d+) \| `([A-Z\-]+)` '
                     r'\| (.*?) \| (Main|Sub) \| (.*?) \| (.*?) \| (.*?) \|\s*$', reg, re.M):
    g, o, code, name, kind, par, files, view = m.groups()
    FIGS[code] = dict(group=g, order=int(o), name=name.strip(), kind=kind,
                      parents=re.findall(r'`([A-Z\-]+)`', par),
                      files=re.findall(r'`([^`]+)`', files) or [files.strip().strip('`')])
    ORDER.append(code)
assert len(ORDER) == 91, "figure tree parse got %d rows" % len(ORDER)
GROUPS = ["Shell", "Instrument", "Mixing & Effects"]
CHILDREN = {}
for code, f in FIGS.items():
    for p in f['parents']:
        CHILDREN.setdefault(p, []).append(code)
for p in CHILDREN:
    CHILDREN[p].sort(key=lambda c: (GROUPS.index(FIGS[c]['group']), FIGS[c]['order']))

def depth(code, seen=()):
    ps = [p for p in FIGS[code]['parents'] if p not in seen]
    return 0 if not ps else 1 + depth(ps[0], seen + (code,))

# ── per-figure callouts + blurbs ────────────────────────────────────────────
call = {}
for code, n, label, imp in re.findall(
        r'^\| ([A-Z][A-Z\-]*)-(\d+) \| (.*?) \| \w+ \| ([A-Z\-0-9]+|-) \| .*? \|\s*$', reg, re.M):
    call.setdefault(code, []).append((int(n), label, imp))
for k in call: call[k].sort()

blurb = {}
for m in re.finditer(r'^### `([A-Z\-]+)` - `[^`]+`\s*\n\s*\n(.*?)(?=\n\| Callout|\n### |\Z)',
                     reg, re.M | re.S):
    t = m.group(2).strip()
    blurb[m.group(1)] = "" if t.startswith("|") else " ".join(t.split())

LABEL = {"%s-%d" % (c, n): lab for c, entries in call.items() for n, lab, _ in entries}

def snippet(label, maxlen=46):
    s = re.split(r' - ', label)[0]
    s = re.sub(r'[`*]', '', s).strip()
    return (s[:maxlen-1] + "\u2026") if len(s) > maxlen else s

# ── destinations that actually exist (buttons only render on a real target) ─
m2 = open(os.path.join(MAN, "manual-2.html"), encoding="utf-8").read()
m3 = open(os.path.join(MAN, "manual-3.html"), encoding="utf-8").read()
M2IDS = set(re.findall(r'id="([A-Z][A-Z\-]*-\d+)"', m2))
M3IDS = set(re.findall(r'id="(IMP-\d+)"', m3))

warns = []
def md_inline(s, here=None):
    s = html.escape(s)
    s = re.sub(r'`([^`]+)`', r'<code>\1</code>', s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', s)
    s = re.sub(r'\*([^*]+)\*', r'<em>\1</em>', s)
    def seeid(m):
        cid = m.group(1)
        if cid in LABEL:
            return 'see <a href="#fig-%s">%s</a>' % (cid, html.escape(snippet(LABEL[cid])))
        warns.append("dangling see-ref %s (in %s)" % (cid, here))
        return 'see ' + html.escape(snippet(cid))
    s = re.sub(r'\bsee ([A-Z][A-Z\-]*-\d+)\b', seeid, s)
    def seefig(m):
        code = m.group(1)
        if code in FIGS:
            return 'see <a href="#%s">%s</a>' % (code, html.escape(FIGS[code]['name']))
        return m.group(0)
    s = re.sub(r'\bsee ([A-Z][A-Z\-]+|[A-Z]{2,})\b(?!-\d)', seefig, s)
    return s

def png_size(fn):
    with open(os.path.join(FIGD, fn), 'rb') as f:
        return struct.unpack('>II', f.read(33)[16:24])

# ── figure sections ─────────────────────────────────────────────────────────
parts, coord_data, crop_data = [], {}, {}
markers_placed = markers_skipped = 0

for code in [c for g in GROUPS for c in ORDER if FIGS[c]['group'] == g]:
    f  = FIGS[code]
    cs = call.get(code, [])
    mk = C.get(code, {})
    for n, (x, y) in mk.items():
        if not (0 <= x <= 100 and 0 <= y <= 100):
            raise SystemExit("OFF-CANVAS marker %s-%s at (%s, %s)" % (code, n, x, y))
    coord_data[code] = {str(n): [round(x, 2), round(y, 2)] for n, (x, y) in sorted(mk.items())}

    views = CROPS.get(code, [(f['files'][0], None, None)])
    crop_data[code] = []
    shots = []
    covered = set()
    for vi, (fn, rect, vlabel) in enumerate(views):
        W, H = png_size(fn)
        rx, ry, rw, rh = rect if rect else (0, 0, 100, 100)
        crop_data[code].append([fn, rect and list(rect), vlabel, W, H])
        for n, (x, y) in mk.items():
            if rx - 0.01 <= x <= rx + rw + 0.01 and ry - 0.01 <= y <= ry + rh + 0.01:
                covered.add(n)
        if rect:
            ar = (rw / 100.0 * W) / (rh / 100.0 * H)
            style = 'width:%.0fpx;aspect-ratio:%.4f' % (rw / 100.0 * W, ar)
            imgst = ('position:absolute;width:%.3f%%;left:-%.3f%%;top:-%.3f%%'
                     % (10000.0 / rw, rx / rw * 100, ry / rh * 100))
            cls = 'shot cropview'
        else:
            style, imgst, cls = '', '', 'shot'
        cap = ('<div class="viewlabel">%s</div>' % html.escape(vlabel)) if vlabel else ''
        shots.append(
            '%s<div class="%s" data-screen="%s" data-view="%d" data-file="%s" '
            'data-rect="%s" data-dims="%d,%d" style="%s">'
            '<img src="figures/%s" alt="%s" style="%s"></div>'
            % (cap, cls, code, vi, html.escape(fn),
               "%g,%g,%g,%g" % (rx, ry, rw, rh), W, H, style,
               html.escape(fn), html.escape(f['name']), imgst))
    hidden = sorted(n for n in mk if n not in covered)
    if hidden:
        markers_skipped += len(hidden)
        warns.append("%s: markers outside every view, invisible: %s" % (code, hidden))
    markers_placed += len(covered)

    rowsh = []
    for n, label, imp in cs:
        cid = "%s-%d" % (code, n)
        dot = ('<button type="button" class="n" data-back="%s" title="Show it on the picture">%d</button>'
               % (cid, n)) if n in mk else '<span class="n off">&ndash;</span>'
        b2 = ('<a class="dbtn" href="manual-2.html#%s">In Depth</a>' % cid) if cid in M2IDS else ''
        b3 = ('<a class="dbtn weeds" href="manual-3.html#%s">In The Weeds</a>' % imp) \
             if imp != '-' and imp in M3IDS else ''
        if imp != '-' and imp not in M3IDS:
            warns.append("dangling IMP %s on %s" % (imp, cid))
        rowsh.append('<tr id="fig-%s"><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>'
                     % (cid, dot, md_inline(label, cid), b2, b3))

    unplaced = [n for n, _, _ in cs if n not in mk]
    note = ""
    if unplaced:
        def unplaced_link(n):
            cid = "%s-%d" % (code, n)
            txt = html.escape(snippet(LABEL[cid]))
            return ('<a href="manual-2.html#%s">%s</a>' % (cid, txt)) if cid in M2IDS else txt
        note = ('<p class="unplaced"><b>Not numbered on the picture:</b> %s &mdash; '
                'behaviours or rules rather than things you can point at.</p>'
                % ", ".join(unplaced_link(n) for n in unplaced))

    crumbs = []
    if f['parents']:
        crumbs.append('Part of ' + ", ".join(
            '<a href="#%s">%s</a>' % (p, html.escape(FIGS[p]['name'])) for p in f['parents']))
    if CHILDREN.get(code):
        crumbs.append('Related: ' + ", ".join(
            '<a href="#%s">%s</a>' % (c, html.escape(FIGS[c]['name'])) for c in CHILDREN[code]))
    crumbhtml = ('<p class="crumbs">%s</p>' % ' &nbsp;&middot;&nbsp; '.join(crumbs)) if crumbs else ''

    table = ('<table class="callouts"><thead><tr><th style="width:3rem">#</th><th>On screen</th>'
             '<th style="width:6.5rem"></th><th style="width:8rem"></th></tr></thead>'
             '<tbody>\n%s\n</tbody></table>' % "\n".join(rowsh)) if rowsh else ''

    parts.append("""
<section class="fig" id="{code}" data-code="{code}">
  <h2 class="chapter">{name}</h2>
  {crumbs}
  {blurb}
  <figure>
{shots}
  </figure>
  {note}
  {table}
</section>""".format(code=code, name=html.escape(f['name']), crumbs=crumbhtml,
                     blurb=('<p class="lede">%s</p>' % md_inline(blurb.get(code, ""), code))
                           if blurb.get(code) else "",
                     shots="\n".join(shots), note=note, table=table))

# ── sidebar ─────────────────────────────────────────────────────────────────
nav = []
for g in GROUPS:
    nav.append('<h4>%s</h4>' % html.escape(g))
    for code in sorted([c for c in ORDER if FIGS[c]['group'] == g],
                       key=lambda c: FIGS[c]['order']):
        nav.append('<a class="d%d" href="#%s" data-code="%s"><span>%s</span></a>'
                   % (min(depth(code), 2), code, code, html.escape(FIGS[code]['name'])))

doc = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BaySickDAW &middot; Manual 1 &middot; Visual Atlas</title>
<link rel="stylesheet" href="assets/manual.css">
<link rel="stylesheet" href="assets/atlas.css">
</head>
<body>

<header class="masthead">
  <div class="wrap">
    <h1><span class="brand">Manual 1</span> &middot; Visual Atlas</h1>
    <p>Every screen, with every control numbered. Point at a thing, get its number.</p>
    <nav class="manuals">
      <a href="index.html">Contents</a>
      <a class="here" href="manual-1.html">1 &middot; Visual Atlas</a>
      <a href="manual-2.html">2 &middot; Control Reference</a>
      <a href="manual-3.html">3 &middot; Under the Hood</a>
    </nav>
  </div>
</header>

<div class="wrap atlas">

<aside class="figtoc noprint">
  <div class="jump">
    <label for="jumpbox">Search the atlas</label>
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
  <button id="nudgetoggle">Enable marker nudging</button>
  <button id="croptoggle">Reshape crop boxes</button>
  <span id="nudgestate">off</span>
  <button id="nudgecopy" disabled>Copy positions</button>
  <span class="hint">Drag any dot; in reshape mode drag the bright box or its corners. One copy carries both.</span>
</div>

<script>
const COORDS = {coords};
const CROPS  = {crops};

(function () {
  function rectOf(shot) { return shot.dataset.rect.split(',').map(Number); }

  function placeMarkers() {
    document.querySelectorAll('.shot').forEach(function (shot) {
      shot.querySelectorAll('.marker').forEach(function (m) { m.remove(); });
      const code = shot.dataset.screen;
      const pts  = COORDS[code] || {};
      const r    = rectOf(shot);
      Object.keys(pts).forEach(function (n) {
        const x = pts[n][0], y = pts[n][1];
        if (x < r[0] - 0.01 || x > r[0] + r[2] + 0.01 ||
            y < r[1] - 0.01 || y > r[1] + r[3] + 0.01) return;
        const m = document.createElement('button');
        m.className = 'marker';
        m.type = 'button';
        m.textContent = n;
        m.dataset.n = n;
        m.style.left = ((x - r[0]) / r[2] * 100) + '%';
        m.style.top  = ((y - r[1]) / r[3] * 100) + '%';
        m.addEventListener('click', function (e) {
          if (document.body.classList.contains('nudging')) return;
          e.preventDefault();
          location.hash = 'fig-' + code + '-' + n;
          const row = document.getElementById('fig-' + code + '-' + n);
          if (row) { row.classList.add('flash'); setTimeout(function () { row.classList.remove('flash'); }, 1200); }
        });
        shot.appendChild(m);
      });
    });
  }
  placeMarkers();

  document.addEventListener('click', function (e) {
    var b = e.target.closest('button.n[data-back]');
    if (!b) return;
    var cid = b.dataset.back;
    var i    = cid.lastIndexOf('-');
    var code = cid.slice(0, i), num = cid.slice(i + 1);
    var shot = null, m = null;
    document.querySelectorAll('.shot[data-screen="' + code + '"]').forEach(function (s) {
      var cand = s.querySelector('.marker[data-n="' + num + '"]');
      if (cand && !m) { shot = s; m = cand; }
    });
    if (!shot) shot = document.querySelector('.shot[data-screen="' + code + '"]');
    if (!shot) return;
    shot.scrollIntoView({ block: 'center' });
    if (m) {
      m.classList.add('pulse');
      setTimeout(function () { m.classList.remove('pulse'); }, 1800);
    }
  });

  // ── authoring: marker nudge ──────────────────────────────────────────────
  let on = false, drag = null;
  const toggle = document.getElementById('nudgetoggle');
  const state  = document.getElementById('nudgestate');
  const copy   = document.getElementById('nudgecopy');

  toggle.addEventListener('click', function () {
    on = !on;
    document.body.classList.toggle('nudging', on);
    toggle.textContent = on ? 'Disable marker nudging' : 'Enable marker nudging';
    state.textContent = on ? 'ON - drag markers' : 'off';
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
    const mx = r[0] + vx / 100 * r[2], my = r[1] + vy / 100 * r[3];
    COORDS[s.dataset.screen][drag.m.dataset.n] = [ +mx.toFixed(2), +my.toFixed(2) ];
  });
  document.addEventListener('pointerup', function () { drag = null; });

  // ── authoring: crop reshape ──────────────────────────────────────────────
  // A cropped shot in reshape mode shows its WHOLE master dimmed, with the
  // live rect as a bright draggable box (corner handles resize).  Dots hide
  // while reshaping; they live in master coordinates, so they never move.
  let cropOn = false, cdrag = null;
  const ctoggle = document.getElementById('croptoggle');

  function applyCrop(shot) {
    const code = shot.dataset.screen, vi = +shot.dataset.view;
    const v = CROPS[code][vi], rect = v[1], W = v[3], H = v[4];
    if (!rect) return;
    shot.dataset.rect = rect.join(',');
    shot.style.width = (rect[2] / 100 * W).toFixed(0) + 'px';
    shot.style.aspectRatio = ((rect[2] / 100 * W) / (rect[3] / 100 * H)).toFixed(4);
    const img = shot.querySelector('img');
    img.style.width = (10000 / rect[2]) + '%';
    img.style.left  = -(rect[0] / rect[2] * 100) + '%';
    img.style.top   = -(rect[1] / rect[3] * 100) + '%';
  }

  function enterReshape(shot) {
    const code = shot.dataset.screen, vi = +shot.dataset.view;
    const v = CROPS[code][vi];
    if (!v || !v[1]) return;
    const W = v[3], H = v[4];
    shot.classList.add('reshaping');
    shot.style.width = W + 'px';
    shot.style.aspectRatio = (W / H).toFixed(4);
    const img = shot.querySelector('img');
    img.style.width = '100%'; img.style.left = '0'; img.style.top = '0';
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
    const v = CROPS[shot.dataset.screen][+shot.dataset.view], r = v[1];
    const box = shot.querySelector('.croprect');
    if (!box) return;
    box.style.left = r[0] + '%'; box.style.top = r[1] + '%';
    box.style.width = r[2] + '%'; box.style.height = r[3] + '%';
  }

  function exitReshape(shot) {
    shot.classList.remove('reshaping');
    const box = shot.querySelector('.croprect');
    if (box) box.remove();
    applyCrop(shot);
  }

  ctoggle.addEventListener('click', function () {
    cropOn = !cropOn;
    document.body.classList.toggle('cropmode', cropOn);
    ctoggle.textContent = cropOn ? 'Done reshaping' : 'Reshape crop boxes';
    state.textContent = cropOn ? 'ON - drag the bright boxes' : (on ? 'ON - drag markers' : 'off');
    copy.disabled = !(on || cropOn);
    document.querySelectorAll('.shot.cropview').forEach(cropOn ? enterReshape : exitReshape);
    if (!cropOn) placeMarkers();
  });

  document.addEventListener('pointerdown', function (e) {
    if (!cropOn) return;
    const box = e.target.closest('.croprect');
    if (!box) return;
    e.preventDefault();
    const shot = box.parentElement;
    const corner = e.target.dataset ? e.target.dataset.corner : null;
    const v = CROPS[shot.dataset.screen][+shot.dataset.view];
    cdrag = { shot: shot, corner: corner || null, start: v[1].slice(),
              sx: e.clientX, sy: e.clientY };
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
      if (r[2] < 2) r[2] = 2;
      if (r[3] < 1) r[3] = 1;
      r[0] = Math.max(0, Math.min(r[0], 100));
      r[1] = Math.max(0, Math.min(r[1], 100));
      if (r[0] + r[2] > 100) r[2] = 100 - r[0];
      if (r[1] + r[3] > 100) r[3] = 100 - r[1];
    }
    CROPS[cdrag.shot.dataset.screen][+cdrag.shot.dataset.view][1] =
        r.map(function (v) { return +v.toFixed(2); });
    syncBox(shot);
  });
  document.addEventListener('pointerup', function () { cdrag = null; });

  copy.addEventListener('click', function () {
    const crops = {};
    Object.keys(CROPS).forEach(function (k) {
      crops[k] = CROPS[k].map(function (v) { return [v[0], v[1], v[2]]; });
    });
    const out = JSON.stringify({ coords: COORDS, crops: crops }, null, 1);
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
doc = doc.replace("{coords}", json.dumps(coord_data, separators=(',', ':')))
doc = doc.replace("{crops}",  json.dumps(crop_data,  separators=(',', ':')))
open(OUT, "w", encoding="utf-8", newline="").write(doc)

total = sum(len(v) for v in call.values())
print("figures        :", len(ORDER))
print("callout rows   :", total)
print("markers visible:", markers_placed, " invisible-skipped:", markers_skipped)
print("In Depth btns  :", len([1 for c in call for n, _, _ in call[c] if "%s-%d" % (c, n) in M2IDS]))
for w in warns: print("WARN:", w)
