import sqlite3,json,datetime
from zoneinfo import ZoneInfo
c=sqlite3.connect('file:/home/khaled/smarthome/data/smarthome.sqlite?mode=ro',uri=True)
def stamp(t):return datetime.datetime.fromtimestamp(t/1000,ZoneInfo('Europe/Berlin')).isoformat()
for at,body in c.execute("select at,body from activity_history where home='Home_Germany' and kind='automation' order by at desc limit 100"):
 d=json.loads(body)
 if d.get('name')=='LivingMotion_Off' and at>1793487600000:
  print(json.dumps({'at':stamp(at),'status':d.get('status'),'conditions':[{k:x.get(k) for k in ['varName','group','actual','varValue','matched','remainingSeconds'] } for x in d.get('conditions',[]) if x.get('varName') in ['Light','Presence']],'actions':d.get('actions')}))
for var in ['Temp','BMPTemp','Light']:
 rows=c.execute("select time,value from history_points where home='Home_Germany' and device=1014 and variable=? order by time desc limit 4",(var,)).fetchall()
 print(var,[(stamp(t),v) for t,v in rows])
