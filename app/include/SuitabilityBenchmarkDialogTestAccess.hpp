#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

class SuitabilityBenchmarkDialog;

class SuitabilityBenchmarkDialogTestAccess {
public:
    static void finish_benchmark(SuitabilityBenchmarkDialog& dialog);
};

#endif // AI_FILE_SORTER_TEST_BUILD
