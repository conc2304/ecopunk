import subprocess,glob,re,struct,sys
GT=sys.argv[1]
for s in ("blueprint","quadrant","temporal"):
    fr=sorted(glob.glob(f"{GT}/{s}/act_*.bmp")); pat=re.sub(r"_\d+\.bmp$","_%03d.bmp",fr[0])
    raw=subprocess.run(["ffmpeg","-v","error","-i",pat,"-vf","format=gray16le,scale=64:36:flags=area,format=gray16le","-f","rawvideo","-"],capture_output=True,check=True).stdout
    n=64*36; frames=[struct.unpack("<%dH"%n,raw[i*n*2:(i+1)*n*2]) for i in range(len(raw)//(n*2))]
    q=[[round(v/257)*257 for v in f] for f in frames]
    mad=lambda a,b: sum(abs(x-y) for x,y in zip(a,b))/n/65535
    hf=[mad(frames[i],frames[i+1]) for i in range(len(frames)-1)]; lf=[mad(q[i],q[i+1]) for i in range(len(q)-1)]
    mx=max(abs(a-b) for a,b in zip(hf,lf)); mean=sum(hf)/len(hf)
    rel=sorted(abs(a-b)/a for a,b in zip(hf,lf) if a>1e-5)
    print(s,"pairs",len(hf),"mean raw16 %.5f mean raw8 %.5f maxAbsErr %.5f median relErr %.3f p95 relErr %.3f"%(mean,sum(lf)/len(lf),mx,rel[len(rel)//2],rel[int(.95*len(rel))]))
