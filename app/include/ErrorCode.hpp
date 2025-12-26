#ifndef ERRORCODE_HPP
#define ERRORCODE_HPP

#include <string>
#include <unordered_map>
#include <libintl.h>

#define _(String) gettext(String)

namespace ErrorCodes {

// Error code enum - covers all possible error scenarios
enum class Code {
    // Success
    SUCCESS = 0,
    
    // Network errors (1000-1099)
    NETWORK_UNAVAILABLE = 1000,
    NETWORK_CONNECTION_FAILED = 1001,
    NETWORK_TIMEOUT = 1002,
    NETWORK_DNS_RESOLUTION_FAILED = 1003,
    NETWORK_SSL_HANDSHAKE_FAILED = 1004,
    NETWORK_SSL_CERTIFICATE_INVALID = 1005,
    NETWORK_PROXY_ERROR = 1006,
    
    // API errors (1100-1199)
    API_AUTHENTICATION_FAILED = 1100,
    API_INVALID_KEY = 1101,
    API_KEY_MISSING = 1102,
    API_RATE_LIMIT_EXCEEDED = 1103,
    API_QUOTA_EXCEEDED = 1104,
    API_INSUFFICIENT_PERMISSIONS = 1105,
    API_INVALID_REQUEST = 1106,
    API_INVALID_RESPONSE = 1107,
    API_RESPONSE_PARSE_ERROR = 1108,
    API_SERVER_ERROR = 1109,
    API_SERVICE_UNAVAILABLE = 1110,
    API_REQUEST_TIMEOUT = 1111,
    API_RETRIES_EXHAUSTED = 1112,
    
    // File system errors (1200-1299)
    FILE_NOT_FOUND = 1200,
    FILE_ACCESS_DENIED = 1201,
    FILE_PERMISSION_DENIED = 1202,
    FILE_ALREADY_EXISTS = 1203,
    FILE_OPEN_FAILED = 1204,
    FILE_READ_FAILED = 1205,
    FILE_WRITE_FAILED = 1206,
    FILE_DELETE_FAILED = 1207,
    FILE_MOVE_FAILED = 1208,
    FILE_COPY_FAILED = 1209,
    DIRECTORY_NOT_FOUND = 1210,
    DIRECTORY_INVALID = 1211,
    DIRECTORY_ACCESS_DENIED = 1212,
    DIRECTORY_CREATE_FAILED = 1213,
    DIRECTORY_NOT_EMPTY = 1214,
    DISK_FULL = 1215,
    DISK_IO_ERROR = 1216,
    PATH_INVALID = 1217,
    PATH_TOO_LONG = 1218,
    
    // Database errors (1300-1399)
    DB_CONNECTION_FAILED = 1300,
    DB_QUERY_FAILED = 1301,
    DB_INIT_FAILED = 1302,
    DB_CORRUPTED = 1303,
    DB_LOCKED = 1304,
    DB_CONSTRAINT_VIOLATION = 1305,
    DB_TRANSACTION_FAILED = 1306,
    DB_READONLY = 1307,
    
    // LLM errors (1400-1499)
    LLM_MODEL_NOT_FOUND = 1400,
    LLM_MODEL_LOAD_FAILED = 1401,
    LLM_MODEL_CORRUPTED = 1402,
    LLM_INFERENCE_FAILED = 1403,
    LLM_CONTEXT_OVERFLOW = 1404,
    LLM_INVALID_PROMPT = 1405,
    LLM_RESPONSE_EMPTY = 1406,
    LLM_RESPONSE_INVALID = 1407,
    LLM_BACKEND_INIT_FAILED = 1408,
    LLM_OUT_OF_MEMORY = 1409,
    LLM_TIMEOUT = 1410,
    LLM_CLIENT_CREATION_FAILED = 1411,
    LLM_GPU_NOT_AVAILABLE = 1412,
    
    // Configuration errors (1500-1599)
    CONFIG_INVALID = 1500,
    CONFIG_MISSING = 1501,
    CONFIG_PARSE_ERROR = 1502,
    CONFIG_SAVE_FAILED = 1503,
    CONFIG_LOAD_FAILED = 1504,
    CONFIG_INVALID_VALUE = 1505,
    CONFIG_REQUIRED_FIELD_MISSING = 1506,
    
