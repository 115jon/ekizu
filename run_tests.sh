#!/bin/bash

pushd "$(pwd)" || exit

cd build/tests || exit
export LLVM_PROFILE_FILE="$PWD/profraw/ekizu_tests-%p.profraw"
rm -rf profraw ekizu_tests.profdata
ctest --output-on-failure
llvm-profdata merge -sparse profraw/ekizu_tests-*.profraw -o ekizu_tests.profdata
llvm-cov report ../bin/ekizu_tests -instr-profile=ekizu_tests.profdata -check-binary-ids

llvm-cov show ../bin/ekizu_tests -instr-profile=ekizu_tests.profdata \
  -format=html -output-dir=coverage_html

popd || exit
