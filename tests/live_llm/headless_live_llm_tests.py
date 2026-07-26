#!/usr/bin/env python3
"""Optional headless live-LLM integration tests for AI File Sorter.

The suite is intentionally black-box: it runs the built app executable with
`--headless`, points it at isolated real fixture directories, and inspects the
status/review JSON emitted by the production headless command.
"""

from __future__ import annotations

import argparse
import configparser
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath
from typing import Any, Callable, Iterable
from urllib.parse import unquote, urlparse


SKIP_EXIT_CODE = 77
LIVE_BACKENDS = {"auto", "cpu", "cuda", "vulkan"}
TERMINAL_STATUS_VALUES = {"completed", "failed", "review_required"}
PROGRESS_LOG_PATH: Path | None = None
ANSI_ENABLED = False
CASE_PROGRESS_ACTIVE = False
CASE_PROGRESS_INDEX = 0
CASE_PROGRESS_TOTAL = 0
CASE_PROGRESS_NAME = ""
CASE_PROGRESS_STARTED = 0.0
CASE_PROGRESS_LAST_SECONDS = -1
CASE_PROGRESS_LAST_WIDTH = 0

ANSI_RESET = "\033[0m"
ANSI_BOLD = "\033[1m"
ANSI_RED = "\033[31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_BLUE = "\033[34m"
ANSI_CYAN = "\033[36m"


@dataclass(frozen=True)
class DownloadFixture:
    key: str
    url: str
    file_name: str
    min_bytes: int


@dataclass
class LiveContext:
    app: Path
    uses_windows_launcher: bool
    backend: str
    text_model: Path
    visual_model: Path | None
    visual_mmproj: Path | None
    work_root: Path
    fixture_cache: Path
    timeout_seconds: int
    require_downloads: bool
    keep_work_dir: bool
    force_visual_cpu: bool
    require_localized_renames: bool
    verbose: bool
    downloaded_fixtures: dict[str, Path]
    run_counter: int = 0


@dataclass
class CaseResult:
    name: str
    status: str
    seconds: float
    detail: str = ""


@dataclass(frozen=True)
class AppSettingsConfig:
    path: Path
    config: configparser.ConfigParser


@dataclass(frozen=True)
class ModelResolution:
    path: Path
    source: str


class LiveTestFailure(AssertionError):
    """Raised when a live case observes an app-level regression."""


class LiveTestSkip(Exception):
    """Raised when an optional live case cannot run in the local environment."""


DOWNLOAD_FIXTURES = [
    DownloadFixture(
        key="w3c_dummy_pdf",
        url="https://www.w3.org/WAI/ER/tests/xhtml/testfiles/resources/pdf/dummy.pdf",
        file_name="dummy.pdf",
        min_bytes=500,
    ),
    DownloadFixture(
        key="wikimedia_png_transparency",
        url="https://commons.wikimedia.org/wiki/Special:Redirect/file/PNG_transparency_demonstration_1.png",
        file_name="png_transparency_demonstration_1.png",
        min_bytes=20_000,
    ),
    DownloadFixture(
        key="wikimedia_jpeg_flower",
        url="https://commons.wikimedia.org/wiki/Special:Redirect/file/JPEG_example_flower.jpg",
        file_name="jpeg_example_flower.jpg",
        min_bytes=20_000,
    ),
    DownloadFixture(
        key="wikimedia_rotating_earth_gif",
        url="https://commons.wikimedia.org/wiki/Special:Redirect/file/Rotating_earth_%28Very_small%29.gif",
        file_name="rotating_earth_very_small.gif",
        min_bytes=10_000,
    ),
]


TEXT_DOCUMENTS = [
    (
        "invoice_2026_vendor_acme.txt",
        """Invoice ACME-2026-1042
        Vendor: Acme Office Supply
        Buyer: HF Studio Operations
        Amount due: 1,284.60 USD
        Payment deadline: 2026-08-15
        Line items: ergonomic keyboards, archive boxes, and printer toner.
        """,
    ),
    (
        "rapport_énergie_Q2.md",
        """# Rapport d'énergie Q2
        Ce rapport résume la consommation électrique des bureaux de Lyon.
        Les recommandations portent sur l'éclairage, les thermostats et
        l'achat de capteurs pour réduire les pics de consommation.
        """,
    ),
    (
        "会议纪要_产品发布.json",
        """{
          "title": "产品发布会议纪要",
          "date": "2026-06-02",
          "topics": ["发布计划", "客户反馈", "本地化文档"],
          "decision": "准备中文版发布说明并确认演示材料。"
        }
        """,
    ),
    (
        "अनुसंधान_नोट्स_दिल्ली.html",
        """<!doctype html>
        <html lang="hi"><body>
        <h1>दिल्ली जलवायु अनुसंधान नोट्स</h1>
        <p>यह दस्तावेज़ तापमान, वर्षा और वायु गुणवत्ता के क्षेत्रीय
        अवलोकनों को सारांशित करता है।</p>
        </body></html>
        """,
    ),
    (
        "budget_überblick_2026.csv",
        "department,quarter,amount,notes\nResearch,Q1,42000,prototype materials\nOperations,Q2,17500,facility upgrades\n",
    ),
    (
        "server_config_audit.yml",
        "service: file-sorter\nreview_date: 2026-07-01\nfindings:\n  - rotate api keys\n  - reduce admin access\n",
    ),
    (
        "contract_vendor_services.xml",
        "<contract><party>Northwind Analytics</party><term>2026 service renewal</term><value currency=\"USD\">24000</value></contract>",
    ),
    (
        "meeting_notes_release_plan.rtf",
        r"{\rtf1\ansi\b Release planning notes\b0\par Topics: installer QA, translations, staged rollout, support readiness.\par}",
    ),
]


LOCALIZED_DOCUMENTS = {
    "English": [
        (
            "IMG_scan_0007.txt",
            "Clinic lab report for patient follow-up. Includes blood pressure, cholesterol, and next appointment notes.",
        ),
        (
            "old_download_42.md",
            "# Software migration checklist\nReview database backups, account provisioning, and post-release verification.",
        ),
    ],
    "French": [
        (
            "scan_sans_nom_été.txt",
            "Facture du fournisseur pour matériel de bureau, livraison à Paris, paiement prévu en septembre 2026.",
        ),
        (
            "note_finale_éléments.md",
            "# Rapport médical\nRésumé de consultation, symptômes, traitement recommandé et rendez-vous de suivi.",
        ),
    ],
    "Simplified Chinese": [
        (
            "IMG_扫描_0001.txt",
            "项目预算说明：研发采购、云服务成本、测试设备和审批时间表。",
        ),
        (
            "下载_最终版.md",
            "# 旅行计划\n北京到上海的行程安排、酒店确认和客户会议议程。",
        ),
    ],
    "Hindi": [
        (
            "scan_नामहीन_01.txt",
            "कार्यालय आपूर्ति की चालान रिपोर्ट, विक्रेता विवरण, भुगतान तिथि और कुल राशि।",
        ),
        (
            "download_final.md",
            "# परियोजना बैठक नोट्स\nटीम निर्णय, कार्य सूची, समय सीमा और जोखिम समीक्षा।",
        ),
    ],
}


LOCALIZED_EXTRA_DOCUMENTS = {
    "English": (
        "config_probe_english.yml",
        "environment: staging\nfeature: live llm rename test\nowner: localization qa\n",
    ),
    "French": (
        "budget_énergie_probe.yml",
        "budget: énergie\nrapport: optimisation des bureaux\nresponsable: équipe finance\n",
    ),
    "Simplified Chinese": (
        "配置_探针.yml",
        "环境: 测试\n主题: 本地化重命名\n负责人: 中文质量团队\n",
    ),
    "Hindi": (
        "कॉन्फ़िग_जांच.yml",
        "पर्यावरण: परीक्षण\nविषय: स्थानीयकृत नामकरण\nस्वामी: हिंदी गुणवत्ता टीम\n",
    ),
}


LOCALIZED_IMAGE_NAMES = {
    "English": ["IMG_0007_東京.png", "vacances-été_0001.jpeg"],
    "French": ["photo_sans_nom_été.png", "image_brute_0002.jpeg"],
    "Simplified Chinese": ["照片_未命名_001.png", "扫描_图像_0002.jpeg"],
    "Hindi": ["तस्वीर_नामहीन_001.png", "फोटो_कच्ची_0002.jpeg"],
}


LANGUAGE_SCRIPT_CHECKS = {
    "Simplified Chinese": re.compile(r"[\u4e00-\u9fff]"),
    "Hindi": re.compile(r"[\u0900-\u097f]"),
}


FRENCH_STEM_HINTS = (
    "facture",
    "rapport",
    "resume",
    "résumé",
    "medical",
    "médical",
    "energie",
    "énergie",
    "budget",
    "bureau",
    "bureaux",
    "contrat",
    "conditions",
    "consultation",
    "element",
    "élément",
    "ete",
    "été",
    "fournisseur",
    "livraison",
    "materiel",
    "matériel",
    "paiement",
    "photo",
    "fleur",
    "image",
    "rendez",
    "sept",
    "septembre",
    "suivi",
    "symptomes",
    "symptômes",
    "traitement",
    "transparent",
    "vendeur",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run optional live LLM tests against the AI File Sorter headless executable."
    )
    parser.add_argument(
        "--app",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_APP"),
        help="Path to the built aifilesorter executable. Defaults to AI_FILE_SORTER_LIVE_APP.",
    )
    parser.add_argument(
        "--model",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_LLM_MODEL"),
        help=(
            "Path to a local text GGUF model. Defaults to AI_FILE_SORTER_LIVE_LLM_MODEL, "
            "then falls back to the selected local model in AI File Sorter settings."
        ),
    )
    parser.add_argument(
        "--visual-model",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_VISUAL_MODEL"),
        help="Optional visual GGUF model for image-content rename cases.",
    )
    parser.add_argument(
        "--visual-mmproj",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_VISUAL_MMPROJ"),
        help="Optional visual mmproj GGUF for image-content rename cases.",
    )
    parser.add_argument(
        "--fixture-cache",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_FIXTURE_CACHE"),
        help="Directory used to cache downloaded public fixtures.",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_WORK_DIR"),
        help="Working directory for generated copies, config, logs, and status JSON.",
    )
    parser.add_argument(
        "--settings-file",
        type=Path,
        default=env_path("AI_FILE_SORTER_LIVE_SETTINGS_FILE"),
        help=(
            "Optional AI File Sorter config.ini used for model fallback. Defaults to the "
            "normal app config path, respecting AI_FILE_SORTER_CONFIG_DIR."
        ),
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=int(os.environ.get("AI_FILE_SORTER_LIVE_TIMEOUT", "900")),
        help="Per-headless-command timeout in seconds. Defaults to 900.",
    )
    parser.add_argument(
        "--backend",
        type=backend_choice,
        default=default_backend(),
        help=(
            "Windows launcher backend selection: auto, cpu, cuda, or vulkan. "
            "Defaults to AI_FILE_SORTER_LIVE_BACKEND, then auto."
        ),
    )
    parser.add_argument(
        "--only",
        default=os.environ.get("AI_FILE_SORTER_LIVE_ONLY", ""),
        help="Run only cases whose names match this regular expression.",
    )
    parser.add_argument(
        "--skip-downloads",
        action="store_true",
        default=os.environ.get("AI_FILE_SORTER_LIVE_SKIP_DOWNLOADS", "").lower() in {"1", "true", "yes"},
        help="Do not download public fixtures; image/PDF-dependent cases may skip.",
    )
    parser.add_argument(
        "--require-downloads",
        action="store_true",
        default=os.environ.get("AI_FILE_SORTER_LIVE_REQUIRE_DOWNLOADS", "").lower() in {"1", "true", "yes"},
        help="Fail instead of skipping when public fixture downloads are unavailable.",
    )
    parser.add_argument(
        "--force-visual-cpu",
        action="store_true",
        default=os.environ.get("AI_FILE_SORTER_LIVE_FORCE_VISUAL_CPU", "").lower() in {"1", "true", "yes"},
        help="Set AI_FILE_SORTER_VISUAL_USE_GPU=0 for image live cases.",
    )
    parser.add_argument(
        "--keep-work-dir",
        action="store_true",
        default=os.environ.get("AI_FILE_SORTER_LIVE_KEEP_WORK_DIR", "").lower() in {"1", "true", "yes"},
        help="Keep generated work files after the run.",
    )
    parser.add_argument(
        "--require-localized-renames",
        action="store_true",
        default=os.environ.get("AI_FILE_SORTER_LIVE_REQUIRE_LOCALIZED_RENAMES", "").lower()
        in {"1", "true", "yes"},
        help=(
            "Fail non-English rename cases when too few destination names show the requested "
            "language/script. By default weak localization is reported as WARN only."
        ),
    )
    parser.add_argument(
        "--color",
        choices=("auto", "always", "never"),
        default=default_color_mode(),
        help=(
            "Colorize terminal output: auto, always, or never. Defaults to "
            "AI_FILE_SORTER_LIVE_COLOR, then always; NO_COLOR disables color."
        ),
    )
    parser.add_argument("--verbose", action="store_true", help="Print app stdout/stderr paths for each case.")
    return parser.parse_args()


def env_path(name: str) -> Path | None:
    value = os.environ.get(name)
    if not value:
        return None
    return Path(value)


def default_color_mode() -> str:
    value = os.environ.get("AI_FILE_SORTER_LIVE_COLOR", "always").strip().lower()
    return value if value in {"auto", "always", "never"} else "always"


def configure_output(color_mode: str) -> None:
    configure_console_stream(sys.stdout)
    configure_console_stream(sys.stderr)
    global ANSI_ENABLED
    ANSI_ENABLED = should_use_ansi(color_mode)
    if ANSI_ENABLED:
        enable_windows_virtual_terminal()


def configure_console_stream(stream: Any) -> None:
    reconfigure = getattr(stream, "reconfigure", None)
    if callable(reconfigure):
        try:
            reconfigure(encoding="utf-8", errors="backslashreplace")
        except (OSError, ValueError):
            pass


def should_use_ansi(color_mode: str) -> bool:
    if os.environ.get("NO_COLOR"):
        return False
    if color_mode == "never":
        return False
    if color_mode == "always":
        return True
    if os.environ.get("FORCE_COLOR"):
        return True
    return bool(getattr(sys.stdout, "isatty", lambda: False)())


def enable_windows_virtual_terminal() -> None:
    if os.name != "nt":
        return
    try:
        import ctypes

        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)
        mode = ctypes.c_uint32()
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            kernel32.SetConsoleMode(handle, mode.value | 0x0004)
    except Exception:
        pass