    // Validation errors (1600-1699)
    VALIDATION_INVALID_INPUT = 1600,
    VALIDATION_INVALID_FORMAT = 1601,
    VALIDATION_INVALID_CATEGORY = 1602,
    VALIDATION_INVALID_SUBCATEGORY = 1603,
    VALIDATION_EMPTY_FIELD = 1604,
    VALIDATION_VALUE_OUT_OF_RANGE = 1605,
    
    // System errors (1700-1799)
    SYSTEM_OUT_OF_MEMORY = 1700,
    SYSTEM_UNSUPPORTED_PLATFORM = 1701,
    SYSTEM_ENVIRONMENT_VARIABLE_NOT_SET = 1702,
    SYSTEM_LIBRARY_LOAD_FAILED = 1703,
    SYSTEM_INIT_FAILED = 1704,
    SYSTEM_RESOURCE_UNAVAILABLE = 1705,
    
    // Categorization errors (1800-1899)
    CATEGORIZATION_NO_FILES = 1800,
    CATEGORIZATION_FAILED = 1801,
    CATEGORIZATION_PARTIAL_FAILURE = 1802,
    CATEGORIZATION_CANCELLED = 1803,
    CATEGORIZATION_TIMEOUT = 1804,
    
    // Download errors (1900-1999)
    DOWNLOAD_FAILED = 1900,
    DOWNLOAD_CURL_INIT_FAILED = 1901,
    DOWNLOAD_INVALID_URL = 1902,
    DOWNLOAD_NETWORK_ERROR = 1903,
    DOWNLOAD_WRITE_ERROR = 1904,
    DOWNLOAD_INCOMPLETE = 1905,
    
    // Generic error
    UNKNOWN_ERROR = 9999
};

// Error information structure
struct ErrorInfo {
    Code code;
    std::string message;
    std::string resolution;
    std::string technical_details;
    
    ErrorInfo(Code c, const std::string& msg, const std::string& res = "", const std::string& tech = "")
        : code(c), message(msg), resolution(res), technical_details(tech) {}
    
    // Get user-friendly formatted error message
    std::string get_user_message() const {
        std::string result = message;
        if (!resolution.empty()) {
            result += "\n\n" + std::string(_("How to fix:")) + "\n" + resolution;
        }
        return result;
    }
    
