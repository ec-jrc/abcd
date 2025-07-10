ABCD-VERSION = 1.0.0

# Modules that do not need a configuration file
SIMPLE_MODULES = dasa chafi cofi sofi califo wadi enfi gzad unzad fifo
# Modules that need a configuration file
COMPLEX_MODULES = pufi waan spec tofcalc
# Modules that need the hardware support, they are not compiled by default
HARDWARE_MODULES = abad2 abcd abps5000a abrp absp

EXECUTABLES_DIRS = bin replay convert examples
EXECUTABLES = \
bin/send_command.py \
bin/log_status_events.py \
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

.PHONY: all clean

all:
	$(foreach module,$(SIMPLE_MODULES) $(COMPLEX_MODULES) $(EXECUTABLES_DIRS),$(MAKE) -C $(module) || exit 1;)

clean:
	$(foreach module,$(SIMPLE_MODULES) $(COMPLEX_MODULES) $(HARDWARE_MODULES) $(EXECUTABLES_DIRS),$(MAKE) -C $(module) clean;)

DESTDIR ?=
PREFIX ?= /usr/local

ROOTDIR = $(DESTDIR)$(PREFIX)
SRCDIR = $(ROOTDIR)/src/abcd-$(ABCD-VERSION)
BINDIR = $(ROOTDIR)/bin
SHAREDIR = $(ROOTDIR)/share/abcd
CONFIGDIR = $(SHAREDIR)/configs
LIBEXECDIR = $(ROOTDIR)/libexec/abcd
# We need to manually add the $(DESTDIR) afterwards, not to mess up with ldconfig
LIBDIR = $(PREFIX)/lib/abcd

install: $(MODULES) $(EXECUTABLES_DIRS)
	mkdir -p $(ROOTDIR)

	@echo "Copying source code..."
	mkdir -p $(SRCDIR)
	cp -r * $(SRCDIR)

	@echo "Copying executables..."
	mkdir -p $(BINDIR)
	$(foreach module,$(SIMPLE_MODULES) $(COMPLEX_MODULES),install -v -m 755 $(module)/$(module) $(BINDIR);)
	$(foreach executable,$(EXECUTABLES),install -v -m 755 $(executable) $(BINDIR);)

	@echo "Installing node.js dependencies..."
	cd wit && npm install
	@echo "Copying wit..."
	mkdir -p $(LIBEXECDIR)
	cp -r wit $(LIBEXECDIR)

	@echo "Copying example configuration files..."
	mkdir -p $(CONFIGDIR)
	$(foreach module,$(COMPLEX_MODULES) $(HARDWARE_MODULES), \
	    mkdir -p $(CONFIGDIR)/$(module); \
	    $(foreach config, $(wildcard $(module)/configs/*.json), \
		    install -v -m 644 $(config) $(CONFIGDIR)/$(module)/;))

	@echo "Copying waan libraries..."
	mkdir -p $(DESTDIR)$(LIBDIR)
	$(foreach lib, $(wildcard waan/src/*.so), \
	    install -v -m 644 $(lib) $(DESTDIR)$(LIBDIR);)

	mkdir -p $(DESTDIR)/etc/ld.so.conf.d/
	echo "$(LIBDIR)" > $(DESTDIR)/etc/ld.so.conf.d/abcd.conf

	@echo "Copying startups..."
	mkdir -p $(SHAREDIR)/startup
	$(foreach startup, $(wildcard startup/*.sh), \
	    install -v -m 755 $(startup) $(SHAREDIR)/startup;)

	@echo "Copying examples..."
	mkdir -p $(SHAREDIR)/examples
	$(foreach example, $(wildcard examples/*), \
	    install -v -m 755 $(example) $(SHAREDIR)/examples;)

	@echo "Copying example data..."
	mkdir -p $(SHAREDIR)/data
	$(foreach datafile, $(wildcard data/example_data_*), \
	    install -v -m 644 $(datafile) $(SHAREDIR)/data;)

#@echo "Creating DEBIAN..."
#mkdir -p "DEBIAN/"
#@echo "Creating post-installation script..."
#echo "ldconfig" > "DEBIAN/postinst"
#chmod +x "DEBIAN/postinst"
#@echo "Creating control file..."
#echo "Package: abcd" > "DEBIAN/control"
#echo "Version: $(ABCD-VERSION)" >> "DEBIAN/control"
#echo "Section: science" >> "DEBIAN/control"
#echo "Priority: optional" >> "DEBIAN/control"
#echo "Architecture: amd64" >> "DEBIAN/control"
#echo "Depends: tmux, libc++1, libc++-dev, libc++abi-dev, libzmq5, libzmq3-dev, libjsoncpp-dev, libjsoncpp25, libjansson4, libjansson-dev, zlib1g, zlib1g-dev, libbz2-1.0, libbz2-dev, python3, python3-zmq, python3-numpy, python3-scipy, python3-matplotlib, nodejs, npm, linux-headers-generic, build-essential, dkms, libgsl-dev, lua5.4, liblua5.4-dev, liblua5.4-0, swig" >> "DEBIAN/control"
#echo "Maintainer: Cristiano Fontana" >> "DEBIAN/control"
#echo "Description: A data acquisition system for signal digitizers from multiple vendors" >> "DEBIAN/control"
#@echo "Creating deb file..."
#dpkg-deb -v --build deb/ abcd
