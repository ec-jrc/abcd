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
%}

%include <std_string.i>
%include <std_vector.i>

// Include the header files for the SWIG parser

%include "events.h"

%include "Digitizer.hpp"
%include "ADQ214.hpp"
%include "ADQ412.hpp"
%include "ADQ14_FWDAQ.hpp"
%include "ADQ14_FWPD.hpp"
