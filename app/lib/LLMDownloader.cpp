#include "LLMDownloader.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>


LLMDownloader::LLMDownloader(const std::string& download_url)
    : url(download_url),
      destination_dir(Utils::get_default_llm_destination())
{
    set_download_destination();
    last_progress_update = std::chrono::steady_clock::now();
}


void LLMDownloader::set_download_destination() {
    std::filesystem::create_directories(destination_dir);
    download_destination = Utils::make_default_path_to_file_from_download_url(url);
}


void LLMDownloader::init_if_needed() {
    if (initialized) return;

    if (!Utils::is_network_available()) {
        // std::cerr << "Still no internet...\n";
        return;
    }

    parse_headers();
    set_download_destination();
    initialized = true;
}


bool LLMDownloader::is_inited()
{
    return initialized;
}


size_t LLMDownloader::write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}


size_t LLMDownloader::discard_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    return size * nmemb;
}


void LLMDownloader::parse_headers()
{
    CURL* curl = curl_easy_init();

    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::lock_guard<std::mutex> lock(mutex);
    curl_headers.clear();
    resumable = false;
    real_content_length = 0;

    setup_header_curl_options(curl);
        
    CURLcode res = curl_easy_perform(curl);

    curl_off_t cl;
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl > 0) {
        real_content_length = cl;
    }

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL HEAD request failed: " + std::string(curl_easy_strerror(res)));
    }
}


int LLMDownloader::progress_func(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t, curl_off_t)
{
    auto* self = static_cast<LLMDownloader*>(clientp);

    if (self->cancel_requested) {
        return 1;
    }

    if (dltotal > 0 && self->on_status_text) {
        std::string msg = "Downloaded " + Utils::format_size(self->resume_offset + dlnow) +
            " / " + Utils::format_size(self->real_content_length);
        self->on_status_text(msg);
    }

    curl_off_t total_size = self->real_content_length;

    if (total_size == 0) {
        return 0;
    }

    double raw_progress = static_cast<double>(self->resume_offset + dlnow) / static_cast<double>(total_size);
    double clamped_progress = std::min(raw_progress, 1.0);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - self->last_progress_update);

    if (elapsed.count() > 100) {
        self->last_progress_update = now;

        if (self->progress_callback) {
            self->progress_callback(clamped_progress);
        }
    }

    return 0;
}


size_t LLMDownloader::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* self = static_cast<LLMDownloader*>(userdata);
    std::string header(buffer, total_size);

    // Normalize and parse header
    auto colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim spaces
        key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        // Lowercase key
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        // Store all headers
        self->curl_headers[key] = value;
    }

    return total_size;
}


void LLMDownloader::start_download(std::function<void(double)> progress_cb,
                                   std::function<void()> on_complete_cb,
                                   std::function<void(const std::string&)> on_status_text)
{
    if (download_thread.joinable()) {
        download_thread.join();
    }

    cancel_requested = false;
    this->progress_callback = std::move(progress_cb);
    this->on_download_complete = std::move(on_complete_cb);
    this->on_status_text = std::move(on_status_text);

    download_thread = std::thread([this]() {
        try {
            perform_download();
        } catch (const std::exception& ex) {
            // std::cerr << "[start_download] Exception: " << ex.what() << std::endl;
        }
    });
}


void LLMDownloader::perform_download()
{
    CURL* curl = curl_easy_init();
    
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    long resume_from = determine_resume_offset();
    resume_offset = resume_from;

    if (resume_from >= real_content_length) {
        notify_download_complete();
        curl_easy_cleanup(curl);
        return;
    }

    FILE* fp = open_output_file(resume_from);
    if (!fp) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open file: " + download_destination);
    }

    setup_download_curl_options(curl, fp, resume_offset);
    CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cancel_requested = false;
    } else {
        mark_download_resumable();
        notify_download_complete();
    }    
}


void LLMDownloader::mark_download_resumable()
{
    std::lock_guard<std::mutex> lock(mutex);
    resumable = true;
}


void LLMDownloader::notify_download_complete()
{
    if (on_download_complete) {
        on_download_complete();
    }
}


void LLMDownloader::setup_common_curl_options(CURL* curl)
{
#ifdef _WIN32
    try {
        const auto cert_path = Utils::ensure_ca_bundle();
        curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
    }
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
}


void LLMDownloader::setup_header_curl_options(CURL* curl)
{    
    setup_common_curl_options(curl);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &LLMDownloader::header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback); // <-- avoids stdout
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
}


void LLMDownloader::setup_download_curl_options(CURL* curl, FILE* fp, long resume_offset)
{
    setup_common_curl_options(curl);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &LLMDownloader::write_data);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &LLMDownloader::header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &LLMDownloader::progress_func);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM, resume_offset);
}


long LLMDownloader::determine_resume_offset() const
{
    if (!is_download_resumable()) return 0;

    FILE* fp = fopen(download_destination.c_str(), "rb");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long offset = ftell(fp);
    fclose(fp);
    return offset;
}


FILE* LLMDownloader::open_output_file(long resume_offset) const
{
    return fopen(download_destination.c_str(), resume_offset > 0 ? "ab" : "wb");
}


bool LLMDownloader::is_download_resumable() const
{
    try {
        if (!std::filesystem::exists(download_destination) ||
            std::filesystem::file_size(download_destination) == 0)
        {
            return false;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Log or handle error
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex);

    auto it = curl_headers.find("accept-ranges");
    auto len_it = curl_headers.find("content-length");

    bool ranges = (it != curl_headers.end() && it->second == "bytes");
    bool has_length = (len_it != curl_headers.end() && std::stoll(len_it->second) > 0);

    return ranges && has_length;
}


bool LLMDownloader::is_download_complete() const
{
    try {
        auto file_size = std::filesystem::file_size(download_destination);
        return static_cast<std::int64_t>(file_size) >= real_content_length;
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
}


// bool LLMDownloader::is_download_complete() const
// {
//     FILE* fp = fopen(download_destination.c_str(), "rb");
//     if (!fp) return false;

//     fseek(fp, 0, SEEK_END);
//     long size = ftell(fp);
//     fclose(fp);

//     return size >= real_content_length;
// }


long long LLMDownloader::get_real_content_length() const
{
    return real_content_length;
}


std::string LLMDownloader::get_download_destination() const
{
    return download_destination;
}


LLMDownloader::DownloadStatus LLMDownloader::get_download_status() const {
    if (is_download_complete()) {
        return DownloadStatus::Complete;
    }

    if (is_download_resumable()) {
        return DownloadStatus::InProgress;
    }

    return DownloadStatus::NotStarted;
}


void LLMDownloader::cancel_download()
{
    std::lock_guard<std::mutex> lock(mutex);
    cancel_requested = true;
}


LLMDownloader::~LLMDownloader() {
    if (download_thread.joinable()) {
        download_thread.join();
    }
}


void LLMDownloader::set_download_url(const std::string& new_url) {
    if (new_url == url) return;

    url = new_url;
    initialized = false;

    try {
        parse_headers();
        set_download_destination();
        initialized = true;
    } catch (const std::exception& ex) {
        // Log errors
    }
}


std::string LLMDownloader::get_download_url()
{
    return url;
}
