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

%include "ADQAPI.h"
