# SPDX-License-Identifier: GPL-2.0-only
# Converts an arbitrary file into a C header holding a byte array + length.
# Invoked in script mode:
#   cmake -DINPUT=<file> -DOUTPUT=<header> -DSYMBOL=<name> -P embed_asset.cmake

file(READ "${INPUT}" hex_content HEX)
string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" bytes "${hex_content}")
list(LENGTH bytes num_bytes)

set(body "")
set(col 0)
foreach(b ${bytes})
  string(APPEND body "0x${b},")
  math(EXPR col "${col}+1")
  if(col EQUAL 16)
    string(APPEND body "\n  ")
    set(col 0)
  endif()
endforeach()

string(TOUPPER "${SYMBOL}" guard)
string(REGEX REPLACE "[^A-Za-z0-9]" "_" guard "${guard}")

file(WRITE "${OUTPUT}"
"#ifndef ${guard}_H
#define ${guard}_H
/* Auto-generated from ${INPUT}. Do not edit. */
static const unsigned char ${SYMBOL}[] = {
  ${body}
};
static const unsigned long ${SYMBOL}_len = ${num_bytes};
#endif
")
