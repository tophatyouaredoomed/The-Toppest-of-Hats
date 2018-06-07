import os
import re

glue = "./bin/parser-test-glue"
skips = []

for filename in os.listdir("./parser-test/"):
    m = re.match("(.*)[.]in$", filename)
    if m:
        testname = m.group(1)
        print "# testing", testname, "..."
        if testname in skips: 
            print "SKIP", testname
        else:
            inp = "parser-test/%s.in" % testname
            out = "parser-test/%s.out" % testname
            if os.system("%(glue)s <%(inp)s >%(out)s" % vars()) != 0:
                raise "%s returned nonzero" % glue
            if os.system("diff %(out)s %(out)s.gold" % vars()) != 0:
                os.system("python ./parser-dump-tool.py diff2dot %(out)s %(out)s.gold -o %(out)s.dot" % vars())
                os.system("dot %(out)s.dot -Tps -o %(out)s.ps" % vars())
                raise "diff returned nonzero, see MISMATCH node in %(out)s.ps" % vars()
            os.unlink(out)
            print "PASS", testname
        