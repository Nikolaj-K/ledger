################################################################################
# F E T C H   V E C T O R I S A T I O N  E X A M P L E S
################################################################################
cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(fetch-vectorise)

# CMake configuration
include(${FETCH_ROOT_CMAKE_DIR}/BuildTools.cmake)

# Compiler Configuration
setup_compiler()

setup_library_examples(fetch-ledger)

