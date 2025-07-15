// Digitizers.i

%module digitizers
%{
// Include the header files for the C++ compiler
// This part is copied verbatim to the output file

#include "ADQ_descriptions.hpp"

#include "Digitizer.hpp"

#include "ADQ214.hpp"
#include "ADQ412.hpp"
#include "ADQ14_FWDAQ.hpp"
#include "ADQ14_FWPD.hpp"
#include "ADQ36_FWDAQ.hpp"
%}

%include <std_string.i>
%include <std_vector.i>

%ignore ABCD::ADQ214::ADQ214;
%ignore ABCD::ADQ214::~ADQ214;

%ignore ABCD::ADQ412::ADQ412;
%ignore ABCD::ADQ412::~ADQ412;

%ignore ABCD::ADQ14_FWDAQ::ADQ14_FWDAQ;
%ignore ABCD::ADQ14_FWDAQ::~ADQ14_FWDAQ;

%ignore ABCD::ADQ14_FWPD::ADQ14_FWPD;
%ignore ABCD::ADQ14_FWPD::~ADQ14_FWPD;

%ignore ABCD::ADQ36_FWDAQ::ADQ36_FWDAQ;
%ignore ABCD::ADQ36_FWDAQ::~ADQ36_FWDAQ;

// Include the header files for the SWIG parser

%include "events.h"

%include "Digitizer.hpp"
%include "ADQ214.hpp"
%include "ADQ412.hpp"
%include "ADQ14_FWDAQ.hpp"
%include "ADQ14_FWPD.hpp"
%include "ADQ36_FWDAQ.hpp"
