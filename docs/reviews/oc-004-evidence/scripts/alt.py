import subprocess,glob,re,struct,sys,math
GT=sys.argv[1]
def ranks(v):
    o=sorted(range(len(v)),key=lambda i:v[i]); r=[0]*len(v)
    for k,i in enumerate(o): r[i]=k
    return r
def pear(a,b):
    n=len(a); ma,mb=sum(a)/n,sum(b)/n
    c=sum((x-ma)*(y-mb) for x,y in zip(a,b)); d=math.sqrt(sum((x-ma)**2 for x in a)*sum((y-mb)**2 for y in b)); return c/d if d else float('nan')
sp=lambda a,b: pear(ranks(a),ranks(b))
def grids(pat,w,h):
    raw=subprocess.run(["ffmpeg","-v","error","-i",pat,"-vf",f"format=gray16le,scale={w}:{h}:flags=area,format=gray16le","-f","rawvideo","-"],capture_output=True,check=True).stdout
    n=w*h; return [struct.unpack("<%dH"%n,raw[i*n*2:(i+1)*n*2]) for i in range(len(raw)//(n*2))]
for s in ("quadrant","blueprint","fragment","temporal","blob"):
    fr=sorted(glob.glob(f"{GT}/{s}/act_*.bmp")); pat=re.sub(r"_\d+\.bmp$","_%03d.bmp",fr[0])
    ref=grids(pat,1280,720) if False else None
    full=[]  # full-res MAD via 320x180 as a stand-in for 'full' (area-averaged, 16x cheaper in python)
    g320=grids(pat,320,180); n=320*180
    gt=[sum(abs(a-b) for a,b in zip(g320[i],g320[i+1]))/n/65535 for i in range(len(g320)-1)]
    out=[]
    for (w,h) in ((64,36),(128,72),(32,18),(16,9)):
        g=grids(pat,w,h); m=w*h
        mad=[sum(abs(a-b) for a,b in zip(g[i],g[i+1]))/m/65535 for i in range(len(g)-1)]
        rms=[math.sqrt(sum((a-b)**2 for a,b in zip(g[i],g[i+1]))/m)/65535 for i in range(len(g)-1)]
        out.append(f"{w}x{h}: MAD P{pear(mad,gt):.2f}/S{sp(mad,gt):.2f} RMS P{pear(rms,gt):.2f}/S{sp(rms,gt):.2f}")
    print(s,"| vs 320x180 MAD |"," | ".join(out),flush=True)
