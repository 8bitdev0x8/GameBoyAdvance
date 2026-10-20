"""Accounting regressions for the blog's reproducible usage report."""
import datetime as dt
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("project_metrics", Path(__file__).parents[1]/"tools/project_metrics.py")
metrics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metrics)


class MetricsTest(unittest.TestCase):
    def test_cache_and_reasoning_are_subsets(self):
        usage = dict(input_tokens=100000, cached_input_tokens=80000,
                     cache_write_input_tokens=10000, output_tokens=1000,
                     reasoning_output_tokens=800, total_tokens=101000)
        self.assertAlmostEqual(metrics.estimate(usage, "gpt-6.1-sol"), .063)

    def test_long_context_applies_to_entire_request(self):
        usage = dict(input_tokens=272001, cached_input_tokens=272000,
                     cache_write_input_tokens=0, output_tokens=1000)
        self.assertAlmostEqual(metrics.estimate(usage, "gpt-6.1-sol"), .069404)
        self.assertIsNone(metrics.estimate(usage, "codex-auto-review"))

    def test_union_does_not_count_parallel_time_twice(self):
        start = metrics.instant("2026-10-20T00:00:00Z")
        self.assertEqual(metrics.union_seconds([(start, start+dt.timedelta(seconds=10)),
                         (start+dt.timedelta(seconds=5), start+dt.timedelta(seconds=20))]), 20)

    def test_dedup_cutoff_and_inherited_parent_records(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/"rollout.jsonl"
            usage = dict(input_tokens=100, cached_input_tokens=80, cache_write_input_tokens=0,
                         output_tokens=10, reasoning_output_tokens=5, total_tokens=110)
            record = dict(thread_id="child", response_id="resp_1", usage=usage,
                          thread_token_usage=usage)
            events = [
                ("session_meta", dict(id="child", timestamp="2026-10-20T00:00:00Z")),
                ("turn_context", dict(model="gpt-6.1-sol")),
                ("event_msg", dict(type="task_started", turn_id="copied", started_at=0)),
                ("event_msg", dict(type="task_started", turn_id="own", started_at=1792454400)),
                ("token_usage_record", dict(record, thread_id="parent", response_id="copied")),
                ("token_usage_record", record),
                ("compacted", dict(latest_token_usage_record=record)),
                ("event_msg", dict(type="token_count", info=dict(total_token_usage=usage))),
            ]
            rows = [dict(timestamp="2026-10-20T00:00:00Z", type=kind, payload=payload)
                    for kind, payload in events]
            rows.append(dict(timestamp="2026-10-20T00:01:00Z", type="token_usage_record",
                             payload=dict(record, response_id="after_cutoff")))
            path.write_text("\n".join(map(json.dumps, rows))+"\n", encoding="utf-8")
            report, calls = metrics.audit([(path, "primary", "child")],
                                          metrics.instant("2026-10-20T00:00:30Z"))
            self.assertEqual(report["development_usage"]["total_tokens"], 110)
            self.assertEqual(len(calls), 1)
            self.assertEqual(report["sessions"][0]["open_turns"], 1)
            self.assertAlmostEqual(report["observed_development_task_hours"], 30/3600)
            self.assertFalse(report["warnings"])


if __name__ == "__main__":
    unittest.main()
