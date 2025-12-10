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


class LLMDownloader
{
public:
    explicit LLMDownloader(const std::string& download_url);
    void init_if_needed();
    bool is_inited();
    void start_download(std::function<void(double)> progress_cb,
                        std::function<void()> on_complete_cb,
                        std::function<void(const std::string &)> on_status_text,
                        std::function<void(const std::string &)> on_error_cb);
    void try_resume_download();
    bool is_download_resumable() const;
    bool is_download_complete() const;

    long long get_real_content_length() const;
    std::string get_download_destination() const;

    void start(std::function<void(double)> on_progress,
               std::function<void(bool, const std::string&)> on_complete);

    std::chrono::steady_clock::time_point last_progress_update;
    
    ~LLMDownloader();

    enum class DownloadStatus {
        NotStarted,
        InProgress,
        Complete
    };
    
    DownloadStatus get_download_status() const;
    void cancel_download();
    void set_download_url(const std::string& new_url);
    std::string get_download_url();

#ifdef AI_FILE_SORTER_TEST_BUILD
    class LLMDownloaderTestAccess {
    public:
        static void set_real_content_length(LLMDownloader& downloader, long long length);
        static void set_download_destination(LLMDownloader& downloader, const std::string& path);
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

    static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream);
    static size_t discard_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
    static int progress_func(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);

    std::string get_default_llm_destination();
    void set_download_destination();
    void parse_headers();
    void perform_download();
    void mark_download_resumable();
    void notify_download_complete();
    void setup_common_curl_options(CURL *curl);
    void setup_header_curl_options(CURL *curl);
    void setup_download_curl_options(CURL *curl, FILE *fp, long resume_offset);
    long determine_resume_offset() const;
    FILE *open_output_file(long resume_offset) const;
    bool has_existing_partial_download() const;
    bool server_supports_resume_locked() const;
    bool has_valid_content_length(const std::string& value) const;

    std::atomic<bool> cancel_requested{false};
    long resume_offset = 0;
};
