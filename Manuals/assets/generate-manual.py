# THE unified manual assembler - Phase G revision (2026-08-13).
# One document, Jeff's three groups, one section per figure, three cumulative
# depth levels (In View / In Depth / In The Weeds). Phase G rulings:
#   - frozen masthead; sidebar = fixed, independently scrolling pane
#   - sidebar is STATIC (never changes with level) and hierarchical per the
#     Sub-of tree, with collapsible nodes
#   - the body collapses at every level: group, figure, and each piece
#     (caption list, each close-up block, each mechanism topic); jumps
#     auto-expand whatever they land in
#   - shared mechanism topics live on their LEAD figure (synth panels carry
#     the synth engine's code); only true collections stay in group-end blocks
import re, os, sys, html, json, struct, importlib.util, datetime

ROOT = r"C:\Users\jeffm\Documents\BaySickDAW"
REG  = os.path.join(ROOT, "Plans & Specs", "System Reference", "Callout Registry.md")
MAN  = os.path.join(ROOT, "Manuals")
SRC2 = os.path.join(MAN, "src-m2")
SRC3 = os.path.join(MAN, "src-m3")
OUT  = os.path.join(MAN, "manual.html")
FIGD = os.path.join(MAN, "figures")

spec = importlib.util.spec_from_file_location(
    "mc", os.path.join(MAN, "assets", "marker-coords.py"))
mc = importlib.util.module_from_spec(spec); spec.loader.exec_module(mc)
C, CROPS = mc.C, mc.CROPS
M2CROPS = getattr(mc, "M2CROPS", {})
CLUSTER_DOTS = getattr(mc, "CLUSTER_DOTS", {})
CDOT_ROWS = {}
for _k, _d in CLUSTER_DOTS.items():
    CDOT_ROWS.setdefault(_k.split(":")[0], set()).update(_d.keys())

# QA-ManualPress M-3: generated Controls tables.  bsd-docs.json is written by
# `BaySickDAW.exe --shot --docs` - every number in the tables comes from the
# code, never from prose memory.  Blurbs are authored per control id in
# control-blurbs.py and merged here.
DOCS = {}
_docs_path = os.path.join(MAN, "assets", "bsd-docs.json")
if os.path.exists(_docs_path):
    DOCS = json.load(open(_docs_path, encoding="utf-8"))
BLURBS = {}
_cb_path = os.path.join(MAN, "assets", "control-blurbs.py")
if os.path.exists(_cb_path):
    _cb_spec = importlib.util.spec_from_file_location("cb", _cb_path)
    _cb = importlib.util.module_from_spec(_cb_spec)
    _cb_spec.loader.exec_module(_cb)
    BLURBS = getattr(_cb, "BLURBS", {})


def _ct_num(v):
    if v is None:
        return ""
    if isinstance(v, float) and v == int(v):
        return str(int(v))
    if isinstance(v, float):
        return ("%.3f" % v).rstrip("0").rstrip(".")
    return str(v)

def _ct_clean(s):
    # JUCE's default slider text renders 7 decimals; keep real formatted
    # strings ("1.2k", "20 Hz") and de-noise the bare numbers.
    if s is None:
        return ""
    s = str(s).strip()
    try:
        return _ct_num(float(s))
    except ValueError:
        return s

_CT_PREFIX = re.compile(r"^(mixer|swing)_[A-Za-z0-9_]+ ")

def _ct_name(name):
    return _CT_PREFIX.sub("", str(name)).strip()

UUID_ID = re.compile(r"^[A-Za-z0-9]+_[0-9a-f]{32}_(.+)$")
FIG_EFFECT = {"Effects Panel": "De-esser", "Effects Panel with Visual": "Reverb"}

def _effect_of(fig):
    if fig in FIG_EFFECT:
        return FIG_EFFECT[fig]
    if fig.startswith("FX Panel "):
        return fig[len("FX Panel "):].rsplit(" ", 1)[0]
    if fig.startswith("Pedal "):
        return fig[len("Pedal "):]
    return None

def _fx_key(fig, rid, label):
    eff = _effect_of(fig)
    if not eff:
        return None
    m = UUID_ID.match(rid or "")
    tail = m.group(1) if m else (label or "").strip().lower().replace(" ", "_")
    if not tail:
        return None
    return "fx|" + eff + "|" + tail

def controls_table_for_figure(*figs):
    return _controls_table_rows([f for f in figs])


# QA-ManualPress (Jeff, 2026-08-28): In View documents the effect panel frame
# once; In Depth documents EVERY effect in Basic and Advanced with its own
# generated control table.  Figures come from the harness ("FX Panel <name>
# <view>.png"); effects whose Advanced view is identical ship one image.
def effect_gallery():
    names = []
    for k in DOCS:
        if k.startswith("FX Panel ") and k.endswith(" Basic"):
            names.append(k[len("FX Panel "):-len(" Basic")])
    if not names:
        return ""
    out = ['<h4 class="ctrlhead">Every effect, panel by panel</h4>',
           '<p>Each effect below is the same frame with its own controls. '
           'Basic is what a slot opens with; Advanced adds the full set '
           '(three effects have no separate Advanced view).</p>']
    for nm in sorted(names):
        out.append('<section class="fxpanel"><h5>%s</h5>' % html.escape(nm))
        for view in ("Basic", "Advanced"):
            fig = "FX Panel %s %s" % (nm, view)
            png = os.path.join(FIGD, fig + ".png")
            if not os.path.exists(png):
                continue
            out.append('<figure><figcaption>%s</figcaption>'
                       '<img src="figures/%s.png" alt="%s" loading="lazy"></figure>'
                       % (view, html.escape(fig), html.escape(fig)))
        tbl = controls_table_for_figure("FX Panel %s Basic" % nm,
                                        "FX Panel %s Advanced" % nm)
        if tbl:
            out.append(tbl)
        out.append('</section>')
    return "\n".join(out)


# Jeff, 2026-08-28: the picker list says what can go in a slot; In Depth
# shows each pedal's real panel with its own generated control table.
def pedal_gallery():
    names = sorted(k[len("Pedal "):] for k in DOCS if k.startswith("Pedal "))
    if not names:
        return ""
    out = ['<h4 class="ctrlhead">Every pedal, panel by panel</h4>',
           '<p>Each pedal below is its own tile on the board, shot from the '
           'running app. Tuner and Graphic EQ are the two locked slots.</p>']
    for nm in names:
        fig = "Pedal " + nm
        png = os.path.join(FIGD, fig + ".png")
        if not os.path.exists(png):
            continue
        out.append('<section class="fxpanel"><h5>%s</h5>' % html.escape(nm))
        out.append('<figure><img src="figures/%s.png" alt="%s" loading="lazy"></figure>'
                   % (html.escape(fig), html.escape(fig)))
        tbl = controls_table_for_figure(fig)
        if tbl:
            out.append(tbl)
        out.append('</section>')
    return "\n".join(out)

