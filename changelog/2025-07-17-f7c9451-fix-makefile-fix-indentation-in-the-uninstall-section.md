# 2025-07-17: fix(makefile): fix indentation in the uninstall section

## Covered commits
- `f7c9451` `2025-07-17` `fix(makefile): fix indentation in the uninstall section`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/Makefile`

## What changed from what, why, and how
The commit corrected behavior in `app/Makefile`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(makefile): fix indentation in the uninstall section`.

Before this commit, the repository reflected the state immediately preceding `f7c9451`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -88,7 +88,6 @@ clean:
 	rm -rf $(OBJ_DIR) $(BIN_DIR) $(RC_OBJ)
 
 install: $(TARGET)
-
 ifeq ($(PLATFORM), Linux)
 	@echo "Installing binary to $(INSTALL_DIR)..."
 	mkdir -p $(INSTALL_DIR)
@@ -141,33 +140,33 @@ else ifeq ($(PLATFORM), Windows (64-bit))
 endif
 
 uninstall:
-	ifeq ($(PLATFORM), Linux)
-		@echo "Uninstalling aifilesorter binary and libraries..."
+ifeq ($(PLATFORM), Linux)
+	@echo "Uninstalling aifilesorter binary and libraries..."
 
-		@echo "Removing binary from /usr/local/bin..."
-		rm -f /usr/local/bin/aifilesorter
+	@echo "Removing binary from /usr/local/bin..."
+	rm -f /usr/local/bin/aifilesorter
 
-		@echo "Removing libraries from /usr/local/lib/aifilesorter..."
-		rm -rf /usr/local/lib/aifilesorter
+	@echo "Removing libraries from /usr/local/lib/aifilesorter..."
+	rm -rf /usr/local/lib/aifilesorter
 
-		@echo "Removing ld config file..."
-		rm -f /etc/ld.so.conf.d/aifilesorter.conf
+	@echo "Removing ld config file..."
+	rm -f /etc/ld.so.conf.d/aifilesorter.conf
 
-		@echo "Running ldconfig..."
-		ldconfig
+	@echo "Running ldconfig..."
+	ldconfig
 
-		@echo "Core uninstallation complete."
+	@echo "Core uninstallation complete."
 
-		@read -p "Do you also want to delete the downloaded local LLM models in ~/.local/share/aifilesorter/llms/? [y/N] " ans; \
-		if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
-			echo "Deleting ~/.local/share/aifilesorter/llms/..."; \
-			rm -rf $$HOME/.local/share/aifilesorter/llms; \
-		else \
-			echo "Keeping downloaded models."; \
-		fi
+	@read -p "Do you also want to delete the downloaded local LLM models in ~/.local/share/aifilesorter/llms/? [y/N] " ans; \
+	if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
+		echo "Deleting ~/.local/share/aifilesorter/llms/..."; \
+		rm -rf $$HOME/.local/share/aifilesorter/llms; \
+	else \
+		echo "Keeping downloaded models."; \
+	fi
 
-	else ifeq ($(PLATFORM), MacOS)
-		rm -f $(INSTALL_DIR)/aifilesorter
-	else ifeq ($(PLATFORM), Windows (64-bit))
-		rm -rf $(INSTALL_DIR)
-	endif
\ No newline at end of file
+else ifeq ($(PLATFORM), MacOS)
+	rm -f $(INSTALL_DIR)/aifilesorter
+else ifeq ($(PLATFORM), Windows (64-bit))
+	rm -rf $(INSTALL_DIR)
+endif
\ No newline at end of file
```

The excerpt is taken from the commit diff for `fix(makefile): fix indentation in the uninstall section`. The most relevant surfaces are `app/Makefile`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
