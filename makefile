
CCX = g++
CCXFLAGS = -std=c++11 -pthread -Wall -Werror

BTREEDIR = btrees
TESTDIR = tests
BMKDIR = benchmarks
OUTDIR = build

BMKMAINS = eval
BTREETESTMAINS = test_btree
OTHERTESTMAINS = test_util tests_hc test_ws

BTREEHS = $(wildcard $(BTREEDIR)/*.h)
TESTHS = $(wildcard $(TESTSDIR)/*.h)
TESTCCS = $(wildcard $(TESTSDIR)/*.cc)
BMKHS = $(wildcard $(BMKDIR)/*.h)
BMKCCS = $(wildcard $(BMKDIR)/*.cc)

TESTMAINS = $(BTREETESTMAINS) $(OTHERTESTMAINS)
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

BTREETESTRUNTARGETS = $(patsubst %, %.tstolc, $(BTREETESTMAINS)) \
					  $(patsubst %, %.tsthybrid, $(BTREETESTMAINS)) \
					  $(patsubst %, %.tstbr, $(BTREETESTMAINS))
OTHERTESTRUNTARGETS = $(patsubst %, %.tst, $(OTHERTESTMAINS))
BMKRUNTARGETS = $(patsubst %, %.bmk, $(BMKMAINS))

tst.all: $(BTREETESTRUNTARGETS) $(OTHERTESTRUNTARGETS)

bmk.all: $(BMKRUNTARGETS)

%.tstolc: $(OUTDIR)/test_%
	$< olc

%.tsthybrid: $(OUTDIR)/test_%
	$< hybrid

%.tstbr: $(OUTDIR)/test_%
	$< br

%.tst: $(OUTDIR)/test_%
	$<

%.bmk: $(OUTDIR)/bmk_%
	$<
