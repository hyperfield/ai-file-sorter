#pragma once
#include "Settings.hpp"
#include <atomic>
#include <curl/system.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <curl/curl.h>


/**
 * @brief Handles downloading local LLM model files with resume support.
 */
class LLMDownloader
{
public:
    /**
     * @brief Constructs a downloader for the given URL.
     * @param download_url URL of the model to download.
     */
    explicit LLMDownloader(const std::string& download_url);
    /**
     * @brief Initializes curl state if not already initialized.
     */
    void init_if_needed();
    /**
     * @brief Returns true if initialization has completed.
     * @return True when initialized.
     */
    bool is_inited();
    /**
     * @brief Starts an async download with callbacks.
     * @param progress_cb Called with progress percentage (0-100).
     * @param on_complete_cb Called when download completes successfully.
     * @param on_status_text Called with status text updates.
     * @param on_error_cb Called with error message on failure.
     */
    void start_download(std::function<void(double)> progress_cb,
                        std::function<void()> on_complete_cb,
                        std::function<void(const std::string &)> on_status_text,
                        std::function<void(const std::string &)> on_error_cb);
    /**
     * @brief Attempts to resume a partial download.
     */
    void try_resume_download();
    /**
     * @brief Returns true if the current download can be resumed.
     * @return True when the download is resumable.
     */
    bool is_download_resumable() const;
    /**
     * @brief Returns true when the file is fully downloaded.
     * @return True if download is complete.
     */
    bool is_download_complete() const;

    /**
     * @brief Returns the server-reported content length.
     * @return Content length in bytes, or 0 if unknown.
     */
    long long get_real_content_length() const;
    /**
     * @brief Returns the resolved destination path for the download.
     * @return Full path to the downloaded file.
     */
    std::string get_download_destination() const;

    /**
     * @brief Starts a download with simpler callbacks.
     * @param on_progress Called with progress percentage (0-100).
     * @param on_complete Called with success flag and message.
     */
    void start(std::function<void(double)> on_progress,
               std::function<void(bool, const std::string&)> on_complete);

    /**
     * @brief Timestamp of last progress update (exposed for UI refresh).
     */
    std::chrono::steady_clock::time_point last_progress_update;
    
    /**
     * @brief Destructor; cancels any active download thread.
     */
    ~LLMDownloader();

    /**
     * @brief Download status for local or remote states.
     */
    enum class DownloadStatus {
        NotStarted,
        InProgress,
        Complete
    };
    
    /**
     * @brief Returns status based on local filesystem state.
     * @return Local download status.
     */
    DownloadStatus get_local_download_status() const;
    /**
     * @brief Returns the live download status.
     * @return Current download status.
     */
    DownloadStatus get_download_status() const;
    /**
     * @brief Cancels an in-progress download.
     */
    void cancel_download();
    /**
     * @brief Updates the download URL (resets internal state).
     * @param new_url New URL to download from.
     */
    void set_download_url(const std::string& new_url);
    /**
     * @brief Returns the current download URL.
     * @return Download URL string.
     */
    std::string get_download_url();

#ifdef AI_FILE_SORTER_TEST_BUILD
    class LLMDownloaderTestAccess {
    public:
        /**
         * @brief Overrides the cached content length for tests.
         * @param downloader Downloader instance under test.
         * @param length Content length in bytes.
         */
        static void set_real_content_length(LLMDownloader& downloader, long long length);
        /**
         * @brief Overrides the download destination path for tests.
         * @param downloader Downloader instance under test.
         * @param path Destination file path.
         */
        static void set_download_destination(LLMDownloader& downloader, const std::string& path);
        /**
         * @brief Seeds resume headers for tests.
         * @param downloader Downloader instance under test.
         * @param content_length Content length in bytes.
         */
        static void set_resume_headers(LLMDownloader& downloader, long long content_length);
    };
#endif

private:
#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class LLMDownloaderTestAccess;
#endif
    bool initialized{false};
    std::string url;
    std::string destination_dir;

    std::thread download_thread;
    std::map<std::string, std::string> curl_headers;
    mutable std::mutex mutex;

    std::function<void(double)> progress_callback;
    std::function<void()> on_download_complete;
    std::function<void(const std::string&)> on_status_text;
    std::function<void(const std::string&)> on_download_error;

    bool resumable{false};
    long long real_content_length{0};
    std::string download_destination;

    /**
     * @brief Curl write callback for file download.
     * @param ptr Data buffer.
     * @param size Element size.
     * @param nmemb Element count.
     * @param stream Output file stream.
     * @return Bytes written.
     */
    static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream);
    /**
     * @brief Curl callback that discards data (used for head requests).
     * @param ptr Data buffer.
     * @param size Element size.
     * @param nmemb Element count.
     * @param userdata User data.
     * @return Bytes processed.
     */
    static size_t discard_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
    /**
     * @brief Curl header callback to parse response headers.
     * @param buffer Header buffer.
     * @param size Element size.
     * @param nitems Element count.
     * @param userdata User data.
     * @return Bytes processed.
     */
    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
    /**
     * @brief Curl progress callback for download updates.
     * @param clientp User data.
     * @param dltotal Total bytes.
     * @param dlnow Bytes downloaded.
     * @return 0 to continue, non-zero to abort.
     */
    static int progress_func(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);

    /**
     * @brief Returns the default directory for storing downloaded LLMs.
     * @return Path to default LLM download directory.
     */
    std::string get_default_llm_destination();
    /**
     * @brief Computes and stores the download destination file path.
     */
    void set_download_destination();
    /**
     * @brief Path to cached metadata file.
     * @return Metadata file path.
     */
    std::string metadata_path() const;
    /**
     * @brief Loads cached metadata for resume and progress.
     */
    void load_cached_metadata();
    /**
     * @brief Persists metadata to disk.
     */
    void persist_cached_metadata() const;
    /**
     * @brief Parses cached or response headers for resume support.
     */
    void parse_headers();
    /**
     * @brief Executes the download in the worker thread.
     */
    void perform_download();
    /**
     * @brief Marks the download as resumable.
     */
    void mark_download_resumable();
    /**
     * @brief Notifies subscribers that the download finished.
     */
    void notify_download_complete();
    /**
     * @brief Applies common curl options shared across requests.
     * @param curl Curl handle.
     */
    void setup_common_curl_options(CURL *curl);
    /**
     * @brief Applies curl options for header probing.
     * @param curl Curl handle.
     */
    void setup_header_curl_options(CURL *curl);
    /**
     * @brief Applies curl options for the download request.
     * @param curl Curl handle.
     * @param fp File pointer for output.
     * @param resume_offset Resume offset in bytes.
     */
    void setup_download_curl_options(CURL *curl, FILE *fp, long resume_offset);
    /**
     * @brief Determines the byte offset for resuming downloads.
     * @return Resume offset in bytes.
     */
    long determine_resume_offset() const;
    /**
     * @brief Opens the output file at the given resume offset.
     * @param resume_offset Resume offset in bytes.
     * @return File pointer or nullptr on failure.
     */
    FILE *open_output_file(long resume_offset) const;
    /**
     * @brief Returns true if a partial download exists.
     * @return True when a partial download exists.
     */
    bool has_existing_partial_download() const;
    /**
     * @brief Returns true when the server supports resuming downloads.
     * @return True when resume is supported.
     */
    bool server_supports_resume_locked() const;
    /**
     * @brief Validates a content-length header value.
     * @param value Header string.
     * @return True when the value is a valid content length.
     */
    bool has_valid_content_length(const std::string& value) const;

    std::atomic<bool> cancel_requested{false};
    long resume_offset = 0;
};
