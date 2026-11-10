src=open('layers.py').read()
src=src.replace("def mark(layer):\n    return solidL(lemn(95,215,105),30,0,NEON,22,'round',layer)+solidL(jpath(-70,310,135),52,0,NEON,28,'butt',layer)",
'''def mark(layer):
    # come il logo di prima: infinito largo, la J SOPRA, col gambo che taglia l'anello di sinistra e il puntino in alto
    jx=-22
    j=f'M{jx},178 V385 A72,72 0 0 1 {jx-144},385 V372'
    dot=f'M{jx-1},111 L{jx+1},114'
    return (solidL(lemn(30,262,200),40,0,NEON,26,'round',layer)
            +solidL(j,48,0,NEON,30,'round',layer)
            +solidL(dot,46,0,NEON,30,'round',layer))''')
src=src.replace("translate(-30,-290)","translate(-28,-285)")
exec(src)
