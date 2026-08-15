cmake_minimum_required(VERSION 3.20)

enable_testing()

add_executable(qsim_tests test_gates.cpp test_qasm.cpp test_vqe_qaoa.cpp)
target_link_libraries(qsim_tests PRIVATE quantum-core Catch2::Catch2WithMain)

add_executable(qsim_benchmarks benchmark_suite.cpp)
target_link_libraries(qsim_benchmarks PRIVATE quantum-core)

include(Catch)
catch_discover_tests(qsim_tests)
