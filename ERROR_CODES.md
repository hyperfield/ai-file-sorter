# AI File Sorter - Error Code Reference

This document provides a comprehensive reference for all error codes in AI File Sorter. Each error includes its code number, description, and recommended resolution steps.

## Quick Error Code Lookup

When you encounter an error, the dialog will display:
- **Error Code**: A unique number identifying the specific error
- **Message**: What went wrong
- **How to fix**: Step-by-step resolution instructions
- **Copy Error Details** button: Click to copy full error information for support

## Error Code Categories

### Network Errors (1000-1099)

#### 1000: Network Unavailable
**Problem**: No internet connection available.

**How to fix**:
- Check your network connection
- Verify your network cable or Wi-Fi is connected
- Try restarting your router
- Contact your network administrator if on a corporate network

---

#### 1001: Network Connection Failed
**Problem**: Failed to connect to the server.

**How to fix**:
- Check your internet connection
- Verify the server URL is correct
- Check if a firewall is blocking the connection
- Try again in a few moments

---

#### 1002: Network Timeout
**Problem**: The network request timed out.

**How to fix**:
- Check your internet connection speed
- Try again - the server may be temporarily slow
- Increase timeout settings if available
- Contact support if the problem persists

---

#### 1003: DNS Resolution Failed
**Problem**: Failed to resolve the server address (DNS error).

**How to fix**:
- Check your internet connection
- Try using a different DNS server (e.g., 8.8.8.8)
- Verify the server URL is typed correctly
- Flush your DNS cache

---

#### 1004: SSL Handshake Failed
**Problem**: SSL/TLS handshake failed - secure connection could not be established.

**How to fix**:
- Check your system date and time are correct
- Update your operating system
- Check if antivirus/firewall is interfering
- Contact your network administrator

---

#### 1005: SSL Certificate Invalid
**Problem**: The server's SSL certificate is invalid or untrusted.

**How to fix**:
- Verify you're connecting to the correct server
- Check your system date and time
- Update your operating system certificates
- Contact support if the issue persists

---

### API Errors (1100-1199)

#### 1100: API Authentication Failed
**Problem**: Authentication failed - invalid credentials.

**How to fix**:
- Verify your API key is correct
- Check if your API key has expired
- Generate a new API key from your account
- Ensure there are no extra spaces in the key

---

#### 1101: API Invalid Key
**Problem**: The API key is invalid or malformed.

**How to fix**:
- Copy the API key again from your account
- Ensure the entire key was copied
- Check for extra spaces or line breaks
- Generate a new API key if needed

---

#### 1102: API Key Missing
**Problem**: API key is required but not provided.

**How to fix**:
- Go to Settings → Select LLM
- Enter your API key
- Save the settings and try again
- Get an API key from your provider if you don't have one

---

#### 1103: API Rate Limit Exceeded
**Problem**: API rate limit exceeded - too many requests.

**How to fix**:
- Wait a few minutes before trying again
- Reduce the number of files being processed
- Consider upgrading your API plan
- The app will automatically retry with delays

---

#### 1104: API Quota Exceeded
**Problem**: API quota exceeded - usage limit reached.

**How to fix**:
- Check your API account usage
- Wait until your quota resets
- Upgrade your API plan for more quota
- Consider using a local LLM as an alternative

---

#### 1105: API Insufficient Permissions
**Problem**: API key does not have sufficient permissions.

**How to fix**:
- Check your API key permissions in your account
- Generate a new key with proper permissions
- Verify you're using the correct API key
- Contact your API provider for assistance

---

#### 1106: API Invalid Request
**Problem**: The API request was invalid or malformed.

**How to fix**:
- This is likely a bug - please report it
- Try updating to the latest version
- Check if your input contains special characters
- Contact support with error details

---

#### 1107: API Invalid Response
**Problem**: The API returned an invalid or unexpected response.

**How to fix**:
- Try again - this may be a temporary server issue
- Check if the API service is experiencing problems
- Verify you're using a supported model
- Update to the latest app version

---

#### 1108: API Response Parse Error
**Problem**: Failed to parse the API response.

**How to fix**:
- Try again - the server may have sent corrupted data
- Check your internet connection
- Update to the latest app version
- Report this error if it persists

---

#### 1109: API Server Error
**Problem**: The API server encountered an error.

