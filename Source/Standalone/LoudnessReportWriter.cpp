#include "LoudnessReportWriter.h"

namespace
{
    juce::String secsToTimecode (double s)
    {
        if (s < 0.0) s = 0.0;
        const int total = (int) s;
        return juce::String (total / 60).paddedLeft ('0', 2) + ":"
             + juce::String (total % 60).paddedLeft ('0', 2) + "."
             + juce::String ((int) ((s - (double) total) * 10.0));
    }

    // A label containing "-->" would close the comment the data block lives in.
    juce::String safe (const juce::String& s)
    {
        return s.replace ("\n", " ").replace ("-->", "- ->");
    }

    juce::String curveCsv (const std::vector<float>& c)
    {
        juce::String s;
        s.preallocateBytes ((size_t) c.size() * 7 + 8);
        for (size_t i = 0; i < c.size(); ++i)
        {
            if (i > 0) s << ",";
            s << juce::String (c[i], 2);
        }
        return s;
    }

    // ── The page's own script: parses the SAME data block the app reads. ────
    // Plain JavaScript, no libraries: the file must open anywhere, offline.
    const char* kScript = R"JS(
(function(){
'use strict';
function parseBlock(){
  var comment=null;
  for (var n=document.body.lastChild; n; n=n.previousSibling)
    if (n.nodeType===8 && n.data.indexOf('BSDAW-REPORT-DATA')===0){comment=n;break;}
  if(!comment) return {ctx:{},takes:[],raw:[]};
  var lines=comment.data.split('\n'); var ctx={}, takes=[], raw=[], cur=null, curRaw=null;
  for (var i=0;i<lines.length;i++){
    var L=lines[i]; if(L==='BSDAW-REPORT-DATA'||L.trim()==='') continue;
    if(L==='[take]'){ cur={st:[],m:[],viol:[],spectrum:[]}; curRaw=[]; takes.push(cur); raw.push(curRaw); continue; }
    if(cur) curRaw.push(L);
    var eq=L.indexOf('='); if(eq<0) continue;
    var k=L.substring(0,eq), v=L.substring(eq+1);
    var t=cur||ctx;
    if(k==='curve') t.st=v.split(',').filter(function(x){return x!=='';}).map(parseFloat);
    else if(k==='mcurve') t.m=v.split(',').filter(function(x){return x!=='';}).map(parseFloat);
    else if(k==='spectrum') t.spectrum=v.split(',').filter(function(x){return x!=='';}).map(parseFloat);
    else if(k==='viol'){ var f=v.split(','); t.viol.push({kind:+f[0],a:+f[1],b:+f[2],worst:+f[3]}); }
    else t[k]=v;
  }
  return {ctx:ctx,takes:takes,raw:raw};
}
var pow=function(v){return Math.pow(10,(v+0.691)/10);};
var lufs=function(p){return p>0?-0.691+10*Math.log10(p):-Infinity;};
function integrated(m){
  var abs=m.filter(function(v){return v>-70;}); if(!abs.length) return -Infinity;
  var mean=abs.reduce(function(a,v){return a+pow(v);},0)/abs.length;
  var gate=lufs(mean)-10; var rel=abs.filter(function(v){return v>gate;}); if(!rel.length) return -Infinity;
  return lufs(rel.reduce(function(a,v){return a+pow(v);},0)/rel.length);
}
function lra(s){
  var abs=s.filter(function(v){return v>-70;}); if(abs.length<2) return 0;
  var mean=lufs(abs.reduce(function(a,v){return a+pow(v);},0)/abs.length);
  var gate=mean-20; var rel=abs.filter(function(v){return v>gate;}).sort(function(a,b){return a-b;});
  if(rel.length<2) return 0;
  var q=function(x){return rel[Math.min(rel.length-1,Math.max(0,Math.round(x*(rel.length-1))))];};
  return q(0.95)-q(0.10);
}
function fmt(v,dp){ return (v===undefined||v===null||!isFinite(v)||v<=-119)?'--.-':v.toFixed(dp===undefined?1:dp); }
function tc(s){ s=Math.max(0,s); var t=Math.floor(s); return (''+Math.floor(t/60)).padStart(2,'0')+':'+(''+(t%60)).padStart(2,'0')+'.'+Math.floor((s-t)*10); }
var HZ=10;
var GREEN='#45c46a', AMBER='#d8a42a', RED='#d85a4a', ACC='#00fff2', DIM='#8f9aa1';
function verdict(v,target){ if(!isFinite(v)||v<=-119) return ACC; var d=Math.abs(v-target); return d<=1?GREEN:(d<=3?AMBER:RED); }

function Chart(canvas,take,selOut){
  var self=this; this.c=canvas; this.t=take; this.sel=selOut;
  this.dur=Math.max(take.st.length,take.m.length)/HZ; this.x0=0; this.x1=Math.max(1,this.dur);
  this.sa=null; this.sb=null; this.hover=null; this.drag=null;
  this.target=parseFloat(take.target||'-14'); this.ceiling=parseFloat(take.ceiling||'-1');
  var dpr=window.devicePixelRatio||1;
  this.resize=function(){ var r=canvas.getBoundingClientRect(); canvas.width=Math.max(300,r.width*dpr); canvas.height=260*dpr; self.draw(); };
  window.addEventListener('resize',this.resize);
  canvas.addEventListener('wheel',function(e){ e.preventDefault(); var r=canvas.getBoundingClientRect(); var fx=(e.clientX-r.left)/r.width; var t=self.x0+fx*(self.x1-self.x0); var k=e.deltaY>0?1.25:0.8; var w=(self.x1-self.x0)*k; w=Math.min(Math.max(w,0.5),self.dur||1); self.x0=t-fx*w; self.x1=self.x0+w; self.clamp(); self.draw(); },{passive:false});
  canvas.addEventListener('mousedown',function(e){ var r=canvas.getBoundingClientRect(); var fx=(e.clientX-r.left)/r.width; var t=self.x0+fx*(self.x1-self.x0); self.drag={mode:e.shiftKey?'sel':'pan',t:t,x:e.clientX,ox0:self.x0,ox1:self.x1}; if(e.shiftKey){self.sa=t;self.sb=t;} });
  window.addEventListener('mousemove',function(e){ var r=canvas.getBoundingClientRect(); var fx=(e.clientX-r.left)/r.width; if(self.drag){ if(self.drag.mode==='pan'){ var dt=(e.clientX-self.drag.x)/r.width*(self.drag.ox1-self.drag.ox0); self.x0=self.drag.ox0-dt; self.x1=self.drag.ox1-dt; self.clamp(); } else { self.sb=self.x0+fx*(self.x1-self.x0); self.report(); } self.draw(); return; } if(e.target===canvas){ self.hover=self.x0+fx*(self.x1-self.x0); self.draw(); } else if(self.hover!==null){ self.hover=null; self.draw(); } });
  window.addEventListener('mouseup',function(){ if(self.drag&&self.drag.mode==='sel'&&self.sa!==null&&Math.abs(self.sb-self.sa)<0.05){self.sa=self.sb=null;self.report();} self.drag=null; self.draw(); });
  canvas.addEventListener('dblclick',function(){ self.x0=0; self.x1=Math.max(1,self.dur); self.sa=self.sb=null; self.report(); self.draw(); });
  this.clamp=function(){ var w=this.x1-this.x0; if(this.x0<0){this.x0=0;this.x1=w;} if(this.x1>this.dur){this.x1=this.dur;this.x0=Math.max(0,this.dur-w);} };
  this.report=function(){ if(this.sa===null||this.sb===null){ this.sel.innerHTML='<span class=dim>Shift-drag on the chart to measure a region.</span>'; return; } var a=Math.min(this.sa,this.sb), b=Math.max(this.sa,this.sb); var ia=Math.max(0,Math.floor(a*HZ)), ib=Math.min(take.st.length,Math.ceil(b*HZ)); var ma=Math.max(0,Math.floor(a*HZ)), mb=Math.min(take.m.length,Math.ceil(b*HZ)); var st=take.st.slice(ia,ib), m=take.m.slice(ma,mb); var I=m.length?integrated(m):-Infinity; var maxS=st.length?Math.max.apply(null,st):-Infinity, maxM=m.length?Math.max.apply(null,m):-Infinity; this.sel.innerHTML='<b>'+tc(a)+' - '+tc(b)+'</b> ('+(b-a).toFixed(1)+' s)&nbsp;&nbsp; Integrated <b style="color:'+verdict(I,this.target)+'">'+fmt(I)+' LUFS</b>&nbsp;&nbsp; LRA <b>'+fmt(lra(st))+' LU</b>&nbsp;&nbsp; Max S <b>'+fmt(maxS)+'</b>&nbsp;&nbsp; Max M <b>'+fmt(maxM)+'</b>'+(m.length?'':' <span class=dim>(no momentary data in this take)</span>'); };
  this.draw=function(){
    var ctx=canvas.getContext('2d'); var W=canvas.width, H=canvas.height; ctx.setTransform(1,0,0,1,0,0); ctx.scale(dpr,dpr); var w=W/dpr, h=H/dpr;
    ctx.fillStyle='#101214'; ctx.fillRect(0,0,w,h);
    var lo=this.target-24, hi=this.target+8;   // weighted below the target: material sits under it far more often than over
    var X=function(t){return (t-self.x0)/(self.x1-self.x0)*w;}; var Y=function(v){return h-(Math.min(Math.max(v,lo),hi)-lo)/(hi-lo)*h;};
    ctx.font='10px sans-serif'; ctx.textBaseline='top';
    for(var v=Math.ceil(lo/6)*6; v<=hi; v+=6){ var y=Y(v); ctx.strokeStyle='#23282c'; ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(w,y); ctx.stroke(); ctx.fillStyle=DIM; ctx.fillText(v,4,y-11); }
    var step=(this.x1-this.x0)>120?30:(this.x1-this.x0)>40?10:(this.x1-this.x0)>10?5:1;
    for(var t=Math.ceil(this.x0/step)*step; t<=this.x1; t+=step){ var x=X(t); ctx.strokeStyle='#1a1e22'; ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,h); ctx.stroke(); ctx.fillStyle=DIM; ctx.fillText(tc(t),x+3,h-13); }
    if(this.sa!==null&&this.sb!==null){ var xa=X(Math.min(this.sa,this.sb)), xb=X(Math.max(this.sa,this.sb)); ctx.fillStyle='rgba(0,255,242,0.10)'; ctx.fillRect(xa,0,xb-xa,h); ctx.strokeStyle='rgba(0,255,242,0.6)'; ctx.beginPath(); ctx.moveTo(xa,0); ctx.lineTo(xa,h); ctx.moveTo(xb,0); ctx.lineTo(xb,h); ctx.stroke(); }
    var i0=Math.max(0,Math.floor(this.x0*HZ)), i1=Math.min(take.st.length-1,Math.ceil(this.x1*HZ));
    if(take.st.length){ ctx.beginPath(); ctx.moveTo(X(i0/HZ),h); for(var i=i0;i<=i1;i++) ctx.lineTo(X(i/HZ),Y(take.st[i])); ctx.lineTo(X(i1/HZ),h); ctx.closePath(); ctx.fillStyle='rgba(0,255,242,0.22)'; ctx.fill(); }
    if(take.m.length){ var m0=Math.max(0,Math.floor(this.x0*HZ)), m1=Math.min(take.m.length-1,Math.ceil(this.x1*HZ)); ctx.beginPath(); var started=false; for(var j=m0;j<=m1;j++){ var mv=take.m[j]; if(mv<=-119){started=false;continue;} var px=X(j/HZ), py=Y(mv); if(!started){ctx.moveTo(px,py);started=true;} else ctx.lineTo(px,py);} ctx.strokeStyle=ACC; ctx.lineWidth=1.3; ctx.stroke(); ctx.lineWidth=1; }
    take.viol.forEach(function(vv){ var xa=X(vv.a), xb=Math.max(X(vv.b),X(vv.a)+2); ctx.fillStyle=vv.kind===0?'rgba(216,90,74,0.45)':'rgba(255,145,0,0.35)'; ctx.fillRect(xa,0,xb-xa,6); });
    var ty=Y(this.target); ctx.setLineDash([5,4]); ctx.strokeStyle=GREEN; ctx.lineWidth=1.4; ctx.beginPath(); ctx.moveTo(0,ty); ctx.lineTo(w,ty); ctx.stroke(); ctx.setLineDash([]); ctx.lineWidth=1; ctx.fillStyle=GREEN; ctx.textAlign='right'; ctx.fillText('TARGET '+this.target.toFixed(1),w-4,ty-12); ctx.textAlign='left';
    if(this.hover!==null&&!this.drag){ var hi2=Math.round(this.hover*HZ); var x2=X(this.hover); ctx.strokeStyle='rgba(255,255,255,0.35)'; ctx.beginPath(); ctx.moveTo(x2,0); ctx.lineTo(x2,h); ctx.stroke(); var sv=take.st[hi2], mv2=take.m[hi2]; var txt=tc(this.hover)+'   S '+fmt(sv)+'   M '+fmt(mv2); var tw=ctx.measureText(txt).width+10; var bx=Math.min(x2+8,w-tw-4); ctx.fillStyle='rgba(22,25,28,0.92)'; ctx.fillRect(bx,8,tw,16); ctx.fillStyle='#e6e8ea'; ctx.fillText(txt,bx+5,11); }
  };
  this.resize(); this.report();
}
document.addEventListener('DOMContentLoaded',function(){
  var data=parseBlock(); var secs=document.querySelectorAll('section.take');
  secs.forEach(function(sec,i){ var take=data.takes[i]; if(!take) return; var cv=sec.querySelector('canvas.chart'); var so=sec.querySelector('.sel'); if(cv&&so) new Chart(cv,take,so); });
  var all=document.getElementById('selall'); if(all) all.addEventListener('change',function(){ document.querySelectorAll('input.pick').forEach(function(c){c.checked=all.checked;}); });
  var save=document.getElementById('save'); if(save) save.addEventListener('click',function(){
    var picks=document.querySelectorAll('input.pick'); var keep=[]; picks.forEach(function(c,i){ if(c.checked) keep.push(i); });
    if(!keep.length){ alert('Tick at least one take.'); return; }
    var doc=document.documentElement.cloneNode(true);
    var csecs=doc.querySelectorAll('section.take'); csecs.forEach(function(s,i){ if(keep.indexOf(i)<0) s.parentNode.removeChild(s); });
    var body=doc.querySelector('body'); for(var n=body.lastChild;n;n=n.previousSibling){ if(n.nodeType===8&&n.data.indexOf('BSDAW-REPORT-DATA')===0){ var lines=['BSDAW-REPORT-DATA']; for(var k in data.ctx){ if(data.ctx.hasOwnProperty(k)) lines.push(k+'='+data.ctx[k]); } keep.forEach(function(i){ lines.push('[take]'); lines=lines.concat(data.raw[i]); }); lines.push('BSDAW-REPORT-DATA'); n.data=lines.join('\n'); break; } }
    var cnt=doc.querySelector('#count'); if(cnt) cnt.textContent=keep.length+' take'+(keep.length===1?'':'s');
    doc.querySelectorAll('canvas.chart').forEach(function(c){ c.removeAttribute('width'); c.removeAttribute('height'); });
    var html='<!doctype html>\n'+doc.outerHTML; var blob=new Blob([html],{type:'text/html'}); var a=document.createElement('a'); a.href=URL.createObjectURL(blob); a.download=(document.title||'Loudness Report').replace(/[\\/:*?"<>|]+/g,'-')+' (selection).html'; document.body.appendChild(a); a.click(); setTimeout(function(){ URL.revokeObjectURL(a.href); a.remove(); },500);
  });
});
})();
)JS";