def log(message: str) -> None:
    clear_case_progress_line()
    print(colorize_message(message), flush=True)
    write_progress_log(message)
    redraw_case_progress_after_log()


def write_progress_log(message: str) -> None:
    if PROGRESS_LOG_PATH is None:
        return
    try:
        with PROGRESS_LOG_PATH.open("a", encoding="utf-8") as handle:
            handle.write(message + "\n")
    except OSError:
        pass


def colorize_message(message: str) -> str:
    if not ANSI_ENABLED:
        return message
    if message.startswith("PASS"):
        return colorize_prefix(message, "PASS", ANSI_BOLD + ANSI_GREEN)
    if message.startswith("FAIL"):
        return colorize_prefix(message, "FAIL", ANSI_BOLD + ANSI_RED)
    if message.startswith("SKIP"):
        return colorize_prefix(message, "SKIP", ANSI_BOLD + ANSI_YELLOW)
    if message.startswith("RUN"):
        return colorize_prefix(message, "RUN", ANSI_BOLD + ANSI_CYAN)
    if message.startswith("WARN"):
        return colorize_prefix(message, "WARN", ANSI_BOLD + ANSI_YELLOW)
    if message.startswith("INFO"):
        return colorize_prefix(colorize_warning_tokens(message), "INFO", ANSI_BLUE)
    if message.startswith("SUMMARY"):
        return colorize_summary(message)
    return colorize_warning_tokens(message)


def colorize_prefix(message: str, prefix: str, color: str) -> str:
    return f"{color}{prefix}{ANSI_RESET}{message[len(prefix):]}"


def colorize_summary(message: str) -> str:
    color = ANSI_BOLD + (ANSI_RED if re.search(r"\bfail=[1-9]", message) else ANSI_GREEN)
    return colorize_prefix(message, "SUMMARY", color)


def colorize_warning_tokens(message: str) -> str:
    message = message.replace("[WARN]", f"{ANSI_BOLD}{ANSI_YELLOW}[WARN]{ANSI_RESET}")
    message = message.replace("status=running", f"status={ANSI_BLUE}running{ANSI_RESET}")
    message = message.replace("status=failed", f"status={ANSI_BOLD}{ANSI_RED}failed{ANSI_RESET}")
    message = message.replace("status=review_required", f"status={ANSI_BOLD}{ANSI_YELLOW}review_required{ANSI_RESET}")
    message = message.replace("status=completed", f"status={ANSI_BOLD}{ANSI_GREEN}completed{ANSI_RESET}")
    message = message.replace("backend=cuda", f"backend={ANSI_BOLD}{ANSI_GREEN}cuda{ANSI_RESET}")
    message = message.replace("backend=vulkan", f"backend={ANSI_BOLD}{ANSI_CYAN}vulkan{ANSI_RESET}")
    message = message.replace("backend=cpu", f"backend={ANSI_BOLD}{ANSI_YELLOW}cpu{ANSI_RESET}")
    message = message.replace("cuda_disabled=0", f"cuda_disabled={ANSI_GREEN}0{ANSI_RESET}")
    message = message.replace("cuda_disabled=1", f"cuda_disabled={ANSI_YELLOW}1{ANSI_RESET}")
    return message


def inline_progress_enabled() -> bool:
    value = os.environ.get("AI_FILE_SORTER_LIVE_INLINE_PROGRESS", "always").strip().lower()
    return value not in {"0", "false", "no", "never", "off"}


