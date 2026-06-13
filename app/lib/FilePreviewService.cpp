#include "IFilePreviewService.hpp"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#include <propsys.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <atomic>
#endif

namespace {

bool open_externally(const std::filesystem::path& file_path)
{
    const QUrl file_url = QUrl::fromLocalFile(QString::fromStdWString(file_path.wstring()));
    return QDesktopServices::openUrl(file_url);
}

#ifdef _WIN32

using Microsoft::WRL::ComPtr;

RECT widget_rect(QWidget* widget)
{
    RECT rect{0, 0, 0, 0};
    if (!widget) {
        return rect;
    }
    rect.right = widget->width();
    rect.bottom = widget->height();
    return rect;
}

std::wstring guid_to_string(REFGUID guid)
{
    wchar_t buffer[64] = {};
    const int written = StringFromGUID2(guid, buffer, 64);
    if (written <= 0) {
        return std::wstring();
    }
    return std::wstring(buffer);
}

std::optional<CLSID> find_preview_handler_clsid(const std::filesystem::path& file_path)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file_path, ec) || ec || !file_path.has_extension()) {
        return std::nullopt;
    }

    const std::wstring extension = file_path.extension().wstring();
    const std::wstring preview_iid = guid_to_string(__uuidof(IPreviewHandler));
    if (preview_iid.empty()) {
        return std::nullopt;
    }

    DWORD required_chars = 0;
    HRESULT hr = AssocQueryStringW(ASSOCF_NOTRUNCATE,
                                   ASSOCSTR_SHELLEXTENSION,
                                   extension.c_str(),
                                   preview_iid.c_str(),
                                   nullptr,
                                   &required_chars);
    if (hr != S_FALSE || required_chars == 0) {
        return std::nullopt;
    }

    std::wstring clsid_text(required_chars, L'\0');
    hr = AssocQueryStringW(ASSOCF_NOTRUNCATE,
                           ASSOCSTR_SHELLEXTENSION,
                           extension.c_str(),
                           preview_iid.c_str(),
                           clsid_text.data(),
                           &required_chars);
    if (FAILED(hr) || required_chars == 0) {
        return std::nullopt;
    }

    while (!clsid_text.empty() && clsid_text.back() == L'\0') {
        clsid_text.pop_back();
    }

    CLSID clsid{};
    if (FAILED(CLSIDFromString(clsid_text.c_str(), &clsid))) {
        return std::nullopt;
    }
    return clsid;
}

class ScopedComApartment {
public:
    ScopedComApartment()
        : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))
    {
    }

    ~ScopedComApartment()
    {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    bool ready() const
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{E_FAIL};
};

class PreviewHandlerFrameSite final : public IPreviewHandlerFrame {
public:
    IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IPreviewHandlerFrame)) {
            *object = static_cast<IPreviewHandlerFrame*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(++ref_count_);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG updated = static_cast<ULONG>(--ref_count_);
        if (updated == 0) {
            delete this;
        }
        return updated;
    }

    IFACEMETHODIMP GetWindowContext(PREVIEWHANDLERFRAMEINFO* frame_info) override
    {
        if (!frame_info) {
            return E_POINTER;
        }
        frame_info->haccel = nullptr;
        frame_info->cAccelEntries = 0;
        return S_OK;
    }

    IFACEMETHODIMP TranslateAccelerator(MSG* message) override
    {
        if (!message) {
            return E_POINTER;
        }
        return S_FALSE;
    }

private:
    std::atomic<ULONG> ref_count_{1};
};

bool initialize_preview_handler(IPreviewHandler* preview_handler,
                                const std::filesystem::path& file_path)
{
    if (!preview_handler) {
        return false;
    }

    const std::wstring wide_path = file_path.wstring();

    {
        ComPtr<IInitializeWithStream> initialize_with_stream;
        if (SUCCEEDED(preview_handler->QueryInterface(IID_PPV_ARGS(&initialize_with_stream)))) {
            ComPtr<IStream> stream;
            HRESULT hr = SHCreateStreamOnFileEx(wide_path.c_str(),
                                                STGM_READ | STGM_SHARE_DENY_NONE,
                                                FILE_ATTRIBUTE_NORMAL,
                                                FALSE,
                                                nullptr,
                                                &stream);
            return SUCCEEDED(hr) &&
                   SUCCEEDED(initialize_with_stream->Initialize(stream.Get(), STGM_READ));
        }
    }

    {
        ComPtr<IInitializeWithItem> initialize_with_item;
        if (SUCCEEDED(preview_handler->QueryInterface(IID_PPV_ARGS(&initialize_with_item)))) {
            ComPtr<IShellItem> shell_item;
            HRESULT hr = SHCreateItemFromParsingName(wide_path.c_str(),
                                                     nullptr,
                                                     IID_PPV_ARGS(&shell_item));
            return SUCCEEDED(hr) &&
                   SUCCEEDED(initialize_with_item->Initialize(shell_item.Get(), STGM_READ));
        }
    }

    {
        ComPtr<IInitializeWithFile> initialize_with_file;
        if (SUCCEEDED(preview_handler->QueryInterface(IID_PPV_ARGS(&initialize_with_file))) &&
            SUCCEEDED(initialize_with_file->Initialize(wide_path.c_str(), STGM_READ))) {
            return true;
        }
    }

    return false;
}

