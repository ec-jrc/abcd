################################################################################
DOT = dot
PANDOC = pandoc

MARKDOWN = 
GRAPH = abcd abcd_simplified hijk lmno pqrs fifo califo

PANDOC_FORMAT = html
PANDOC_OUTPUT = $(MARKDOWN:=.$(PANDOC_FORMAT))

DOT_TERMINAL = pdf
DOT_OUTPUT = $(GRAPH:=.$(DOT_TERMINAL))

.PHONY: all clean

all: $(DOT_OUTPUT) $(PANDOC_OUTPUT)

%.$(DOT_TERMINAL): %.dot
	$(DOT) -T$(DOT_TERMINAL) $< -o $@

%.html: %.md
	$(PANDOC) -s -t$(PANDOC_FORMAT) $< -o $@

clean:
	rm -f $(DOT_OUTPUT) $(PANDOC_OUTPUT)
