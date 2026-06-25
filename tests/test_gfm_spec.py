import json
from pathlib import Path

import pytest

from comverter import markdown_to_html

SPEC_PATH = Path(__file__).parent / "clean-gfm-tests.json"

with open(SPEC_PATH, "r", encoding="utf-8") as f:
    spec_examples = json.load(f)


@pytest.mark.parametrize(
    "example",
    [pytest.param(e, id=f"#{e['example']:04d}-{e['section']}") for e in spec_examples],
)
def test_spec_example(example):
    result = markdown_to_html(example["markdown"])
    assert result == example["html"], (
        f"\nExample #{example['example']} ({example['section']}, "
        f"line {example['start_line']})\n"
        f"--- markdown ---\n{example['markdown']}\n"
        f"--- expected ---\n{example['html']}\n"
        f"--- got ---\n{result}\n"
    )
