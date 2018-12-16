
CCX = g++
CCXFLAGS = -std=c++11 -pthread -Wall -Werror

BTREEDIR = btrees
TESTDIR = tests
BMKDIR = benchmarks
OUTDIR = build

BMKMAINS = eval
BTREETESTMAINS = test_btree
OTHERTESTMAINS = test_util test_ws test_btree_hybrid

BTREEHS = $(wildcard $(BTREEDIR)/*.h)
TESTHS = $(wildcard $(TESTSDIR)/*.h)
TESTCCS = $(wildcard $(TESTSDIR)/*.cc)
BMKHS = $(wildcard $(BMKDIR)/*.h)
BMKCCS = $(wildcard $(BMKDIR)/*.cc)

TESTMAINS = $(BTREETESTMAINS) $(OTHERTESTMAINS)
TESTMAINSTARGETS = $(patsubst %, $(OUTDIR)/test_%, $(TESTMAINS))
BMKMAINSTARGETS = $(patsubst %, $(OUTDIR)/bmk_%, $(BMKMAINS))

CCXFLAGS += -I $(BTREEDIR) -I $(TESTDIR) -I $(BMKDIR) -I btrees/libcuckoo/

.PHONY: all

all: $(TESTMAINSTARGETS) $(BMKMAINSTARGETS)

$(OUTDIR)/test_%: $(TESTDIR)/%.cc $(BTREEHS) $(TESTHS)
	@mkdir -p $(OUTDIR)
	$(CCX) $(CCXFLAGS) -ggdb -o $@ $<

$(OUTDIR)/bmk_%: $(BMKDIR)/%.cc $(BTREEHS) $(BMKHS)
	@mkdir -p $(OUTDIR)
	
	$(CCX) $(CCXFLAGS) -O3 -o $@ $<

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
	@true
