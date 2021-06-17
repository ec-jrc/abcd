# File names
MODULES = abcd hivo dasa pqrs chafi cofi wafi enfi pufi gzad unzad fifo waps waph spec tofcalc replay_raw replay_events adr2ade

.PHONY: all clean $(MODULES)

all: $(MODULES) include/defaults.json
	$(foreach module,$(MODULES),$(MAKE) -C $(module);)

include/defaults.json: include/defaults.h
	python3 bin/defaults2json.py include/defaults.h -o include/defaults.json

clean:
	$(foreach module,$(MODULES),$(MAKE) -C $(module) clean;)