**How to fix**:
- Wait a few minutes and try again
- Check the API service status page
- The error is on the server side, not your fault
- Contact API support if the issue persists

---

#### 1110: API Service Unavailable
**Problem**: The API service is temporarily unavailable.

**How to fix**:
- Wait a few minutes and try again
- Check the service status page
- Try using a different model if available
- Consider using a local LLM temporarily

---

#### 1111: API Request Timeout
**Problem**: The API request timed out.

**How to fix**:
- Try again - the server may be experiencing high load
- Reduce the number of files being processed
- Check your internet connection
- The app will automatically retry

---

#### 1112: API Retries Exhausted
**Problem**: Maximum retry attempts exhausted.

**How to fix**:
- Wait a few minutes before trying again
- Check your internet connection
- Verify the API service is operational
- Try processing fewer files at once

---

### File System Errors (1200-1299)

#### 1200: File Not Found
**Problem**: The file was not found.

**How to fix**:
- Verify the file exists at the specified location
- Check if the file was moved or deleted
- Ensure the file path is correct
- Refresh and try again

---

#### 1201: File Access Denied
**Problem**: Access to the file was denied.

**How to fix**:
- Check if you have permission to access this file
- Try running the application as administrator/root
- Verify the file is not locked by another program
- Check file permissions

---

#### 1202: File Permission Denied
**Problem**: Permission denied - cannot access the file.

**How to fix**:
- Ensure you have read/write permissions
- Try running with elevated privileges
- Check if the file is read-only
- Verify ownership of the file

---

#### 1210: Directory Not Found
**Problem**: The directory was not found.

**How to fix**:
- Verify the directory exists
- Check if the path is correct
- Ensure the directory wasn't moved or deleted
- Create the directory if it should exist

---

#### 1211: Directory Invalid
**Problem**: The directory path is invalid.

**How to fix**:
- Check the path syntax
- Remove any invalid characters
- Ensure the path is not too long
- Verify the path format for your OS

---

#### 1212: Directory Access Denied
**Problem**: Access to the directory was denied.

**How to fix**:
- Check directory permissions
- Try running with administrator/root privileges
- Verify you own the directory
- Check if the directory is system-protected

---

#### 1215: Disk Full
**Problem**: The disk is full - no space available.

**How to fix**:
- Free up disk space by deleting unnecessary files
- Move files to another drive
- Empty the recycle bin/trash
- Uninstall unused programs

---

#### 1217: Path Invalid
**Problem**: The path is invalid.

**How to fix**:
- Check the path syntax
- Remove invalid characters
- Ensure the path exists
- Verify the path format is correct

---

### Database Errors (1300-1399)

#### 1300: DB Connection Failed
**Problem**: Failed to connect to the database.

**How to fix**:
- Check if the database file exists
- Verify file permissions
- Try restarting the application
- The database may be corrupted - check logs

---

#### 1301: DB Query Failed
**Problem**: Database query failed.

**How to fix**:
- This may indicate data corruption
- Try restarting the application
- Clear the cache and try again
- Contact support if the problem persists

---

#### 1303: DB Corrupted
**Problem**: The database is corrupted.

**How to fix**:
- Try clearing the categorization cache
- Backup and delete the database file
- The app will recreate it on next launch
- Contact support if data recovery is needed

---

#### 1304: DB Locked
**Problem**: The database is locked by another process.

**How to fix**:
- Close other instances of the application
- Wait a moment and try again
- Restart the application
- Check for stuck processes

---

### LLM Errors (1400-1499)

#### 1400: LLM Model Not Found
**Problem**: The LLM model file was not found.

**How to fix**:
- Download the model from Settings → Select LLM
- Verify the model path is correct
- Check if the model was deleted or moved
- Redownload the model if needed

---

#### 1401: LLM Model Load Failed
**Problem**: Failed to load the LLM model.

**How to fix**:
- Verify the model file is not corrupted
- Check if you have enough RAM
- Try a smaller model
- Ensure the model is compatible
- Check application logs for details

---

#### 1402: LLM Model Corrupted
**Problem**: The LLM model file appears to be corrupted.

**How to fix**:
- Delete and redownload the model
- Verify the download completed successfully
- Check disk integrity
- Try a different model

---

#### 1403: LLM Inference Failed
**Problem**: LLM inference failed - could not generate response.