def start_case_progress(index: int, total: int, name: str) -> None:
    global CASE_PROGRESS_ACTIVE
    global CASE_PROGRESS_INDEX
    global CASE_PROGRESS_TOTAL
    global CASE_PROGRESS_NAME
    global CASE_PROGRESS_STARTED
    global CASE_PROGRESS_LAST_SECONDS
    global CASE_PROGRESS_LAST_WIDTH

    CASE_PROGRESS_ACTIVE = True
    CASE_PROGRESS_INDEX = index
    CASE_PROGRESS_TOTAL = total
    CASE_PROGRESS_NAME = name
    CASE_PROGRESS_STARTED = time.monotonic()
    CASE_PROGRESS_LAST_SECONDS = -1
    CASE_PROGRESS_LAST_WIDTH = 0

    run_message = case_progress_message(0)
    if inline_progress_enabled():
        write_progress_log(run_message)
        update_case_progress(force=True)
    else:
        log(run_message)


def update_case_progress(*, force: bool = False) -> None:
    global CASE_PROGRESS_LAST_SECONDS
    global CASE_PROGRESS_LAST_WIDTH

    if not CASE_PROGRESS_ACTIVE or not inline_progress_enabled():
        return
    elapsed = max(0, int(time.monotonic() - CASE_PROGRESS_STARTED))
    if not force and elapsed == CASE_PROGRESS_LAST_SECONDS:
        return
    CASE_PROGRESS_LAST_SECONDS = elapsed
    message = case_progress_message(elapsed)
    padding = max(0, CASE_PROGRESS_LAST_WIDTH - len(message))
    sys.stdout.write("\r" + colorize_message(message) + (" " * padding))
    sys.stdout.flush()
    CASE_PROGRESS_LAST_WIDTH = len(message)


def redraw_case_progress_after_log() -> None:
    if CASE_PROGRESS_ACTIVE and inline_progress_enabled():
        update_case_progress(force=True)


def finish_case_progress() -> None:
    global CASE_PROGRESS_ACTIVE
    clear_case_progress_line()
    CASE_PROGRESS_ACTIVE = False


def clear_case_progress_line() -> None:
    global CASE_PROGRESS_LAST_WIDTH
    if not CASE_PROGRESS_ACTIVE or not inline_progress_enabled() or CASE_PROGRESS_LAST_WIDTH <= 0:
        return
    sys.stdout.write("\r" + (" " * CASE_PROGRESS_LAST_WIDTH) + "\r")
    sys.stdout.flush()
    CASE_PROGRESS_LAST_WIDTH = 0


def case_progress_message(elapsed_seconds: int) -> str:
    return (
        f"RUN  {CASE_PROGRESS_INDEX:02d}/{CASE_PROGRESS_TOTAL:02d} "
        f"{CASE_PROGRESS_NAME} | elapsed={elapsed_seconds:>4}s"
    )


def configure_progress_log(work_root: Path) -> None:
    global PROGRESS_LOG_PATH
    PROGRESS_LOG_PATH = work_root / "progress.log"
    try:
        PROGRESS_LOG_PATH.write_text("", encoding="utf-8")
        latest_progress_pointer_path().write_text(str(work_root), encoding="utf-8")
    except OSError:
        pass


def latest_progress_pointer_path() -> Path:
    return Path(tempfile.gettempdir()) / "aifs-live-llm-latest.txt"


def clear_progress_pointer(work_root: Path) -> None:
    pointer = latest_progress_pointer_path()
    try:
        if pointer.exists() and pointer.read_text(encoding="utf-8").strip() == str(work_root):
            pointer.unlink()
    except OSError:
        pass


def backend_choice(value: str) -> str:
    normalized = value.strip().lower()
    if normalized not in LIVE_BACKENDS:
        choices = ", ".join(sorted(LIVE_BACKENDS))
        raise argparse.ArgumentTypeError(f"expected one of: {choices}")
    return normalized


def default_backend() -> str:
    value = os.environ.get("AI_FILE_SORTER_LIVE_BACKEND", "auto")
    try:
        return backend_choice(value)
    except argparse.ArgumentTypeError as exc:
        log(f"WARN: ignoring invalid AI_FILE_SORTER_LIVE_BACKEND={value!r}: {exc}; using auto")
        return "auto"


def resolve_app_entrypoint(app: Path) -> Path:
    resolved = app.resolve()
    if os.name != "nt":
        return resolved

    if resolved.name.casefold() == "aifilesorter-bin.exe":
        launcher = resolved.with_name("aifilesorter.exe")
        if launcher.exists():
            log(f"INFO: replacing direct Windows binary with launcher: {launcher}")
            return launcher.resolve()
        log(
            "WARN: running aifilesorter-bin.exe directly; bundled Windows backend DLL "
            "selection requires the sibling aifilesorter.exe launcher."
        )
    return resolved


def is_windows_launcher(app: Path) -> bool:
    return (
        os.name == "nt"
        and app.name.casefold() == "aifilesorter.exe"
        and app.with_name("aifilesorter-bin.exe").exists()
    )


def main() -> int:
    configure_console_stream(sys.stdout)
    configure_console_stream(sys.stderr)
    args = parse_args()
    configure_output(args.color)
    settings_config = load_app_settings_config(args.settings_file)
    missing_model_reason = ""
    if args.model is None:
        resolution, missing_model_reason = resolve_text_model_from_app_settings(settings_config)
        if resolution:
            args.model = resolution.path
            log(f"INFO: using text LLM from {resolution.source}: {resolution.path}")

    if args.visual_model is None and args.visual_mmproj is None:
        visual_resolution = resolve_visual_model_from_app_settings(settings_config)
        if visual_resolution:
            args.visual_model = visual_resolution[0].path
            args.visual_mmproj = visual_resolution[1].path
            log(
                "INFO: using visual LLM from "
                f"{visual_resolution[0].source}: model={args.visual_model} mmproj={args.visual_mmproj}"
            )

    skip_reason = first_missing_required_argument(args, missing_model_reason)
    if skip_reason:
        log(f"SKIP: {skip_reason}")
        return SKIP_EXIT_CODE

    assert args.app is not None
    assert args.model is not None

    app = resolve_app_entrypoint(args.app)
    uses_windows_launcher = is_windows_launcher(app)
    if uses_windows_launcher:
        log(f"INFO: using Windows launcher backend selection ({args.backend}): {app}")
    model = args.model.resolve()
    if not app.exists():
        log(f"SKIP: app executable does not exist: {app}")
        return SKIP_EXIT_CODE
    if not model.exists():
        log(f"SKIP: text LLM model does not exist: {model}")
        return SKIP_EXIT_CODE

    visual_model = existing_optional_path(args.visual_model, "visual model")
    visual_mmproj = existing_optional_path(args.visual_mmproj, "visual mmproj")
    if bool(visual_model) != bool(visual_mmproj):
        log("WARN: image-content cases need both --visual-model and --visual-mmproj; image cases will skip.")
        visual_model = None
        visual_mmproj = None

    created_temp_work = False
    if args.work_dir:
        work_root = args.work_dir.resolve()
        work_root.mkdir(parents=True, exist_ok=True)
    else:
        work_root = Path(tempfile.mkdtemp(prefix="aifs-live-llm-")).resolve()
        created_temp_work = True

    fixture_cache = (args.fixture_cache or (Path.home() / ".cache" / "ai-file-sorter" / "live-fixtures")).resolve()
    fixture_cache.mkdir(parents=True, exist_ok=True)
    configure_progress_log(work_root)
    log(f"INFO: live LLM work_dir={work_root}")
    log(f"INFO: live LLM progress_log={work_root / 'progress.log'}")
    log(f"INFO: live LLM latest_pointer={latest_progress_pointer_path()}")
    log(f"INFO: live LLM fixture_cache={fixture_cache}")
    log(f"INFO: live LLM text_model={model}")

    try:
        exit_code = 1
        downloaded_fixtures = (
            {}
            if args.skip_downloads
            else download_public_fixtures(fixture_cache, args.require_downloads)
        )
        ctx = LiveContext(
            app=app,
            uses_windows_launcher=uses_windows_launcher,
            backend=args.backend,
            text_model=model,
            visual_model=visual_model,
            visual_mmproj=visual_mmproj,
            work_root=work_root,
            fixture_cache=fixture_cache,
            timeout_seconds=max(60, args.timeout),
            require_downloads=args.require_downloads,
            keep_work_dir=args.keep_work_dir or args.work_dir is not None,
            force_visual_cpu=args.force_visual_cpu,
            require_localized_renames=args.require_localized_renames,
            verbose=args.verbose,
            downloaded_fixtures=downloaded_fixtures,
        )
        exit_code = run_cases(ctx, args.only)
        return exit_code
    finally:
        if created_temp_work and not args.keep_work_dir and exit_code != 1:
            shutil.rmtree(work_root, ignore_errors=True)
            clear_progress_pointer(work_root)
        elif created_temp_work and not args.keep_work_dir:
            log(f"INFO preserving work directory after failure for logs: {work_root}")


