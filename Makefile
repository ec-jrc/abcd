ABCD-VERSION = 1.0.0

MODULES = dasa chafi cofi sofi califo wadi enfi pufi gzad unzad fifo waan spec tofcalc
HARDWARE_MODULES = abad2 abcd abps5000a abrp absp
EXECUTABLES_DIRS = bin replay convert examples
EXECUTABLES = \
bin/send_command.py \
bin/read_events.py \
bin/filter_timestamps \
bin/sort_ade \
bin/split_events_files.py \
bin/fan-in.py \
bin/fan-out.py \
bin/save_to_file.py \
bin/close_file.py \
bin/plot_Evst.py \
bin/plot_PSD.py \
bin/plot_spectra.py \
bin/plot_timestamps.py \
bin/plot_ToF.py \
bin/plot_waveforms.py \
replay/replay_raw.py \
replay/replay_raw \
replay/replay_events \
convert/adr2adeadw.py \
convert/ade2ascii \
convert/ade2ascii.m \
convert/ade2ascii.py \
convert/adr2ade \
convert/adr2events.py \
convert/adr2configs.py \
convert/adw2ascii

DEBDIR = deb/
SRCDIR = usr/src/abcd-$(ABCD-VERSION)/
BINDIR = usr/bin/
LIBDIR = usr/lib/abcd/
SHAREDIR = usr/share/abcd/
CONFIGDIR = $(SHAREDIR)/configs/
WITDIR = opt/local/abcd/

.PHONY: all clean deb

all: $(MODULES) $(EXECUTABLES_DIRS)
	$(foreach module,$(MODULES),$(MAKE) -C $(module);)
	$(foreach executables_dirs,$(EXECUTABLES_DIRS),$(MAKE) -C $(executables_dirs);)

clean:
	$(foreach module,$(MODULES),$(MAKE) -C $(module) clean;)
	$(foreach module,$(HARDWARE_MODULES),$(MAKE) -C $(module) clean;)
	$(foreach executables_dirs,$(EXECUTABLES_DIRS) ,$(MAKE) -C $(executables_dirs) clean;)

deb: $(MODULES) $(EXECUTABLES_DIRS)
	@echo "Cleaning..."
	#make clean
	rm -rf $(DEBDIR)/DEBIAN/
	rm -rf $(DEBDIR)/usr/
	rm -rf $(DEBDIR)/etc/
	rm -rf $(DEBDIR)/opt/
	@echo "Copying sources..."
	mkdir -p $(DEBDIR)$(SRCDIR)
	cp common_definitions.mk $(DEBDIR)$(SRCDIR)
	cp Makefile $(DEBDIR)$(SRCDIR)
	$(foreach executables_dirs,$(EXECUTABLES_DIRS) ,cp -r $(executables_dirs) $(DEBDIR)$(SRCDIR);)
	$(foreach module,$(MODULES),cp -r $(module) $(DEBDIR)$(SRCDIR)$(module);)
	$(foreach module,$(HARDWARE_MODULES),cp -r $(module) $(DEBDIR)$(SRCDIR)$(module);)
	@echo "Compiling..."
	make
	@echo "Copying executables..."
	mkdir -p $(DEBDIR)$(BINDIR)
	$(foreach module,$(MODULES),cp $(module)/$(module) $(DEBDIR)$(BINDIR);)
	$(foreach executable,$(EXECUTABLES),cp $(executable) $(DEBDIR)$(BINDIR);)
	@echo "Installing node.js dependencies..."
	#cd wit && npm install
	@echo "Copying wit..."
	mkdir -p $(DEBDIR)$(WITDIR)
	cp -r wit $(DEBDIR)$(WITDIR)
	@echo "Copying example configuration files..."
	mkdir -p $(DEBDIR)$(CONFIGDIR)
	$(foreach module,$(HARDWARE_MODULES),mkdir -p $(DEBDIR)$(CONFIGDIR)$(module);)
	$(foreach module,$(HARDWARE_MODULES),cp $(module)/configs/*.json $(DEBDIR)$(CONFIGDIR)$(module)/;)
	mkdir -p $(DEBDIR)$(CONFIGDIR)tofcalc/
	cp tofcalc/configs/*.json $(DEBDIR)$(CONFIGDIR)tofcalc/
	mkdir -p $(DEBDIR)$(CONFIGDIR)waan/
	cp waan/configs/*.json $(DEBDIR)$(CONFIGDIR)waan/
	@echo "Copying waan libraries..."
	mkdir -p $(DEBDIR)$(LIBDIR)
	cp waan/src/lib*.so $(DEBDIR)$(LIBDIR)
	mkdir -p $(DEBDIR)/etc/ld.so.conf.d/
	echo "/""$(LIBDIR)" > $(DEBDIR)/etc/ld.so.conf.d/abcd.conf
	@echo "Copying startups..."
	mkdir -p $(DEBDIR)$(SHAREDIR)
	cp -r startup $(DEBDIR)$(SHAREDIR)
	@echo "Copying examples..."
	cp -r examples $(DEBDIR)$(SHAREDIR)
	@echo "Copying example data..."
	mkdir -p $(DEBDIR)$(SHAREDIR)data/
	cp data/example_data* $(DEBDIR)$(SHAREDIR)data/
	@echo "Creating DEBIAN..."
	mkdir -p $(DEBDIR)"DEBIAN/"
	@echo "Creating post-installation script..."
	echo "ldconfig" > $(DEBDIR)"DEBIAN/postinst"
	chmod +x $(DEBDIR)"DEBIAN/postinst"
	@echo "Creating control file..."
	echo "Package: abcd" > $(DEBDIR)"DEBIAN/control"
	echo "Version: $(ABCD-VERSION)" >> $(DEBDIR)"DEBIAN/control"
	echo "Section: science" >> $(DEBDIR)"DEBIAN/control"
	echo "Priority: optional" >> $(DEBDIR)"DEBIAN/control"
	echo "Architecture: amd64" >> $(DEBDIR)"DEBIAN/control"
	echo "Depends: tmux, libc++1, libc++-dev, libc++abi-dev, libzmq5, libzmq3-dev, libjsoncpp-dev, libjsoncpp25, libjansson4, libjansson-dev, zlib1g, zlib1g-dev, libbz2-1.0, libbz2-dev, python3, python3-zmq, python3-numpy, python3-scipy, python3-matplotlib, nodejs, npm, linux-headers-generic, build-essential, dkms, libgsl-dev, lua5.4, liblua5.4-dev, liblua5.4-0, swig" >> $(DEBDIR)"DEBIAN/control"
	echo "Maintainer: Cristiano Fontana" >> $(DEBDIR)"DEBIAN/control"
	echo "Description: A data acquisition system for signal digitizers from multiple vendors" >> $(DEBDIR)"DEBIAN/control"
	@echo "Creating deb file..."
	dpkg-deb -v --build deb/ abcd
