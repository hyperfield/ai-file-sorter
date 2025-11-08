# CMake generated Testfile for 
# Source directory: /home/evoid/projects/ai-file-sorter/app
# Build directory: /home/evoid/projects/ai-file-sorter/build-tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[ai_file_sorter_tests]=] "/home/evoid/projects/ai-file-sorter/build-tests/ai_file_sorter_tests")
set_tests_properties([=[ai_file_sorter_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;331;add_test;/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;0;")
add_test([=[integration_run_all]=] "/usr/bin/cmake" "-E" "env" "bash" "/home/evoid/projects/ai-file-sorter/app/../tests/run_all_tests.sh")
set_tests_properties([=[integration_run_all]=] PROPERTIES  PASS_REGULAR_EXPRESSION "All tests completed successfully." WORKING_DIRECTORY "/home/evoid/projects/ai-file-sorter/app/.." _BACKTRACE_TRIPLES "/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;334;add_test;/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;0;")
subdirs("llama-build")
subdirs("catch2-build")