def first_missing_required_argument(args: argparse.Namespace, missing_model_reason: str) -> str | None:
    if args.app is None:
        return "--app or AI_FILE_SORTER_LIVE_APP is required"
    if args.model is None:
        return missing_model_reason or (
            "--model or AI_FILE_SORTER_LIVE_LLM_MODEL is required, and no local model "
            "could be resolved from AI File Sorter settings"
        )
    return None


def load_app_settings_config(explicit_settings_file: Path | None) -> AppSettingsConfig | None:
    path = (explicit_settings_file.resolve() if explicit_settings_file else default_app_settings_path())
    if not path or not path.exists():
        return None

    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    try:
        with path.open("r", encoding="utf-8-sig") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error) as exc:
        log(f"WARN: could not read AI File Sorter settings for model fallback: {path}: {exc}")
        return None
    return AppSettingsConfig(path=path, config=parser)


def default_app_settings_path() -> Path | None:
    override_root = os.environ.get("AI_FILE_SORTER_CONFIG_DIR")
    if override_root:
        return Path(override_root).expanduser() / "AIFileSorter" / "config.ini"

    if os.name == "nt":
        appdata = os.environ.get("APPDATA")
        if appdata:
            return Path(appdata) / "AIFileSorter" / "config.ini"
        return None

    home = os.environ.get("HOME")
    if not home:
        return None
    if sys.platform == "darwin":
        return Path(home) / "Library" / "Application Support" / "AIFileSorter" / "config.ini"
    return Path(home) / ".config" / "AIFileSorter" / "config.ini"


def resolve_text_model_from_app_settings(
    settings_config: AppSettingsConfig | None,
) -> tuple[ModelResolution | None, str]:
    if not settings_config:
        return None, "no AI File Sorter settings file was found for model fallback"

    config = settings_config.config
    choice = get_ini_value(config, "Settings", "LLMChoice", "Unset")
    if choice in {"Remote", "Remote_OpenAI", "Remote_Gemini", "Remote_Custom", "Unset", ""}:
        return None, (
            f"AI File Sorter settings at {settings_config.path} do not select a local GGUF model "
            f"(LLMChoice={choice or 'Unset'})"
        )

    if choice == "Custom":
        return resolve_custom_text_model(settings_config)

    return resolve_builtin_text_model(settings_config, choice)


def resolve_custom_text_model(settings_config: AppSettingsConfig) -> tuple[ModelResolution | None, str]:
    config = settings_config.config
    candidate_ids = unique_values(
        [get_ini_value(config, "LLMs", "ActiveCustomId", "")]
        + parse_ini_list(get_ini_value(config, "LLMs", "CustomIds", ""))
    )
    for custom_id in candidate_ids:
        path_text = get_ini_value(config, f"LLM_{custom_id}", "Path", "")
        if not path_text:
            continue
        candidate = expand_path(path_text)
        if candidate.exists():
            return ModelResolution(
                path=candidate,
                source=f"AI File Sorter settings ({settings_config.path}, custom LLM '{custom_id}')",
            ), ""
        return None, (
            f"AI File Sorter settings selected custom LLM '{custom_id}', but its Path does not exist: "
            f"{candidate}"
        )
    return None, f"AI File Sorter settings at {settings_config.path} select Custom, but no custom LLM Path was found"


def resolve_builtin_text_model(
    settings_config: AppSettingsConfig,
    choice: str,
) -> tuple[ModelResolution | None, str]:
    url_env = builtin_text_model_url_env(choice)
    if not url_env:
        return None, f"AI File Sorter settings selected unsupported local LLMChoice={choice}"

    url = env_or_embedded_env(url_env)
    if not url and choice == "Local_3b_legacy":
        url = (
            "https://huggingface.co/Mungert/Llama-3.2-3B-Instruct-GGUF/resolve/main/"
            "Llama-3.2-3B-Instruct-bf16-q4_k.gguf"
        )
    if not url:
        return None, f"could not resolve built-in model download URL env var {url_env} for LLMChoice={choice}"

    storage_dir = llm_storage_dir_from_settings(settings_config.config)
    if not storage_dir:
        return None, f"could not resolve AI File Sorter model storage directory for LLMChoice={choice}"

    candidate = storage_dir / file_name_from_url(url)
    if candidate.exists():
        return ModelResolution(
            path=candidate,
            source=f"AI File Sorter settings ({settings_config.path}, {choice})",
        ), ""
    return None, (
        f"AI File Sorter settings selected {choice}, but the downloaded model file was not found: "
        f"{candidate}"
    )


def resolve_visual_model_from_app_settings(
    settings_config: AppSettingsConfig | None,
) -> tuple[ModelResolution, ModelResolution] | None:
    if not settings_config:
        return None

    config = settings_config.config
    visual_model_id = get_ini_value(config, "Settings", "VisualModelId", "")
    custom_id = custom_visual_id(visual_model_id)
    if not custom_id:
        return None

    model_text = get_ini_value(config, f"LLM_{custom_id}", "Path", "")
    mmproj_text = get_ini_value(config, f"LLM_{custom_id}", "MmprojPath", "")
    if not model_text or not mmproj_text:
        return None
    model_path = expand_path(model_text)
    mmproj_path = expand_path(mmproj_text)
    if not model_path.exists() or not mmproj_path.exists():
        return None
    source = f"AI File Sorter settings ({settings_config.path}, custom visual LLM '{custom_id}')"
    return ModelResolution(model_path, source), ModelResolution(mmproj_path, source)


def builtin_text_model_url_env(choice: str) -> str:
    return {
        "Local_3b": "LOCAL_LLM_3B_DOWNLOAD_URL",
        "Local_4b_Gemma": "LOCAL_LLM_3B_DOWNLOAD_URL",
        "Local_7b": "LOCAL_LLM_7B_DOWNLOAD_URL",
        "Local_7b_Gemma": "LOCAL_LLM_7B_GEMMA_DOWNLOAD_URL",
        "Local_3b_legacy": "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL",
    }.get(choice, "")


def llm_storage_dir_from_settings(config: configparser.ConfigParser) -> Path | None:
    configured = get_ini_value(config, "Settings", "LlmStorageDir", "")
    if configured:
        return expand_path(configured)

    env_override = os.environ.get("AI_FILE_SORTER_LLM_STORAGE_DIR") or os.environ.get("AI_FILE_SORTER_LLM_DIR")
    if env_override:
        return expand_path(env_override)

    if os.name == "nt":
        appdata = os.environ.get("APPDATA")
        if appdata:
            return Path(appdata) / "aifilesorter" / "llms"
        return None

    home = os.environ.get("HOME")
    if not home:
        return None
    if sys.platform == "darwin":
        return Path(home) / "Library" / "Application Support" / "aifilesorter" / "llms"
    return Path(home) / ".local" / "share" / "aifilesorter" / "llms"


def env_or_embedded_env(name: str) -> str:
    value = os.environ.get(name, "").strip()
    if value:
        return value
    return embedded_env_values().get(name, "")


def embedded_env_values() -> dict[str, str]:
    env_path = Path(__file__).resolve().parents[2] / "app" / "resources" / ".env"
    values: dict[str, str] = {}
    try:
        for line in env_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            values[key.strip()] = value.strip()
    except OSError:
        return {}
    return values


def get_ini_value(config: configparser.ConfigParser, section: str, key: str, fallback: str) -> str:
    if not config.has_section(section):
        return fallback
    return config.get(section, key, fallback=fallback).strip()


def parse_ini_list(value: str) -> list[str]:
    delimiter = ";" if ";" in value else ","
    return [part.strip() for part in value.split(delimiter) if part.strip()]


def unique_values(values: Iterable[str]) -> list[str]:
    out: list[str] = []
    seen: set[str] = set()
    for value in values:
        if not value or value in seen:
            continue
        seen.add(value)
        out.append(value)
    return out


def expand_path(value: str) -> Path:
    expanded = os.path.expandvars(os.path.expanduser(value.strip()))
    return Path(expanded)


def file_name_from_url(url: str) -> str:
    parsed = urlparse(url)
    name = Path(unquote(parsed.path)).name
    if not name:
        raise LiveTestFailure(f"could not extract model filename from URL: {url}")
    return name


def custom_visual_id(value: str) -> str:
    prefix = "custom:"
    if not value.startswith(prefix):
        return ""
    return value[len(prefix):].strip()


def existing_optional_path(path: Path | None, label: str) -> Path | None:
    if path is None:
        return None
    resolved = path.resolve()
    if not resolved.exists():
        log(f"WARN: {label} does not exist, skipping dependent cases: {resolved}")
        return None
    return resolved