    const char* kStyle = R"CSS(
:root{--bg:#16191c;--panel:#1e2125;--edge:#2c3036;--ink:#e6e8ea;--dim:#8f9aa1;--acc:#00fff2;--green:#45c46a;--amber:#d8a42a;--red:#d85a4a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.5 "Segoe UI",Roboto,sans-serif}
.wrap{max-width:1100px;margin:0 auto;padding:22px 18px 60px}
header{display:flex;align-items:flex-end;justify-content:space-between;gap:16px;border-bottom:2px solid var(--acc);padding-bottom:12px;margin-bottom:18px;flex-wrap:wrap}
h1{margin:0;font-size:22px;font-weight:600}h2{margin:0;font-size:15px;font-weight:600}
.sub{color:var(--dim);font-size:12px}.toolbar{display:flex;gap:12px;align-items:center;font-size:12px;color:var(--dim)}
button{background:var(--panel);color:var(--ink);border:1px solid var(--edge);border-radius:4px;padding:6px 12px;cursor:pointer}button:hover{border-color:var(--acc)}
section.take{background:var(--panel);border:1px solid var(--edge);border-radius:8px;padding:14px 16px;margin-bottom:18px}
.takehead{display:flex;align-items:center;gap:10px;margin-bottom:10px}.takehead .scope{color:var(--dim);font-size:12px;margin-left:auto}
.cells{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px;margin-bottom:12px}
.cell{background:#101214;border:1px solid var(--edge);border-radius:6px;padding:8px 10px}.cell .k{font-size:10px;letter-spacing:.06em;color:var(--dim)}.cell .v{font-size:20px;font-weight:700;line-height:1.2}.cell .u{font-size:11px;color:var(--dim)}
.chartwrap{position:relative;border:1px solid var(--edge);border-radius:6px;overflow:hidden}canvas.chart{display:block;width:100%;height:260px;cursor:crosshair}
.hint{font-size:11px;color:var(--dim);margin:6px 0 2px}.sel{font-size:13px;margin:4px 0 10px;min-height:20px}.dim{color:var(--dim)}
table{border-collapse:collapse;width:100%;font-size:12px}th,td{text-align:left;padding:4px 8px;border-bottom:1px solid var(--edge)}th{color:var(--dim);font-weight:600}
svg.spec{width:100%;height:160px;display:block;border:1px solid var(--edge);border-radius:6px;margin-top:10px}
.pass{color:var(--green)}.warn{color:var(--amber)}.fail{color:var(--red)}
)CSS";
}

juce::String LoudnessReportWriter::esc (const juce::String& s)
{
    return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;");
}

juce::String LoudnessReportWriter::buildDataBlock (const std::vector<Take>& takes,
                                                   const Context& ctx)
{
    juce::String d;
    d << dataOpenTag() << "\n";
    d << "project=" << safe (ctx.projectName)    << "\n";
    d << "stamp="   << safe (ctx.timestampLabel) << "\n";
    for (const auto& t : takes)
    {
        const auto& m = t.m;
        d << "[take]\n";
        d << "label="     << safe (t.label)      << "\n";
        d << "scope="     << safe (t.scopeLabel) << "\n";
        d << "spec="      << (int) m.specId                    << "\n";
        d << "customtgt=" << juce::String (m.customTargetLufs, 2) << "\n";
        d << "target="    << juce::String (m.resolvedSpec().integratedLufs, 2) << "\n";
        d << "ceiling="   << juce::String (m.resolvedSpec().maxTruePeakDb, 2)  << "\n";
        d << "integrated="<< juce::String (m.integratedLufs, 3)   << "\n";
        d << "truepeak="  << juce::String (m.truePeakDb, 3)       << "\n";
        d << "lra="       << juce::String (m.lraLu, 3)            << "\n";
        d << "maxshort="  << juce::String (m.maxShortTermLufs, 3) << "\n";
        d << "maxmom="    << juce::String (m.maxMomentaryLufs, 3) << "\n";
        d << "duration="  << juce::String (m.durationSeconds, 3)  << "\n";
        d << "truncated=" << (m.violationsTruncated ? 1 : 0)      << "\n";
        d << "curve="     << curveCsv (m.lufsCurve)               << "\n";
        d << "mcurve="    << curveCsv (m.momentaryCurve)          << "\n";
        if (! m.spectrumDb.empty())
            d << "spectrum=" << curveCsv (m.spectrumDb) << "\n";
        for (const auto& v : m.violations)
            d << "viol=" << (int) v.kind << "," << juce::String (v.startSecs, 3) << ","
              << juce::String (v.endSecs, 3) << "," << juce::String (v.worstValue, 3) << "\n";
    }
    d << dataCloseTag() << "\n";
    return d;
}

juce::String LoudnessReportWriter::takeSection (const Take& t, int index)
{
    const auto& m    = t.m;
    const auto  spec = m.resolvedSpec();
    const bool  checks = spec.checksIntegrated;
    const float dI   = std::abs (m.integratedLufs - spec.integratedLufs);
    const char* iCls = ! checks ? "" : (dI <= spec.toleranceLu ? "pass" : (dI <= 3.0f ? "warn" : "fail"));
    const bool  tpOk = m.truePeakDb <= spec.maxTruePeakDb;

    auto cell = [] (const char* k, const juce::String& v, const juce::String& u, const char* cls = "")
    {
        juce::String s;
        s << "<div class='cell'><div class='k'>" << k << "</div><div class='v " << cls << "'>"
          << v << "</div><div class='u'>" << u << "</div></div>";
        return s;
    };
    auto f1 = [] (float v) { return v <= -119.0f ? juce::String ("--.-") : juce::String (v, 1); };

    juce::String h;
    h << "<section class='take' data-idx='" << index << "'>";
    h << "<div class='takehead'><input type='checkbox' class='pick' checked title='Include in Save selected'/>"
      << "<h2>" << esc (t.label) << "</h2><span class='scope'>" << esc (t.scopeLabel) << "</span></div>";
    h << "<div class='cells'>";
    h << cell ("INTEGRATED", f1 (m.integratedLufs),
               checks ? juce::String ("LUFS  target " + juce::String (spec.integratedLufs, 1))
                      : juce::String ("LUFS (no target in this spec)"), iCls);
    h << cell ("SHORT-TERM MAX", f1 (m.maxShortTermLufs), "LUFS");
    h << cell ("MOMENTARY MAX",  f1 (m.maxMomentaryLufs),  "LUFS");
    h << cell ("LOUDNESS RANGE", juce::String (m.lraLu, 1), "LU");
    h << cell ("TRUE PEAK", f1 (m.truePeakDb), "dBTP  ceiling " + juce::String (spec.maxTruePeakDb, 1), tpOk ? "pass" : "fail");
    h << cell ("LENGTH", secsToTimecode (m.durationSeconds), juce::String (spec.name));
    h << "</div>";
    h << "<div class='chartwrap'><canvas class='chart'></canvas></div>";
    h << "<div class='hint'>Filled = short-term, line = momentary, dashed = target, marks along the top = moments over the ceiling. "
         "Wheel zooms, drag pans, double-click resets, <b>shift-drag selects a region</b>.</div>";
    h << "<div class='sel'></div>";

    if (! m.violations.empty())
    {
        h << "<table><tr><th>Moment</th><th>What</th><th>Worst</th></tr>";
        for (const auto& v : m.violations)
            h << "<tr><td>" << secsToTimecode (v.startSecs) << " - " << secsToTimecode (v.endSecs)
              << "</td><td>" << (v.kind == LoudnessViolation::Kind::TruePeak ? "True peak over the ceiling" : "Short-term over the limit")
              << "</td><td>" << juce::String (v.worstValue, 1)
              << (v.kind == LoudnessViolation::Kind::TruePeak ? " dBTP" : " LUFS") << "</td></tr>";
        h << "</table>";
        if (m.violationsTruncated)
            h << "<div class='hint'>Only the first " << LoudnessViolation::kMaxRows << " moments are listed.</div>";
    }

    if (! m.spectrumDb.empty())
    {
        constexpr float kSpecTop = 0.0f, kSpecBot = -96.0f;
        constexpr float kSw = 900.0f, kSh = 160.0f;
        h << "<svg class='spec' viewBox='0 0 " << (int) kSw << " " << (int) kSh << "' preserveAspectRatio='none'>";
        h << "<rect x='0' y='0' width='" << (int) kSw << "' height='" << (int) kSh << "' fill='#101214'/>";
        static const float kMarks[] = { 50.f, 100.f, 500.f, 1000.f, 5000.f, 10000.f };
        const float lo = std::log (BuilderPage::MeasureResult::kSpectrumMinHz);
        const float span = std::log (BuilderPage::MeasureResult::kSpectrumMaxHz) - lo;
        for (float hz : kMarks)
        {
            const float x = kSw * (std::log (hz) - lo) / span;
            h << "<line x1='" << juce::String (x, 1) << "' y1='0' x2='" << juce::String (x, 1)
              << "' y2='" << (int) kSh << "' stroke='#23282c' stroke-width='1'/>";
            h << "<text x='" << juce::String (x + 3.0f, 1) << "' y='" << (int) kSh - 4
              << "' fill='#8f9aa1' font-size='9'>"
              << (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz)) << "</text>";
        }
        juce::String sp, fill;
        const int n = (int) m.spectrumDb.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = kSw * (float) i / (float) juce::jmax (1, n - 1);
            const float tt = juce::jlimit (0.0f, 1.0f, (m.spectrumDb[(size_t) i] - kSpecBot) / (kSpecTop - kSpecBot));
            const float y = kSh - tt * kSh;
            sp << juce::String (x, 1) << "," << juce::String (y, 1) << " ";
        }
        fill << "0," << (int) kSh << " " << sp << (int) kSw << "," << (int) kSh;
        h << "<polygon points='" << fill << "' fill='rgba(0,255,242,0.18)'/>";
        h << "<polyline points='" << sp << "' fill='none' stroke='#00fff2' stroke-width='1.4'/>";
        h << "</svg><div class='hint'>Average level per frequency band across the whole render.</div>";
    }
    h << "</section>";
    return h;
}

