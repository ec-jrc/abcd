# File names
MODULES = dasa chafi cofi sofi wafi wadi enfi pufi gzad unzad fifo waps waph waan spec tofcalc replay convert

.PHONY: all clean $(MODULES)

all: $(MODULES)
	$(foreach module,$(MODULES),$(MAKE) -C $(module);)

clean:
	$(foreach module,$(MODULES),$(MAKE) -C $(module) clean;)
