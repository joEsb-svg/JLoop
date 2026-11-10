import math
def defs(uid, neon):
    return f'''<defs>
<filter id="metal{uid}" filterUnits="userSpaceOnUse" x="-2000" y="-2000" width="5000" height="5000" color-interpolation-filters="sRGB">
 <feTurbulence type="fractalNoise" baseFrequency="0.035" numOctaves="5" seed="{3+uid}" result="n"/>
 <feColorMatrix in="n" type="matrix" values="0 0 0 0 0.10  0 0 0 0 0.11  0 0 0 0 0.12  2.2 0 0 0 -1.05" result="spots"/>
 <feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="{9+uid}" result="grain"/>
 <feColorMatrix in="grain" type="saturate" values="0" result="g2"/>
 <feComposite in="g2" in2="SourceGraphic" operator="arithmetic" k1="0" k2="0.22" k3="0.78" k4="-0.06" result="tex"/>
 <feComposite in="spots" in2="tex" operator="atop" result="t2"/>
 <feDiffuseLighting in="n" surfaceScale="2.2" lighting-color="#ffffff" result="light"><feDistantLight azimuth="225" elevation="55"/></feDiffuseLighting>
 <feComposite in="t2" in2="light" operator="arithmetic" k1="1.05" k2="0" k3="0" k4="0" result="lit"/>
 <feComposite in="lit" in2="SourceAlpha" operator="in"/>
</filter>
<filter id="glow{uid}" filterUnits="userSpaceOnUse" x="-2000" y="-2000" width="5000" height="5000" color-interpolation-filters="sRGB">
 <feTurbulence type="fractalNoise" baseFrequency="0.02" numOctaves="3" seed="5" result="n"/>
 <feColorMatrix in="n" type="matrix" values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 1.4 -0.35" result="mask"/>
 <feComposite in="SourceGraphic" in2="mask" operator="arithmetic" k1="0.55" k2="0.75" k3="0" k4="0" result="core"/>
 <feGaussianBlur in="SourceGraphic" stdDeviation="14" result="b1"/>
 <feGaussianBlur in="SourceGraphic" stdDeviation="40" result="b2"/>
 <feMerge><feMergeNode in="b2"/><feMergeNode in="b1"/><feMergeNode in="core"/></feMerge>
</filter>
<linearGradient id="side{uid}" gradientUnits="userSpaceOnUse" x1="-200" y1="0" x2="300" y2="0"><stop offset="0" stop-color="#2a2f33"/><stop offset="1" stop-color="#0d0f11"/></linearGradient>
</defs>'''
def jpath(x,y,s):
    # centro della J: gambo alto, gancio tondo a sinistra con estremita' piatta
    st=x+s*0.30; top=y-s*0.95; bot=y+s*0.45; r=s*0.42
    return f'M{st:.1f},{top:.1f} V{bot:.1f} A{r:.1f},{r:.1f} 0 0 1 {st-2*r:.1f},{bot:.1f} V{bot-s*0.12:.1f}'
def lemn(cx,cy,a,n=220):
    pts=[]
    for i in range(n+1):
        t=i/n*2*math.pi+math.pi/2; s,c=math.sin(t),math.cos(t); d=1+s*s
        pts.append((cx+a*c/d, cy+a*s*c/d*1.3))
    return 'M'+' L'.join(f'{px:.1f},{py:.1f}' for px,py in pts)+' Z'
def solid(d,w,uid,neon,depth,cap='butt'):
    dx,dy=depth*0.75,-depth*0.55
    out=''
    # retro scuro (estrusione): tanti strati
    for k in range(22,11,-1):
        f=k/14
        out+=f'<path d="{d}" transform="translate({dx*f:.1f},{dy*f:.1f})" stroke="url(#side{uid})" stroke-width="{w:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    # anima al neon
    out+=f'<g filter="url(#glow{uid})">'
    for k in range(11,2,-1):
        f=k/14
        out+=f'<path d="{d}" transform="translate({dx*f:.1f},{dy*f:.1f})" stroke="{neon}" stroke-width="{w:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    out+='</g>'
    # faccia di metallo con bordo smussato e riga incisa
    out+=f'<path d="{d}" stroke="#9aa1a8" stroke-width="{w+6:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    out+=f'<path d="{d}" stroke="#1c1f22" stroke-width="{w+1:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round"/>'
    out+=f'<path d="{d}" stroke="#565b60" stroke-width="{w-3:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round" filter="url(#metal{uid})"/>'
    out+=f'<path d="{d}" stroke="#ffffff" stroke-opacity=".10" stroke-width="{w-3:.1f}" fill="none" stroke-linecap="{cap}" stroke-linejoin="round" transform="translate(-1,-1)" />'
    return out
def word(cx,y,neon,size=40):
    return (f'<text x="{cx}" y="{y}" text-anchor="middle" font-family="Inter,Segoe UI,Arial" font-weight="900" letter-spacing="14" font-size="{size}" fill="{neon}" opacity=".35" filter="blur(6px)">JLOOP</text>'
            f'<text x="{cx}" y="{y}" text-anchor="middle" font-family="Inter,Segoe UI,Arial" font-weight="900" letter-spacing="14" font-size="{size}" fill="#c9ced3" stroke="{neon}" stroke-width="1">JLOOP</text>')
def cap(cx,y,t,size=20,col="#9fb2c6"): return f'<text x="{cx}" y="{y}" text-anchor="middle" font-family="Inter,Segoe UI,Arial" font-weight="600" font-size="{size}" fill="{col}">{t}</text>'

W,H=1650,700
svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><rect width="100%" height="100%" fill="#000"/>'
cards=[('#3dff8a','M1 · Verde come il tuo esempio','solo la J', 'j'),
       ('#ff7a1a','M2 · Arancio JLoop','solo la J, coi colori dell\'app','j'),
       ('#ff7a1a','M3 · J + infinito','stesso stile, con l\'infinito di adesso','ji')]
for i,(neon,t,d,kind) in enumerate(cards):
    cx=275+i*550; uid=i
    svg+=defs(uid,neon)+f'<rect x="{cx-250}" y="20" width="500" height="660" rx="18" fill="#07090b" stroke="#1d242c"/>'
    if kind=='j':
        svg+=solid(jpath(cx-10,300,160),62,uid,neon,34)
    else:
        svg+=solid(lemn(cx+95,215,105),30,uid,neon,22,'round')
        svg+=solid(jpath(cx-70,310,135),52,uid,neon,28)
    svg+=word(cx,560,neon)+cap(cx,610,t,24,"#f3f6fa")+cap(cx,645,d,17)
open('metal.svg','w').write(svg+'</svg>')
# icone piccole (come sul telefono)
svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="900" height="300"><rect width="100%" height="100%" fill="#1b2430"/>'
for i,(neon,kind) in enumerate([('#3dff8a','j'),('#ff7a1a','j'),('#ff7a1a','ji')]):
    cx=150+i*300; uid=10+i
    svg+=defs(uid,neon)+f'<g transform="translate({cx-90},50) scale(0.36)"><rect width="500" height="500" rx="110" fill="#050607"/>'
    if kind=='j': svg+=solid(jpath(240,330,170),66,uid,neon,36)
    else: svg+=solid(lemn(330,200,110),32,uid,neon,22,'round')+solid(jpath(180,330,150),56,uid,neon,30)
    svg+='</g>'+cap(cx,270,['M1','M2','M3'][i]+' · icona sul telefono',16,"#dfe8f1")
open('icons.svg','w').write(svg+'</svg>')
