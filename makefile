
CCX = g++
CCXFLAGS = -std=c++11 -pthread

BTREEDIR = btrees
TESTDIR = tests
BMKDIR = benchmarks
OUTDIR = build

TESTMAINS = test_btree test_util
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

all: tst bmk

tst: $(TESTMAINSTARGETS)

bmk: $(BMKMAINSTARGETS)

$(OUTDIR):
	mkdir $@

$(OUTDIR)/test_%: $(OUTDIR) $(TESTDIR)/%.cc $(TESTCCS) $(BTREEHS) $(TESTHS)
	$(CCX) $(CCXFLAGS) -ggdb -o $@ $(filter-out $<, $^)

$(OUTDIR)/bmk_%: $(OUTDIR) $(BMKDIR)/%.cc $(BMKCCS) $(BTREEHS) $(BMKHS)
	$(CCX) $(CCXFLAGS) -O3 -o $@ $(filter-out $<, $^)

clean:
	rm -rf $(OUTDIR)

#####
# Some convenience targets for running benchmark and tests

TESTRUNTARGETS = $(patsubst %, %.tst, $(TESTMAINS))
BMKRUNTARGETS = $(patsubst %, %.bmk, $(BMKMAINS))

#.PHONY: tst.all bmk.all $(TESTRUNTARGETS) $(BMKRUNTARGETS)

tst.all: $(TESTRUNTARGETS)

bmk.all: $(BMKRUNTARGETS)

%.tst: $(OUTDIR)/test_%
	$<

%.bmk: $(OUTDIR)/bmk_%
	$<
