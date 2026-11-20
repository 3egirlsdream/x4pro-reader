#!/usr/bin/env python3
"""Compile the unmodified production TXT reader against in-memory HAL/display adapters."""
from pathlib import Path
import shutil
import subprocess
import argparse
parser = argparse.ArgumentParser()
parser.add_argument('--book', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / 'build/txt-paging-test'
out.mkdir(parents=True, exist_ok=True)
for name in ['TxtReaderActivity.cpp', 'TxtReaderActivity.h']:
    shutil.copyfile(root / 'src/activities/reader' / name, out / name)
(out/'TestSupport.h').write_text(r'''
#pragma once
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define tr(id) #id
constexpr int O_RDWR=1,O_CREAT=2,UI_12_FONT_ID=12;
inline unsigned long millis() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline size_t writes=0,flushes=0;
inline bool failWrite=false;
struct HalFile {
 std::string* bytes=nullptr; size_t pos=0;
 operator bool()const{return bytes!=nullptr;}
 size_t size()const{return bytes?bytes->size():0;}
 size_t position()const{return pos;}
 bool seek(size_t n){pos=n;return bytes&&n<=bytes->size();}
 int read(void* p,size_t n){n=std::min(n,size()-pos);if(n)memcpy(p,bytes->data()+pos,n);pos+=n;return n;}
 size_t write(const void*p,size_t n){if(!bytes||failWrite)return 0;++writes;if(pos+n>bytes->size())bytes->resize(pos+n);memcpy(bytes->data()+pos,p,n);pos+=n;return n;}
 void flush(){++flushes;} void close(){bytes=nullptr;pos=0;}
};
struct StorageType {
 std::map<std::string,std::string> files;
 bool openFileForRead(const char*,const std::string& p,HalFile& f){if(!files.count(p))return false;f={&files[p],0};return true;}
 bool openFileForWrite(const char*,const std::string&p,HalFile&f){files[p].clear();f={&files[p],0};return true;}
 HalFile open(const char*p,int){return {&files[p],0};}
};
inline StorageType Storage;
inline size_t bytesRead=0,readCalls=0,measureCalls=0,measuredCodepoints=0;
#include "TxtReadCache.h"
class Txt {
 std::string path,cache; mutable TxtReadCache readCache; std::unique_ptr<uint8_t[]> storage{new uint8_t[TxtReadCache::CAPACITY]}; public:
 Txt(std::string p,std::string):path(p),cache("/.crosspoint/"+p.substr(1)){readCache.reset(storage.get());}
 bool load(){return Storage.files.count(path);}
 size_t getFileSize()const{return Storage.files.at(path).size();}
 std::string getTitle()const{return path;}
 const std::string&getCachePath()const{return cache;}
 void setupCacheDir(){}
 bool readContent(uint8_t*p,size_t at,size_t count)const{return readCache.read([](void*c,uint8_t*out,size_t pos,size_t len){auto*self=static_cast<const Txt*>(c);++readCalls;bytesRead+=len;memcpy(out,Storage.files.at(self->path).data()+pos,len);return true;},const_cast<Txt*>(this),getFileSize(),p,at,count);}
};
namespace performance {enum class Stage{TxtLayout,TxtPersist};inline void record(Stage,unsigned long){}struct Scope{Scope(Stage){}};}
struct EpdFontFamily {enum {REGULAR=0,BOLD=1};};
struct FontCacheManager {struct Scope{void endScanAndPrewarm(){}};Scope createPrewarmScope(){return {};}};
struct GfxRenderer {
 std::vector<std::string> drawn;
 int getScreenWidth()const{return 480;}int getScreenHeight()const{return 800;}
 int getLineHeight(int)const{return 24;}
 void getOrientedViewableTRBL(int*t,int*r,int*b,int*l)const{*t=*r=*b=*l=0;}
 bool isSdCardFont(int)const{return false;}
 void ensureSdCardFontReady(int,const char*,int)const{}
 int getTextAdvanceX(int,const char*p,int)const{++measureCalls;int n=0;for(;*p;++p)if((uint8_t(*p)&0xc0)!=0x80){n+=12;++measuredCodepoints;}return n;}
 void clearScreen(){drawn.clear();}
 void drawCenteredText(int,int,const char*,bool,int){}
 void drawText(int,int,int,const char*p){drawn.emplace_back(p);}
 void displayBuffer(){}
 FontCacheManager*getFontCacheManager(){static FontCacheManager f;return &f;}
};
struct MappedInputManager {struct Labels {const char*btn1="",*btn2="",*btn3="",*btn4="";}; Labels mapLabels(const char*,const char*,const char*,const char*)const{return {};}};
struct RenderLock {RenderLock()=default;template<class T>explicit RenderLock(T&){}};
struct ScreenshotInfo {enum class ReaderType{Txt};ReaderType readerType;char title[128];int currentPage=0,totalPages=0,progressPercent=0;};
struct CrossPointSettings {
 enum{LEFT_ALIGN,RIGHT_ALIGN,CENTER_ALIGN,JUSTIFIED};
 int screenMargin=12,paragraphAlignment=LEFT_ALIGN,textAntiAliasing=0;
 int getReaderFontId()const{return 14;}
 struct Spec{bool showsTitle()const{return true;}};Spec statusBarSpec()const{return {};}
};
inline CrossPointSettings SETTINGS;
struct Theme {int getStatusBarHeight()const{return 24;}};
struct UITheme {static Theme&getInstance(){static Theme t;return t;}};
struct GUIType {template<class...T>void drawStatusBar(T&&...){}template<class...T>void drawButtonHints(T&&...){} };
inline GUIType GUI;
struct BidiUtils {static constexpr int RTL_PARAGRAPH_PROBE_DEPTH=10;static bool startsWithRtl(const char*,int){return false;}};
namespace ReaderUtils {inline void displayBaseWithRefreshCycle(GfxRenderer&,int&){}inline void displayWithRefreshCycle(GfxRenderer&,int&){} template<class F>void renderAntiAliased(GfxRenderer&,F f){f();}}
inline size_t progressWrites=0;
inline bool failProgress=false;
namespace ProgressFile {enum class Format{Txt};inline bool open(const std::string&p,Format,HalFile&f){return Storage.openFileForRead("TRS",p+"/progress.bin",f);}
inline bool writeAtomic(const std::string&p,const uint8_t*b,size_t n){if(failProgress)return false;++progressWrites;Storage.files[p+"/progress.bin"].assign((const char*)b,n);return true;}}
template<class T>requires std::is_array_v<T> auto makeUniqueNoThrow(size_t n){return std::unique_ptr<T>(new(std::nothrow)std::remove_extent_t<T>[n]);}
template<class T,class...A>requires (!std::is_array_v<T>) auto makeUniqueNoThrow(A&&...a){return std::unique_ptr<T>(new(std::nothrow)T(std::forward<A>(a)...));}
class ReaderActivity {
 protected:GfxRenderer&renderer;MappedInputManager&mappedInput;std::string bookPath;int pagesUntilFullRefresh=0;
 ReaderActivity(const char*,GfxRenderer&r,MappedInputManager&m,std::string p,bool):renderer(r),mappedInput(m),bookPath(p){}
 virtual bool loadBook()=0;virtual std::string getBookTitle()const=0;virtual void renderBook()=0;void markPageRendered(){} void requestUpdate(){}
 public:virtual ~ReaderActivity()=default;void onEnter(){loadBook();}void render(RenderLock&&){renderBook();}
 virtual bool pageTurn(bool)=0;virtual bool skipPages(int)=0;virtual bool isAtEndOfBook()const=0;virtual void onReturnFromEndOfBook()=0;virtual ScreenshotInfo getScreenshotInfo()const=0;
};
''')
# All hardware shims include one shared adapter; production UTF-8 and serialization remain real.
for header in ['Txt.h','BidiUtils.h','FontCacheManager.h','GfxRenderer.h','HalStorage.h','I18n.h','Memory.h',
               'Performance.h','CrossPointSettings.h','ProgressFile.h','ReaderActivity.h','ReaderUtils.h','fontIds.h','components/UITheme.h']:
    p=out/header;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "TestSupport.h"\n')
(out/'test.cpp').write_text(r'''
#include "TxtReaderActivity.h"
#include <cassert>
#include <iostream>
#include <fstream>
int main(int argc,char**argv) {
 GfxRenderer gfx;MappedInputManager input;
 std::string book;book.reserve(3000000);for(int i=0;i<50000;++i)book+="这是低功耗电子书的中文段落，验证首屏按需分页。\n";
 Storage.files["/large.txt"]=book;
 TxtReaderActivity reader(gfx,input,"/large.txt",false);reader.onEnter();
 auto started=millis();reader.render(RenderLock{});auto elapsed=millis()-started;
 assert(readCalls==1&&bytesRead<=8192&&measureCalls<2000);
 assert(reader.getScreenshotInfo().currentPage==1&&!reader.isAtEndOfBook());
 std::cout<<"3 MB cold open: "<<elapsed<<" ms host layout; "<<readCalls<<" read, "<<bytesRead<<" bytes (display/I-O emulated)\n";
 for(int i=0;i<20;++i){assert(reader.pageTurn(true));reader.render(RenderLock{});}
 const auto readsBefore=readCalls, writesBefore=writes, flushesBefore=flushes, progressBefore=progressWrites;
 for(int repeat=0;repeat<50;++repeat)reader.render(RenderLock{});
 assert(readCalls==readsBefore&&writes==writesBefore&&flushes==flushesBefore&&progressWrites==progressBefore);
 std::cout<<"50 same-page redraws: 0 book reads, 0 index writes/flushes, 0 progress writes\n";
 const auto page=gfx.drawn;const int expected=reader.getScreenshotInfo().currentPage;
 TxtReaderActivity resumed(gfx,input,"/large.txt",false);resumed.onEnter();resumed.render(RenderLock{});
 assert(resumed.getScreenshotInfo().currentPage==expected&&gfx.drawn==page);
 assert(resumed.pageTurn(false));resumed.render(RenderLock{});assert(resumed.pageTurn(true));resumed.render(RenderLock{});assert(gfx.drawn==page);
 assert(Storage.files["/.crosspoint/large.txt/index.bin"].size()<1024);
 assert(resumed.skipPages(10));resumed.render(RenderLock{});assert(resumed.getScreenshotInfo().currentPage==expected+10);
 assert(resumed.skipPages(-10));resumed.render(RenderLock{});assert(resumed.getScreenshotInfo().currentPage==expected);
 Storage.files["/tiny.txt"]="一二三四五六七八九十。";
 TxtReaderActivity tiny(gfx,input,"/tiny.txt",false);tiny.onEnter();tiny.render(RenderLock{});assert(tiny.pageTurn(true));assert(tiny.isAtEndOfBook());tiny.onReturnFromEndOfBook();assert(!tiny.isAtEndOfBook());
 const auto beforeReflow=Storage.files["/.crosspoint/large.txt/progress.bin"];
 Storage.files["/.crosspoint/large.txt/index.bin"]=std::string(30,'x');
 TxtReaderActivity damaged(gfx,input,"/large.txt",false);damaged.onEnter();damaged.render(RenderLock{});assert(!damaged.isAtEndOfBook());
 for(int i=0;i<expected+5;++i) damaged.render(RenderLock{});
 assert(damaged.getScreenshotInfo().currentPage==expected && gfx.drawn==page);
 assert(Storage.files["/.crosspoint/large.txt/progress.bin"]==beforeReflow);
 auto legacy=beforeReflow.substr(0,4);
 Storage.files["/.crosspoint/large.txt/progress.bin"]=legacy;
 SETTINGS.screenMargin+=8;
 TxtReaderActivity reflow(gfx,input,"/large.txt",false);reflow.onEnter();
 for(int i=0;i<expected+10;++i)reflow.render(RenderLock{});
 const auto migrated=Storage.files["/.crosspoint/large.txt/progress.bin"];
 uint32_t previousOffset=0,newOffset=0;
 memcpy(&previousOffset,beforeReflow.data()+4,4);memcpy(&newOffset,migrated.data()+4,4);
 assert(migrated.size()==12 && newOffset>0 && newOffset<=previousOffset);
 SETTINGS.screenMargin-=8;
 Storage.files["/fail.txt"]=book;
 TxtReaderActivity failure(gfx,input,"/fail.txt",false);failure.onEnter();
 const auto beforeFailure=progressWrites;failWrite=true;failure.render(RenderLock{});assert(progressWrites==beforeFailure);
 failWrite=false;failProgress=true;failure.render(RenderLock{});assert(progressWrites==beforeFailure);
 failProgress=false;failure.render(RenderLock{});assert(progressWrites==beforeFailure+1);
 assert(Storage.files["/.crosspoint/fail.txt/index.bin"].size()>=34);
 std::cout<<"Failed index and progress writes retry without advancing persisted position\n";
 std::cout<<"Incremental offsets, append-only cache, resume, backward/forward, EOF and damaged cache tests passed\n";
 if(argc>1) {
  std::ifstream file(argv[1],std::ios::binary); assert(file);
  Storage.files["/water.txt"]=std::string(std::istreambuf_iterator<char>(file),{});
  TxtReaderActivity water(gfx,input,"/water.txt",false);water.onEnter();
  size_t worstCalls=0,worstCodepoints=0;const auto initialReads=readCalls,initialBytes=bytesRead;
  for(int page=0;page<120;++page) {
   measureCalls=measuredCodepoints=0;water.render(RenderLock{});
   worstCalls=std::max(worstCalls,measureCalls);worstCodepoints=std::max(worstCodepoints,measuredCodepoints);
   assert(measureCalls<2000&&measuredCodepoints<40000);
   if(!water.pageTurn(true)||water.isAtEndOfBook())break;
  }
  std::cout<<"Actual TXT read-ahead: "<<readCalls-initialReads<<" reads, "<<bytesRead-initialBytes<<" bytes\n";
  std::cout<<"Actual TXT first 120 pages: max measure calls="<<worstCalls<<" max measured codepoints="<<worstCodepoints<<"; no paragraph-quadratic fallback (HAL/display/font metrics emulated)\n";
 }
}
''')
command=['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined','-g','-I'+str(out),
         '-I'+str(root/'lib/Txt'),'-I'+str(root/'lib/Utf8'),'-I'+str(root/'lib/Serialization'),
         str(out/'TxtReaderActivity.cpp'),str(root/'lib/Txt/CjkLineBreak.cpp'),str(root/'lib/Utf8/Utf8.cpp'),str(out/'test.cpp'),'-o',str(out/'test')]
subprocess.run(command,check=True)
subprocess.run([str(out/'test')]+([str(args.book.resolve())] if args.book else []),check=True,timeout=30)

subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',
                '-I'+str(root/'lib/Txt'),str(root/'test/reading_performance/TxtReadCacheTest.cpp'),
                '-o',str(out/'cache-test')],check=True)
subprocess.run([str(out/'cache-test')],check=True,timeout=30)