def run_cases(ctx: LiveContext, only: str) -> int:
    cases: list[tuple[str, Callable[[LiveContext], None]]] = [
        ("categorize_without_subcategories", case_categorize_without_subcategories),
        ("categorize_with_subcategories", case_categorize_with_subcategories),
        ("categorize_selected_file_boundary", case_selected_file_boundary),
        ("categorize_whitelist_restrictions", case_whitelist_restrictions),
        ("rename_documents_english", lambda c: case_rename_documents_language(c, "English")),
        ("rename_documents_french", lambda c: case_rename_documents_language(c, "French")),
        ("rename_documents_simplified_chinese", lambda c: case_rename_documents_language(c, "Simplified Chinese")),
        ("rename_documents_hindi", lambda c: case_rename_documents_language(c, "Hindi")),
        ("rename_images_english", lambda c: case_rename_images_language(c, "English")),
        ("rename_images_french", lambda c: case_rename_images_language(c, "French")),
        ("rename_images_simplified_chinese", lambda c: case_rename_images_language(c, "Simplified Chinese")),
        ("rename_images_hindi", lambda c: case_rename_images_language(c, "Hindi")),
        ("rename_media_metadata_flac", case_rename_media_metadata_flac),
        ("categorize_and_rename_review_plan", case_categorize_and_rename_review_plan),
    ]

    if only:
        matcher = re.compile(only)
        cases = [(name, fn) for name, fn in cases if matcher.search(name)]
        if not cases:
            log(f"SKIP: no live cases match --only={only!r}")
            return SKIP_EXIT_CODE

    results: list[CaseResult] = []
    total = len(cases)
    for index, (name, fn) in enumerate(cases, start=1):
        started = time.monotonic()
        start_case_progress(index, total, name)
        try:
            fn(ctx)
            result = CaseResult(name=name, status="PASS", seconds=time.monotonic() - started)
        except LiveTestSkip as exc:
            result = CaseResult(name=name, status="SKIP", seconds=time.monotonic() - started, detail=str(exc))
        except Exception as exc:
            result = CaseResult(name=name, status="FAIL", seconds=time.monotonic() - started, detail=str(exc))
        results.append(result)
        finish_case_progress()
        print_case_result(result)

    print_case_summary(results, ctx)
    if any(result.status == "FAIL" for result in results):
        return 1
    if all(result.status == "SKIP" for result in results):
        return SKIP_EXIT_CODE
    return 0


def print_case_summary(results: list[CaseResult], ctx: LiveContext) -> None:
    log("")
    totals = {
        "PASS": sum(1 for result in results if result.status == "PASS"),
        "SKIP": sum(1 for result in results if result.status == "SKIP"),
        "FAIL": sum(1 for result in results if result.status == "FAIL"),
    }
    log(
        "SUMMARY "
        f"pass={totals['PASS']} skip={totals['SKIP']} fail={totals['FAIL']} "
        f"work_dir={ctx.work_root}"
    )
    if not ctx.keep_work_dir:
        log(
            "INFO work directory will be removed after non-failing runs; "
            "failures keep it for logs and fixtures."
        )


def print_case_result(result: CaseResult) -> None:
    duration = f"{result.seconds:.1f}s"
    suffix = f" - {result.detail}" if result.detail else ""
    log(f"{result.status:4} {duration:>7} {result.name}{suffix}")


def download_public_fixtures(cache_dir: Path, require_downloads: bool) -> dict[str, Path]:
    downloaded: dict[str, Path] = {}
    for fixture in DOWNLOAD_FIXTURES:
        destination = cache_dir / fixture.file_name
        if destination.exists() and destination.stat().st_size >= fixture.min_bytes:
            downloaded[fixture.key] = destination
            continue
        try:
            request = urllib.request.Request(
                fixture.url,
                headers={"User-Agent": "AIFileSorterLiveTests/1.0"},
            )
            with urllib.request.urlopen(request, timeout=45) as response:
                payload = response.read()
            if len(payload) < fixture.min_bytes:
                raise LiveTestFailure(
                    f"downloaded {fixture.key} is unexpectedly small: {len(payload)} bytes"
                )
            tmp_destination = destination.with_suffix(destination.suffix + ".tmp")
            tmp_destination.write_bytes(payload)
            tmp_destination.replace(destination)
            downloaded[fixture.key] = destination
        except (OSError, urllib.error.URLError, LiveTestFailure) as exc:
            message = f"could not download {fixture.url}: {exc}"
            if require_downloads:
                raise LiveTestFailure(message) from exc
            log(f"WARN: {message}; dependent cases may skip.")
    return downloaded


def case_categorize_without_subcategories(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "categorize_without_subcategories")
    create_categorization_fixtures(target_dir, ctx.downloaded_fixtures, text_limit=4)
    result = run_headless(
        ctx,
        case_name="categorize_without_subcategories",
        operation="categorize",
        paths=[target_dir],
        overrides=base_categorization_overrides(use_subcategories=False, use_whitelist=False),
        auto_apply=False,
    )
    assert_review_required(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 4, "categorization review entries")
    for entry in entries:
        assert_nonempty(entry.get("category"), "category", entry)
        assert_no_subcategory_destination(entry)


def case_categorize_with_subcategories(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "categorize_with_subcategories")
    create_categorization_fixtures(target_dir, ctx.downloaded_fixtures, text_limit=5)
    result = run_headless(
        ctx,
        case_name="categorize_with_subcategories",
        operation="categorize",
        paths=[target_dir],
        overrides=base_categorization_overrides(use_subcategories=True, use_whitelist=False),
        auto_apply=False,
    )
    assert_review_required(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 5, "subcategory categorization review entries")
    with_subcategory = [entry for entry in entries if str(entry.get("subcategory") or "").strip()]
    assert_count_at_least(with_subcategory, 1, "entries with subcategories")
    for entry in entries:
        assert_nonempty(entry.get("category"), "category", entry)


def case_selected_file_boundary(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "categorize_selected_file_boundary")
    selected = write_fixture(
        target_dir,
        "selected_invoice_contrat.txt",
        "Selected contract invoice: payment milestones, vendor obligations, renewal terms, and compliance notes.",
    )
    sibling = write_fixture(
        target_dir,
        "unselected_sibling_should_remain.txt",
        "This sibling file must not be analyzed or moved by a selected-file headless run.",
    )
    result = run_headless(
        ctx,
        case_name="categorize_selected_file_boundary",
        operation="categorize",
        paths=[selected],
        overrides=base_categorization_overrides(use_subcategories=False, use_whitelist=False),
        auto_apply=True,
    )
    assert_completed(result.status)
    entries = review_entries(result.status)
    assert_count_exact(entries, 1, "selected-file categorization entries")
    if not sibling.exists():
        raise LiveTestFailure(f"unselected sibling was moved or removed: {sibling}")
    if selected.exists():
        raise LiveTestFailure(f"selected file was not moved during auto-apply: {selected}")


def case_whitelist_restrictions(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "categorize_whitelist_restrictions")
    write_fixture(
        target_dir,
        "accounts_payable_invoice_2026.txt",
        "Accounts payable invoice for office software, printer supplies, subscription renewal, and finance team billing.",
    )
    write_fixture(
        target_dir,
        "travel_budget_plan.md",
        "# Travel budget\nFlight options, hotel reservation, visa checklist, and customer visit agenda.",
    )
    write_fixture(
        target_dir,
        "office_supplier_contract.xml",
        "<contract><subject>Office supplier renewal</subject><billing>Quarterly invoice terms</billing></contract>",
    )

    allowed_categories = ["Documents", "Finance"]
    allowed_subcategories = ["Invoices", "Reports", "Travel", "Contracts"]
    overrides = base_categorization_overrides(
        use_subcategories=True,
        use_whitelist=True,
    )
    overrides["allowedCategories"] = allowed_categories
    overrides["allowedSubcategories"] = allowed_subcategories
    result = run_headless(
        ctx,
        case_name="categorize_whitelist_restrictions",
        operation="categorize",
        paths=[target_dir],
        overrides=overrides,
        auto_apply=False,
    )
    assert_review_required(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 3, "whitelist categorization review entries")
    for entry in entries:
        category = normalized_label(entry.get("category"))
        subcategory = normalized_label(entry.get("subcategory"))
        if category not in {normalized_label(value) for value in allowed_categories}:
            raise LiveTestFailure(f"category escaped whitelist: {entry}")
        if subcategory and subcategory not in {normalized_label(value) for value in allowed_subcategories}:
            raise LiveTestFailure(f"subcategory escaped whitelist: {entry}")


def case_rename_documents_language(ctx: LiveContext, language: str) -> None:
    target_dir = make_case_dir(ctx, f"rename_documents_{slug(language)}")
    for name, content in LOCALIZED_DOCUMENTS[language]:
        write_fixture(target_dir, name, content)
    extra_name, extra_content = LOCALIZED_EXTRA_DOCUMENTS[language]
    write_fixture(target_dir, extra_name, extra_content)
    result = run_headless(
        ctx,
        case_name=f"rename_documents_{slug(language)}",
        operation="rename",
        paths=[target_dir],
        overrides=base_rename_overrides(language, documents=True, images=False, media_metadata=False),
        auto_apply=True,
    )
    assert_completed(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 3, f"{language} document rename entries")
    assert_apply_count_at_least(result.status, "renamedCount", 2)
    assert_renamed_entries(
        entries,
        language=language,
        allow_unrenamed=True,
        require_language_signal=ctx.require_localized_renames,
    )


