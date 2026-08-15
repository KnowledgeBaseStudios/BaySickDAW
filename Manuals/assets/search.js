/* Shared search for all three manuals.
 *
 * The index is built from the PAGE ITSELF at load, not from a side file, so it
 * can never drift out of step with what is written.  An exact id still jumps
 * straight there; anything else is treated as keywords.
 */
(function () {
  var box = document.getElementById('jumpbox');
  if (!box) return;

  var hint  = document.getElementById('jumphint');
  var panel = document.createElement('div');
  panel.className = 'results';
  panel.id = 'searchresults';
  box.parentNode.appendChild(panel);

  // ── Build the index ───────────────────────────────────────────────────────
  var items = [];

  function chapterOf(el) {
    var n = el;
    while (n) {
      var p = n.previousElementSibling;
      while (p) {
        if (p.tagName === 'H2') return p.textContent.trim();
        p = p.previousElementSibling;
      }
      n = n.parentElement;
    }
    return '';
  }

  // Manual 1 - the figures themselves, by display name (codes never shown,
  // still searchable), then one row per callout in each caption table.
  document.querySelectorAll('section.fig[id]').forEach(function (fig) {
    var t = fig.querySelector('h2').textContent.trim();
    items.push({
      el: fig, id: fig.id, title: t, sub: '',
      ctx: '',
      hay: (fig.id + ' ' + t).toLowerCase()
    });
  });
  document.querySelectorAll('table.callouts tr[id^="fig-"]').forEach(function (tr) {
    var id  = tr.id.slice(4);
    var lab = tr.cells.length > 1 ? tr.cells[1].textContent.trim() : '';
    var fig = tr.closest('section.fig');
    items.push({
      el: tr, id: id, title: lab || id, sub: '',
      ctx: fig ? fig.querySelector('h2').textContent.trim() : '',
      hay: (id + ' ' + lab).toLowerCase()
    });
  });

  // Manual 2 - one entry per control.
  document.querySelectorAll('.ctrl[id]').forEach(function (d) {
    var name  = d.querySelector('.name');
    var where = d.querySelector('.where');
    var body  = [];
    d.querySelectorAll('p, td, th, b').forEach(function (n) { body.push(n.textContent); });
    name  = name  ? name.textContent.trim()  : '';
    where = where ? where.textContent.trim() : '';
    items.push({
      el: d, id: d.id, title: name || d.id, sub: where,
      ctx: chapterOf(d),
      hay: (d.id + ' ' + name + ' ' + where + ' ' + body.join(' ')).toLowerCase()
    });
  });

  // Manual 3 - one entry per headed topic, text gathered to the next heading.
  document.querySelectorAll('h3.section[id], h4.sub[id]').forEach(function (h) {
    if (h.closest('section.fig')) return;          // Manual 1 handles its own
    var body = [], n = h.nextElementSibling;
    while (n && !/^H[234]$/.test(n.tagName)) { body.push(n.textContent); n = n.nextElementSibling; }
    var t = h.textContent.trim();
    items.push({
      el: h, id: h.id, title: t, sub: '',
      ctx: chapterOf(h),
      hay: (h.id + ' ' + t + ' ' + body.join(' ')).toLowerCase()
    });
  });

  var ID_RE = /^[A-Z][A-Z\-]*-\d+$/;

  function land(el, id) {
    if (!el) return;
    if (window.__revealLevel) window.__revealLevel(el);
    el.scrollIntoView({ block: 'center' });
    el.classList.add('flash');
    setTimeout(function () { el.classList.remove('flash'); }, 1500);
    if (id) history.replaceState(null, '', '#' + id);
  }

  function esc(s) { return s.replace(/[&<>]/g, function (c) {
    return c === '&' ? '&amp;' : c === '<' ? '&lt;' : '&gt;'; }); }

  function mark(text, terms) {
    var out = esc(text);
    terms.forEach(function (t) {
      if (!t) return;
      out = out.replace(new RegExp('(' + t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'ig'),
                        '<mark>$1</mark>');
    });
    return out;
  }

  var results = [], sel = -1;

  function render(list, terms) {
    results = list; sel = -1;
    if (!list.length) { panel.classList.remove('open'); panel.innerHTML = ''; return; }
    panel.innerHTML = list.map(function (it, i) {
      return '<button type="button" data-i="' + i + '">' +
             '<span class="t">' + mark(it.title, terms) + '</span>' +
             (it.sub ? '<span class="s">' + mark(it.sub, terms) + '</span>' : '') +
             (it.ctx ? '<span class="c">' + esc(it.ctx) + '</span>' : '') +
             '</button>';
    }).join('');
    panel.classList.add('open');
  }

  function search(q) {
    var terms = q.toLowerCase().split(/\s+/).filter(Boolean);
    if (!terms.length) { render([], []); hint.textContent = ''; return; }

    var scored = [];
    items.forEach(function (it) {
      var ok = true, score = 0;
      for (var i = 0; i < terms.length; i++) {
        var at = it.hay.indexOf(terms[i]);
        if (at < 0) { ok = false; break; }
        // Earlier hits are almost always in the id or the name, so weight them.
        score += at < 60 ? 12 : 1;
      }
      if (ok) scored.push([score, it]);
    });
    scored.sort(function (a, b) { return b[0] - a[0]; });

    hint.textContent = scored.length
      ? scored.length + ' match' + (scored.length === 1 ? '' : 'es')
      : 'Nothing found.';
    render(scored.slice(0, 25).map(function (p) { return p[1]; }), terms);
  }

  var timer = null;
  box.addEventListener('input', function () {
    clearTimeout(timer);
    timer = setTimeout(function () { search(box.value.trim()); }, 120);
  });

  box.addEventListener('keydown', function (e) {
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      if (!results.length) return;
      e.preventDefault();
      sel += (e.key === 'ArrowDown' ? 1 : -1);
      if (sel < 0) sel = results.length - 1;
      if (sel >= results.length) sel = 0;
      panel.querySelectorAll('button').forEach(function (b, i) {
        b.classList.toggle('on', i === sel);
        if (i === sel) b.scrollIntoView({ block: 'nearest' });
      });
      return;
    }
    if (e.key === 'Escape') { panel.classList.remove('open'); box.blur(); return; }
    if (e.key !== 'Enter') return;

    var v = box.value.trim().toUpperCase().replace(/\s+/g, '');
    if (ID_RE.test(v) || /^[A-Z][A-Z\-]{1,}$/.test(v)) {
      var el = document.getElementById(v) || document.getElementById('fig-' + v);
      if (el) { panel.classList.remove('open'); hint.textContent = ''; land(el, v); return; }
      if (ID_RE.test(v)) { hint.textContent = 'No entry called ' + v + '.'; return; }
    }
    if (results.length) {
      var pick = results[sel < 0 ? 0 : sel];
      panel.classList.remove('open');
      land(pick.el, pick.id);
    }
  });

  panel.addEventListener('click', function (e) {
    var b = e.target.closest('button[data-i]');
    if (!b) return;
    var pick = results[+b.dataset.i];
    panel.classList.remove('open');
    land(pick.el, pick.id);
  });

  document.addEventListener('click', function (e) {
    if (!e.target.closest('.jump')) panel.classList.remove('open');
  });

  box.setAttribute('placeholder', box.dataset.ph || 'id or keywords');
  box.setAttribute('aria-label', 'Search this manual');
})();
