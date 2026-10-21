#!/usr/bin/env python3
"""Exercise production TXT storage and performance export with failure-injecting host HAL."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
out = root/'build/performance-storage'
out.mkdir(parents=True, exist_ok=True)
(out/'Support.h').write_text(r'''
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
inline uint32_t clockMs=0;
inline unsigned long millis(){return clockMs;}
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define CROSSPOINT_VERSION "1.6.5-integrated.0.4.2-perf.1"
inline bool noPsram=false,shortRead=false,shortWrite=false;
inline size_t reads=0,writes=0,psramBytes=0;
struct HalFile {
 std::string* bytes=nullptr;size_t pos=0;
 size_t size()const{return bytes?bytes->size():0;}
 bool seek(size_t n){pos=n;return bytes&&n<=size();}
 int read(void*p,size_t n){++reads;n=std::min(n,size()-pos);if(shortRead&&n)--n;memcpy(p,bytes->data()+pos,n);pos+=n;return n;}
 size_t write(const void*p,size_t n){++writes;if(shortWrite&&n)--n;if(pos+n>size())bytes->resize(pos+n);memcpy(bytes->data()+pos,p,n);pos+=n;return n;}
 void close(){bytes=nullptr;}void flush(){}bool available(){return pos<size();}
};
struct StorageType {
 std::map<std::string,std::string> files;
 bool exists(const char*p){return files.count(p);}bool mkdir(const char*){return true;}
 bool openFileForRead(const char*,const std::string&p,HalFile&f){if(!files.count(p))return false;f={&files[p],0};return true;}
 bool openFileForWrite(const char*,const std::string&p,HalFile&f){files[p].clear();f={&files[p],0};return true;}
 bool remove(const char*p){files.erase(p);return true;}bool removeDir(const char*){return true;}
 bool rename(const char*a,const char*b){if(!files.count(a))return false;files[b]=files[a];files.erase(a);return true;}
};
inline StorageType Storage;
struct HalMemory {
 struct Deleter{void operator()(uint8_t*p)const{if(p){psramBytes-=16384;delete[] p;}}};
 using PsramBuffer=std::unique_ptr<uint8_t[],Deleter>;
 static PsramBuffer allocatePsram(size_t n){if(noPsram)return {};psramBytes+=n;return PsramBuffer(new uint8_t[n]);}
};
struct FsHelpers {
 static bool ext(const std::string&s,const char*e){return s.ends_with(e);}
 static bool hasTxtExtension(const std::string&s){return ext(s,".txt");}
 static bool hasBmpExtension(const std::string&s){return ext(s,".bmp");}
 static bool hasJpgExtension(const std::string&s){return ext(s,".jpg");}
};
struct JpegToBmpConverter{static bool jpegFileToBmpStream(HalFile&,HalFile&){return false;}};
''')
for name in ['Arduino.h','HalStorage.h','HalMemory.h','Logging.h','FsHelpers.h','JpegToBmpConverter.h']:
 (out/name).write_text('#include "Support.h"\n')
(out/'test.cpp').write_text(r'''
#include <Txt.h>
#include <Performance.h>
#include <cassert>
#include <iostream>
#include <thread>
int main(){
 std::string text(100000,'x');for(size_t i=0;i<text.size();++i)text[i]=char(i*37);
 Storage.files["/book.txt"]=text;uint8_t buffer[8192];
 {
  Txt book("/book.txt","/.crosspoint");assert(book.load());assert(psramBytes==16384);
  assert(book.readContent(buffer,0,8192));assert(!memcmp(buffer,text.data(),8192));
  const auto before=reads;assert(book.readContent(buffer,0,8192));assert(reads==before);
  shortRead=true;assert(!book.readContent(buffer,80000,8192));shortRead=false;
  assert(book.readContent(buffer,80000,8192));assert(!memcmp(buffer,text.data()+80000,8192));
  assert(!book.readContent(buffer,99999,2));
 }
 assert(psramBytes==0);noPsram=true;
 {Txt book("/book.txt","/.crosspoint");assert(book.load());assert(!psramBytes);assert(book.readContent(buffer,2000,8192));assert(!memcmp(buffer,text.data()+2000,8192));}
 using namespace performance;
 auto run=[](){for(int i=0;i<10000;++i)record(Stage::TxtLayout,7);};
 std::thread a(run),b(run),c(run),d(run);a.join();b.join();c.join();d.join();
 const auto& counter=counters[static_cast<unsigned>(Stage::TxtLayout)];
 assert(counter.count==40000&&counter.totalMs==280000&&counter.maxMs==7);
 clockMs=0xfffffff0U;{Scope timing(Stage::Boot);clockMs=0x10;}assert(counters[0].lastMs==32);
 const auto before=writes;record(Stage::TxtPersist,1);assert(writes==before);
 assert(saveSnapshot());const auto saved=Storage.files["/.crosspoint/performance-last.csv"];
 assert(saved.find("txt_layout,40000,280000,7,7")!=std::string::npos);
 shortWrite=true;assert(!saveSnapshot());assert(Storage.files["/.crosspoint/performance-last.csv"]==saved);
 shortWrite=false;assert(saveSnapshot());
 std::cout<<"Production TXT: exact short-read failure/retry, cache reuse, release and no-PSRAM fallback passed\n";
 std::cout<<"Performance: 40000 concurrent samples, timer rollover, no hot-path writes, snapshot and failed-export preservation passed\n";
}
''')
subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',
 '-I'+str(out),'-I'+str(root/'lib/Txt'),'-I'+str(root/'lib/Performance'),str(root/'lib/Txt/Txt.cpp'),
 str(root/'lib/Performance/Performance.cpp'),str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=30)