def controls_table(code):
    return _controls_table_rows([os.path.splitext(os.path.basename(f))[0]
                                 for f in (FIGS[code].get('files') or [])])

def _controls_table_rows(figlist):
    rows_out, seen = [], set()
    for fig in figlist:
        for row in (DOCS.get(fig) or {}).get('controls', []):
            rid = row.get('id') or ''
            param = row.get('param') or {}
            name = _ct_name(param.get('name') or row.get('label')
                            or rid.replace('_', ' ').strip().title())
            if not name:
                continue
            key = rid or (fig + '|' + name)
            if key in seen:
                if rid:
                    continue          # same param shot in two figures
                n = 2                 # same-label knobs on sibling stages
                while (key + '#' + str(n)) in seen:
                    n += 1
                key = key + '#' + str(n)
            seen.add(key)
            fxk = _fx_key(fig, rid, row.get('label'))
            blurb = (BLURBS.get(fxk) or BLURBS.get(rid) or BLURBS.get(key) or '')
            choices = param.get('choices') or row.get('choices') or []
            if choices:
                rng = ", ".join(choices)
            elif row.get('kind') == 'toggle' or param.get('kind') == 'toggle':
                rng = "on / off"
            elif row.get('minText') is not None:
                rng = "%s to %s" % (row.get('minText'), row.get('maxText'))
            elif param:
                rng = "%s to %s" % (_ct_num(param.get('min')),
                                    _ct_num(param.get('max')))
            else:
                rng = ""
            default = _ct_clean(param.get('defaultText') or row.get('defText') or '')
            if choices:
                try:
                    di = int(float(default))
                    if 0 <= di < len(choices):
                        default = choices[di]
                except ValueError:
                    pass
            if row.get('minText') is not None and not choices:
                rng = "%s to %s" % (_ct_clean(row.get('minText')),
                                    _ct_clean(row.get('maxText')))
            if not fxk and (name, default, rng) in {(r[0], r[2], r[3]) for r in rows_out}:
                continue   # per-strip clones document once
            rows_out.append((name, blurb, default, rng))
    if not rows_out:
        return ""
    body = "\n".join(
        "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>"
        % tuple(html.escape(str(x)) for x in r) for r in rows_out)
    return ('<h4 class="ctrlhead">Controls</h4>\n'
            '<table class="controls"><thead><tr><th>Control</th>'
            '<th>What it does</th><th>Default</th><th>Range</th></tr></thead>'
            '<tbody>\n%s\n</tbody></table>\n' % body)

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
CHILDREN = {}
for code, f in FIGS.items():
    for p in f['parents']:
        CHILDREN.setdefault(p, []).append(code)
for p in CHILDREN:
    CHILDREN[p].sort(key=lambda c: (GROUPS.index(FIGS[c]['group']), FIGS[c]['order']))

call = {}
for code, n, label, imp in re.findall(
        r'^\| ([A-Z][A-Z\-]*)-(\d+) \| (.*?) \| \w+ \| ([A-Z\-0-9]+|-) \| .*? \|\s*$', reg, re.M):
    call.setdefault(code, []).append((int(n), label, imp))
for k in call: call[k].sort()

# QA-ManualPress M-4 option C (Jeff's ruling, 2026-08-28): control-figure
# dots anchor themselves too.  App components declare the callout they are
# the target of (kDotAnchor in SharedUI.h) and the harness dumps their live
# bounds; a declared callout's dot is placed from the real layout, and an
# undeclared one keeps its hand coordinate and is reported.  The dot sits at
# the anchor's left edge, vertically centred - the hand convention.
DOT_ANCHOR_GEN = 0
DOT_ANCHOR_TODO = []
for _code in sorted(set(list(C.keys()) + list(call.keys()))):
    _files = (FIGS.get(_code) or {}).get('files') or []
    if not _files:
        continue
    _fig = os.path.splitext(os.path.basename(_files[0]))[0]
    _anch = (DOCS.get(_fig) or {}).get('anchors') or []
    if not _anch:
        continue
    _by = {}
    for _a in _anch:
        _m = re.match(r'^([A-Z]+)-(\d+)$', str(_a.get('anchor') or '').strip())
        if _m and _m.group(1) == _code:
            _by[int(_m.group(2))] = _a['bounds']
    if not _by:
        continue
    _want = [t[0] for t in call.get(_code, [])] or sorted(C.get(_code, {}))
    _cur = dict(C.get(_code, {}))
    _hit = 0
    for _n in _want:
        _b = _by.get(_n)
        if not _b:
            continue
        # A declared anchor can still sit outside the shot: a scrolled rail,
        # a control hidden in this figure's state.  Skip and report - never
        # place a dot the reader cannot see.
        _cx = _b[0] + _b[2] / 2.0
        _cy = _b[1] + _b[3] / 2.0
        if not (0.0 <= _cx <= 100.0 and 0.0 <= _cy <= 100.0):
            continue
        # Placement follows the hand convention (Jeff, verified against his
        # coords by rendering them): a CONTROL takes its dot just below,
        # in the caption whitespace, so the knob face stays readable; a
        # large REGION takes its top edge, centred, so it labels the block
        # without covering what is inside it.
        if _b[3] <= 20.0:
            _dx, _dy = _b[0] + _b[2] / 2.0, _b[1] + _b[3] + 1.6
        else:
            _dx, _dy = _b[0] + _b[2] / 2.0, _b[1] + 2.6
        # keep the whole marker inside the crop, not just its centre
        _cur[_n] = (round(min(max(_dx, 1.4), 98.6), 2),
                    round(min(max(_dy, 2.0), 97.6), 2))
        _hit += 1
    if _hit:
        C[_code] = _cur
        DOT_ANCHOR_GEN += _hit
        _missing = [n for n in _want if n not in _by]
        if _missing:
            DOT_ANCHOR_TODO.append("%s: %d/%d anchored, hand coords kept for %s"
                                   % (_code, _hit, len(_want),
                                      ",".join(str(x) for x in _missing[:12])))

# QA-ManualPress M-4: menu-figure dots self-anchor.  Menu callouts number
# down the rows by convention, and the harness emits every captured menu's
# row rects into bsd-docs.json - so a conforming figure's dot map is
# rebuilt from the menu itself and tracks menu changes.  A figure whose
# callout count does not match its row count keeps its hand coordinates
# and is reported (generated-plus-exceptions).
MENU_DOT_REPORT = []
MENU_DOT_GENERATED = 0
for _code in list(C.keys()):
    _files = (FIGS.get(_code) or {}).get('files') or []
    if not _files:
        continue
    _fig = os.path.splitext(os.path.basename(_files[0]))[0]
    _rows = (DOCS.get(_fig) or {}).get('menurows')
    if not _rows:
        continue
    _want = len(call.get(_code, [])) or len(C.get(_code, {}))
    _withH = [r for r in _rows if not r.get('sep')]
    _noH = [r for r in _withH if not r.get('header')]
    _anchors = None
    for _cand in (_withH, _noH):
        if len(_cand) == _want:
            _anchors = _cand
            break
    if _anchors is None:
        MENU_DOT_REPORT.append(
            "%s (%s): %d callouts vs %d rows (%d excl headers) - hand coords kept"
            % (_code, _fig, _want, len(_withH), len(_noH)))
        continue
    _gen = {}
    for _i, _r in enumerate(_anchors):
        _d = _r['dot']
        _gen[_i + 1] = (round(_d[0], 2), round(_d[1], 2))
    C[_code] = _gen
    MENU_DOT_GENERATED += 1


