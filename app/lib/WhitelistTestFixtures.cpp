#include "WhitelistTestFixtures.hpp"

#include <fstream>
#include <stdexcept>
#include <system_error>

namespace {

std::vector<std::string> make_large_category_sample()
{
    std::vector<std::string> categories;
    categories.reserve(100);
    for (int i = 0; i < 80; ++i) {
        categories.push_back("Archive Bucket " + std::to_string(i));
    }

    const std::vector<std::string> semantic_categories = {
        "Documents",
        "Manuals",
        "Spreadsheets",
        "Invoices",
        "Receipts",
        "Bank Statements",
        "Certificates",
        "Presentations",
        "Source Code",
        "Research Papers",
        "Photos",
        "Screenshots",
        "Installation Guides",
        "AB Testing ☁️",
        "Abroad ☁️",
        "Contracts",
        "Reports",
        "Tax Documents",
        "Project Notes"
    };
    categories.insert(categories.end(), semantic_categories.begin(), semantic_categories.end());
    return categories;
}

std::vector<WhitelistTestFixtures::SampleFile> make_sample_files()
{
    return {
        {"nikon_camera_manual.pdf",
         "Camera manual sample: aperture setup, lens maintenance, and operating instructions.\n",
         "Manuals"},
        {"quarterly_budget_spreadsheet.xlsx",
         "Spreadsheet sample: quarterly budget, formulas, totals, and department spend.\n",
         "Spreadsheets"},
        {"acme_invoice_2024_03.pdf",
         "Invoice sample: vendor ACME, invoice number, due date, and billed amount.\n",
         "Invoices"},
        {"bank_statement_march_2024.pdf",
         "Bank statement sample: account activity, deposits, withdrawals, and balance.\n",
         "Bank Statements"},
        {"warranty_certificate_laptop.pdf",
         "Certificate sample: warranty coverage, serial number, and effective period.\n",
         "Certificates"},
        {"product_launch_presentation.pptx",
         "Presentation sample: launch timeline, target audience, and campaign plan.\n",
         "Presentations"},
        {"backup_script_python.py",
         "Source code sample: Python script for backup automation.\n",
         "Source Code"},
        {"ab_testing_cloud_notes.txt",
         "AB testing sample: cloud experiment variants, conversion metrics, and notes.\n",
         "AB Testing ☁️"},
        {"linux_installation_guide.md",
         "Installation guide sample: Linux setup steps and troubleshooting notes.\n",
         "Installation Guides"},
        {"holiday_photo_beach.jpg",
         "Photo sample placeholder: family holiday beach image.\n",
         "Photos"},
        {"dashboard_screenshot.png",
         "Screenshot sample placeholder: analytics dashboard capture.\n",
         "Screenshots"},
        {"contract_vendor_services.pdf",
         "Contract sample: vendor services agreement, terms, and signatures.\n",
         "Contracts"}
    };
}

} // namespace

WhitelistTestFixtures::Preset WhitelistTestFixtures::large_whitelist_preset()
{
    return Preset{
        "Test - Large Whitelist LLM",
        make_large_category_sample(),
        {},
        {},
        make_sample_files()
    };
}

void WhitelistTestFixtures::write_sample_files(const std::filesystem::path& directory,
                                               const Preset& preset)
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        throw std::runtime_error("Could not create test sample directory: " + ec.message());
    }

    for (const auto& sample : preset.files) {
        const auto path = directory / sample.file_name;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Could not write test sample file: " + path.string());
        }
        out << sample.contents;
    }
}
