################################################################################
# F E T C H   L E D G E R   T E S T S
################################################################################
cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(fetch-ledger)

# CMake configuration
include(${FETCH_ROOT_CMAKE_DIR}/BuildTools.cmake)

# Compiler Configuration
setup_compiler()

add_fetch_test(proof_of_work_test fetch-ledger consensus/proof_of_work.cpp)
add_fetch_test(ledger_identifier_test fetch-ledger identifier_test.cpp)
add_fetch_gtest(chaincode_test fetch-ledger chaincode)
add_fetch_gtest(executors_test fetch-ledger executors)
target_include_directories(executors_test PRIVATE chaincode)
add_fetch_test(main_chain_test fetch-ledger consensus/main_chain.cpp) 
add_fetch_test(transaction_types_test fetch-ledger transaction_types.cpp)
