
CCX = g++
CCXFLAGS = -std=c++11

BTREEDIR = btrees
TESTDIR = tests
BMKDIR = benchmarks
OUTDIR = build

TESTMAINS = test1
BMKMAINS = bmk1

BTREEHS = $(wildcard $(BTREEDIR)/*.h)
TESTHS = $(wildcard $(TESTSDIR)/*.h)
TESTCCS = $(wildcard $(TESTSDIR)/*.cc)
BMKHS = $(wildcard $(BMKDIR)/*.h)
BMKCCS = $(wildcard $(BMKDIR)/*.cc)

TESTMAINSTARGETS = $(patsubst %, $(OUTDIR)/test_%, $(TESTMAINS))
BMKMAINSTARGETS = $(patsubst %, $(OUTDIR)/bmk_%, $(BMKMAINS))

CCXFLAGS += -I $(BTREEDIR) -I $(TESTDIR) -I $(BMKDIR)

.PHONY: all tst bmk

all: $(OUTDIR) tst bmk

tst: $(TESTMAINSTARGETS)

bmk: $(BMKMAINSTARGETS)

$(OUTDIR):
	mkdir $@

$(OUTDIR)/test_%: $(TESTDIR)/%.cc $(TESTCCS) $(BTREEHS) $(TESTHS)
	$(CCX) $(CCXFLAGS) -ggdb -o $@ $<

$(OUTDIR)/bmk_%: $(BMKDIR)/%.cc $(BMKCCS) $(BTREEHS) $(BMKHS)
	$(CCX) $(CCXFLAGS) -O3 -o $@ $<

clean:
	rm -rf $(OUTDIR)
