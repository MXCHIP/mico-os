import unittest
import pexpect
import time

global config
global mtfterm

class TestMDNS(unittest.TestCase):

	def testdname_size(self):
		mtfterm.sendline("mdns-dname-size-tests");
		mtfterm.expect(self, "Success");

	def testdname_cmp(self):
		mtfterm.sendline("mdns-dname-cmp-tests");
		mtfterm.expect(self, "Success");

	def testincrement_name(self):
		mtfterm.sendline("mdns-increment-name-tests");
		mtfterm.expect(self, "Success");

	def testtxt_to_c_ncpy(self):
		mtfterm.sendline("mdns-txt-to-c-ncpy-tests");
		mtfterm.expect(self, "Success");

	def testresponder(self):
		mtfterm.sendline("mdns-responder-tests");
		mtfterm.expect(self, "Success");

