from collections import Counter
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
TASK_PATH = ROOT / "docs" / "task.md"
ISSUE_PATTERN = re.compile(
    r"https://github\.com/jm2/Quake-III-Arena/issues/(\d+)")

EXPECTED_PRIORITIES = {
    "P0": (29, 35, 37, 36, 41, 42, 43, 44, 45, 46, 47, 48),
    "P1": (
        22, 23, 1, 2, 8, 50, 15, 16, 17, 20, 19, 5, 4, 11, 12, 13,
        14, 3, 24, 18, 49, 38, 39, 40,
    ),
    "P2": (6, 7, 9, 10, 21, 25, 26, 27, 28, 33, 51, 34),
    "P3": (30, 31, 32, 52),
}


class ReviewLedgerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = TASK_PATH.read_text(encoding="utf-8")
        cls.lines = cls.text.splitlines()

    def test_every_confirmed_issue_is_linked_exactly_once(self):
        numbers = [int(number) for number in ISSUE_PATTERN.findall(self.text)]
        counts = Counter(numbers)
        expected_numbers = {
            number
            for priority in EXPECTED_PRIORITIES.values()
            for number in priority
        }
        self.assertEqual(expected_numbers, set(range(1, 53)))
        self.assertEqual(set(counts), expected_numbers)
        self.assertEqual(
            {number: count for number, count in counts.items() if count != 1},
            {},
        )

    def test_priority_sections_are_ordered_and_have_expected_issues(self):
        headings = {
            priority: self.text.index(f"## {priority} ")
            for priority in ("P0", "P1", "P2", "P3")
        }
        self.assertLess(headings["P0"], headings["P1"])
        self.assertLess(headings["P1"], headings["P2"])
        self.assertLess(headings["P2"], headings["P3"])

        ends = {
            "P0": headings["P1"],
            "P1": headings["P2"],
            "P2": headings["P3"],
            "P3": self.text.index("## Completed review work"),
        }
        for priority, expected in EXPECTED_PRIORITIES.items():
            section = self.text[headings[priority]:ends[priority]]
            found = tuple(
                int(number) for number in ISSUE_PATTERN.findall(section)
            )
            self.assertEqual(found, expected, priority)

    def test_issue_entries_are_open_checkboxes_with_severity(self):
        for index, line in enumerate(self.lines):
            if not ISSUE_PATTERN.search(line):
                continue
            self.assertTrue(line.startswith("- [ ] [#"), line)
            entry = "\n".join(self.lines[index:index + 2])
            self.assertIn("**", entry, entry)

    def test_continuation_queue_contains_unfinished_work(self):
        continuation = self.text[self.text.index("## Exact continuation point"):]
        self.assertGreaterEqual(continuation.count("- [ ]"), 5)
        self.assertIn("P0 implementation order", continuation)


if __name__ == "__main__":
    unittest.main()
