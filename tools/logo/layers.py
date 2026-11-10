exec(open('metal.py').read().split("W,H=1650,700")[0])
def solidL(d,w,uid,neon,depth,cap='butt',layer='all'):
    dx,dy=depth*0.75,-depth*0.55
    out=''
    if layer in ('all','back'):
        for k in range(22,11,-1):
            f=k/14; out+=f'<path d="{d}" transform="translate({dx*f:.1f},{dy*f:.1f})" stroke="url(#side{uid})" stroke-width="{w:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    if layer=='back':
        for k in range(11,2,-1):
            f=k/14; out+=f'<path d="{d}" transform="translate({dx*f:.1f},{dy*f:.1f})" stroke="#2b1609" stroke-width="{w:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    if layer in ('all','glow'):
        out+=f'<g filter="url(#glow{uid})">'
        for k in range(11,2,-1):
            f=k/14; out+=f'<path d="{d}" transform="translate({dx*f:.1f},{dy*f:.1f})" stroke="{neon}" stroke-width="{w:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
        out+='</g>'
    if layer in ('all','front'):
        out+=f'<path d="{d}" stroke="#9aa1a8" stroke-width="{w+6:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
        out+=f'<path d="{d}" stroke="#1c1f22" stroke-width="{w+1:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
        out+=f'<path d="{d}" stroke="#565b60" stroke-width="{w-3:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round" filter="url(#metal{uid})"/>'
        out+=f'<path d="{d}" stroke="#ffffff" stroke-opacity=".10" stroke-width="{w-3:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round" transform="translate(-1,-1)"/>'
    return out
NEON='#ff7a1a'
def mark(layer):
    return solidL(lemn(95,215,105),30,0,NEON,22,'round',layer)+solidL(jpath(-70,310,135),52,0,NEON,28,'butt',layer)
def page(W,H,tx,ty,sc,body,bg=None):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">'+defs(0,NEON)
            +(f'<rect width="100%" height="100%" fill="{bg}"/>' if bg else '')
            +f'<g transform="translate({tx},{ty}) scale({sc}) translate(-30,-290)">'+body+'</g></svg>')
for L in ('back','glow','front'):
    open(f'splash_{L}.svg','w').write(page(1000,860,500,430,1.4,mark(L)))
open('logo_mark.svg','w').write(page(400,360,200,185,0.55,mark('all')))
open('icon.svg','w').write(page(512,512,256,262,0.78,mark('all'),'#050607'))
open('preview.svg','w').write(page(1000,860,500,430,1.4,mark('all'),'#000'))
# scritta
w=f'''<svg xmlns="http://www.w3.org/2000/svg" width="900" height="170"><defs><filter id="b" x="-20%" y="-50%" width="140%" height="200%"><feGaussianBlur stdDeviation="9"/></filter>
<linearGradient id="m" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d9dde1"/><stop offset=".5" stop-color="#8d949b"/><stop offset="1" stop-color="#5b6167"/></linearGradient></defs>
<text x="450" y="118" text-anchor="middle" font-family="Inter,Segoe UI,Arial" font-weight="900" letter-spacing="34" font-size="92" fill="{NEON}" opacity=".55" filter="url(#b)">JLOOP</text>
<text x="450" y="118" text-anchor="middle" font-family="Inter,Segoe UI,Arial" font-weight="900" letter-spacing="34" font-size="92" fill="url(#m)" stroke="{NEON}" stroke-width="1.5">JLOOP</text></svg>'''
open('splash_word.svg','w').write(w)
