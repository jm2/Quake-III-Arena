"""Structural checks for the docs/task.md burndown ledger (#229).

The expected issue set is derived from the ledger itself, so filing a new
issue only requires adding its entry; nothing here needs editing.
"""
from collections import Counter
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
TASK_PATH = ROOT / "docs" / "task.md"
ISSUE_URL = r"https://github\.com/jm2/Quake-III-Arena/issues/(\d+)"
ISSUE_PATTERN = re.compile(ISSUE_URL)
ENTRY_PATTERN = re.compile(r"^- \[([ x])\] \[#(\d+) [^\]]*\]\(" + ISSUE_URL + r"\)(.*)$")
SECTION_PATTERN = re.compile(r"^## B(\d+) — \S")
SEVERITY_PATTERN = re.compile(
    r"\*\*(?:assurance gate|critical|high|moderate-high|medium-low|medium|low)\*\*")


class ReviewLedgerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Read the checked-in ledger once for structural validation."""
        cls.text = TASK_PATH.read_text(encoding="utf-8")
        cls.lines = cls.text.splitlines()
        cls.entries = []
        section = None
        for number, line in enumerate(cls.lines, 1):
            heading = SECTION_PATTERN.match(line)
            if heading:
                section = int(heading.group(1))
            match = ENTRY_PATTERN.match(line)
            if match:
                cls.entries.append((number, section, match))

    def test_burndown_sections_are_in_order(self):
        """B0, B1, ... appear once each and in ascending order."""
        sections = [int(m.group(1)) for m in map(SECTION_PATTERN.match, self.lines) if m]
        self.assertGreater(len(sections), 0)
        self.assertEqual(sections, list(range(len(sections))))

    def test_every_issue_has_exactly_one_entry(self):
        """Each issue is a checkbox entry once; its link text and URL agree."""
        self.assertGreater(len(self.entries), 0)
        counts = Counter(int(m.group(2)) for _, _, m in self.entries)
        self.assertEqual({n: c for n, c in counts.items() if c != 1}, {})
        for line, _, match in self.entries:
            self.assertEqual(match.group(2), match.group(3), f"line {line}")

    def test_entries_live_in_burndown_sections(self):
        """No entry precedes the first B section heading."""
        for line, section, _ in self.entries:
            self.assertIsNotNone(section, f"line {line} is outside a B section")

    def test_links_only_reference_entered_issues(self):
        """Any other issue link in the ledger must point at an issue with an entry."""
        entered = {int(m.group(2)) for _, _, m in self.entries}
        linked = {int(n) for n in ISSUE_PATTERN.findall(self.text)}
        self.assertEqual(linked - entered, set())

    def test_issue_links_outside_entries_are_not_checkboxes(self):
        """A checkbox line that links an issue must be a well-formed entry."""
        for number, line in enumerate(self.lines, 1):
            if line.startswith("- [") and ISSUE_PATTERN.search(line):
                self.assertRegex(line, ENTRY_PATTERN, f"line {number}")

    def test_entries_carry_a_severity(self):
        """Every entry states a recognized severity."""
        for line, _, match in self.entries:
            self.assertRegex(match.group(4), SEVERITY_PATTERN, f"line {line}")


if __name__ == "__main__":
    unittest.main()
