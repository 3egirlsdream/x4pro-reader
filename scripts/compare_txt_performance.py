#!/usr/bin/env python3
"""Run the archived 0.4.1 production reader with its host harness and added I/O counters."""
from pathlib import Path
import argparse, subprocess, tarfile
p=argparse.ArgumentParser()
p.add_argument('--baseline-archive',type=Path,required=True)
p.add_argument('--book',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
out=root/'build/txt-baseline';out.mkdir(parents=True,exist_ok=True)
with tarfile.open(a.baseline_archive) as tar:
 for name in ['src/activities/reader/TxtReaderActivity.cpp','src/activities/reader/TxtReaderActivity.h','scripts/test_txt_paging.py']:
  data=tar.extractfile('x4pro-integrated-reader/'+name).read()
  (out/Path(name).name).write_bytes(data)
s=(out/'test_txt_paging.py').read_text()
s=s.replace('root = Path(__file__).resolve().parents[1]', 'root = Path('+repr(str(root))+')')
s=s.replace("out = root / 'build/txt-paging-test'",'out = Path('+repr(str(out/'compiled'))+')')
s=s.replace("root / 'src/activities/reader' / name",'Path('+repr(str(out))+') / name')
s=s.replace('struct HalFile {','inline size_t indexWrites=0,flushes=0;\nstruct HalFile {')
s=s.replace('if(!bytes)return 0;if(pos+n>','if(!bytes)return 0;++indexWrites;if(pos+n>')
s=s.replace('void flush(){}','void flush(){++flushes;}')
s=s.replace('namespace ProgressFile {','inline size_t progressWrites=0;\nnamespace ProgressFile {')
s=s.replace('{Storage.files[p+"/progress.bin"]','{++progressWrites;Storage.files[p+"/progress.bin"]')
s=s.replace(' const auto page=gfx.drawn;', ''' const auto beforeReads=readCalls,beforeWrites=indexWrites,beforeFlushes=flushes,beforeProgress=progressWrites;
 for(int repeat=0;repeat<50;++repeat)reader.render(RenderLock{});
 std::cout<<"50 same-page redraws: "<<readCalls-beforeReads<<" book reads, "<<indexWrites-beforeWrites<<" index writes, "<<flushes-beforeFlushes<<" flushes, "<<progressWrites-beforeProgress<<" progress writes\\n";
 const auto page=gfx.drawn;''')
s=s.replace('  size_t worstCalls=0,worstCodepoints=0;','  size_t worstCalls=0,worstCodepoints=0;const auto initialReads=readCalls,initialBytes=bytesRead;')
s=s.replace('  std::cout<<"Actual TXT first 120 pages:', '  std::cout<<"Actual TXT I/O: "<<readCalls-initialReads<<" reads, "<<bytesRead-initialBytes<<" bytes\\n";\n  std::cout<<"Actual TXT first 120 pages:')
script=out/'run.py';script.write_text(s)
subprocess.run(['python3',str(script),'--book',str(a.book.resolve())],check=True)
