#include "FolderStructureTemplates.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "FolderTreeCatalog.hpp"
#include "Utils.hpp"

namespace {

using FolderStructureTemplates::Descriptor;
using FolderStructureTemplates::Id;

std::filesystem::path relative_directory_path(const std::string& relative_path) {
    const auto validation = FolderTreeCatalog::validate_relative_folder_path(relative_path);
    if (!validation.valid) {
        return {};
    }
    return Utils::utf8_to_path(validation.normalized_path);
}

}  // namespace

namespace FolderStructureTemplates {

const std::vector<Descriptor>& all() {
    static const std::vector<Descriptor> descriptors = {
        Descriptor{.id = Id::JohnnyDecimal,
                   .name = "Johnny.Decimal starter",
                   .description = "Numbered areas and categories for a Johnny.Decimal-like archive.",
                   .available = true,
                   .relative_directories = {"10-19 Admin",
                                            "10-19 Admin/11 Finance",
                                            "10-19 Admin/12 Contracts",
                                            "10-19 Admin/13 Vendors",
                                            "10-19 Admin/14 Legal",
                                            "20-29 Work",
                                            "20-29 Work/21 Projects",
                                            "20-29 Work/22 Proposals",
                                            "20-29 Work/23 Meeting Notes",
                                            "20-29 Work/24 Client Briefs",
                                            "30-39 Personal",
                                            "30-39 Personal/31 Travel",
                                            "30-39 Personal/32 Health",
                                            "30-39 Personal/33 Family",
                                            "30-39 Personal/34 Home",
                                            "40-49 Media",
                                            "40-49 Media/41 Photos",
                                            "40-49 Media/42 Videos",
                                            "40-49 Media/43 Screenshots",
                                            "40-49 Media/44 Design Assets",
                                            "50-59 Reference",
                                            "50-59 Reference/51 Manuals",
                                            "50-59 Reference/52 Research",
                                            "50-59 Reference/53 Articles",
                                            "50-59 Reference/54 Notes",
                                            "90-99 Archive",
                                            "90-99 Archive/91 Completed",
                                            "90-99 Archive/92 Old Projects"}},
        Descriptor{.id = Id::Para,
                   .name = "PARA",
                   .description = "Projects, Areas, Resources, and Archives for personal knowledge work.",
                   .available = true,
                   .relative_directories = {"Projects", "Projects/Active", "Projects/Waiting", "Areas", "Areas/Home",
                                            "Areas/Work", "Resources", "Resources/Reading", "Resources/Templates",
                                            "Archives", "Archives/Completed"}},
        Descriptor{.id = Id::Gtd,
                   .name = "GTD workflow",
                   .description = "Getting Things Done style folders for inboxes, actions, projects, and reference.",
                   .available = true,
                   .relative_directories = {"Inbox", "Next Actions", "Waiting For", "Projects", "Someday Maybe",
                                            "Reference", "Archive"}},
        Descriptor{.id = Id::Chronological,
                   .name = "Chronological folders",
                   .description = "Date-based folders. Planned for a later version.",
                   .available = false,
                   .relative_directories = {}},
        Descriptor{.id = Id::ClientProjectDeliverable,
                   .name = "Client, project, deliverable",
                   .description = "Client and project workspaces. Planned for a later version.",
                   .available = false,
                   .relative_directories = {}},
        Descriptor{.id = Id::DepartmentFunction,
                   .name = "Department folders",
                   .description = "Business-oriented folders grouped by department or function.",
                   .available = true,
                   .relative_directories = {"Administration", "Customer Support", "Engineering", "Finance",
                                            "Human Resources", "IT", "Legal", "Marketing", "Operations", "Sales"}},
        Descriptor{
            .id = Id::MediaType,
            .name = "Media type folders",
            .description = "Folders grouped by file and media type.",
            .available = true,
            .relative_directories = {"Documents", "Documents/PDFs", "Documents/Spreadsheets", "Documents/Presentations",
                                     "Images", "Images/Photos", "Images/Screenshots", "Images/Design Assets", "Videos",
                                     "Audio", "Archives", "Installers", "Other"}},
        Descriptor{.id = Id::StatusLifecycle,
                   .name = "Status lifecycle",
                   .description = "Workflow folders grouped by review, progress, approval, and archive status.",
                   .available = true,
                   .relative_directories = {"Inbox", "To Review", "In Progress", "Waiting", "Approved", "Published",
                                            "Archived", "Rejected"}},
        Descriptor{.id = Id::KnowledgeBase,
                   .name = "Knowledge base",
                   .description = "Zettelkasten-adjacent folders for notes, references, topics, and maps of content.",
                   .available = true,
                   .relative_directories = {"Inbox", "Fleeting Notes", "Literature Notes", "Permanent Notes",
                                            "References", "Topics", "Maps of Content", "Archive"}},
        Descriptor{.id = Id::NumberedPrefix,
                   .name = "Numbered prefix folders",
                   .description = "Simple numeric prefixes for predictable ordering in any file manager.",
                   .available = true,
                   .relative_directories = {"01 Inbox", "02 Projects", "03 Documents", "04 Media", "05 Reference",
                                            "06 Software", "07 Finance", "08 Personal", "09 Archive", "99 Misc"}}};

    return descriptors;
}

const Descriptor* find(Id id) {
    const auto& descriptors = all();
    const auto match = std::find_if(descriptors.begin(), descriptors.end(),
                                    [id](const Descriptor& descriptor) { return descriptor.id == id; });
    return match == descriptors.end() ? nullptr : &(*match);
}

CreationResult create(const std::filesystem::path& root, Id id) {
    CreationResult result;
    const Descriptor* const descriptor = find(id);
    if (!descriptor) {
        result.error = "Unknown folder structure.";
        return result;
    }
    if (!descriptor->available) {
        result.error = "This folder structure is not available yet.";
        return result;
    }
    if (root.empty()) {
        result.error = "Choose a destination folder.";
        return result;
    }
    if (!root.is_absolute()) {
        result.error = "Choose an absolute destination folder.";
        return result;
    }

    std::error_code ec;
    const bool root_exists = std::filesystem::exists(root, ec);
    if (ec) {
        result.error = "Could not inspect the destination folder.";
        return result;
    }
    if (root_exists && !std::filesystem::is_directory(root, ec)) {
        result.error = "The destination path is not a folder.";
        return result;
    }
    if (!root_exists && !std::filesystem::create_directories(root, ec)) {
        if (ec) {
            result.error = "Could not create the destination folder.";
            return result;
        }
    }

    for (const std::string& relative : descriptor->relative_directories) {
        const std::filesystem::path relative_path = relative_directory_path(relative);
        if (relative_path.empty()) {
            result.error = "The folder structure contains an invalid folder path.";
            result.success = false;
            return result;
        }

        const std::filesystem::path target = root / relative_path;
        ec.clear();
        if (std::filesystem::exists(target, ec)) {
            if (std::filesystem::is_directory(target, ec)) {
                result.existing_directories.push_back(target);
                continue;
            }
            result.error = "A file already exists where a folder should be created.";
            result.success = false;
            return result;
        }

        ec.clear();
        if (!std::filesystem::create_directories(target, ec)) {
            if (!ec && std::filesystem::is_directory(target, ec)) {
                result.existing_directories.push_back(target);
                continue;
            }
            result.error = "Could not create one or more folders.";
            result.success = false;
            return result;
        }
        result.created_directories.push_back(target);
    }

    result.success = true;
    return result;
}

}  // namespace FolderStructureTemplates
