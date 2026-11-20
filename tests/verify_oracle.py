import subprocess
import sys
import difflib

TEST_CASES = [
    "panel_basic",
    "panel_multiline",
    "table_basic",
    "rule_basic",
    "columns_basic",
]

def run_python_oracle(test_name: str) -> bytes:
    res = subprocess.run(
        [sys.executable, "tests/oracle_gen.py", test_name],
        capture_output=True,
        check=True
    )
    return res.stdout

def run_cpp_test(test_name: str) -> bytes:
    res = subprocess.run(
        ["./build/Release/test_runner.exe", test_name],
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