def case_rename_images_language(ctx: LiveContext, language: str) -> None:
    require_visual_artifacts(ctx)
    require_downloaded(ctx, ["wikimedia_png_transparency", "wikimedia_jpeg_flower"])
    target_dir = make_case_dir(ctx, f"rename_images_{slug(language)}")
    for source_key, name in zip(
        ["wikimedia_png_transparency", "wikimedia_jpeg_flower"],
        LOCALIZED_IMAGE_NAMES[language],
    ):
        shutil.copy2(ctx.downloaded_fixtures[source_key], target_dir / name)

    result = run_headless(
        ctx,
        case_name=f"rename_images_{slug(language)}",
        operation="rename",
        paths=[target_dir],
        overrides=base_rename_overrides(language, documents=False, images=True, media_metadata=False),
        auto_apply=True,
    )
    assert_completed(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 2, f"{language} image rename entries")
    assert_apply_count_at_least(result.status, "renamedCount", 2)
    assert_renamed_entries(
        entries,
        language=language,
        require_language_signal=ctx.require_localized_renames,
    )


def case_rename_media_metadata_flac(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "rename_media_metadata_flac")
    create_flac_with_vorbis_comments(target_dir / "untitled_नमूना_audio.flac")
    result = run_headless(
        ctx,
        case_name="rename_media_metadata_flac",
        operation="rename",
        paths=[target_dir],
        overrides=base_rename_overrides("English", documents=False, images=False, media_metadata=True),
        auto_apply=True,
    )
    assert_completed(result.status)
    entries = review_entries(result.status)
    if not entries:
        raise LiveTestSkip(
            "standalone headless audio/video metadata rename produced no review entries in this build"
        )
    assert_count_at_least(entries, 1, "media metadata rename entries")
    assert_apply_count_at_least(result.status, "renamedCount", 1)
    destination_names = [status_entry_name(entry, "destinationName").lower() for entry in entries]
    if not any("international" in name or "rename" in name or "2026" in name for name in destination_names):
        raise LiveTestFailure(f"media rename did not include expected metadata signal: {destination_names}")


def case_categorize_and_rename_review_plan(ctx: LiveContext) -> None:
    target_dir = make_case_dir(ctx, "categorize_and_rename_review_plan")
    write_fixture(
        target_dir,
        "scan_final_001.txt",
        "Board meeting minutes covering quarterly revenue, product launch risks, hiring plan, and investor action items.",
    )
    write_fixture(
        target_dir,
        "contrat_fournisseur_brouillon.md",
        "# Vendor contract draft\nScope of work, support obligations, renewal terms, and billing schedule.",
    )
    result = run_headless(
        ctx,
        case_name="categorize_and_rename_review_plan",
        operation="categorize-and-rename",
        paths=[target_dir],
        overrides=base_categorize_and_rename_overrides("French"),
        auto_apply=False,
    )
    assert_review_required(result.status)
    entries = review_entries(result.status)
    assert_count_at_least(entries, 2, "categorize-and-rename review entries")
    review_file = result.status.get("reviewFile") or result.status.get("review", {}).get("reviewFile")
    if not review_file:
        raise LiveTestFailure(f"review-required status did not include a review plan path: {result.status}")
    if not Path(str(review_file)).exists():
        raise LiveTestFailure(f"review plan file was not written: {review_file}")
    assert_renamed_entries(
        entries,
        language="French",
        allow_unrenamed=True,
        require_language_signal=ctx.require_localized_renames,
    )
    for entry in entries:
        assert_nonempty(entry.get("category"), "category", entry)


def create_categorization_fixtures(
    target_dir: Path,
    downloaded: dict[str, Path],
    *,
    text_limit: int | None = None,
    include_downloaded: bool = False,
    include_media: bool = False,
) -> None:
    documents = TEXT_DOCUMENTS if text_limit is None else TEXT_DOCUMENTS[:text_limit]
    for name, content in documents:
        write_fixture(target_dir, name, content)

    if not include_downloaded and not include_media:
        return

    pdf = downloaded.get("w3c_dummy_pdf")
    if include_downloaded and pdf:
        shutil.copy2(pdf, target_dir / "scan_合同_dummy.pdf")

    png = downloaded.get("wikimedia_png_transparency")
    if include_downloaded and png:
        shutil.copy2(png, target_dir / "IMG_0007_東京.png")

    jpg = downloaded.get("wikimedia_jpeg_flower")
    if include_downloaded and jpg:
        shutil.copy2(jpg, target_dir / "vacances-été_0001.jpeg")

    gif = downloaded.get("wikimedia_rotating_earth_gif")
    if include_downloaded and gif:
        shutil.copy2(gif, target_dir / "动画_earth_spin.gif")

    if include_media:
        create_flac_with_vorbis_comments(target_dir / "audio_note_नमूना.flac")


def write_fixture(directory: Path, name: str, content: str) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    normalized = textwrap.dedent(content).strip() + "\n"
    path.write_text(normalized, encoding="utf-8")
    return path


def create_flac_with_vorbis_comments(path: Path) -> None:
    comments = [
        "TITLE=International Rename Probe",
        "ARTIST=AIFS Live Ensemble",
        "ALBUM=Regression Sessions",
        "DATE=2026-07-25",
    ]
    streaminfo = bytes(34)
    vorbis_comments = make_vorbis_comment_payload(comments)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"fLaC"
        + flac_metadata_block_header(block_type=0, length=len(streaminfo), is_last=False)
        + streaminfo
        + flac_metadata_block_header(block_type=4, length=len(vorbis_comments), is_last=True)
        + vorbis_comments
    )


def make_vorbis_comment_payload(comments: Iterable[str]) -> bytes:
    payload = bytearray()
    vendor = b"AIFileSorterLiveTests"
    payload.extend(len(vendor).to_bytes(4, "little"))
    payload.extend(vendor)
    encoded_comments = [comment.encode("utf-8") for comment in comments]
    payload.extend(len(encoded_comments).to_bytes(4, "little"))
    for comment in encoded_comments:
        payload.extend(len(comment).to_bytes(4, "little"))
        payload.extend(comment)
    return bytes(payload)


def flac_metadata_block_header(*, block_type: int, length: int, is_last: bool) -> bytes:
    if block_type < 0 or block_type > 0x7F:
        raise ValueError(f"invalid FLAC metadata block type: {block_type}")
    if length < 0 or length > 0xFFFFFF:
        raise ValueError(f"invalid FLAC metadata block length: {length}")
    return bytes(
        [
            (0x80 if is_last else 0x00) | block_type,
            (length >> 16) & 0xFF,
            (length >> 8) & 0xFF,
            length & 0xFF,
        ]
    )


@dataclass
class HeadlessRunResult:
    status: dict[str, Any]
    command: list[str]
    stdout_path: Path
    stderr_path: Path
    status_path: Path


def run_headless(
    ctx: LiveContext,
    case_name: str,
    operation: str,
    paths: list[Path],
    overrides: dict[str, Any],
    auto_apply: bool,
) -> HeadlessRunResult:
    ctx.run_counter += 1
    run_dir = ctx.work_root / "runs" / f"{ctx.run_counter:02d}_{case_name}"
    run_dir.mkdir(parents=True, exist_ok=True)

    config_root = run_dir / "config-root"
    seed_config(config_root, ctx)

    overrides_path = run_dir / "settings-overrides.json"
    overrides_path.write_text(json.dumps(overrides, ensure_ascii=False, indent=2), encoding="utf-8")
    status_path = run_dir / "status.json"
    stdout_path = run_dir / "stdout.txt"
    stderr_path = run_dir / "stderr.txt"

    command = [
        str(ctx.app),
        *windows_launcher_backend_args(ctx),
        "--headless",
        "--operation",
        operation,
        "--status-file",
        str(status_path),
        "--settings-overrides-file",
        str(overrides_path),
        "--job-id",
        f"live-{ctx.run_counter:02d}-{case_name}",
        "--headless-auto-apply" if auto_apply else "--review-only",
    ]
    for path in paths:
        command.extend(["--path", str(path)])

    env = os.environ.copy()
    env["AI_FILE_SORTER_CONFIG_DIR"] = str(config_root)
    env.setdefault("QT_QPA_PLATFORM", "minimal" if os.name == "nt" else "offscreen")
    if ctx.force_visual_cpu:
        env["AI_FILE_SORTER_VISUAL_USE_GPU"] = "0"

    command_path = run_dir / "command.txt"
    command_path.write_text(command_for_display(command) + "\n", encoding="utf-8")
    log(
        f"INFO {case_name}: launch | operation={operation} | "
        f"mode={'auto-apply' if auto_apply else 'review-only'} | backend_request={ctx.backend}"
    )
    log(
        f"INFO {case_name}: files | status={status_path} | "
        f"stdout={stdout_path} | stderr={stderr_path}"
    )
    if ctx.verbose:
        log(f"INFO {case_name}: command={command_for_display(command)}")

    proc_returncode, last_status_signature = run_command_with_status_progress(
        command=command,
        env=env,
        cwd=ctx.work_root,
        timeout_seconds=ctx.timeout_seconds,
        status_path=status_path,
        stdout_path=stdout_path,
        stderr_path=stderr_path,
        case_name=case_name,
    )

    if proc_returncode != 0 and not status_path.exists():
        raise LiveTestFailure(
            f"{case_name} exited {proc_returncode} before writing status; "
            f"stdout={tail_file(stdout_path)} stderr={tail_file(stderr_path)} status={status_path}"
        )
    if ctx.uses_windows_launcher and proc_returncode == 0:
        status = wait_for_terminal_status(
            status_path,
            ctx.timeout_seconds,
            case_name=case_name,
            initial_signature=last_status_signature,
        )
    else:
        status = read_status(status_path)
    if proc_returncode != 0:
        raise LiveTestFailure(
            f"{case_name} exited {proc_returncode}: {status_summary(status)}; "
            f"stdout={tail_file(stdout_path)} stderr={tail_file(stderr_path)} status={status_path}"
        )
    if status.get("status") == "failed":
        raise LiveTestFailure(f"{case_name} failed: {status_summary(status)} status={status_path}")
    return HeadlessRunResult(
        status=status,
        command=command,
        stdout_path=stdout_path,
        stderr_path=stderr_path,
        status_path=status_path,
    )