    // Get full error details including technical info
    std::string get_full_details() const {
        std::string result = std::string(_("Error Code:")) + " " + std::to_string(static_cast<int>(code)) + "\n\n";
        result += message;
        if (!resolution.empty()) {
            result += "\n\n" + std::string(_("How to fix:")) + "\n" + resolution;
        }
        if (!technical_details.empty()) {
            result += "\n\n" + std::string(_("Technical Details:")) + "\n" + technical_details;
        }
        return result;
    }
};

// Error catalog with all error codes, messages, and resolutions
class ErrorCatalog {
public:
    static ErrorInfo get_error_info(Code code, const std::string& context = "") {
        static const std::unordered_map<Code, std::pair<std::string, std::string>> catalog = {
            // Network errors
            {Code::NETWORK_UNAVAILABLE, 
                {_("No internet connection available."),
                 _("• Check your network connection\n• Verify your network cable or Wi-Fi is connected\n• Try restarting your router\n• Contact your network administrator if on a corporate network")}},
            {Code::NETWORK_CONNECTION_FAILED, 
                {_("Failed to connect to the server."),
                 _("• Check your internet connection\n• Verify the server URL is correct\n• Check if a firewall is blocking the connection\n• Try again in a few moments")}},
            {Code::NETWORK_TIMEOUT, 
                {_("The network request timed out."),
                 _("• Check your internet connection speed\n• Try again - the server may be temporarily slow\n• Increase timeout settings if available\n• Contact support if the problem persists")}},
            {Code::NETWORK_DNS_RESOLUTION_FAILED, 
                {_("Failed to resolve the server address (DNS error)."),
                 _("• Check your internet connection\n• Try using a different DNS server (e.g., 8.8.8.8)\n• Verify the server URL is typed correctly\n• Flush your DNS cache")}},
            {Code::NETWORK_SSL_HANDSHAKE_FAILED, 
                {_("SSL/TLS handshake failed - secure connection could not be established."),
                 _("• Check your system date and time are correct\n• Update your operating system\n• Check if antivirus/firewall is interfering\n• Contact your network administrator")}},
            {Code::NETWORK_SSL_CERTIFICATE_INVALID, 
                {_("The server's SSL certificate is invalid or untrusted."),
                 _("• Verify you're connecting to the correct server\n• Check your system date and time\n• Update your operating system certificates\n• Contact support if the issue persists")}},
            
            // API errors
            {Code::API_AUTHENTICATION_FAILED, 
                {_("Authentication failed - invalid credentials."),
                 _("• Verify your API key is correct\n• Check if your API key has expired\n• Generate a new API key from your account\n• Ensure there are no extra spaces in the key")}},
            {Code::API_INVALID_KEY, 
                {_("The API key is invalid or malformed."),
                 _("• Copy the API key again from your account\n• Ensure the entire key was copied\n• Check for extra spaces or line breaks\n• Generate a new API key if needed")}},
            {Code::API_KEY_MISSING, 
                {_("API key is required but not provided."),
                 _("• Go to Settings → Select LLM\n• Enter your API key\n• Save the settings and try again\n• Get an API key from your provider if you don't have one")}},
            {Code::API_RATE_LIMIT_EXCEEDED, 
                {_("API rate limit exceeded - too many requests."),
                 _("• Wait a few minutes before trying again\n• Reduce the number of files being processed\n• Consider upgrading your API plan\n• The app will automatically retry with delays")}},
            {Code::API_QUOTA_EXCEEDED, 
                {_("API quota exceeded - usage limit reached."),
                 _("• Check your API account usage\n• Wait until your quota resets\n• Upgrade your API plan for more quota\n• Consider using a local LLM as an alternative")}},
            {Code::API_INSUFFICIENT_PERMISSIONS, 
                {_("API key does not have sufficient permissions."),
                 _("• Check your API key permissions in your account\n• Generate a new key with proper permissions\n• Verify you're using the correct API key\n• Contact your API provider for assistance")}},
            {Code::API_INVALID_REQUEST, 
                {_("The API request was invalid or malformed."),
                 _("• This is likely a bug - please report it\n• Try updating to the latest version\n• Check if your input contains special characters\n• Contact support with error details")}},
            {Code::API_INVALID_RESPONSE, 
                {_("The API returned an invalid or unexpected response."),
                 _("• Try again - this may be a temporary server issue\n• Check if the API service is experiencing problems\n• Verify you're using a supported model\n• Update to the latest app version")}},
            {Code::API_RESPONSE_PARSE_ERROR, 
                {_("Failed to parse the API response."),
                 _("• Try again - the server may have sent corrupted data\n• Check your internet connection\n• Update to the latest app version\n• Report this error if it persists")}},
            {Code::API_SERVER_ERROR, 
                {_("The API server encountered an error."),
                 _("• Wait a few minutes and try again\n• Check the API service status page\n• The error is on the server side, not your fault\n• Contact API support if the issue persists")}},
            {Code::API_SERVICE_UNAVAILABLE, 
                {_("The API service is temporarily unavailable."),
                 _("• Wait a few minutes and try again\n• Check the service status page\n• Try using a different model if available\n• Consider using a local LLM temporarily")}},
            {Code::API_REQUEST_TIMEOUT, 
                {_("The API request timed out."),
                 _("• Try again - the server may be experiencing high load\n• Reduce the number of files being processed\n• Check your internet connection\n• The app will automatically retry")}},
            {Code::API_RETRIES_EXHAUSTED, 
                {_("Maximum retry attempts exhausted."),
                 _("• Wait a few minutes before trying again\n• Check your internet connection\n• Verify the API service is operational\n• Try processing fewer files at once")}},
            
            // File system errors
            {Code::FILE_NOT_FOUND, 
                {_("The file was not found."),
                 _("• Verify the file exists at the specified location\n• Check if the file was moved or deleted\n• Ensure the file path is correct\n• Refresh and try again")}},
            {Code::FILE_ACCESS_DENIED, 
                {_("Access to the file was denied."),
                 _("• Check if you have permission to access this file\n• Try running the application as administrator/root\n• Verify the file is not locked by another program\n• Check file permissions")}},
            {Code::FILE_PERMISSION_DENIED, 
                {_("Permission denied - cannot access the file."),
                 _("• Ensure you have read/write permissions\n• Try running with elevated privileges\n• Check if the file is read-only\n• Verify ownership of the file")}},
            {Code::FILE_ALREADY_EXISTS, 
                {_("A file with this name already exists."),
                 _("• Choose a different name or location\n• Delete or rename the existing file\n• Enable automatic renaming if available\n• Move the existing file to backup")}},
            {Code::FILE_OPEN_FAILED, 
                {_("Failed to open the file."),
                 _("• Check if the file is locked by another program\n• Verify you have permission to open this file\n• Try closing other programs using this file\n• Restart the application")}},
            {Code::FILE_WRITE_FAILED, 
                {_("Failed to write to the file."),
                 _("• Check if you have write permissions\n• Verify there is enough disk space\n• Ensure the disk is not write-protected\n• Try a different location")}},
            {Code::DIRECTORY_NOT_FOUND, 
                {_("The directory was not found."),
                 _("• Verify the directory exists\n• Check if the path is correct\n• Ensure the directory wasn't moved or deleted\n• Create the directory if it should exist")}},
            {Code::DIRECTORY_INVALID, 
                {_("The directory path is invalid."),
                 _("• Check the path syntax\n• Remove any invalid characters\n• Ensure the path is not too long\n• Verify the path format for your OS")}},
            {Code::DIRECTORY_ACCESS_DENIED, 
                {_("Access to the directory was denied."),
                 _("• Check directory permissions\n• Try running with administrator/root privileges\n• Verify you own the directory\n• Check if the directory is system-protected")}},
            {Code::DISK_FULL, 
                {_("The disk is full - no space available."),
                 _("• Free up disk space by deleting unnecessary files\n• Move files to another drive\n• Empty the recycle bin/trash\n• Uninstall unused programs")}},
            {Code::PATH_INVALID, 
                {_("The path is invalid."),
                 _("• Check the path syntax\n• Remove invalid characters\n• Ensure the path exists\n• Verify the path format is correct")}},
            
            // Database errors
            {Code::DB_CONNECTION_FAILED, 
                {_("Failed to connect to the database."),
                 _("• Check if the database file exists\n• Verify file permissions\n• Try restarting the application\n• The database may be corrupted - check logs")}},
            {Code::DB_QUERY_FAILED, 
                {_("Database query failed."),
                 _("• This may indicate data corruption\n• Try restarting the application\n• Clear the cache and try again\n• Contact support if the problem persists")}},
            {Code::DB_INIT_FAILED, 
                {_("Failed to initialize the database."),
                 _("• Check disk space availability\n• Verify write permissions\n• Try deleting and recreating the database\n• Check application logs for details")}},
            {Code::DB_CORRUPTED, 
                {_("The database is corrupted."),
                 _("• Try clearing the categorization cache\n• Backup and delete the database file\n• The app will recreate it on next launch\n• Contact support if data recovery is needed")}},
            {Code::DB_LOCKED, 
                {_("The database is locked by another process."),
                 _("• Close other instances of the application\n• Wait a moment and try again\n• Restart the application\n• Check for stuck processes")}},
            
            // LLM errors
            {Code::LLM_MODEL_NOT_FOUND, 
                {_("The LLM model file was not found."),
                 _("• Download the model from Settings → Select LLM\n• Verify the model path is correct\n• Check if the model was deleted or moved\n• Redownload the model if needed")}},
            {Code::LLM_MODEL_LOAD_FAILED, 
                {_("Failed to load the LLM model."),
                 _("• Verify the model file is not corrupted\n• Check if you have enough RAM\n• Try a smaller model\n• Ensure the model is compatible\n• Check application logs for details")}},
            {Code::LLM_MODEL_CORRUPTED, 
                {_("The LLM model file appears to be corrupted."),
                 _("• Delete and redownload the model\n• Verify the download completed successfully\n• Check disk integrity\n• Try a different model")}},
            {Code::LLM_INFERENCE_FAILED, 
                {_("LLM inference failed - could not generate response."),
                 _("• Try again with different input\n• Restart the application\n• Try a different model\n• Check if you have enough RAM\n• Report this error if it persists")}},
            {Code::LLM_CONTEXT_OVERFLOW, 
                {_("Input exceeds model's context length."),
                 _("• Process fewer files at once\n• Use a model with larger context\n• Simplify the input\n• Split the task into smaller batches")}},
            {Code::LLM_OUT_OF_MEMORY, 
                {_("Out of memory while running the model."),
                 _("• Close other applications to free memory\n• Use a smaller model\n• Process fewer files at once\n• Add more RAM if possible\n• Enable system swap/page file")}},
            {Code::LLM_TIMEOUT, 
                {_("LLM processing timed out."),
                 _("• Try again - processing may take time\n• Use a faster model\n• Process fewer files at once\n• Check if your system is under heavy load")}},
            {Code::LLM_CLIENT_CREATION_FAILED, 
                {_("Failed to create LLM client."),
                 _("• Check your LLM configuration in settings\n• Verify API keys if using remote LLM\n• Ensure model files exist if using local LLM\n• Restart the application")}},
            {Code::LLM_GPU_NOT_AVAILABLE, 
                {_("GPU acceleration is not available."),
                 _("• Install appropriate GPU drivers\n• Check CUDA/Vulkan installation\n• The app will use CPU (slower but functional)\n• Update graphics drivers")}},
            
            // Configuration errors
            {Code::CONFIG_INVALID, 
                {_("The configuration is invalid."),
                 _("• Reset settings to defaults\n• Check for invalid values\n• Delete the config file to recreate it\n• Contact support if the issue persists")}},
            {Code::CONFIG_MISSING, 
                {_("Configuration file is missing."),
                 _("• The app will create a new config file\n• Restore from backup if available\n• Reconfigure your settings")}},
            {Code::CONFIG_SAVE_FAILED, 
                {_("Failed to save configuration."),
                 _("• Check disk space\n• Verify write permissions\n• Try running with elevated privileges\n• Check if the config file is read-only")}},
            {Code::CONFIG_REQUIRED_FIELD_MISSING, 
                {_("A required configuration field is missing."),
                 _("• Reconfigure the application settings\n• Restore config from backup\n• Reset to default settings\n• Update to the latest version")}},
            
            // System errors
            {Code::SYSTEM_OUT_OF_MEMORY, 
                {_("The system is out of memory."),
                 _("• Close other applications\n• Restart the application\n• Process fewer files at once\n• Add more RAM to your system\n• Enable virtual memory/swap")}},
            {Code::SYSTEM_UNSUPPORTED_PLATFORM, 
                {_("This feature is not supported on your platform."),
                 _("• Check system requirements\n• Update your operating system\n• Use an alternative feature if available\n• Contact support for platform-specific builds")}},
            {Code::SYSTEM_ENVIRONMENT_VARIABLE_NOT_SET, 
                {_("A required environment variable is not set."),
                 _("• This is likely a bug - please report it\n• Try reinstalling the application\n• Contact support with error details")}},
            {Code::SYSTEM_LIBRARY_LOAD_FAILED, 
                {_("Failed to load a required system library."),
                 _("• Reinstall the application\n• Install missing system libraries\n• Update your operating system\n• Contact support for assistance")}},
            
            // Categorization errors
            {Code::CATEGORIZATION_NO_FILES, 
                {_("There are no files or directories to categorize."),
                 _("• Select a directory with files\n• Check if the directory is empty\n• Verify file filters if applied\n• Ensure files are accessible")}},
            {Code::CATEGORIZATION_FAILED, 
                {_("File categorization failed."),
                 _("• Check your internet connection (if using remote LLM)\n• Verify your API key (if using remote LLM)\n• Try using a different model\n• Check application logs for details")}},
            {Code::CATEGORIZATION_TIMEOUT, 
                {_("Categorization timed out."),
                 _("• Try processing fewer files\n• Use a faster model\n• Check if the LLM service is responsive\n• Increase timeout settings if available")}},
            
            // Download errors
            {Code::DOWNLOAD_FAILED, 
                {_("Download failed."),
                 _("• Check your internet connection\n• Verify you have enough disk space\n• Try again - the server may be temporarily unavailable\n• Check if a firewall is blocking downloads")}},
            {Code::DOWNLOAD_CURL_INIT_FAILED, 
                {_("Failed to initialize download system."),
                 _("• Restart the application\n• Reinstall the application\n• Check system libraries\n• Contact support if the issue persists")}},
            {Code::DOWNLOAD_INVALID_URL, 
                {_("The download URL is invalid."),
                 _("• This is likely a bug - please report it\n• Update to the latest version\n• Contact support with error details")}},
            
            // Unknown error
            {Code::UNKNOWN_ERROR, 
                {_("An unknown error occurred."),
                 _("• Try the operation again\n• Restart the application\n• Check application logs\n• Contact support with error details")}}
        };
        
        auto it = catalog.find(code);
        if (it != catalog.end()) {
            return ErrorInfo(code, it->second.first, it->second.second, context);
        }
        
        // Fallback for uncatalogued errors
        return ErrorInfo(code, 
            _("An error occurred. Error code: ") + std::to_string(static_cast<int>(code)),
            _("• Try the operation again\n• Restart the application\n• Contact support with this error code"),
            context);
    }
};

} // namespace ErrorCodes

#endif // ERRORCODE_HPP
