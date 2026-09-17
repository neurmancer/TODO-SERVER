.PHONY: all clean run rebuild

all clean run rebuild:
	$(MAKE) -C Server $@
