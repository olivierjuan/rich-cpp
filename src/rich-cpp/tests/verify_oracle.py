import subprocess
import sys
import difflib

TEST_CASES = [
    "panel_basic",
    "panel_multiline",
    "table_basic",
    "rule_basic",
    "columns_basic",
    "box_double",
    "box_heavy",
    "box_ascii",
    "text_wrap",
    "text_wrap_cjk",
    "text_wrap_emoji",
    "tree_basic",
    "align_center",
    "align_right",
    "align_left",
    "constrain_basic",
    "layout_row",
    "layout_column",
    "status_static",
    "live_static",
    "progress_tracker",
    "progress_speed",
    "export_svg",
    "export_html",
    "emoji_basic",
    "pretty_list",
    "pretty_dict",
    "pretty_scalars",
    "pretty_options",
    "json_basic",
    "syntax_basic",
    "markdown_basic",
    "markdown_extras",
    "progress_bar_basic",
    "markup_basic",
    "padding_basic",
    "track_basic",
    "style_sgr",
    "style_sgr_256",
    "log_basic",
    "log_sgr",
    "theme_push",
    "capture_basic",
    "screen_basic",
    "control_basic",
    "colorfile_basic",
    "fileproxy_basic",
    "from_ansi_basic",
    "highlighter_basic",
    "text_extras_basic",
    "table_extras",
    "box_variants",
    "panel_extras",
    "rule_extras",
    "tree_extras",
    "columns_extras",
    "progress_extras",
    "spinner_registry",
    "live_extras",
    "pretty_sgr",
    "syntax_extras",
    "number_extras",
    "markup_escape",
]

import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = os.path.join(ROOT, "tests")

def run_python_oracle(test_name: str) -> bytes:
    res = subprocess.run(
        [sys.executable, os.path.join(TESTS, "oracle_gen.py"), test_name],
        capture_output=True,
        check=True
    )
    return res.stdout

def run_cpp_test(test_name: str) -> bytes:
    res = subprocess.run(
        [os.path.normpath(os.path.join(ROOT, os.pardir, os.pardir, "build", "Release", "test_runner.exe")), test_name],
        capture_output=True,
        check=True
    )
    return res.stdout

def main():
    total = len(TEST_CASES)
    passed = 0
    print(f"Running {total} Python-Rich vs C++-Rich Oracle validation tests...\n")

    for name in TEST_CASES:
        py_out = run_python_oracle(name)
        cpp_out = run_cpp_test(name)

        if py_out == cpp_out:
            print(f"  [PASS] {name} - exact byte match ({len(py_out)} bytes)")
            passed += 1
        else:
            print(f"  [FAIL] {name} - byte mismatch!")
            py_str = py_out.decode("utf-8", errors="replace")
            cpp_str = cpp_out.decode("utf-8", errors="replace")
            diff = difflib.unified_diff(
                py_str.splitlines(keepends=True),
                cpp_str.splitlines(keepends=True),
                fromfile="Python Rich",
                tofile="C++ Rich"
            )
            print("".join(diff))

    print(f"\nResult: {passed}/{total} tests passed.")
    if passed != total:
        sys.exit(1)

if __name__ == "__main__":
    main()
