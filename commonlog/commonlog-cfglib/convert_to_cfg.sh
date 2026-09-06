#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

IN=${1:-$SCRIPT_DIR/log-default.txt}
OUT=${2:-$SCRIPT_DIR/commonlog.conf}

[ -f "$IN" ] || { echo "convert_to_cfg.sh: input file $IN not found" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"

python3 - "$IN" "$OUT" <<'PY'
import sys,struct

CFG_MAGIC=0x43464731
CFG_VERSION=2
MAX_APP=16
MAX_CTX=16
MAX_LCTX=16

inp,out=sys.argv[1:3]

gs,gl=1,4
st={"add_clogdetails_in_msg":1,"sink_file_max_size_mb":5,"sink_file_rotate_count":2}
apps={}
ctx={}
lctx={}
count=0

def die(m):
    sys.exit(f"convert_to_cfg.sh: {m}")

def num(name,v,line,maxv):
    if not v.isdigit() or int(v)>maxv:
        die(f"line {line}: invalid {name} '{v}'")
    return int(v)

with open(inp) as f:
    for line_no,line in enumerate(f,1):
        line=line.strip()
        if not line or line.startswith('#'):
            continue

        flds=line.split()
        typ=flds.pop(0)

        if typ=="GLOBAL":
            if len(flds)!=2: die(f"line {line_no}: GLOBAL expects 2 values")
            gs=num("sink",flds[0],line_no,5)
            gl=num("level",flds[1],line_no,6)

        elif typ=="SETTING":
            if len(flds)!=2: die(f"line {line_no}: SETTING expects 2 values")
            k,v=flds

            if k=="add_clogdetails_in_msg":
                if v not in ("0","1"):
                    die(f"line {line_no}: invalid {k} '{v}'")
                st[k]=int(v)

            elif k=="sink_file_max_size_mb":
                st[k]=num(k,v,line_no,65535)
                if st[k]==0:
                    die(f"line {line_no}: invalid {k} '{v}'")

            elif k=="sink_file_rotate_count":
                st[k]=num(k,v,line_no,65535)

            else:
                die(f"line {line_no}: unknown SETTING key '{k}'")

        elif typ=="APP":
            if len(flds)!=3: die(f"line {line_no}: APP expects 3 values")

            app=num("app_id",flds[0],line_no,65535)
            sink=num("sink",flds[1],line_no,5)
            level=num("level",flds[2],line_no,6)

            if app not in apps and len(apps)>=MAX_APP:
                die(f"line {line_no}: APP {app} would exceed limit")

            apps[app]=(sink,level)

        elif typ=="CTX":
            if len(flds)!=3: die(f"line {line_no}: CTX expects 3 values")

            app=num("app_id",flds[0],line_no,65535)
            cid=num("context_id",flds[1],line_no,65535)
            lvl=num("level",flds[2],line_no,6)

            if (app,cid) not in ctx and len(ctx)>=MAX_CTX:
                die(f"line {line_no}: CTX {app} {cid} would exceed limit")

            ctx[(app,cid)]=lvl

        elif typ=="LCTX":
            if len(flds)!=2: die(f"line {line_no}: LCTX expects 2 values")

            cid=num("context_id",flds[0],line_no,65535)
            lvl=num("level",flds[1],line_no,6)

            if cid not in lctx and len(lctx)>=MAX_LCTX:
                die(f"line {line_no}: LCTX {cid} would exceed limit")

            lctx[cid]=lvl

        else:
            die(f"line {line_no}: unknown record type '{typ}'")

        count+=1

with open(out,"wb") as f:

    f.write(struct.pack(
        "<IIIII",
        CFG_MAGIC,
        CFG_VERSION,
        len(apps),
        len(ctx),
        len(lctx)
    ))

    f.write(struct.pack("<BB",gs,gl))

    f.write(struct.pack(
        "<BBHH",
        st["add_clogdetails_in_msg"],
        0,
        st["sink_file_max_size_mb"],
        st["sink_file_rotate_count"]
    ))

    for app,(sink,level) in apps.items():
        f.write(struct.pack("<HBB",app,sink,level))

    for (app,cid),lvl in ctx.items():
        f.write(struct.pack("<HHBB",app,cid,lvl,0))

    for cid,lvl in lctx.items():
        f.write(struct.pack("<HBB",cid,lvl,0))

print(f"convert_to_cfg.sh: wrote {count} record(s) from {inp} to {out}")
PY

chmod 0644 "$OUT"
