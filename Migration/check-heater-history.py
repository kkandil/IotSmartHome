import sqlite3,json
c=sqlite3.connect('file:/home/khaled/smarthome/data/smarthome.sqlite?mode=ro',uri=True)
for r in c.execute("select body from documents where collection='Devices'"):
 d=json.loads(r[0])
 if any(x in d.get('Name','').lower() for x in ['heat','testdev']):
  print({k:d.get(k) for k in ['id','Name','firmwareVersion']})
  print([dict(zip(['time','value','confirmed','source'],a)) for a in c.execute('select time,value,confirmed,source from control_changes where device=? order by id desc limit 8',(d['id'],))])
