# Genere l'icone pixel art de GOUFFRE (32x32 agrandie sans lissage)
from PIL import Image
import math, sys
W=32
img=Image.new("RGBA",(W,W),(0,0,0,0)); px=img.load()
def inside_round(x,y,r=6):
    cx=min(max(x,r),W-1-r); cy=min(max(y,r),W-1-r)
    return (x-cx)**2+(y-cy)**2<=r*r+r
for y in range(W):
    t=y/(W-1)
    top=(46,30,64); bot=(12,8,18)
    c=tuple(int(top[i]+(bot[i]-top[i])*t) for i in range(3))
    for x in range(W):
        if inside_round(x,y):
            # briques du gouffre
            row=y//5; off=(row%2)*4
            mortar = (y%5==0) or ((x+off)%8==0)
            cc=tuple(max(0,v-10) for v in c) if mortar else c
            px[x,y]=cc+(255,)
# bord bronze
for y in range(W):
    for x in range(W):
        if px[x,y][3] and any(not (0<=x+dx<W and 0<=y+dy<W and px[x+dx,y+dy][3]) for dx,dy in ((1,0),(-1,0),(0,1),(0,-1))):
            px[x,y]=(150,100,45,255)
def blend(x,y,col,a):
    if 0<=x<W and 0<=y<W and px[x,y][3]:
        r,g,b,_=px[x,y]; px[x,y]=(min(255,int(r+col[0]*a)),min(255,int(g+col[1]*a)),min(255,int(b+col[2]*a)),255)
# halo
cx,cy=18,12
for y in range(W):
    for x in range(W):
        d=math.hypot(x+0.5-cx,y+0.5-cy)
        if d<13: f=(1-d/13)**2; blend(x,y,(255,150,60),0.9*f)
# trainee
for (tx,ty,r,a) in ((11,19,3,0.7),(7,24,2,0.5),(4,28,1,0.35)):
    for y in range(W):
        for x in range(W):
            if (x-tx)**2+(y-ty)**2<=r*r+r: blend(x,y,(255,170,70),a)
def disc(cx,cy,r,col):
    for y in range(W):
        for x in range(W):
            if (x-cx)**2+(y-cy)**2<=r*r+r and px[x,y][3]: px[x,y]=col+(255,)
disc(18,12,6,(176,80,30)); disc(17,11,5,(243,150,47)); disc(16,10,3,(255,205,90)); disc(15,9,1,(255,240,180))
px[14,8]=(255,255,255,255)
# etincelles
for (x,y) in ((25,5),(26,6),(24,6),(25,7),(6,14),(28,20)):
    if px[x,y][3]: px[x,y]=(255,240,200,255)
# petit crane en bas a droite
skull=[".###.","#####","#.#.#","#####",".#.#."]
for j,row in enumerate(skull):
    for i,ch in enumerate(row):
        if ch=='#': px[22+i,23+j]=(234,223,196,255)
        elif ch=='.' and j in (2,): px[22+i,23+j]=(255,60,60,255)
out=sys.argv[1]
sizes=[16,24,32,48,64,128,256]
imgs=[img.resize((s,s),Image.NEAREST) for s in sizes]
imgs[-1].save(out,format="ICO",sizes=[(s,s) for s in sizes],append_images=imgs[:-1])
img.resize((256,256),Image.NEAREST).save(out.replace(".ico","_256.png"))
print("icone ok")