def windows_launcher_backend_args(ctx: LiveContext) -> list[str]:
    if not ctx.uses_windows_launcher:
        return []
    if ctx.backend == "cpu":
        return ["--cuda=off", "--vulkan=off"]
    if ctx.backend == "cuda":
        return ["--cuda=on", "--vulkan=off"]
    if ctx.backend == "vulkan":
        return ["--cuda=off", "--vulkan=on"]
    return []


def command_for_display(command: list[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(command)
    return " ".join(shell_quote(part) for part in command)


def shell_quote(value: str) -> str:
    if re.fullmatch(r"[A-Za-z0-9_@%+=:,./\\-]+", value):
        return value
    return "'" + value.replace("'", "'\"'\"'") + "'"


def run_command_with_status_progress(
    *,
    command: list[str],
    env: dict[str, str],
    cwd: Path,
    timeout_seconds: int,
    status_path: Path,
    stdout_path: Path,
    stderr_path: Path,
    case_name: str,
) -> tuple[int, str]:
    started = time.monotonic()
    deadline = started + timeout_seconds
    last_signature = ""

    with stdout_path.open("w", encoding="utf-8", errors="replace") as stdout_file, \
            stderr_path.open("w", encoding="utf-8", errors="replace") as stderr_file:
        proc = subprocess.Popen(
            command,
            env=env,
            cwd=str(cwd),
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=stdout_file,
            stderr=stderr_file,
        )

        while True:
            returncode = proc.poll()
            last_signature = maybe_log_status_progress(status_path, case_name, last_signature)
            now = time.monotonic()
            update_case_progress()
            if returncode is not None:
                stdout_file.flush()
                stderr_file.flush()
                last_signature = maybe_log_status_progress(status_path, case_name, last_signature, force=True)
                return returncode, last_signature
            if now >= deadline:
                proc.kill()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass
                raise LiveTestFailure(f"{case_name} timed out after {timeout_seconds}s; logs: {stdout_path.parent}")
            time.sleep(1)


def maybe_log_status_progress(
    status_path: Path,
    case_name: str,
    last_signature: str,
    *,
    force: bool = False,
) -> str:
    status = try_read_status(status_path)
    if not status:
        return last_signature
    signature = status_progress_signature(status)
    if signature == last_signature:
        return last_signature
    log(f"INFO {case_name}: {status_progress_summary(status)}")
    return signature


def try_read_status(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def status_progress_signature(status: dict[str, Any]) -> str:
    return "|".join(
        [
            str(status.get("status") or ""),
            str(status.get("message") or ""),
            str(status.get("error") or ""),
            json.dumps(status.get("runtime") or {}, sort_keys=True),
        ]
    )


def status_progress_summary(status: dict[str, Any]) -> str:
    parts = [
        f"status={status.get('status') or '<missing>'}",
    ]
    runtime = status.get("runtime")
    if isinstance(runtime, dict):
        backend = str(runtime.get("gpuBackend") or "")
        ggml_dir = str(runtime.get("ggmlDir") or "")
        llama_device = str(runtime.get("llamaDevice") or "")
        cuda_disabled = str(runtime.get("ggmlDisableCuda") or "")
        if backend:
            parts.append(f"backend={backend}")
        if llama_device:
            parts.append(f"device={llama_device}")
        if cuda_disabled:
            parts.append(f"cuda_disabled={cuda_disabled}")
        if ggml_dir:
            parts.append(f"ggml_dir={short_path(ggml_dir)}")
    message = str(status.get("message") or "").strip()
    if message:
        parts.append(f"message={compact(message)}")
    error = str(status.get("error") or "").strip()
    if error:
        parts.append(f"error={compact(error)}")
    return " | ".join(parts)


def short_path(value: str) -> str:
    try:
        path = Path(value)
        if path.name and path.parent.name:
            return str(path.parent.name + "/" + path.name)
        if path.name:
            return path.name
    except (OSError, ValueError):
        pass
    return value


def compact(value: str, limit: int = 240) -> str:
    one_line = re.sub(r"\s+", " ", value).strip()
    if len(one_line) <= limit:
        return one_line
    return one_line[: limit - 3] + "..."


def seed_config(config_root: Path, ctx: LiveContext) -> None:
    config_dir = config_root / "AIFileSorter"
    config_dir.mkdir(parents=True, exist_ok=True)
    sections: list[tuple[str, list[tuple[str, str]]]] = [
        (
            "Settings",
            [
                ("LLMChoice", "Custom"),
                ("Language", "English"),
                ("CategoryLanguage", "English"),
                ("HeadlessReviewBeforeApply", "true"),
                ("UseSubcategories", "true"),
                ("UseWhitelist", "false"),
                ("CategorizeFiles", "true"),
                ("CategorizeDirectories", "false"),
                ("IncludeSubdirectories", "false"),
                ("AnalyzeImagesByContent", "false"),
                ("ProcessImagesOnly", "false"),
                ("OfferRenameImages", "true"),
                ("RenameImagesOnly", "false"),
                ("AnalyzeDocumentsByContent", "true"),
                ("ProcessDocumentsOnly", "false"),
                ("OfferRenameDocuments", "true"),
                ("RenameDocumentsOnly", "false"),
                ("AddAudioVideoMetadataToFilename", "true"),
                ("AddDocumentDateToCategory", "false"),
                ("AddImageDateToCategory", "false"),
                ("AddImageDatePlaceToFilename", "false"),
            ],
        ),
        (
            "LLMs",
            [
                ("ActiveCustomId", "live_text"),
                ("CustomIds", "live_text,live_visual" if ctx.visual_model and ctx.visual_mmproj else "live_text"),
            ],
        ),
        (
            "LLM_live_text",
            [
                ("Name", "Live Text LLM"),
                ("Path", path_for_ini(ctx.text_model)),
            ],
        ),
    ]
    if ctx.visual_model and ctx.visual_mmproj:
        sections[0][1].append(("VisualModelId", "custom:live_visual"))
        sections.append(
            (
                "LLM_live_visual",
                [
                    ("Name", "Live Visual LLM"),
                    ("Path", path_for_ini(ctx.visual_model)),
                    ("MmprojPath", path_for_ini(ctx.visual_mmproj)),
                ],
            )
        )

    write_ini(config_dir / "config.ini", sections)
    write_live_whitelist_ini(config_dir / "whitelists.ini")


def write_ini(path: Path, sections: list[tuple[str, list[tuple[str, str]]]]) -> None:
    lines: list[str] = []
    for section, values in sections:
        lines.append(f"[{section}]")
        for key, value in values:
            lines.append(f"{key}={value}")
        lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_live_whitelist_ini(path: Path) -> None:
    sections = [
        ("__meta__", [("BuiltInSeedVersion", "4")]),
        (
            "Live LLM Whitelist",
            [
                ("Categories", "Documents, Finance"),
                ("Subcategories", "Invoices, Reports, Travel, Contracts"),
            ],
        ),
    ]
    write_ini(path, sections)


def path_for_ini(path: Path) -> str:
    return str(path.resolve()).replace("\\", "/")


def base_categorization_overrides(
    *,
    use_subcategories: bool,
    use_whitelist: bool,
) -> dict[str, Any]:
    overrides: dict[str, Any] = {
        "categoryLanguage": "English",
        "useSubcategories": use_subcategories,
        "useWhitelist": use_whitelist,
        "categorizeFiles": True,
        "categorizeDirectories": False,
        "includeSubdirectories": False,
        "analyzeImagesByContent": False,
        "processImagesOnly": False,
        "offerRenameImages": False,
        "renameImagesOnly": False,
        "analyzeDocumentsByContent": True,
        "processDocumentsOnly": False,
        "offerRenameDocuments": False,
        "renameDocumentsOnly": False,
        "addAudioVideoMetadataToFilename": True,
    }
    if use_whitelist:
        overrides["activeWhitelist"] = "Live LLM Whitelist"
    return overrides


def base_rename_overrides(
    language: str,
    *,
    documents: bool,
    images: bool,
    media_metadata: bool,
) -> dict[str, Any]:
    return {
        "categoryLanguage": language,
        "useSubcategories": False,
        "useWhitelist": False,
        "categorizeFiles": False,
        "categorizeDirectories": False,
        "includeSubdirectories": False,
        "analyzeImagesByContent": images,
        "processImagesOnly": images,
        "offerRenameImages": images,
        "renameImagesOnly": images,
        "analyzeDocumentsByContent": documents,
        "processDocumentsOnly": documents,
        "offerRenameDocuments": documents,
        "renameDocumentsOnly": documents,
        "addAudioVideoMetadataToFilename": media_metadata,
    }


def base_categorize_and_rename_overrides(language: str) -> dict[str, Any]:
    overrides = base_categorization_overrides(use_subcategories=True, use_whitelist=False)
    overrides.update(
        {
            "categoryLanguage": language,
            "analyzeImagesByContent": False,
            "processImagesOnly": False,
            "analyzeDocumentsByContent": True,
            "processDocumentsOnly": False,
            "addAudioVideoMetadataToFilename": False,
        }
    )
    return overrides


def make_case_dir(ctx: LiveContext, case_name: str) -> Path:
    path = ctx.work_root / "fixtures" / f"{ctx.run_counter + 1:02d}_{case_name}"
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    return path


def require_visual_artifacts(ctx: LiveContext) -> None:
    if not ctx.visual_model or not ctx.visual_mmproj:
        raise LiveTestSkip("set AI_FILE_SORTER_LIVE_VISUAL_MODEL and AI_FILE_SORTER_LIVE_VISUAL_MMPROJ")


def require_downloaded(ctx: LiveContext, keys: Iterable[str]) -> None:
    missing = [key for key in keys if key not in ctx.downloaded_fixtures]
    if missing:
        message = f"missing downloaded fixtures: {', '.join(missing)}"
        if ctx.require_downloads:
            raise LiveTestFailure(message)
        raise LiveTestSkip(message)


def read_status(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise LiveTestFailure(f"status file was not written: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise LiveTestFailure(f"status file is not valid JSON: {path}: {exc}") from exc


def wait_for_terminal_status(
    path: Path,
    timeout_seconds: int,
    *,
    case_name: str = "",
    initial_signature: str = "",
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout_seconds
    last_status: dict[str, Any] | None = None
    last_decode_error: json.JSONDecodeError | None = None
    last_signature = initial_signature
    while time.monotonic() < deadline:
        update_case_progress()
        if path.exists():
            try:
                status = json.loads(path.read_text(encoding="utf-8"))
            except json.JSONDecodeError as exc:
                last_decode_error = exc
            else:
                last_status = status if isinstance(status, dict) else {}
                if case_name:
                    signature = status_progress_signature(last_status)
                    if signature != last_signature:
                        log(f"INFO {case_name}: {status_progress_summary(last_status)}")
                        last_signature = signature
                if str(last_status.get("status") or "") in TERMINAL_STATUS_VALUES:
                    return last_status
        time.sleep(0.5)

    if last_status is not None:
        raise LiveTestFailure(f"status file did not reach a terminal state: {path}: {last_status}")
    if last_decode_error is not None:
        raise LiveTestFailure(f"status file is not valid JSON: {path}: {last_decode_error}") from last_decode_error
    raise LiveTestFailure(f"status file was not written within {timeout_seconds}s: {path}")


def status_summary(status: dict[str, Any]) -> str:
    return (
        f"status={status.get('status')!r} "
        f"message={status.get('message')!r} "
        f"error={status.get('error')!r}"
    )


def tail(value: str, limit: int = 800) -> str:
    value = value.strip()
    if len(value) <= limit:
        return value
    return value[-limit:]


def tail_file(path: Path, limit: int = 800) -> str:
    try:
        return tail(path.read_text(encoding="utf-8", errors="replace"), limit)
    except OSError as exc:
        return f"<could not read {path}: {exc}>"


def assert_completed(status: dict[str, Any]) -> None:
    if status.get("status") != "completed":
        raise LiveTestFailure(f"expected completed status, got {status_summary(status)}")


def assert_review_required(status: dict[str, Any]) -> None:
    if status.get("status") != "review_required":
        raise LiveTestFailure(f"expected review_required status, got {status_summary(status)}")


def review_entries(status: dict[str, Any]) -> list[dict[str, Any]]:
    review = status.get("review")
    if not isinstance(review, dict):
        raise LiveTestFailure(f"status does not contain review object: {status}")
    entries = review.get("entries")
    if not isinstance(entries, list):
        raise LiveTestFailure(f"status review does not contain entries array: {status}")
    return [entry for entry in entries if isinstance(entry, dict)]


def assert_count_at_least(values: list[Any], minimum: int, label: str) -> None:
    if len(values) < minimum:
        raise LiveTestFailure(f"expected at least {minimum} {label}, got {len(values)}")


def assert_count_exact(values: list[Any], expected: int, label: str) -> None:
    if len(values) != expected:
        raise LiveTestFailure(f"expected {expected} {label}, got {len(values)}")


def assert_apply_count_at_least(status: dict[str, Any], field: str, minimum: int) -> None:
    apply = status.get("apply")
    if not isinstance(apply, dict):
        raise LiveTestFailure(f"status does not contain apply object: {status}")
    value = apply.get(field)
    if not isinstance(value, int) or value < minimum:
        raise LiveTestFailure(f"expected apply.{field} >= {minimum}, got {value}: {apply}")


def assert_nonempty(value: Any, label: str, entry: dict[str, Any]) -> None:
    if not str(value or "").strip():
        raise LiveTestFailure(f"entry missing {label}: {entry}")


def assert_no_subcategory_destination(entry: dict[str, Any]) -> None:
    category = str(entry.get("category") or "").strip()
    destination = str(entry.get("destination") or "")
    if not category or not destination:
        return
    parent_parts = path_parts(destination)[:-1]
    if len(parent_parts) >= 2 and normalized_label(parent_parts[-2]) == normalized_label(category):
        raise LiveTestFailure(f"destination appears to include a subcategory while disabled: {entry}")


def assert_renamed_entries(
    entries: list[dict[str, Any]],
    *,
    language: str,
    allow_unrenamed: bool = False,
    require_language_signal: bool = False,
) -> None:
    renamed_entries = [entry for entry in entries if bool(entry.get("renamed")) or changed_destination_name(entry)]
    if not allow_unrenamed and len(renamed_entries) != len(entries):
        raise LiveTestFailure(f"some entries were not renamed: {entries}")
    if allow_unrenamed and not renamed_entries:
        raise LiveTestFailure(f"expected at least one renamed entry: {entries}")
    localized_signal_count = 0
    for entry in renamed_entries:
        source_name = status_entry_name(entry, "fileName")
        destination_name = status_entry_name(entry, "destinationName")
        if source_name == destination_name:
            raise LiveTestFailure(f"rename kept the same file name: {entry}")
        if suffix_from_name(source_name) != suffix_from_name(destination_name):
            raise LiveTestFailure(f"rename changed file extension: {entry}")
        if has_language_signal(destination_name, language):
            localized_signal_count += 1
    assert_language_signal_count(
        renamed_entries,
        language,
        localized_signal_count,
        allow_unrenamed,
        require_language_signal,
    )


def changed_destination_name(entry: dict[str, Any]) -> bool:
    return status_entry_name(entry, "fileName") != status_entry_name(entry, "destinationName")


def status_entry_name(entry: dict[str, Any], key: str) -> str:
    value = str(entry.get(key) or "").strip()
    if value:
        return value
    path_key = "source" if key == "fileName" else "destination"
    path = str(entry.get(path_key) or "")
    return name_from_path(path)


def name_from_path(value: str) -> str:
    if "\\" in value or re.match(r"^[A-Za-z]:", value):
        return PureWindowsPath(value).name
    return Path(value).name


def suffix_from_name(value: str) -> str:
    return Path(name_from_path(value)).suffix.lower()


def assert_language_signal_count(
    renamed_entries: list[dict[str, Any]],
    language: str,
    localized_signal_count: int,
    allow_unrenamed: bool,
    require_language_signal: bool,
) -> None:
    if not language_requires_signal(language):
        return
    required = 1 if allow_unrenamed else max(1, (len(renamed_entries) + 1) // 2)
    if localized_signal_count >= required:
        return
    destination_names = [status_entry_name(entry, "destinationName") for entry in renamed_entries]
    message = (
        f"{language} rename only had {localized_signal_count}/{len(renamed_entries)} "
        f"localized filename signals; expected at least {required}: {destination_names}"
    )
    if require_language_signal:
        raise LiveTestFailure(message)
    log(f"WARN: {message}")


def language_requires_signal(language: str) -> bool:
    return language == "French" or language in LANGUAGE_SCRIPT_CHECKS


def has_language_signal(file_name: str, language: str) -> bool:
    stem = Path(file_name).stem
    checker = LANGUAGE_SCRIPT_CHECKS.get(language)
    if checker:
        return bool(checker.search(stem))
    if language == "French":
        return any(hint in stem.casefold() for hint in FRENCH_STEM_HINTS)
    return True


def path_parts(value: str) -> list[str]:
    return [part for part in re.split(r"[\\/]+", value) if part]


def normalized_label(value: Any) -> str:
    return str(value or "").strip().casefold()


def slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


if __name__ == "__main__":
    sys.exit(main())
