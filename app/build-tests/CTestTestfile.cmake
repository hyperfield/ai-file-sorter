# CMake generated Testfile for 
# Source directory: /home/evoid/projects/ai-file-sorter/app
# Build directory: /home/evoid/projects/ai-file-sorter/app/build-tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[ai_file_sorter_tests]=] "/home/evoid/projects/ai-file-sorter/app/build-tests/ai_file_sorter_tests")
set_tests_properties([=[ai_file_sorter_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;329;add_test;/home/evoid/projects/ai-file-sorter/app/CMakeLists.txt;0;")
subdirs("llama-build")
subdirs("catch2-build")
