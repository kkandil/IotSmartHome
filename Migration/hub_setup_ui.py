import os,subprocess,sys,re,xml.etree.ElementTree as E
from pathlib import Path
adb=os.path.join(os.environ['LOCALAPPDATA'],'Android/Sdk/platform-tools/adb.exe')
def run(*a):return subprocess.check_output([adb,'-s','emulator-5554',*a])
def tree():
 run('shell','uiautomator','dump','/sdcard/hub-ui.xml')
 return E.fromstring(run('shell','cat','/sdcard/hub-ui.xml'))
mode=sys.argv[1]
if mode=='tap':
 target=sys.argv[2];nodes=[n for n in tree().iter('node') if n.get('text')==target or n.get('content-desc')==target]
 if not nodes:raise SystemExit('Control not found: '+target)
 n=nodes[0];x,y,x2,y2=map(int,re.findall(r'\d+',n.get('bounds')));run('shell','input','tap',str((x+x2)//2),str((y+y2)//2))
elif mode=='shot':
 tree();run('shell','screencap','-p','/sdcard/hub-guide.png');run('pull','/sdcard/hub-guide.png',str(Path(__file__).parent/'Hub-Setup-Guide/images'/sys.argv[2]));print('Screenshot saved: '+sys.argv[2])
elif mode=='dump':
 for n in tree().iter('node'):
  if n.get('text') or n.get('content-desc'): print(n.get('text') or n.get('content-desc'),n.get('bounds'))
