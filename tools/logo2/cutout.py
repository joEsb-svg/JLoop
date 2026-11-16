"""Scontorna il logo di AudioNaut (casco su fondo bianco) e prepara le immagini dell'app.
Uso: python3 cutout.py   (da questa cartella; scrive in ../../Resources e ../../ci/windows)"""
from PIL import Image, ImageDraw, ImageFilter
import numpy as np
from scipy import ndimage as nd

a = np.asarray(Image.open('logo_originale.jpg').convert('RGB')).astype(float)
mn = a.min(axis=2)
white = mn > 232
lab, n = nd.label(white)
sizes = nd.sum(white, lab, range(1, n + 1))
bg = np.zeros_like(white)
for i, s in enumerate(sizes, 1):
    if s > 40: bg |= lab == i
edge = nd.binary_dilation(bg, iterations=3) & ~bg
alpha = np.ones(mn.shape); alpha[bg] = 0
alpha[edge] = np.clip((250 - mn[edge]) / 100.0, 0, 1)
A = alpha[..., None]
fg = np.clip(np.where(A > 0.01, (a - (1 - A) * 255) / np.maximum(A, 1e-3), 0), 0, 255)
im = Image.fromarray(np.dstack([fg, alpha * 255]).astype(np.uint8), 'RGBA')
im = im.crop(im.getbbox())
w, h = im.size; s = max(w, h) + 8
helmet = Image.new('RGBA', (s, s), (0, 0, 0, 0)); helmet.paste(im, ((s - w) // 2, (s - h) // 2), im)
helmet.save('helmet.png')

R = '../../Resources/'
helmet.save(R + 'splash_front.png', optimize=True)
helmet.resize((320, 320), Image.LANCZOS).save(R + 'logo_mark.png', optimize=True)

# icona: fondo "blu notte" (sfumatura radiale), angoli arrotondati, casco all'86%
S = 1024
y, x = np.mgrid[0:S, 0:S]
d = np.sqrt((x - S * 0.5) ** 2 + (y - S * 0.4) ** 2) / (S * 0.75)
t = np.clip(d, 0, 1)[..., None]
c0, c1 = np.array([0x1b, 0x25, 0x50]), np.array([0x07, 0x0a, 0x18])
bgc = (c0 * (1 - t) + c1 * t).astype(np.uint8)
icon = Image.fromarray(np.dstack([bgc, np.full((S, S), 255, np.uint8)]), 'RGBA')
hs = int(S * 0.86)
hh = helmet.resize((hs, hs), Image.LANCZOS)
icon.alpha_composite(hh, ((S - hs) // 2, (S - hs) // 2 + int(S * 0.01)))
mask = Image.new('L', (S * 4, S * 4), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, S * 4 - 1, S * 4 - 1), radius=int(S * 4 * 0.22), fill=255)
icon.putalpha(mask.resize((S, S), Image.LANCZOS))
icon.resize((512, 512), Image.LANCZOS).save(R + 'icon.png', optimize=True)
icon.save('icon_1024.png')
icon.save('../../ci/windows/jloop.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print('ok', helmet.size)
