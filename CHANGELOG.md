# Changelog

## 1.x.x Release

### 1.3.0

- In this update the libraries of the `waan` module were simplified.
  There are now some convenient macroes that simplify the reading of the parameters from the `json_t` object.

- New `waan` library: `libLogDecay`.
  This library computes the logarithm of the decaying tail of pulses and it fits it with a straight line.
  The purpose it to determine the decaying constants.

- The building of the binary packages was also changed, it is now easier to maintain.
  The result is that the web interface module `wit` is in a separate `deb` package called `abcd-wit`.

### 1.2.2

- The PSD filter `pufi` can now use different polygons on the PSD vs energy plot for the different channels.

- Ported `absp` and `waan` to an improved logging utility.
  Now the outputs of the two modules can be saved to a log file with the `-l` option.

### 1.1.1

- The configuration parsing of `waan` is now more robust in the case of missing `user_config` entries.

- The interface for the ADQ36 in the `absp` module can now also use the "level matrix" feature of the digitizer.

### 1.1.0

- New waan library: `libMultiLeftThr`.
  This library is based on `libLeftThr` and it can detect mulitple pulses on one waveform.
  
- New waan library: `libMultiPSD`.
  This library is based on `libPSD` and it can calculate the integral of the multiple pulses from `libMultiLeftThr`.
  
- `cofi` can now directly read data from `ade` and `adr` files.
  To use this feature the data input address (`-A` option) should start with `file://`

### 1.0.0

- This is the first release of ABCD with a new build system based on CMake and a new installation structure.
There are deb binary packages available for Ubuntu 24.04 LTS.

- This release changes completely the installation directory structure.
  The aim was to simplify the startups of ABCD.
  Now modules are installed in the `PATH` and thus they may be called from wherever.
  New example startups are provided in `/usr/share/abcd/startup`.


- Relevant files for ABCD are now saved in `/usr/share/abcd` where there are subdirectories with example configurations for the various modules.

- The standard `waan` libraries are now installed in a system-wide directory and `ldconfig` is made aware of them.
  This means that in the configurations of waan libraries should be called simply by their name (e.g. `libPSD.so`) without specifying the path.
  In in `/usr/share/abcd/waan` there are example configurations.

- Users may still write their own custom libraries.
  Copy the directory `/usr/share/abcd/waan_libraries` to a user-owned location and put the custom library in `waan_libraries/src` directory.
  The provided Makefile should be able to compile simple libraries that do not depend on other libraries.
  To use the custom library the `waan` configuration should then have the full path of the custom library.

- The web interface is located in `/usr/libexec/abcd/wit` and the `deb` package contains all the dependencies.
  The script `wit.sh` is available in the `PATH` to easily launch the interface.
  The script may accept one argument that is the configuration file.

- The `abcd-core` package contains the core system with no dependencies on hardware modules.
  This package should be sufficient to use ABCD with replays.
  The other two binary packages depend on the interfaces provided by the vendors, so install them only if you have installed the vendor libraries.
  The installation may be done with:

```
dpkg -i abcd-core_1.0.0_amd64.deb
```

  If there are unmet dependencies it will complain, then use

```
apt install --fix-broken
```