blurb = {}
for m in re.finditer(r'^### `([A-Z\-]+)` - `[^`]+`\s*\n\s*\n(.*?)(?=\n\| Callout|\n### |\Z)',
                     reg, re.M | re.S):
    t = m.group(2).strip()
    blurb[m.group(1)] = "" if t.startswith("|") else " ".join(t.split())

TOPIC = {}
for m in re.finditer(r'^\| (IMP-\d+) \| (.*?) \| ', reg, re.M):
    TOPIC[m.group(1)] = re.split(r' - ', m.group(2).strip())[0].strip()
LABEL = {"%s-%d" % (c, n): lab for c, es in call.items() for n, lab, _ in es}

def snippet(label, maxlen=46):
    s = re.split(r' - ', label)[0]
    s = re.sub(r'[`*]', '', s).strip()
    return (s[:maxlen-1] + "\u2026") if len(s) > maxlen else s

# ── topic homes (G7: lead figures carry their mechanisms) ──────────────────
FIG_HOME = {
 "IMP-14": "EQ", "IMP-15": "EQ", "IMP-16": "EQ",
 "IMP-35": "BSNAM", "IMP-36": "BSNAM", "IMP-37": "BSNAM",
 "IMP-38": "BSV", "IMP-39": "BSPIT", "IMP-40": "BSA", "IMP-23": "BSPDL",
 "IMP-48": "ANLZ", "IMP-49": "ANLZ", "IMP-50": "VUMTR",
 "IMP-60": "MIX", "IMP-61": "MIXSM", "IMP-62": "FXI",
 "IMP-63": "EVT", "IMP-68": "EVT", "IMP-64": "PRMMNU", "IMP-65": "BLD",
 "IMP-66": "TRAN", "IMP-67": "PR", "IMP-69": "UNDO",
 "IMP-70": "FMENU", "IMP-73": "FMENU", "IMP-76": "EXP", "IMP-71": "BUNDLE",
 "IMP-77": "BSPLUG", "IMP-78": "BSPLUG", "IMP-79": "KEYS",
 "IMP-87": "BSHARM", "IMP-88": "BSP",
 "IMP-81": "BSSBOENV", "IMP-82": "BSSBFLT", "IMP-83": "BSSBLFO",
 "IMP-84": "BSSBMOD", "IMP-85": "BSSBOSC", "IMP-86": "BSSBOSC",
 "IMP-89": "BSRDMAIN",
}
END_BLOCKS = [
 ("Shell", "How the shell works underneath",
  ["IMP-57", "IMP-58", "IMP-59", "IMP-80", "IMP-56", "IMP-54", "IMP-55",
   "IMP-74", "IMP-75", "IMP-72"], "safety"),
 ("Instrument", "Pitch and time, under the hood",
  ["IMP-41", "IMP-42", "IMP-43", "IMP-44", "IMP-45", "IMP-46", "IMP-47"], None),
 ("Instrument", "The pedals, under the hood",
  ["IMP-%d" % n for n in range(17, 35) if n != 23], None),
 ("Mixing & Effects", "The effects, under the hood",
  ["IMP-%d" % n for n in range(1, 14)] + ["IMP-51", "IMP-52", "IMP-53"], None),
]
HOMED = set(FIG_HOME)
for _, _, ts, _ in END_BLOCKS: HOMED.update(ts)
assert not [t for t in TOPIC if t not in HOMED]

def frag(name):
    p = os.path.join(SRC3, name + ".html")
    return open(p, encoding="utf-8").read().strip() if os.path.exists(p) else ""

def topic_details(tid):
    # G12: no collapsible headers in the body - plain heading + content.
    return ('<div class="tp" id="%s"><h4 class="topichead">%s</h4>\n%s</div>'
            % (tid, html.escape(TOPIC.get(tid, tid)), frag(tid)))

def png_size(fn):
    with open(os.path.join(FIGD, fn), 'rb') as f:
        return struct.unpack('>II', f.read(33)[16:24])

