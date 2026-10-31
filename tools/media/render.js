// Renders frames.jsonl over panel.svg.fragment the way sdk/browser/display.mjs applies frames.
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [frag, frames, outdir, scale, pick] = process.argv.slice(2);
  const lines = fs.readFileSync(frames, 'utf8').trim().split('\n').map(JSON.parse);
  fs.mkdirSync(outdir, { recursive: true });
  const html = `<html><body style="margin:0;background:#000"><svg id="s" xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220"><rect width="480" height="220" fill="#000"/>${fs.readFileSync(frag,'utf8')}</svg></body></html>`;
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 480, height: 220 }, deviceScaleFactor: +scale });
  await p.setContent(html);
  const which = pick ? pick.split(',').map(Number) : lines.map((_, i) => i);
  for (const i of which) {
    await p.evaluate((f) => {
      const q = (v) => Math.floor(v / 1024), d = (v) => String(v / 1024);
      const pal = [0, 1, 2].map(k => document.getElementById('diabolus_palette_' + k));
      f.n.forEach(([x, y, s, c, sn, shade, paint], k) => {
        const e = document.getElementById('diabolus_node_' + k);
        const a = q(s * c), bb = q(s * sn);
        e.setAttribute('transform', `matrix(${d(a)} ${d(bb)} ${d(-bb)} ${d(a)} ${d(x)} ${d(y)})`);
        if (paint) {
          e.setAttribute('stroke', pal[shade].getAttribute('stroke'));
          e.setAttribute('stroke-width', pal[shade].getAttribute('stroke-width'));
          if (paint === 2 && e.getAttribute('fill') !== 'none') e.setAttribute('fill', pal[shade].getAttribute('fill'));
        }
      });
      f.t.forEach(([x, y, s, str], k) => {
        const e = document.getElementById('diabolus_text_' + k);
        e.setAttribute('transform', `matrix(${d(s)} 0 0 ${d(s)} ${d(x)} ${d(y)})`);
        e.textContent = str;
      });
    }, lines[i]);
    await p.screenshot({ path: `${outdir}/f${String(i).padStart(4, '0')}.png` });
  }
  await b.close();
})();