juce::String LoudnessReportWriter::buildHtml (const std::vector<Take>& takes, const Context& ctx)
{
    juce::String h;
    h << "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
      << "<meta name='viewport' content='width=device-width, initial-scale=1'>"
      << "<title>" << esc (ctx.projectName) << " - Loudness Report</title>"
      << "<style>" << kStyle << "</style></head><body><div class='wrap'>";
    h << "<header><div><h1>" << esc (ctx.projectName) << "</h1><div class='sub'>Loudness report - "
      << esc (ctx.timestampLabel) << " - <span id='count'>" << (int) takes.size()
      << (takes.size() == 1 ? " take" : " takes") << "</span></div></div>";
    h << "<div class='toolbar'><label><input type='checkbox' id='selall' checked/> Select all</label>"
         "<button id='save'>Save selected as report</button></div></header>";
    for (int i = 0; i < (int) takes.size(); ++i)
        h << takeSection (takes[(size_t) i], i);
    h << "</div>";
    // The data block sits in the body (the script walks body's child nodes for it).
    h << buildDataBlock (takes, ctx);
    h << "<script>" << kScript << "</script></body></html>";
    return h;
}

bool LoudnessReportWriter::writeSession (const juce::File& dir, const juce::String& base,
                                         const std::vector<Take>& takes, const Context& ctx,
                                         juce::String& outErr)
{
    if (! dir.exists() && ! dir.createDirectory())
    {
        outErr = "Could not create the Reports folder.";
        return false;
    }
    const juce::File html = dir.getChildFile (base + ".html");
    if (! html.replaceWithText (buildHtml (takes, ctx)))
    {
        outErr = "Could not write " + html.getFileName();
        return false;
    }
    return true;
}

