// La scritta AUDIONAUT dell'apertura (Inter Display ExtraBold, sfumatura azzurro-viola). Uso: node word.js
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1200, height: 200 } });
  await p.setContent(`<style>*{margin:0}body{background:transparent;display:flex;align-items:center;justify-content:center;height:200px}
  div{font-family:'Inter Display','Inter';font-weight:800;font-size:120px;letter-spacing:22px;padding-left:22px;background:linear-gradient(90deg,#4fe3ff,#7d8bff 50%,#c06bff);-webkit-background-clip:text;color:transparent;line-height:1}</style><div>AUDIONAUT</div>`);
  const el = await p.$('div'); await el.screenshot({ path: '../../Resources/splash_word.png', omitBackground: true });
  await b.close();
})();
