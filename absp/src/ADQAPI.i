// ADQAPI.i

%module ADQAPI
%{
// Include the header files for the C++ compiler
// This part is copied verbatim to the output file

#include "ADQAPI.h"

%}

%include <std_string.i>
%include <std_vector.i>

%rename(pin_function) ADQPortParametersPin::function;
%rename(function_parameters) ADQParameters::function;

// Ignoring deprecated functions
%ignore ADQAPI_GetRevision;
%ignore ADQControlUnit_DeleteADQDSP;
%ignore ADQControlUnit_DeleteDSU;
%ignore ADQControlUnit_DeleteADQ112;
%ignore ADQControlUnit_DeleteADQ114;
%ignore ADQControlUnit_DeleteADQ214;
%ignore ADQControlUnit_DeleteADQ212;
%ignore ADQControlUnit_DeleteADQ108;
%ignore ADQControlUnit_DeleteADQ412;
%ignore ADQControlUnit_DeleteSDR14;
%ignore ADQControlUnit_DeleteADQ1600;
%ignore ADQControlUnit_DeleteADQ208;

%include "ADQAPI.h"
