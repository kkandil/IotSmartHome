import sqlite3
c=sqlite3.connect('file:/home/khaled/smarthome/data/smarthome.sqlite?mode=ro',uri=True)
print(c.execute('select source,count(*) from control_changes group by source').fetchall())
