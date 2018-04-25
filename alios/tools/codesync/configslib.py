import ConfigParser

class configslib:
    def __init__(self):
        self.srcbase = ""
        self.dstlink = ""
        self.synctype = "notnano"
        self.modules=""

    def get_vendor_configs(self, base):
        config = ConfigParser.RawConfigParser()
        config.read('vendors.cfg')
        self.dstbase = base
        if config.has_section(base):
            if config.has_option(base, 'source'):
                self.srcbase = config.get(base, 'source')
            if config.has_option(base, 'dest'):
                self.dstlink = config.get(base, 'dest')
            if config.has_option(base, 'modules'):
                self.modules = config.get(base, "modules")
            if config.has_option(base, 'type'):
                self.synctype = config.get(base, 'type')
            return 0
        return 1