warns = []
def md_inline(s, here=None):
    s = html.escape(s)
    s = re.sub(r'`([^`]+)`', r'<code>\1</code>', s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', s)
    s = re.sub(r'\*([^*]+)\*', r'<em>\1</em>', s)
    def seeid(m):
        cid = m.group(1)
        if cid in LABEL:
            return 'see <a href="#row-%s">%s</a>' % (cid, html.escape(snippet(LABEL[cid])))
        warns.append("dangling see-ref %s (%s)" % (cid, here))
        return 'see ' + html.escape(snippet(cid))
    s = re.sub(r'\bsee ([A-Z][A-Z\-]*-\d+)\b', seeid, s)
    def seefig(m):
        code = m.group(1)
        return ('see <a href="#%s">%s</a>' % (code, html.escape(FIGS[code]['name']))
                if code in FIGS else m.group(0))
    return re.sub(r'\bsee ([A-Z][A-Z\-]+|[A-Z]{2,})\b(?!-\d)', seefig, s)

coord_data, crop_data, m2crop_data = {}, {}, {}
markers_total = 0

def l1_views(code):
    global markers_total
    f = FIGS[code]
    mk = C.get(code, {})
    for n, (x, y) in mk.items():
        if not (0 <= x <= 100 and 0 <= y <= 100):
            raise SystemExit("OFF-CANVAS %s-%s" % (code, n))
    coord_data[code] = {str(n): [round(x, 2), round(y, 2)] for n, (x, y) in sorted(mk.items())}
    views = CROPS.get(code, [(f['files'][0], None, None)])
    crop_data[code] = []
    shots, covered = [], set()
    for vi, (fn, rect, vlabel) in enumerate(views):
        W, H = png_size(fn)
        rx, ry, rw, rh = rect if rect else (0, 0, 100, 100)
        crop_data[code].append([fn, rect and list(rect), vlabel, W, H])
        for n, (x, y) in mk.items():
            if rx - .01 <= x <= rx + rw + .01 and ry - .01 <= y <= ry + rh + .01:
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
        # width/height attributes reserve the image's space BEFORE it lazy-
        # loads: without them every load reflows the page, and a jump taken
        # mid-load lands short of its anchor.  Each label+shot pair is one
        # flex unit so narrow views (the two menus) sit side by side while
        # full-width views keep a row each.
        shots.append('<div class="vu">%s<div class="%s" data-screen="%s" data-view="%d" '
                     'data-file="%s" data-rect="%g,%g,%g,%g" data-dims="%d,%d" style="%s">'
                     '<img src="figures/%s" alt="%s" width="%d" height="%d" '
                     'style="%s" loading="lazy"></div></div>'
                     % (cap, cls, code, vi, html.escape(fn), rx, ry, rw, rh, W, H, style,
                        html.escape(fn), html.escape(f['name']), W, H, imgst))
    hidden = sorted(n for n in mk if n not in covered)
    if hidden: warns.append("%s markers invisible: %s" % (code, hidden))
    markers_total += len(covered)
    return '<div class="views">%s</div>' % "\n".join(shots)

def l1_table(code):
    # G10 (Jeff): no per-row In Depth / In The Weeds buttons - the global
    # level switch replaced them. Rows are number + label only.
    rows = []
    for n, label, imp in call.get(code, []):
        cid = "%s-%d" % (code, n)
        dot = ('<button type="button" class="n" data-back="%s" title="Show it on the picture">%d</button>'
               % (cid, n)) if (n in C.get(code, {}) or n in CDOT_ROWS.get(code, set())) \
              else '<span class="n off">&ndash;</span>'
        rows.append('<tr id="row-%s"><td>%s</td><td>%s</td></tr>'
                    % (cid, dot, md_inline(label, cid)))
    if not rows: return ""
    return ('<table class="callouts"><thead><tr><th style="width:3rem">#</th><th>On screen</th>'
            '</tr></thead><tbody>\n%s\n</tbody></table>' % "\n".join(rows))

def cluster_html(code, fn, rect, dots, label, key, src=None):
    W, H = png_size(fn)
    rx, ry, rw, rh = rect
    ar = (rw / 100.0 * W) / (rh / 100.0 * H)
    px = rw / 100.0 * W
    scale = min(2.0, 260.0 / px) if px < 260 else 1.0
    style = 'width:%.0fpx;aspect-ratio:%.4f' % (px * scale, ar)
    imgst = ('position:absolute;width:%.3f%%;left:-%.3f%%;top:-%.3f%%'
             % (10000.0 / rw, rx / rw * 100, ry / rh * 100))
    dh = []
    for n in dots:
        pt = (src if src is not None else C.get(code, {})).get(n)
        if pt is None:
            # CLUSTER_DOTS clusters list every row of the section; rows
            # without a hand-placed dot are simply undrawn, not an error.
            if src is None:
                warns.append("%s cluster dot %d missing" % (code, n))
            continue
        dh.append('<button type="button" class="marker"%s data-goto="%s-%d" '
                  'style="left:%.2f%%;top:%.2f%%">%d</button>'
                  % (' data-nonudge="1"' if src is not None else '',
                     code, n, (pt[0] - rx) / rw * 100, (pt[1] - ry) / rh * 100, n))
    return ('<div class="shot cropview m2c" data-screen="%s" data-m2key="%s" '
            'data-file="%s" data-rect="%g,%g,%g,%g" data-dims="%d,%d" style="%s">'
            '<img src="figures/%s" alt="" style="%s" loading="lazy">%s</div>'
            % (code, key, html.escape(fn), rx, ry, rw, rh, W, H, style,
               html.escape(fn), imgst, "".join(dh)))

CLUSTER_RE = re.compile(r'<figure data-cluster="([^|"]*)\|([\d,\s]*)"\s*/?>(?:</figure>)?')

def l2_chapter(code):
    p = os.path.join(SRC2, GSLUG[FIGS[code]['group']], code + ".html")
    if not os.path.exists(p):
        return '<p class="pending">This chapter is being written.</p>'
    src = open(p, encoding="utf-8").read()
    src = re.sub(r'<figure data-views="([A-Z\-]+)"\s*/?>(?:</figure>)?', '', src)
    pieces = CLUSTER_RE.split(src)
    # pieces = [intro, label1, dots1, body1, label2, dots2, body2, ...]
    out = [pieces[0]]
    idx = 0
    for i in range(1, len(pieces), 3):
        label, dots_s, body = pieces[i], pieces[i + 1], pieces[i + 2]
        nums = [int(x) for x in dots_s.split(",") if x.strip()]
        key = "%s:%d" % (code, idx); idx += 1
        fn = FIGS[code]['files'][0]
        # A cluster whose stored crop names a DIFFERENT master shows that
        # image instead (an open menu, an alternate view).  The figure's
        # dots live in the main master's space, so none are drawn there.
        custom = (key in M2CROPS and isinstance(M2CROPS[key], list)
                  and M2CROPS[key][0] and M2CROPS[key][0] != fn)
        if custom:
            fn = M2CROPS[key][0]
        pts = [C.get(code, {}).get(n) for n in nums]
        pts = [q for q in pts if q]
        if key in M2CROPS:
            rect = tuple(M2CROPS[key][1] if isinstance(M2CROPS[key], list) else M2CROPS[key])
        elif pts:
            xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
            x0, x1 = max(0.0, min(xs) - 4.5), min(100.0, max(xs) + 4.5)
            y0, y1 = max(0.0, min(ys) - 6.0), min(100.0, max(ys) + 6.0)
            if x1 - x0 < 14: cx = (x0 + x1) / 2; x0, x1 = max(0.0, cx - 7), min(100.0, cx + 7)
            if y1 - y0 < 10: cy = (y0 + y1) / 2; y0, y1 = max(0.0, cy - 5), min(100.0, cy + 5)
            rect = (round(x0, 2), round(y0, 2), round(x1 - x0, 2), round(y1 - y0, 2))
        else:
            rect = (0.0, 0.0, 100.0, 100.0)
        m2crop_data[key] = [fn, list(rect)]
        cdots = CLUSTER_DOTS.get(key)
        out.append('<div class="cl"><div class="viewlabel">%s</div>\n%s\n%s</div>'
                   % (html.escape(label) if label else "Close-up",
                      cluster_html(code, fn, rect,
                                   nums if (not custom or cdots) else [],
                                   label, key,
                                   src=cdots if custom else None), body))
    return "\n".join(out)

def l3_block(code):
    homed = sorted([t for t, h in FIG_HOME.items() if h == code], key=lambda t: int(t[4:]))
    shared = []
    for n, label, imp in call.get(code, []):
        if imp != '-' and imp not in homed and imp not in shared:
            shared.append(imp)
    parts = []
    if shared:
        links = ", ".join('<a data-level="weeds" href="#%s">%s</a>'
                          % (t, html.escape(TOPIC.get(t, t))) for t in shared)
        parts.append('<p class="howworks"><b>How this works:</b> %s.</p>' % links)
    for t in homed:
        parts.append(topic_details(t))
    return "\n".join(parts)

# ── static hierarchical nav (G4/G6): Jeff's tree, never changes ────────────
def nav_node(code, depth):
    kids = [c for c in CHILDREN.get(code, [])
            if FIGS[c]['group'] == FIGS[code]['group'] and FIGS[c]['parents'][:1] == [code]]
    kids.sort(key=lambda c: FIGS[c]['order'])
    tw = ('<button type="button" class="tw" aria-label="fold"></button>'
          if kids else '<span class="tw none"></span>')
    h = ['<div class="nv d%d">%s<a href="#%s">%s</a>' % (min(depth, 3), tw, code,
                                                         html.escape(FIGS[code]['name']))]
    if kids:
        h.append('<div class="kids">')
        for k in kids:
            h.append(nav_node(k, depth + 1))
        h.append('</div>')
    h.append('</div>')
    return "".join(h)

nav = []
for g in GROUPS:
    nav.append('<div class="nv grpnv"><button type="button" class="tw"></button>'
               '<a href="#g-%s" class="grplink">%s</a><div class="kids">'
               % (GSLUG[g], html.escape(g)))
    tops = [c for c in ORDER if FIGS[c]['group'] == g and
            (not FIGS[c]['parents'] or
             FIGS[FIGS[c]['parents'][0]]['group'] != g)]
    placed = set()
    def mark(code):
        placed.add(code)
        for k in CHILDREN.get(code, []):
            if FIGS[k]['group'] == g and FIGS[k]['parents'][:1] == [code]:
                mark(k)
    for t in sorted(tops, key=lambda c: FIGS[c]['order']):
        mark(t)
    orphans = [c for c in ORDER if FIGS[c]['group'] == g and c not in placed]
    for t in sorted(tops + orphans, key=lambda c: FIGS[c]['order']):
        if t in tops:
            nav.append(nav_node(t, 0))
        else:
            nav.append('<div class="nv d1"><span class="tw none"></span>'
                       '<a href="#%s">%s</a></div>' % (t, html.escape(FIGS[t]['name'])))
    nav.append('</div></div>')

# ── body ────────────────────────────────────────────────────────────────────
parts = []
for g in GROUPS:
    parts.append('<section class="grp"><h2 class="grpsum" id="g-%s">%s</h2>'
                 % (GSLUG[g], html.escape(g)))
    for code in sorted([c for c in ORDER if FIGS[c]['group'] == g],
                       key=lambda c: FIGS[c]['order']):
        f = FIGS[code]
        crumbs = []
        if f['parents']:
            crumbs.append('Part of ' + ", ".join('<a href="#%s">%s</a>'
                          % (p, html.escape(FIGS[p]['name'])) for p in f['parents']))
        if CHILDREN.get(code):
            crumbs.append('Related: ' + ", ".join('<a href="#%s">%s</a>'
                          % (c, html.escape(FIGS[c]['name'])) for c in CHILDREN[code]))
        ch = ('<p class="crumbs">%s</p>' % ' &nbsp;&middot;&nbsp; '.join(crumbs)) if crumbs else ''
        bl = ('<p class="lede">%s</p>' % md_inline(blurb.get(code, ""), code)) if blurb.get(code) else ''
        l3 = l3_block(code)
        parts.append("""
<section class="figd" id="{code}" data-code="{code}">
  <h3 class="figsum">{name}</h3>
  {crumbs}
  <div class="l1">{blurb}
  <figure>{views}</figure>
  {table}</div>
  <div class="l2">{controls}{chapter}</div>
  {l3}
</section>""".format(code=code, name=html.escape(f['name']), crumbs=ch, blurb=bl,
                     views=l1_views(code), table=l1_table(code),
                     controls=controls_table(code)
                              + (effect_gallery() if code == 'FX' else '')
                              + (pedal_gallery() if code == 'BSPDLP' else ''),
                     chapter=l2_chapter(code),
                     l3=('<div class="l3">%s</div>' % l3) if l3 else ''))
    bi = 0
    for eg, title, topics, extra in END_BLOCKS:
        if eg != g: continue
        bid = "uth-%s-%d" % (GSLUG[g], bi); bi += 1
        body = "\n".join(topic_details(t) for t in topics)
        if extra:
            body += ('\n<div class="tp" id="safe-inputs">'
                     '<h4 class="topichead">Reading files that came from somewhere else</h4>\n%s</div>'
                     % frag(extra))
        parts.append('<section class="figd l3only" id="%s">'
                     '<h3 class="figsum">%s</h3><div class="l3">%s</div></section>'
                     % (bid, html.escape(title), body))
    parts.append('</section>')

doc_head = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BaySickDAW Manual</title>
<link rel="stylesheet" href="assets/manual.css">
<link rel="stylesheet" href="assets/atlas.css">
<style>
.l2, .l3, section.l3only { display: none; }
body.level-depth .l2 { display: block; }
body.level-weeds .l2, body.level-weeds .l3, body.level-weeds section.l3only { display: block; }

/* G11: header flush with the window top, exactly one title row tall, and it
   spans ONLY the content column - the sidebar owns the full left edge. */
.masthead { position: sticky; top: 0; z-index: 60; margin-left: 256px;
  padding: 0 !important; min-height: 0 !important;
  border-bottom: 1px solid var(--line); }
.masthead .wrap { display: flex; align-items: center; gap: 20px;
  padding: 5px 14px !important; margin: 0 !important; max-width: none !important; }
.masthead h1 { margin: 0 !important; padding: 0 !important;
  font-size: 1.15rem !important; line-height: 1.2 !important; white-space: nowrap; }
.masthead p { display: none; }
.levelbar { display: flex; gap: 8px; align-items: center; margin: 0; }
.levelbar button { padding: 4px 13px; border: 1px solid var(--line); border-radius: 16px;
  background: var(--panel-2); color: var(--text); cursor: pointer; font-size: 13px; }
.levelbar button.on { border-color: var(--accent); color: var(--accent); font-weight: 600; }
.levelbar .lhint { font-size: 11.5px; color: var(--text-dim); }

.figtoc { position: fixed; left: 0; top: 0; bottom: 0; width: 256px;
  overflow-y: auto; border-right: 1px solid var(--line); border-radius: 0;
  background: var(--panel); padding: 10px 10px 60px; font-size: 13px; z-index: 70; }
/* The content column is pinned at its FULLSCREEN width - measured from
   the actual screen at load (--fsw, set in JS; display scaling means it
   cannot be hardcoded).  Shrinking the window then never rescales the
   screenshots, and scroll stays VERTICAL ONLY (Jeff): a smaller window
   clips the right edge; a scroll clamp keeps the left edge pinned so
   nothing slides under the reference bar. */
html, body { overflow-x: clip; }
main { margin-left: 272px; width: calc(var(--fsw, 100vw) - 289px); }
.views { display: flex; flex-wrap: wrap; gap: 14px; align-items: flex-start; }
.views .vu { min-width: 0; }
/* One 1.3 MB page: offscreen sections opt out of layout/paint so a
   details toggle reflows its own section, not the whole document (Jeff
   reported whole-manual lag opening code dropdowns). Chromium renders
   contained sections on demand for scroll, anchor jumps and find-in-page. */
section.figd { content-visibility: auto; contain-intrinsic-size: auto 900px; }
details.fullfn > pre { content-visibility: auto; contain-intrinsic-size: auto 600px; }
.tp { content-visibility: auto; contain-intrinsic-size: auto 500px; }
/* Text and tables wrap to the LIVE window with a right gutter mirroring the
   left (Jeff); only the figure strip keeps the pinned fullscreen width -
   screenshots never rescale, per the vertical-only-scroll ruling above. */
main p, main table, main pre, main h3, main h4, main h5,
main .lede, main .crumbs, main details.fullfn, main .tp
{ max-width: calc(100vw - 344px); }
@media print {
  .masthead, .figtoc, .nudge { display: none !important; }
  main { margin-left: 0 !important; width: auto !important; min-width: 0 !important; }
  html, body { overflow-x: visible; }
  main p, main table, main pre, main h3, main h4, main h5,
  main .lede, main .crumbs, main details.fullfn, main .tp
  { max-width: none; }
  section.figd, details.fullfn > pre, .tp { content-visibility: visible; }
  section.figd { break-before: page; }
}
.wrap.atlas { display: block; max-width: none; padding-right: 22px; }
/* Authoring bar renders only when the page is ASKED for it (?nudge=1).
   The Debug build's F1 asks; Release, the installed copy and a plain
   browser open never do (Jeff, 2026-08-16).  Gate class lives on <html>
   because setLevel REPLACES body.className wholesale. */
.nudge { left: 256px; display: none; }
html.nudge-on .nudge { display: flex; }

.nv { line-height: 1.15; }
/* Long names must wrap INSIDE the anchor box, beside the fold arrow -
   an uncapped inline-block drops below the arrow flush left instead. */
.nv > a { display: inline-block; max-width: calc(100% - 20px);
  vertical-align: top; padding: 3px 4px; border-radius: 3px;
  color: var(--text); text-decoration: none; }
.nv > a:hover { background: var(--panel-2); }
.nv .kids { margin-left: 14px; }
.nv.closed > .kids { display: none; }
.tw { width: 14px; height: 14px; margin-right: 1px; border: 0; background: none;
  vertical-align: top; margin-top: 3px;
  color: var(--text-dim); cursor: pointer; padding: 0; font-size: 10px; }
.tw::before { content: '\\25BE'; }
.nv.closed > .tw::before { content: '\\25B8'; }
.tw.none { display: inline-block; cursor: default; }
.tw.none::before { content: ''; }
.grpnv > a.grplink { font-size: 12px; letter-spacing: .08em; text-transform: uppercase;
  color: var(--text-dim); font-weight: 700; }

h2.grpsum { font-size: 1.5rem; font-weight: 700; margin: 26px 0 8px; color: var(--text); }
section.figd { scroll-margin-top: 42px; margin: 0 0 30px; }
h3.figsum { font-size: 1.25rem; font-weight: 650;
  border-bottom: 1px solid var(--line); padding: 6px 0; margin: 0 0 6px; }
.cl { margin: 14px 0; }
.tp { margin: 16px 0; }
h4.topichead { margin: 0 0 8px; color: var(--accent-2); }

.topic { margin: 18px 0; }
p.howworks { font-size: 13px; color: var(--text-dim); }
.m2c { margin: 10px 0 6px; }
.l2 p:target, .l2 p.flash { background: rgba(0,229,255,.12); border-radius: 4px; }
div.codeblock { margin: 14px 0; }
div.codehead { font: 11px ui-monospace, Consolas, monospace; color: var(--text-dim);
  padding: 4px 10px; border: 1px solid var(--line); border-bottom: 0;
  border-radius: 6px 6px 0 0; background: var(--panel); }
div.codeblock pre, details.fullfn pre, pre.formulas { margin: 0; padding: 10px 12px;
  border: 1px solid var(--line); border-radius: 0 0 6px 6px; background: var(--code-bg);
  overflow-x: auto; font: 12px/1.5 ui-monospace, Consolas, monospace; color: var(--text); }
details.fullfn { margin: 8px 0 14px; }
details.fullfn summary { cursor: pointer; font-size: 12.5px; color: var(--accent); padding: 4px 0; }
details.fullfn pre { border-radius: 6px; margin-top: 6px; }
pre.formulas { border-radius: 6px; margin: 10px 0 14px; white-space: pre-wrap; }
p.behind { font-size: 12.5px; color: var(--text-dim); }
a.dbtn { display: inline-block; padding: 2px 9px; border: 1px solid var(--accent);
  border-radius: 11px; color: var(--accent); font-size: 11px; text-decoration: none;
  white-space: nowrap; }
a.dbtn:hover { background: var(--accent); color: #001318; }
a.dbtn.weeds { border-color: var(--accent-2); color: var(--accent-2); }
a.dbtn.weeds:hover { background: var(--accent-2); color: #00201f; }

@media print {
  .figtoc, .nudge, .levelbar { display: none; }
  .masthead { position: static; }
  main { margin-left: 0; }
}
</style>
</head>
<body class="level-view">

<header class="masthead">
  <div class="wrap">
    <h1><span class="brand">BaySickDAW</span> Manual</h1>
    <div class="levelbar noprint">
      <button id="lv-view" class="on">In View</button>
      <button id="lv-depth">In Depth</button>
      <button id="lv-weeds">In The Weeds</button>
      <span class="lhint" id="lhint">Pictures and pointers only.</span>
      <span class="lhint" style="margin-left:auto">built {stamp}</span>
    </div>
  </div>
</header>

<aside class="figtoc noprint">
  <div class="jump">
    <label for="jumpbox">Search the manual</label>
    <input id="jumpbox" type="text" placeholder="control or screen name" autocomplete="off">
    <div id="jumphint"></div>
  </div>
{nav}
</aside>

<div class="wrap atlas">
<main>
{parts}
</main>
</div>

<div class="nudge noprint" id="nudgebar">
  <button id="nudgetoggle">Enable dot nudging</button>
  <button id="croptoggle">Reshape boxes</button>
  <span id="nudgestate">off</span>
  <button id="nudgecopy" disabled>Copy positions</button>
  <span class="hint">One copy carries dots, view boxes and close-up boxes.</span>
</div>
"""

doc_js = """
<script>
const COORDS  = {coords};
const CROPS   = {crops};
const M2CROPS = {m2crops};

(function () {
  // Pin the content column at the screen's own width (display scaling makes
  // this unknowable from CSS).  Sideways motion is prevented by
  // overflow-x: clip in the CSS - unlike hidden, clip forbids programmatic
  // scrolling too, so nothing can slide under the reference bar and no
  // scroll-clamping script is needed (a clamp would cancel the smooth
  // scroll of every jump a few frames in).
  document.documentElement.style.setProperty('--fsw',
    (window.screen.width || window.innerWidth) + 'px');

  const HINTS = { view: 'Pictures and pointers only.',
                  depth: 'Adds the full teaching for every control.',
                  weeds: 'Adds the code and the math behind everything.' };
  function setLevel(l) {
    document.body.className = 'level-' + l;
    ['view','depth','weeds'].forEach(function (k) {
      document.getElementById('lv-' + k).classList.toggle('on', k === l);
    });
    document.getElementById('lhint').textContent = HINTS[l];
    try { localStorage.setItem('bsdaw-manual-level', l); } catch (e) {}
  }
  ['view','depth','weeds'].forEach(function (k) {
    document.getElementById('lv-' + k).addEventListener('click', function () { setLevel(k); });
  });
  let saved = 'view';
  try { saved = localStorage.getItem('bsdaw-manual-level') || 'view'; } catch (e) {}
  // ?level=view|depth|weeds overrides the saved level (the PDF builds use
  // it); ?print=1 eager-loads every image so print capture sees them all.
  const q = new URLSearchParams(location.search);
  if (HINTS[q.get('level')]) saved = q.get('level');
  setLevel(saved);
  if (q.has('print'))
    document.querySelectorAll('img[loading]').forEach(function (im) {
      im.loading = 'eager';
    });
  if (q.get('nudge') === '1')
    document.documentElement.classList.add('nudge-on');

  const RANK = { view: 0, depth: 1, weeds: 2 };
  function levelOf() { return document.body.className.replace('level-', ''); }
  function expandTo(el) {
    for (let d = el.closest('details'); d; d = d.parentElement && d.parentElement.closest('details'))
      d.open = true;
  }
  function ensureVisible(el) {
    let need = 'view';
    if (el.closest('.l3') || el.closest('section.l3only')) need = 'weeds';
    else if (el.closest('.l2')) need = 'depth';
    if (RANK[need] > RANK[levelOf()]) setLevel(need);
    expandTo(el);
  }
  window.__revealLevel = ensureVisible;
  // A jump can change the level and open collapsed blocks, and offscreen
  // sections are content-visibility:auto - so at click time the target's
  // height is still an ESTIMATE and a single scroll lands short (the old
  // "click it twice" bug).  Scroll, let layout settle, scroll again.
  function scrollToStable(el, block) {
    const go = function () { el.scrollIntoView({ block: block || 'start' }); };
    go();
    requestAnimationFrame(function () {
      requestAnimationFrame(function () { go(); setTimeout(go, 60); });
    });
  }

  document.addEventListener('click', function (e) {
    const a = e.target.closest('a[href^="#"]');
    if (!a) return;
    const t = document.getElementById(a.getAttribute('href').slice(1));
    if (!t) return;
    e.preventDefault();
    if (a.dataset.level && RANK[a.dataset.level] > RANK[levelOf()]) setLevel(a.dataset.level);
    ensureVisible(t);
    scrollToStable(t, a.dataset.level ? 'center' : 'start');
    t.classList.add('flash');
    setTimeout(function () { t.classList.remove('flash'); }, 1400);
    history.replaceState(null, '', a.getAttribute('href'));
  });
  if (location.hash) {
    const t = document.getElementById(location.hash.slice(1));
    if (t) { ensureVisible(t); scrollToStable(t, 'start'); }
  }

  // sidebar fold toggles
  document.addEventListener('click', function (e) {
    const tw = e.target.closest('.tw');
    if (!tw || tw.classList.contains('none')) return;
    tw.parentElement.classList.toggle('closed');
  });

  function rectOf(shot) { return shot.dataset.rect.split(',').map(Number); }

  function placeMarkers() {
    document.querySelectorAll('.l1 .shot').forEach(function (shot) {
      shot.querySelectorAll('.marker').forEach(function (m) { m.remove(); });
      const code = shot.dataset.screen, pts = COORDS[code] || {}, r = rectOf(shot);
      Object.keys(pts).forEach(function (n) {
        const x = pts[n][0], y = pts[n][1];
        if (x < r[0]-.01 || x > r[0]+r[2]+.01 || y < r[1]-.01 || y > r[1]+r[3]+.01) return;
        const m = document.createElement('button');
        m.className = 'marker'; m.type = 'button'; m.textContent = n; m.dataset.n = n;
        m.style.left = ((x - r[0]) / r[2] * 100) + '%';
        m.style.top  = ((y - r[1]) / r[3] * 100) + '%';
        m.addEventListener('click', function (e) {
          if (document.body.classList.contains('nudging')) return;
          e.preventDefault();
          const row = document.getElementById('row-' + code + '-' + n);
          if (row) {
            expandTo(row);
            scrollToStable(row, 'center');
            row.classList.add('flash');
            setTimeout(function () { row.classList.remove('flash'); }, 1200);
          }
        });
        shot.appendChild(m);
      });
    });
  }
  placeMarkers();

  document.addEventListener('click', function (e) {
    var b = e.target.closest('button.n[data-back]');
    if (!b) return;
    var cid = b.dataset.back, i = cid.lastIndexOf('-');
    var code = cid.slice(0, i), num = cid.slice(i + 1);
    var m = null, shot = null;
    document.querySelectorAll('.l1 .shot[data-screen="' + code + '"]').forEach(function (s) {
      var c = s.querySelector('.marker[data-n="' + num + '"]');
      if (c && !m) { m = c; shot = s; }
    });
    if (!shot) {
      // Rows whose only dot lives on a custom-file close-up (CLUSTER_DOTS):
      // the close-up sits in the In Depth layer, so raise the level first.
      m = document.querySelector('.m2c .marker[data-goto="' + cid + '"]');
      if (!m) return;
      if (document.body.classList.contains('level-view')) setLevel('depth');
      shot = m.parentElement;
    }
    expandTo(shot);
    scrollToStable(shot, 'center');
    if (m) { m.classList.add('pulse'); setTimeout(function () { m.classList.remove('pulse'); }, 1800); }
  });

  document.addEventListener('click', function (e) {
    var b = e.target.closest('.marker[data-goto]');
    if (!b || document.body.classList.contains('nudging')) return;
    var p = document.getElementById(b.dataset.goto);
    if (!p) return;
    ensureVisible(p);
    p.scrollIntoView({ block: 'center' });
    p.classList.add('flash');
    setTimeout(function () { p.classList.remove('flash'); }, 1400);
  });

  // ── authoring ────────────────────────────────────────────────────────────
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

  function keyOfMarker(m, shot) {
    if (m.dataset.goto) {
      const i = m.dataset.goto.lastIndexOf('-');
      return [m.dataset.goto.slice(0, i), m.dataset.goto.slice(i + 1)];
    }
    return [shot.dataset.screen, m.dataset.n];
  }
  document.addEventListener('pointerdown', function (e) {
    if (!on) return;
    const m = e.target.closest('.marker');
    if (!m || m.dataset.nonudge) return;
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
    const kn = keyOfMarker(drag.m, s);
    if (!COORDS[kn[0]]) COORDS[kn[0]] = {};
    COORDS[kn[0]][kn[1]] = [ +(r[0] + vx / 100 * r[2]).toFixed(2),
                             +(r[1] + vy / 100 * r[3]).toFixed(2) ];
    syncCounterparts(kn, drag.m);
  });
  document.addEventListener('pointerup', function () { drag = null; });

  function syncCounterparts(kn, skip) {
    const pt = (COORDS[kn[0]] || {})[kn[1]];
    if (!pt) return;
    document.querySelectorAll('.shot').forEach(function (s) {
      if (s.classList.contains('reshaping')) return;
      s.querySelectorAll('.marker').forEach(function (m) {
        if (m === skip) return;
        const k2 = keyOfMarker(m, s);
        if (k2[0] !== kn[0] || String(k2[1]) !== String(kn[1])) return;
        const r = rectOf(s);
        m.style.left = ((pt[0] - r[0]) / r[2] * 100).toFixed(2) + '%';
        m.style.top  = ((pt[1] - r[1]) / r[3] * 100).toFixed(2) + '%';
      });
    });
  }

  function applyRect(shot, rect) {
    const dims = shot.dataset.dims.split(',').map(Number);
    shot.dataset.rect = rect.join(',');
    const px = rect[2] / 100 * dims[0];
    const scale = (shot.classList.contains('m2c') && px < 260) ? Math.min(2, 260 / px) : 1;
    shot.style.width = (px * scale).toFixed(0) + 'px';
    shot.style.aspectRatio = ((rect[2] / 100 * dims[0]) / (rect[3] / 100 * dims[1])).toFixed(4);
    const img = shot.querySelector('img');
    img.style.width = (10000 / rect[2]) + '%';
    img.style.left  = -(rect[0] / rect[2] * 100) + '%';
    img.style.top   = -(rect[1] / rect[3] * 100) + '%';
  }
  function storeOf(shot) {
    if (shot.dataset.m2key) {
      if (!M2CROPS[shot.dataset.m2key])
        M2CROPS[shot.dataset.m2key] = [shot.dataset.file, rectOf(shot)];
      return M2CROPS[shot.dataset.m2key];
    }
    return CROPS[shot.dataset.screen][+shot.dataset.view];
  }
  function enterReshape(shot) {
    const st = storeOf(shot);
    if (!st || !st[1]) return;
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
    const r = storeOf(shot)[1];
    const box = shot.querySelector('.croprect');
    if (!box) return;
    box.style.left = r[0] + '%'; box.style.top = r[1] + '%';
    box.style.width = r[2] + '%'; box.style.height = r[3] + '%';
  }
  function exitReshape(shot) {
    shot.classList.remove('reshaping');
    const box = shot.querySelector('.croprect');
    if (box) box.remove();
    const rect = storeOf(shot)[1];
    applyRect(shot, rect);
    shot.querySelectorAll('.marker').forEach(function (m) {
      m.style.display = '';
      const kn = keyOfMarker(m, shot);
      const pt = (COORDS[kn[0]] || {})[kn[1]];
      if (pt) {
        m.style.left = ((pt[0] - rect[0]) / rect[2] * 100).toFixed(2) + '%';
        m.style.top  = ((pt[1] - rect[1]) / rect[3] * 100).toFixed(2) + '%';
      }
    });
    if (!shot.classList.contains('m2c')) placeMarkers();
  }
  ctoggle.addEventListener('click', function () {
    cropOn = !cropOn;
    document.body.classList.toggle('cropmode', cropOn);
    ctoggle.textContent = cropOn ? 'Done reshaping' : 'Reshape boxes';
    state.textContent = cropOn ? 'ON - drag the bright boxes' : (on ? 'ON - drag dots' : 'off');
    copy.disabled = !(on || cropOn);
    document.querySelectorAll('.shot.cropview').forEach(cropOn ? enterReshape : exitReshape);
  });
  document.addEventListener('pointerdown', function (e) {
    if (!cropOn) return;
    const box = e.target.closest('.croprect');
    if (!box) return;
    e.preventDefault();
    const shot = box.parentElement;
    cdrag = { shot: shot, corner: e.target.dataset ? e.target.dataset.corner : null,
              start: storeOf(shot)[1].slice(), sx: e.clientX, sy: e.clientY };
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
      if (r[3] < 1) r[3] = 1;
      r[0] = Math.max(0, Math.min(r[0], 100));
      r[1] = Math.max(0, Math.min(r[1], 100));
      if (r[0] + r[2] > 100) r[2] = 100 - r[0];
      if (r[1] + r[3] > 100) r[3] = 100 - r[1];
    }
    storeOf(shot)[1] = r.map(function (v) { return +v.toFixed(2); });
    syncBox(shot);
  });
  document.addEventListener('pointerup', function () { cdrag = null; });

  copy.addEventListener('click', function () {
    const crops = {};
    Object.keys(CROPS).forEach(function (k) {
      crops[k] = CROPS[k].map(function (v) { return [v[0], v[1], v[2]]; });
    });
    const out = JSON.stringify({ coords: COORDS, crops: crops, m2crops: M2CROPS }, null, 1);
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

doc = doc_head.replace("{nav}", "\n".join(nav)).replace("{parts}", "\n".join(parts)) + doc_js
doc = doc.replace("{stamp}", datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
doc = doc.replace("{coords}", json.dumps(coord_data, separators=(',', ':')))
doc = doc.replace("{crops}",  json.dumps(crop_data,  separators=(',', ':')))
doc = doc.replace("{m2crops}", json.dumps(m2crop_data, separators=(',', ':')))
open(OUT, "w", encoding="utf-8", newline="").write(doc)

print("figures        :", len(ORDER))
print("markers        :", markers_total)
print("clusters       :", len(m2crop_data))
print("ctrl dots gen  : %d dots" % DOT_ANCHOR_GEN)
for _t in DOT_ANCHOR_TODO:
    print("  DOT TODO " + _t)
print("menu dots gen  : %d figures" % MENU_DOT_GENERATED)
for _m in MENU_DOT_REPORT:
    print("  DOT MISMATCH " + _m)
print("topics placed  :", len(re.findall(r'class="tp" id="IMP-', doc)), "of", len(TOPIC))
print("size           : %.1f MB" % (len(doc) / 1e6))
for w in warns: print("WARN:", w)