bool LoudnessReportWriter::write (const juce::File& reportsDir, const juce::String& base,
                                  const BuilderPage::MeasureResult& m, const Context& ctx,
                                  bool, juce::String& outErr)
{
    Take t;
    t.label      = ctx.timestampLabel;
    t.scopeLabel = ctx.scopeLabel;
    t.m          = m;
    return writeSession (reportsDir, base + " - Loudness Report", { t }, ctx, outErr);
}

bool LoudnessReportWriter::readEmbeddedAll (const juce::File& htmlFile,
                                            std::vector<Take>& out, Context& outCtx)
{
    const juce::String text = htmlFile.loadFileAsString();
    const int a = text.indexOf (dataOpenTag());
    if (a < 0) return false;
    const int b = text.indexOf (a, dataCloseTag());
    if (b < 0) return false;

    juce::StringArray lines;
    lines.addLines (text.substring (a + (int) std::strlen (dataOpenTag()), b));

    Take* cur = nullptr;
    auto parseCurve = [] (const juce::String& val, std::vector<float>& dst)
    {
        juce::StringArray pts;
        pts.addTokens (val, ",", "");
        dst.reserve ((size_t) pts.size());
        for (const auto& p : pts)
            if (p.isNotEmpty()) dst.push_back (p.getFloatValue());
    };

    for (const auto& line : lines)
    {
        if (line.trim().isEmpty()) continue;
        if (line == "[take]") { out.emplace_back(); cur = &out.back(); continue; }
        const int eq = line.indexOf ("=");
        if (eq < 0) continue;
        const juce::String key = line.substring (0, eq);
        const juce::String val = line.substring (eq + 1);

        if (cur == nullptr)
        {
            // Header keys, plus the legacy single-take layout where the take's
            // own keys sat at top level: open an implicit take for those.
            if      (key == "project") { outCtx.projectName = val; continue; }
            else if (key == "stamp")   { outCtx.timestampLabel = val; continue; }
            else if (key == "scope")   { outCtx.scopeLabel = val; continue; }
            out.emplace_back(); cur = &out.back();
            cur->label = outCtx.timestampLabel; cur->scopeLabel = outCtx.scopeLabel;
        }
        auto& m = cur->m;
        if      (key == "label")     cur->label      = val;
        else if (key == "scope")     cur->scopeLabel = val;
        else if (key == "customtgt") m.customTargetLufs = val.getFloatValue();
        else if (key == "spec")      m.specId = (LoudnessSpec::Id) juce::jlimit (0, (int) LoudnessSpec::Id::Count - 1, val.getIntValue());
        else if (key == "integrated")m.integratedLufs   = val.getFloatValue();
        else if (key == "truepeak")  m.truePeakDb       = val.getFloatValue();
        else if (key == "lra")       m.lraLu            = val.getFloatValue();
        else if (key == "maxshort")  m.maxShortTermLufs = val.getFloatValue();
        else if (key == "maxmom")    m.maxMomentaryLufs = val.getFloatValue();
        else if (key == "duration")  m.durationSeconds  = val.getDoubleValue();
        else if (key == "truncated") m.violationsTruncated = (val.getIntValue() != 0);
        else if (key == "curve")     parseCurve (val, m.lufsCurve);
        else if (key == "mcurve")    parseCurve (val, m.momentaryCurve);
        else if (key == "spectrum")  parseCurve (val, m.spectrumDb);
        else if (key == "viol")
        {
            juce::StringArray f;
            f.addTokens (val, ",", "");
            if (f.size() >= 4)
                m.violations.push_back ({ (LoudnessViolation::Kind) f[0].getIntValue(),
                                          f[1].getDoubleValue(), f[2].getDoubleValue(), f[3].getFloatValue() });
        }
    }
    if (out.empty()) return false;

    // Verdicts are recomputed, never stored: the spec table is the one authority.
    for (auto& t : out)
    {
        const auto spec = t.m.resolvedSpec();
        t.m.truePeakInSpec   = (t.m.truePeakDb <= spec.maxTruePeakDb);
        t.m.integratedInSpec = (! spec.checksIntegrated)
                             || (std::abs (t.m.integratedLufs - spec.integratedLufs) <= spec.toleranceLu);
        if (t.label.isEmpty()) t.label = outCtx.timestampLabel;
    }
    return true;
}

bool LoudnessReportWriter::readEmbedded (const juce::File& htmlFile,
                                         BuilderPage::MeasureResult& out, Context& outCtx)
{
    std::vector<Take> takes;
    if (! readEmbeddedAll (htmlFile, takes, outCtx)) return false;
    out = takes.front().m;
    if (outCtx.scopeLabel.isEmpty()) outCtx.scopeLabel = takes.front().scopeLabel;
    return true;
}
