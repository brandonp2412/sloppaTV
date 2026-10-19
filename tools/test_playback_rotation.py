import unittest
from datetime import UTC, datetime, timedelta

from playback_rotation import select_cases

NOW = datetime(2026, 10, 19, 6, tzinfo=UTC)


def case(key, **extra):
    return {"key": key, "title": key, "status": "open", "attempts": [], **extra}


class RotationTests(unittest.TestCase):
    def select(self, *items, changes=()):
        return {x["key"]: x for x in select_cases({"items": list(items)}, "current", NOW, set(changes))}

    def test_title_cap_even_with_new_fix(self):
        item = case("a", attempts=[{"run_id": "current"}, {"run_id": "current"}])
        self.assertEqual(self.select(item, changes=["a"])["a"]["reason"], "two-attempt cap")

    def test_duplicate_title_entries_share_cap(self):
        a = case("a", title="Same Film", attempts=[{"run_id": "current"}] * 2)
        b = case("b", title="same   film")
        self.assertEqual(self.select(a, b)["b"]["reason"], "two-attempt cap")

    def test_shared_root_cause_cap(self):
        a = case("a", root_cause="timeout", attempts=[{"run_id": "current"}] * 2)
        b = case("b", root_cause="timeout")
        self.assertEqual(self.select(a, b)["b"]["reason"], "two-attempt cap")

    def test_shared_cause_cooldown_and_material_change(self):
        a = case("a", root_cause="timeout", attempts=[{"at": NOW.isoformat()}])
        b = case("b", root_cause="timeout")
        self.assertFalse(self.select(a, b)["b"]["eligible"])
        self.assertTrue(self.select(a, b, changes=["timeout"])["b"]["eligible"])

    def test_cooldown_boundary(self):
        item = case("a", last_checked_at=(NOW - timedelta(hours=24)).isoformat())
        self.assertTrue(self.select(item)["a"]["eligible"])
        item["last_checked_at"] = (NOW - timedelta(hours=23)).isoformat()
        self.assertFalse(self.select(item)["a"]["eligible"])

    def test_previous_run_rotates_even_with_fix(self):
        a = case("a", attempts=[{"run_id": "previous", "at": NOW.isoformat()}])
        self.assertEqual(self.select(a, case("b"), changes=["a"])["a"]["reason"], "rotate away from previous run")

    def test_resolved_cases_do_not_force_rotation(self):
        a = case("a", attempts=[{"run_id": "previous", "at": (NOW - timedelta(days=2)).isoformat()}])
        self.assertTrue(self.select(a, case("b", status="resolved_verified_device_playback"))["a"]["eligible"])

    def test_gumball_requires_specific_change(self):
        key = "the-rival-the-amazing-world-of-gumball-s06e01"
        self.assertFalse(self.select(case(key), changes=["unrelated-fix"])[key]["eligible"])
        self.assertTrue(self.select(case(key), changes=[key])[key]["eligible"])


if __name__ == "__main__":
    unittest.main()
