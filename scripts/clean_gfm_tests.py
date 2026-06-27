import json
from pathlib import Path

root = Path(__file__).parent.parent

with open(
    root / "tests" / "commonmark-spec-0.31.2.json", "r", encoding="utf-8"
) as file:
    commonmark_tests = json.load(file)

commonmark_tests_dict = {test["markdown"]: test for test in commonmark_tests}

with open(root / "tests" / "gfm-spec-0.29.0-13.json", "r", encoding="utf-8") as file:
    gfm_tests = json.load(file)

clean_gfm = []

for test in gfm_tests:
    markdown = test["markdown"]
    if markdown in commonmark_tests_dict:
        commonmark_test = commonmark_tests_dict[markdown]
        if test["html"] != commonmark_test["html"]:
            html1 = test["html"]
            html2 = commonmark_tests_dict[markdown]["html"]
            print(
                f"Test {markdown!r} has different HTML output in GFM and "
                f"CommonMark:\nGFM:        {html1!r}\nCommonMark: {html2!r}\n"
            )
    else:
        clean_gfm.append(test)

with open(root / "tests" / "clean-gfm-tests.json", "w", encoding="utf-8") as file:
    json.dump(clean_gfm, file, indent=2)
