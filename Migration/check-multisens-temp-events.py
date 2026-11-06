import sqlite3,json
c=sqlite3.connect('file:/home/khaled/smarthome/data/smarthome.sqlite?mode=ro',uri=True)
c.row_factory=sqlite3.Row
for r in c.execute("select body,matched,last_run from conditional_events where home=?",('Home_Germany',)):
 d=json.loads(r['body'])
 if 'LivingMotion' in d.get('name',''):print('EVENT',json.dumps(dict(r)))
for r in c.execute("select at,body from activity_history where home=? and kind='automation' order by at desc limit 100",('Home_Germany',)):
 d=json.loads(r['body'])
 if 'LivingMotion_Off' in str(d):print('RUN',json.dumps(dict(r)))
for r in c.execute("select collection,body from documents where db=?",('Home_Germany',)):
 d=json.loads(r['body'])
 if str(d.get('DeviceId',d.get('deviceID',d.get('id',''))))=='1014':
  if 'VarName' in d:print('VALUE',json.dumps(d))
for r in c.execute("select variable,min(value) low,max(value) high,count(*) samples from history_points where home=? and device=1014 and variable in ('Temp','BMPTemp') and time>strftime('%s','now')*1000-86400000 group by variable",('Home_Germany',)):print('TEMP_DAY',json.dumps(dict(r)))
