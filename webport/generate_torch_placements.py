from pathlib import Path
import yaml
indoor=[];outdoor=[]
for p in Path('assets/dungeon').glob('dungeon-*.yaml'):
 room=int(p.stem.split('-')[1]);d=yaml.safe_load(p.read_text())
 for layer in ['Layer1','Layer2','Layer3']:
  for o in d.get(layer,[]):
   if 'Torch' not in str(o.get('n','')):continue
   ident=int(o['n'].split('-')[0],16);x=o['x']*8;y=o['y']*8
   a,b=map(int,str(o.get('s','0*0')).split('*'));count=a*4+b+1
   for i in range(count):
    if ident in [0x55,0x56]:
     for xx in [8,24]:indoor.append((room,x+i*96+xx,y+8,0))
    elif ident in [0x7f,0x80]:
     for yy in [8,24]:indoor.append((room,x+8,y+i*96+yy,0))
    elif ident==0x3d:indoor.append((room,x+i*48+8,y+6,0))
    elif ident==0x87:indoor.append((room,x+8,y+i*48+6,0))
    elif ident==0x120:indoor.append((room,x+8,y+6,0))
for p in Path('assets/overworld').glob('overworld-*.yaml'):
 area=int(p.stem.split('-')[1]);d=yaml.safe_load(p.read_text())
 if area>=128:continue
 bx=(area&7)*512;by=((area&63)>>3)*512
 for e in d.get('Exits',[]):
  x,y=e['xy']
  for dx in [-32,32]:outdoor.append((area,bx+x+dx,by+y+24,1))
# Hand-authored line of braziers down the pyramid, including its summit.
for y in [110,180,270,350,435,505]:
 for x in [2004,2076]:outdoor.append((91,x,0x600+y,1))
indoor=sorted(set(indoor));outdoor=sorted(set(outdoor))
with open('src/rt_torch_placements.h','w') as f:
 f.write('// Generated from original room objects, entrances, and authored prologue positions.\n')
 f.write('typedef struct TorchPlacement { unsigned short area,x,y; unsigned char prop; } TorchPlacement;\n')
 for name,data in [('kIndoorTorches',indoor),('kOutdoorTorches',outdoor)]:
  f.write(f'static const TorchPlacement {name}[] = {{\n')
  for t in data:f.write('  {%d,%d,%d,%d},\n'%t)
  f.write('};\n')
print('Permanent interior flame positions:',len(indoor),'Outdoor torch candidates:',len(outdoor))