**How to fix**:
- Try again with different input
- Restart the application
- Try a different model
- Check if you have enough RAM
- Report this error if it persists

---

#### 1404: LLM Context Overflow
**Problem**: Input exceeds model's context length.

**How to fix**:
- Process fewer files at once
- Use a model with larger context
- Simplify the input
- Split the task into smaller batches

---

#### 1409: LLM Out of Memory
**Problem**: Out of memory while running the model.

**How to fix**:
- Close other applications to free memory
- Use a smaller model
- Process fewer files at once
- Add more RAM if possible
- Enable system swap/page file

---

#### 1410: LLM Timeout
**Problem**: LLM processing timed out.

**How to fix**:
- Try again - processing may take time
- Use a faster model
- Process fewer files at once
- Check if your system is under heavy load

---

#### 1411: LLM Client Creation Failed
**Problem**: Failed to create LLM client.

**How to fix**:
- Check your LLM configuration in settings
- Verify API keys if using remote LLM
- Ensure model files exist if using local LLM
- Restart the application

---

### Configuration Errors (1500-1599)

#### 1500: Config Invalid
**Problem**: The configuration is invalid.

**How to fix**:
- Reset settings to defaults
- Check for invalid values
- Delete the config file to recreate it
- Contact support if the issue persists

---

#### 1501: Config Missing
**Problem**: Configuration file is missing.

**How to fix**:
- The app will create a new config file
- Restore from backup if available
- Reconfigure your settings

---

#### 1503: Config Save Failed
**Problem**: Failed to save configuration.

**How to fix**:
- Check disk space
- Verify write permissions
- Try running with elevated privileges
- Check if the config file is read-only

---

### System Errors (1700-1799)

#### 1700: System Out of Memory
**Problem**: The system is out of memory.

**How to fix**:
- Close other applications
- Restart the application
- Process fewer files at once
- Add more RAM to your system
- Enable virtual memory/swap

---

#### 1701: System Unsupported Platform
**Problem**: This feature is not supported on your platform.

**How to fix**:
- Check system requirements
- Update your operating system
- Use an alternative feature if available
- Contact support for platform-specific builds

---

#### 1702: System Environment Variable Not Set
**Problem**: A required environment variable is not set.

**How to fix**:
- This is likely a bug - please report it
- Try reinstalling the application
- Contact support with error details

---

### Categorization Errors (1800-1899)

#### 1800: Categorization No Files
**Problem**: There are no files or directories to categorize.

**How to fix**:
- Select a directory with files
- Check if the directory is empty
- Verify file filters if applied
- Ensure files are accessible

---

#### 1801: Categorization Failed
**Problem**: File categorization failed.

**How to fix**:
- Check your internet connection (if using remote LLM)
- Verify your API key (if using remote LLM)
- Try using a different model
- Check application logs for details

---

#### 1804: Categorization Timeout
**Problem**: Categorization timed out.

**How to fix**:
- Try processing fewer files
- Use a faster model
- Check if the LLM service is responsive
- Increase timeout settings if available

---

### Download Errors (1900-1999)

#### 1900: Download Failed
**Problem**: Download failed.

**How to fix**:
- Check your internet connection
- Verify you have enough disk space
- Try again - the server may be temporarily unavailable
- Check if a firewall is blocking downloads

---

#### 1901: Download CURL Init Failed
**Problem**: Failed to initialize download system.

**How to fix**:
- Restart the application
- Reinstall the application
- Check system libraries
- Contact support if the issue persists

---

#### 1902: Download Invalid URL
**Problem**: The download URL is invalid.

**How to fix**:
- This is likely a bug - please report it
- Update to the latest version
- Contact support with error details

---

## Getting Help

If you encounter an error that you cannot resolve:

1. **Copy Error Details**: Click the "Copy Error Details" button in the error dialog
2. **Check Logs**: Look at application logs for more information
3. **Search Issues**: Search the GitHub issues for similar problems
4. **Report**: Open a new issue with:
   - Error code and full error details
   - Steps to reproduce
   - Your system information (OS, version)
   - Application version

## For Developers

See `app/ERROR_CODE_EXAMPLES.cpp` for code examples on:
- Throwing errors with error codes
- Catching and handling AppException
- Getting error information programmatically
- Using convenience macros

Error codes are defined in `app/include/ErrorCode.hpp` and can be easily extended for new error scenarios.
