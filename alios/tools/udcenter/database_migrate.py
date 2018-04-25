import sys, traceback
import sqlite3 as sql

class Migrate():
    def __init__(self):
        self.dbase = sql.connect('controller.db', check_same_thread = False)

    def database_excute_sqlcmd(self, sqlcmd):
        with self.dbase:
            cur = self.dbase.cursor()
            try:
                cur.execute(sqlcmd)
            except:
                traceback.print_exc()
                ret = None
            else:
                ret = cur.fetchall()
        return ret

    def database_migrate(self):
        map = {
            'd8cb8a46411d':'bcdd562273b85f5f',
            '4437e6eec8f5':'35b77ece53b582e2',
            'd8cb8a42f007':'be14abb1d065f09e',
            'b827eba017fc':'b01e3af4cc9066b6'
            }
        sqlcmd = 'SELECT * FROM Devices'
        rows = self.database_excute_sqlcmd(sqlcmd)
        if rows == None:
            return False
        for row in rows:
            (device, uuid, timeout) = row
            client_uuid = device[0:12]
            if client_uuid not in map:
                print "no match device {0}".format(row)
                continue
            new_device = map[client_uuid] + device[12:]
            sqlcmd = "INSERT INTO Devices VALUES('{0}', '{1}', '{2}')".format(new_device, uuid, timeout)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                print "error: insert '{0}' into databese failed".format([new_device, uuid, timeout])
                continue
            sqlcmd = "DELETE FROM Devices WHERE device = '{0}'".format(device)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                print "error: delete '{0}' from database failed".format([device, uuid, timeout])
        return True

if __name__ == '__main__':
    mg = Migrate()
    ret = mg.database_migrate()
    if ret:
        print "succeed"
        ret = 0
    else:
        print "failed"
        ret = 1
    sys.exit(ret)
