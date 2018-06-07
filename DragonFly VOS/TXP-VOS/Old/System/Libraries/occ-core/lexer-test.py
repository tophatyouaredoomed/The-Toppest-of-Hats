import os
import re
import sys

glue = "./bin/lexer-test-glue"
skips = ["hex-number", "number", "oct-number", "asm", 
         "operators-punctuators2"
	]

for filename in os.listdir("./lexer-test/"):
    m = re.match("(.*)[.]in$", filename)
    if m:
        testname = m.group(1)
        print "# testing", testname, "..."
        if testname in skips: 
            print "SKIP", testname
        else:
            inp = "lexer-test/%s.in" % testname
            out = "lexer-test/%s.out" % testname
            cmd = "%(glue)s <%(inp)s >%(out)s" % vars()
            status = os.system(cmd)
            if status != 0:
                print >>sys.stderr, "+ " + cmd
                print >>sys.stderr, "returned %d" % status
                sys.exit(1)
            cmd = "diff %(out)s %(out)s.gold" % vars()
            status = os.system(cmd)
            if status != 0:
                print >>sys.stderr, "+ " + cmd
                print >>sys.stderr, "returned %d" % status
                sys.exit(1)
            os.unlink(out)
            print "PASS", testname
        