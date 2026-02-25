# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#
# Two-step cross-language test:
#   1. Generator creates a .bin from .json
#   2. Verifier reads the .bin and checks it against .json
#
# Usage:
#   cmake -DGENERATOR="cmd;arg1" -DVERIFIER="cmd;arg1" -DJSON=path -DTMPBIN=path -P run_gen_verify.cmake

if(NOT GENERATOR OR NOT VERIFIER OR NOT JSON OR NOT TMPBIN)
    message(FATAL_ERROR "Required variables: GENERATOR, VERIFIER, JSON, TMPBIN")
endif()

# Ensure output directory exists
get_filename_component(tmpdir "${TMPBIN}" DIRECTORY)
file(MAKE_DIRECTORY "${tmpdir}")

# Step 1: Generate
separate_arguments(GEN_CMD NATIVE_COMMAND "${GENERATOR}")
execute_process(
    COMMAND ${GEN_CMD} "${JSON}" "${TMPBIN}"
    RESULT_VARIABLE gen_rc
    OUTPUT_VARIABLE gen_out
    ERROR_VARIABLE gen_err
)
if(NOT gen_rc EQUAL 0)
    message(FATAL_ERROR "Generator failed (rc=${gen_rc}):\n${gen_out}\n${gen_err}")
endif()
message(STATUS "Generator: ${gen_out}")

# Step 2: Verify
separate_arguments(VER_CMD NATIVE_COMMAND "${VERIFIER}")
execute_process(
    COMMAND ${VER_CMD} "${TMPBIN}" "${JSON}"
    RESULT_VARIABLE ver_rc
    OUTPUT_VARIABLE ver_out
    ERROR_VARIABLE ver_err
)
if(NOT ver_rc EQUAL 0)
    message(FATAL_ERROR "Verifier failed (rc=${ver_rc}):\n${ver_out}\n${ver_err}")
endif()
message(STATUS "Verifier: ${ver_out}")
