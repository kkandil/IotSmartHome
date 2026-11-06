import sqlite3,json
db=sqlite3.connect('file:/home/khaled/smarthome/data/smarthome.sqlite?mode=ro',uri=True)
for device in (1015,1016):
    rows=db.execute("SELECT confirmed,count(*),min(time),max(time) FROM control_changes WHERE home=? AND device=? AND variable='Power' GROUP BY confirmed",('Home_Germany',device)).fetchall()
    print(json.dumps({'device':device,'groups':rows}))
