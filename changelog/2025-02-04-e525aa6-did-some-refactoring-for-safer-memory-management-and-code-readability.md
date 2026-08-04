# 2025-02-04: Did some refactoring for safer memory management and code readability. Added error logging. Hidden files are no longer processed. Fixed api-key-encryption

## Covered commits
- `e525aa6` `2025-02-04` `Did some refactoring for safer memory management and code readability. Added error logging. Hidden files are no longer processed. Fixed api-key-encryption`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`
- `M` `app/include/CategorizationDialog.hpp`
- `M` `app/include/CryptoManager.hpp`
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/include/FileScanner.hpp`
- `M` `app/include/LLMClient.hpp`
- `M` `app/include/MainApp.hpp`
- `R082` `app/include/CategorizedFile.hpp	app/include/MovableCategorizedFile.hpp`
- `A` `app/include/Types.hpp`
- `M` `app/include/app_version.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/CategorizationProgressDialog.cpp`
- `M` `app/lib/CryptoManager.cpp`
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MainAppHelpActions.cpp`
- `R077` `app/lib/CategorizedFile.cpp	app/lib/MovableCategorizedFile.cpp`
- `M` `app/lib/Updater.cpp`
- `M` `app/main.cpp`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/Makefile`, `app/include/CategorizationDialog.hpp`, `app/include/CryptoManager.hpp`, `app/include/DatabaseManager.hpp`, `app/include/FileScanner.hpp`, `app/include/LLMClient.hpp`, `app/include/MainApp.hpp`, `app/include/CategorizedFile.hpp	app/include/MovableCategorizedFile.hpp`, and 15 more file(s). It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `e525aa6`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -136,7 +136,8 @@ endif
 # Compiler and flags
 CXX = g++
 CXXFLAGS += -std=c++20 -Wall $(shell pkg-config --cflags gtkmm-3.0)
-LDFLAGS += $(shell pkg-config --libs gtkmm-3.0) -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl
+# CXXFLAGS += -fsanitize=address -g			# For debugging memory leaks
+LDFLAGS += $(shell pkg-config --libs gtkmm-3.0) -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lX11 -lfontconfig
 INCLUDE_DIRS = -I./include
 
 # Directories
@@ -153,7 +154,7 @@ OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))
 
 # Main rules
 all: $(TARGET)
-	@echo "Building for $(PLATFORM)"
+	@echo "Finished building AI File Sorter for $(PLATFORM)"
 
 $(TARGET): $(OBJS)
 	mkdir -p $(BIN_DIR)
```

The excerpt is taken from the commit diff for `Did some refactoring for safer memory management and code readability. Added error logging. Hidden files are no longer processed. Fixed api-key-encryption`. The most relevant surfaces are `app/Makefile`, `app/include/CategorizationDialog.hpp`, `app/include/CryptoManager.hpp`, `app/include/DatabaseManager.hpp`, `app/include/FileScanner.hpp`, and 18 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
