#!/usr/bin/env python3
"""Visualize geometry emitted by test_back_navigation.py; this is not a device screenshot."""
import argparse,csv,json
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
rows=[]
for row in csv.DictReader(args.geometry.open()):
 rows.append({k:v if k=='theme' else int(v) for k,v in row.items()})
args.output.mkdir(parents=True,exist_ok=True)
data=json.dumps(rows)
html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>X4 Pro 返回触区证据</title>
<style>body{font:16px system-ui;background:#f5f6f7;color:#222;max-width:1000px;margin:32px auto;padding:0 20px}h1{font-size:24px}select{font:inherit;padding:8px;margin-right:16px}svg{width:100%;max-height:65vh;background:white;border:1px solid #ddd;margin:20px 0}p{line-height:1.7}code{background:#eee;padding:4px}#detail{font-variant-numeric:tabular-nums}</style>
<h1>X4 Pro 返回命中区</h1><p>数据来自生产布局函数、主题参数与 SDK header 绘制的主机回归。不是实机截图。绿色为新触区，虚线为原 48px 方框，蓝色为实际 32px 图标边界；灰色状态栏和列表区域不可被返回触区侵占。</p>
<label>主题 <select id="theme"><option>Base</option><option>Lyra</option><option>Lyra3</option><option>RoundedRaff</option></select></label>
<label>方向 <select id="rotation"><option value="0">竖屏</option><option value="1">横屏 CW</option><option value="2">倒置竖屏</option><option value="3">横屏 CCW</option></select></label>
<label>字体光学偏移 <select id="scale"><option>0</option><option selected>5</option><option>12</option></select></label>
<p>偏移 0/5/12px 是字体度量边界样本，不代表设备的三个 UI 缩放档位。横屏样本加入 12px 安全区偏移。</p>
<svg id="screen" role="img" aria-label="标题栏触区示意"></svg><p id="detail"></p><p>触摸容差：每个轴最多 28px；按住 ≥1200ms 不执行屏幕返回。点按按下时绑定页面，切页、旋转或隐藏后失效。首帧发布前的触摸不会事后落在新出现的箭头上。天气底部按钮 50px 高，受版权/状态文本空间限制，中央间隔不再触发返回或刷新。</p>
<script>const rows=DATA;const canvas=document.getElementById("screen"),theme=document.getElementById("theme"),rotation=document.getElementById("rotation"),scale=document.getElementById("scale"),detail=document.getElementById("detail");function draw(){let r=rows.find(r=>r.theme===theme.value&&r.orientation===+rotation.value&&r.scale===+scale.value);canvas.setAttribute('viewBox',`0 0 ${r.screenWidth} 220`);let bx=r.bandX+4,iy=r.buttonY+8;canvas.innerHTML=`<rect x="${r.bandX}" y="${r.bandY}" width="${r.bandW}" height="${r.strip}" fill="#e5e7eb"/><rect x="0" y="${r.bandY+r.bandH}" width="${r.screenWidth}" height="130" fill="#f4f4f4"/><text x="110" y="${r.bandY+16}" font-size="12">STATUS BAR</text><text x="100" y="${iy+24}" font-size="24">Page title</text><text x="100" y="${r.bandY+r.bandH+48}" font-size="16">List / content</text><rect x="${r.targetX}" y="${r.targetY}" width="${r.targetW}" height="${r.targetH}" fill="#16a34a22" stroke="#15803d" stroke-width="2"/><rect x="${bx}" y="${r.buttonY}" width="48" height="48" fill="none" stroke="#666" stroke-dasharray="4 3"/><rect x="${bx+8}" y="${iy}" width="32" height="32" fill="#2563eb22" stroke="#2563eb"/><path d="M${bx+30} ${iy+7} l-9 9 9 9" fill="none" stroke="#111" stroke-width="3"/>`;detail.textContent=`${r.theme} · ${r.screenWidth}×${r.screenHeight} · 新触区 [${r.targetX},${r.targetY}) → [${r.targetX+r.targetW},${r.targetY+r.targetH})，${r.targetW}×${r.targetH}px`;}for(let e of [theme,rotation,scale])e.onchange=draw;draw();</script></html>'''.replace('DATA',data)
(args.output/'hit-targets.html').write_text(html)
from PIL import Image,ImageDraw,ImageFont
im=Image.new('RGB',(1080,650),'#f5f6f7');d=ImageDraw.Draw(im)
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',18);small=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',15)
d.text((30,20),'X4 Pro - production header geometry / host evidence',fill='#111',font=font)
d.text((30,50),'Green: navigation target   Blue: 32px icon   Gray outline: old 48px box',fill='#444',font=small)
for n,t in enumerate(['Base','Lyra','Lyra3','RoundedRaff']):
 r=next(r for r in rows if r['theme']==t and r['orientation']==0 and r['scale']==5)
 ox=30+(n%2)*530;oy=95+(n//2)*225
 d.text((ox,oy),f"{t} - target {r['targetW']} x {r['targetH']} px",fill='#111',font=font);oy+=35
 d.rectangle((ox,oy,ox+480,oy+170),fill='white',outline='#ccc')
 d.rectangle((ox,oy+r['bandY'],ox+480,oy+r['bandY']+r['strip']-1),fill='#e5e7eb')
 d.text((ox+108,oy+r['bandY']+3),'STATUS',font=small,fill='#555')
 d.line((ox,oy+r['bandY']+r['bandH'],ox+480,oy+r['bandY']+r['bandH']),fill='#bbb')
 x=ox+r['targetX'];y=oy+r['targetY'];d.rectangle((x,y,x+r['targetW']-1,y+r['targetH']-1),fill='#e0f5e8',outline='#15803d',width=2)
 bx=ox+r['bandX']+4;by=oy+r['buttonY'];d.rectangle((bx,by,bx+47,by+47),outline='#888')
 d.rectangle((bx+8,by+8,bx+39,by+39),fill='#e2ecff',outline='#2563eb')
 d.line([(bx+30,by+15),(bx+21,by+24),(bx+30,by+33)],fill='#111',width=3)
 d.text((ox+100,by+12),'Page title',font=font,fill='#111')
 d.text((ox+100,oy+130),'List / content',font=small,fill='#777')
d.text((30,578),'Portrait, optical drop 5px. Interactive HTML covers 48 layout samples.',font=small,fill='#333')
d.text((30,604),'Not a device screenshot. No panel latency or physical touch accuracy is inferred.',font=small,fill='#333')
im.save(args.output/'hit-targets.png')
print('Rendered 48 production geometry samples as HTML + PNG')
