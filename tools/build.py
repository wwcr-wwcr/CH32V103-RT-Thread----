"""Build the CH32V307 base + LVGL with MounRiver GCC. No IDE-generated makefile required."""
from pathlib import Path
import argparse, subprocess, concurrent.futures, os
parser=argparse.ArgumentParser()
parser.add_argument('--toolchain', default=r'D:\MounRiver_Studio3\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];os.chdir(root)
bin=Path(args.toolchain);gcc=str(bin/'riscv-none-embed-gcc.exe')
out=Path('obj');out.mkdir(exist_ok=True)
includes=['Core','Debug','User','Peripheral/inc','drivers','rtthread','rtthread/include','rtthread/include/libc','rtthread/libcpu/risc-v/common','rtthread/components/drivers/include','rtthread/components/finsh','lvgl','lvgl_port']
flags=['-march=rv32imacxw','-mabi=ilp32','-msmall-data-limit=8','-msave-restore','-Os','-g','-ffunction-sections','-fdata-sections','-fno-common','-fsigned-char','-Wall']
sources=[]
for folder in ['Core','Debug','User','Peripheral','drivers','rtthread','Startup','lvgl','lvgl_port']:
 sources.extend(f for f in Path(folder).rglob('*') if f.suffix in ['.c','.S'] and f.name!='startup_ch32v30x_D8.S')
def compile(src):
 obj=out/'check'/src.with_suffix(src.suffix+'.o');obj.parent.mkdir(parents=True,exist_ok=True)
 cmd=[gcc]+flags+['-I'+s for s in includes]+(['-std=gnu99'] if src.suffix=='.c' else ['-x','assembler-with-cpp'])+['-c',str(src),'-o',str(obj)]
 r=subprocess.run(cmd,capture_output=True,text=True,errors='replace')
 if r.stdout or r.stderr: print(r.stdout+r.stderr,flush=True)
 if r.returncode: raise RuntimeError(str(src))
 return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: objects=list(pool.map(compile,sorted(sources)))
rsp=out/'objects.rsp';rsp.write_text('\n'.join('"'+f.as_posix()+'"' for f in objects))
elf=out/'CH32V307-RTT-LVGL.elf'
subprocess.run([gcc]+flags+['-T','Ld/Link.ld','-nostartfiles','-Wl,--gc-sections,-Map,obj/CH32V307-RTT-LVGL.map','--specs=nano.specs','--specs=nosys.specs','@'+str(rsp),'-lm','-o',str(elf)],check=True)
for fmt,ext in [('ihex','hex'),('binary','bin')]:
 subprocess.run([str(bin/'riscv-none-embed-objcopy.exe'),'-O',fmt,str(elf),str(elf.with_suffix('.'+ext))],check=True)
subprocess.run([str(bin/'riscv-none-embed-size.exe'),str(elf)],check=True)
print('PASS: full compile and link',flush=True)