class WindowsPreviewSession {
public:
    WindowsPreviewSession() = default;

    ~WindowsPreviewSession()
    {
        unload();
    }

    bool start(const std::filesystem::path& file_path,
               HWND host_window,
               const RECT& rect)
    {
        if (!host_window || !std::filesystem::exists(file_path)) {
            return false;
        }

        com_apartment_ = std::make_unique<ScopedComApartment>();
        if (!com_apartment_->ready()) {
            com_apartment_.reset();
            return false;
        }

        const auto clsid = find_preview_handler_clsid(file_path);
        if (!clsid.has_value()) {
            unload();
            return false;
        }

        HRESULT hr = CoCreateInstance(*clsid,
                                      nullptr,
                                      CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                                      IID_PPV_ARGS(&preview_handler_));
        if (FAILED(hr) || !preview_handler_) {
            unload();
            return false;
        }

        site_.Attach(new PreviewHandlerFrameSite());
        object_with_site_.Reset();
        preview_handler_.As(&object_with_site_);
        if (object_with_site_ &&
            FAILED(object_with_site_->SetSite(static_cast<IUnknown*>(site_.Get())))) {
            unload();
            return false;
        }

        if (!initialize_preview_handler(preview_handler_.Get(), file_path)) {
            unload();
            return false;
        }

        hr = preview_handler_->SetWindow(host_window, const_cast<RECT*>(&rect));
        if (FAILED(hr) || FAILED(preview_handler_->SetRect(const_cast<RECT*>(&rect))) ||
            FAILED(preview_handler_->DoPreview())) {
            unload();
            return false;
        }

        return true;
    }

    void resize(const RECT& rect)
    {
        if (preview_handler_) {
            preview_handler_->SetRect(const_cast<RECT*>(&rect));
        }
    }

private:
    void unload()
    {
        if (preview_handler_) {
            preview_handler_->Unload();
        }
        if (object_with_site_) {
            object_with_site_->SetSite(nullptr);
        }
        object_with_site_.Reset();
        preview_handler_.Reset();
        site_.Reset();
        com_apartment_.reset();
    }

    std::unique_ptr<ScopedComApartment> com_apartment_;
    ComPtr<IPreviewHandler> preview_handler_;
    ComPtr<IObjectWithSite> object_with_site_;
    ComPtr<PreviewHandlerFrameSite> site_;
};

class WindowsPreviewDialog final : public QDialog {
public:
    explicit WindowsPreviewDialog(std::filesystem::path file_path,
                                  QWidget* parent = nullptr)
        : QDialog(parent),
          file_path_(std::move(file_path))
    {
        setModal(true);
        setSizeGripEnabled(true);
        resize(960, 720);

        const QFileInfo info(QString::fromStdWString(file_path_.wstring()));
        setWindowTitle(info.fileName());

        auto* layout = new QVBoxLayout(this);
        host_widget_ = new QWidget(this);
        host_widget_->setAttribute(Qt::WA_NativeWindow);
        host_widget_->setMinimumSize(320, 240);
        layout->addWidget(host_widget_, 1);

        auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        auto* button_layout = new QHBoxLayout();
        button_layout->addStretch(1);
        button_layout->addWidget(button_box);
        layout->addLayout(button_layout);
    }

    bool run()
    {
        if (!host_widget_) {
            return false;
        }

        host_widget_->winId();
        const HWND host_window = reinterpret_cast<HWND>(host_widget_->winId());
        if (!session_.start(file_path_, host_window, widget_rect(host_widget_))) {
            return false;
        }

        exec();
        return true;
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        if (host_widget_) {
            session_.resize(widget_rect(host_widget_));
        }
    }

private:
    std::filesystem::path file_path_;
    QWidget* host_widget_{nullptr};
    WindowsPreviewSession session_;
};

class WindowsFilePreviewService final : public IFilePreviewService {
public:
    bool preview_file(const std::filesystem::path& file_path,
                      QWidget* parent) override
    {
        WindowsPreviewDialog dialog(file_path, parent);
        if (dialog.run()) {
            return true;
        }
        return open_externally(file_path);
    }
};

#endif

class ExternalFilePreviewService final : public IFilePreviewService {
public:
    bool preview_file(const std::filesystem::path& file_path,
                      QWidget* parent) override
    {
        (void)parent;
        return open_externally(file_path);
    }
};

} // namespace

std::unique_ptr<IFilePreviewService> create_file_preview_service()
{
#ifdef _WIN32
    return std::make_unique<WindowsFilePreviewService>();
#else
    return std::make_unique<ExternalFilePreviewService>();
#endif
}
