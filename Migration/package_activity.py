from pathlib import Path
import tarfile, shutil, hashlib
root=Path('.');out=root/'Migration/Activity-Health-Update'
files=['activity-history.js','esp-action.js','events.js','home-sync.js','local-server.js','protocol.js','device-templates.js','control-history.js','variable-state.js']
for name in files:shutil.copy2(root/'HubServer'/name,out/name)
with tarfile.open(out/'activity-hub-update.tar.gz','w:gz') as t:
 for name in files+['install-activity-update.sh']:t.add(out/name,arcname=name,recursive=False)
shutil.copy2(Path(__import__('os').environ['TEMP'])/'smarthome-relay-startup-20261025/app/outputs/apk/debug/app-debug.apk',out/'SmartHome-activity-health-debug.apk')
shutil.copy2(root/'PlatformIO/RelayController_1020/.pio/build/esp12e/firmware.bin',out/'3IOSwitch-1.3.0.bin')
shutil.copy2(root/'PlatformIO/MultiSens/.pio/build/nodemcuv2/firmware.bin',out/'MultiSens-1.3.0.bin')
(out/'SHA256SUMS.txt').write_text('\n'.join(hashlib.sha256((out/n).read_bytes()).hexdigest()+'  '+n for n in ['activity-hub-update.tar.gz','SmartHome-activity-health-debug.apk','3IOSwitch-1.3.0.bin','MultiSens-1.3.0.bin'])+'\n',encoding='utf-8')
print('Packaged Pi update, APK, and two firmware builds')
