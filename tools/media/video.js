// Renders the MP4's frames: the machine's screen (driven by screen.jsonl) on a
// 1280x720 black page, with captions in white.
const { chromium } = require('playwright');
const fs = require('fs');
const [frag, frames, outdir, from, to] = process.argv.slice(2);
const lines = fs.readFileSync(frames, 'utf8').trim().split('\n').map(JSON.parse);
const names = ['Vespers', 'Devils March', 'Ashen', 'Black Cantor', 'Reliquary', 'Unlit', 'Adversary', 'Midnight',
  'Endless', 'Hollow Saint', 'Last Office', 'Diabolus'];
const tags = ['a quiet flue chorus in a chapel', 'full, bright and close, with a marching echo',
  'a lone hollow flute dissolving into a huge room', 'nasal and reedy, singing through dark repeats · tremulant',
  'a single full principal, still and sacred', 'slow-speaking pipes in the dark · tremulant',
  'many ranks, bitter and out of tune', 'a balanced chorus with a long echo', 'every rank, in an endless room · tremulant',
  'hollow ranks, half-lit', 'a swell from nothing into deep repeats · tremulant', 'the full organ, bright and unsteady'];
const FPS = 25, total = lines.length;
(async () => {
  fs.mkdirSync(outdir, { recursive: true });
  const html = `<html><head><style>
  body{margin:0;background:#000;width:1280px;height:720px;overflow:hidden;font-family:'DejaVu Serif',serif;color:#fff}
  #scr{position:absolute;left:64px;top:52px}
  .c{position:absolute;left:0;width:1280px;text-align:center;letter-spacing:.22em}
  #top{top:16px;font-size:15px;letter-spacing:.3em}
  #name{top:600px;font-size:38px}
  #tag{top:652px;font-size:17px;letter-spacing:.08em;font-style:italic}
  #foot{top:694px;font-size:11px;letter-spacing:.06em}
  #card{position:absolute;inset:0;background:#000;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center}
  #card h1{font-weight:normal;font-size:64px;letter-spacing:.3em;margin:0 0 18px}
  #card p{font-size:20px;letter-spacing:.1em;margin:6px;font-style:italic}
  </style></head><body>
  <svg id="scr" xmlns="http://www.w3.org/2000/svg" width="1152" height="528" viewBox="0 0 480 220"><rect width="480" height="220" fill="#000"/>${fs.readFileSync(frag, 'utf8')}</svg>
  <div class="c" id="top">DIABOLUS PORTATIVE · AN OPEN-XY MACHINE FOR THE OP-XY</div>
  <div class="c" id="name"></div><div class="c" id="tag"></div>
  <div class="c" id="foot">Organ: the machine's own code, rendered offline, with the OP-XY's envelope and tremolo emulated. Sub bass and drums were added for the song.</div>
  <div id="card"><h1 id="h"></h1><p id="p1"></p><p id="p2"></p></div>
  </body></html>`;
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1280, height: 720 } });
  await p.setContent(html);
  for (let i = +from; i < Math.min(+to, total); i++) {
    const f = lines[i], t = i / FPS, end = total / FPS;
    // Title card for the first 4.5 s, credits for the last 7 s, crossfaded.
    let card = 0, h = '', p1 = '', p2 = '';
    if (t < 5.5) { card = t < 4.5 ? 1 : 1 - (t - 4.5); h = 'DIABOLUS PORTATIVE'; p1 = 'a sample-free medieval portative organ'; p2 = 'an Open-XY machine for the OP-XY'; }
    if (t > end - 8) { card = Math.min(1, (t - (end - 8))); h = 'DIABOLUS PORTATIVE';
      p1 = 'inspired by an idea from Franny on the Custom Operator Discord · image updates by DWD';
      p2 = 'github.com/SammiBabe666/Diabolus-Portative'; }
    await p.evaluate(({ f, card, h, p1, p2, name, tag }) => {
      const q = (v) => Math.floor(v / 1024), d = (v) => String(v / 1024);
      const pal = [0, 1, 2].map(k => document.getElementById('diabolus_palette_' + k));
      f.n.forEach(([x, y, s, c, sn, shade, paint], k) => {
        const e = document.getElementById('diabolus_node_' + k), a = q(s * c), bb = q(s * sn);
        e.setAttribute('transform', `matrix(${d(a)} ${d(bb)} ${d(-bb)} ${d(a)} ${d(x)} ${d(y)})`);
        if (paint) {
          e.setAttribute('stroke', pal[shade].getAttribute('stroke'));
          e.setAttribute('stroke-width', pal[shade].getAttribute('stroke-width'));
          if (paint === 2) e.setAttribute('fill', pal[shade].getAttribute('fill'));
        }
      });
      f.t.forEach(([x, y, s, str], k) => {
        const e = document.getElementById('diabolus_text_' + k);
        e.setAttribute('transform', `matrix(${d(s)} 0 0 ${d(s)} ${d(x)} ${d(y)})`);
        e.textContent = str;
      });
      document.getElementById('name').textContent = name;
      document.getElementById('tag').textContent = tag;
      const cd = document.getElementById('card');
      cd.style.opacity = card; cd.style.display = card > 0 ? 'flex' : 'none';
      document.getElementById('h').textContent = h;
      document.getElementById('p1').textContent = p1;
      document.getElementById('p2').textContent = p2;
    }, { f, card, h, p1, p2, name: f.p >= 0 ? `${String(f.p + 1).padStart(2, '0')} / 12 · ${names[f.p].toUpperCase()}` : '',
         tag: f.p >= 0 ? tags[f.p] : (t < end / 2 ? 'a drone, before the first preset' : 'the drone, after the last') });
    await p.screenshot({ path: `${outdir}/v${String(i).padStart(5, '0')}.png` });
  }
  await b.close();
})();
