"""Audit local project usage without exporting prompts, tool results or credentials.

Reads the selected rollout and its recorded descendants. SQLite discovery is
read-only. Per-response usage is deduplicated by response_id, including usage
embedded in compaction events. Cumulative snapshots are diagnostics, never sums.
"""
from __future__ import annotations

import argparse
import collections
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import sqlite3

FIELDS = ("input_tokens", "cached_input_tokens", "cache_write_input_tokens",
          "output_tokens", "reasoning_output_tokens", "total_tokens")
PRICE_SOURCE = "https://developers.openai.com/api/docs/models/gpt-6.1-sol"


def instant(value):
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def clean_path(value):
    return Path(value.removeprefix("\\\\?\\"))


def norm_path(value):
    return str(clean_path(value)).replace("\\", "/").rstrip("/").lower()


def first_meta(path):
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            row = json.loads(line)
            if row.get("type") == "session_meta":
                return row["payload"]
    raise ValueError(f"No session metadata: {path.name}")


def discover(root, state_db):
    meta = first_meta(root)
    root_id = meta.get("id", meta.get("session_id"))
    sources = [(root, "primary", root_id)]
    if not state_db:
        return sources
    connection = sqlite3.connect(state_db.resolve().as_uri() + "?mode=ro", uri=True)
    try:
        found = {root_id}
        frontier = [root_id]
        while frontier:
            parent = frontier.pop()
            for (child,) in connection.execute(
                    "SELECT child_thread_id FROM thread_spawn_edges WHERE parent_thread_id=?", (parent,)):
                if child in found:
                    continue
                found.add(child)
                frontier.append(child)
                row = connection.execute("SELECT rollout_path FROM threads WHERE id=?", (child,)).fetchone()
                if row:
                    sources.append((clean_path(row[0]), "helper", child))
        # Reviews have no reliable spawn edge. Keep same-workspace reviews separate;
        # their billing attribution and published model price are not established.
        for tid, path, cwd, model in connection.execute(
                "SELECT id,rollout_path,cwd,model FROM threads WHERE model=?", ("codex-auto-review",)):
            if norm_path(cwd) == norm_path(meta["cwd"]) and tid not in found:
                sources.append((clean_path(path), "internal_review", tid))
    finally:
        connection.close()
    return sources


def union_seconds(intervals):
    merged = []
    for start, end in sorted(intervals):
        if end < start:
            raise ValueError("Negative task interval")
        if merged and start <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(end, merged[-1][1]))
        else:
            merged.append((start, end))
    return sum((end-start).total_seconds() for start, end in merged)


def estimate(usage, model, multiplier=1):
    if model != "gpt-6.1-sol":
        return None
    long = usage["input_tokens"] > 272000
    ordinary = usage["input_tokens"] - usage["cached_input_tokens"] - usage["cache_write_input_tokens"]
    if ordinary < 0:
        raise ValueError("Cached and written tokens exceed input")
    rates = (4, .2, 5, 15) if long else (2, .1, 2.5, 10)
    return multiplier * sum(n*r for n, r in zip(
        (ordinary, usage["cached_input_tokens"], usage["cache_write_input_tokens"], usage["output_tokens"]), rates)) / 1e6


def audit(sources, cutoff):
    seen = set()
    sessions, calls, warnings = [], [], []
    for index, (path, role, thread_id) in enumerate(sources):
        if not path.exists():
            warnings.append(f"Missing {role} rollout; totals incomplete")
            continue
        meta = first_meta(path)
        if meta.get("id", meta.get("session_id")) != thread_id:
            raise ValueError("Rollout identity mismatch")
        created = instant(meta["timestamp"])
        if created > cutoff:
            continue
        model, tier = "unknown", "unrecorded"
        starts, intervals, finished = {}, [], set()
        totals = collections.Counter({field: 0 for field in FIELDS})
        latest_record, latest_counter = {}, {}
        compaction_ids, own_ids = set(), set()
        counts = collections.Counter()
        source_hash = hashlib.sha256()
        with path.open("rb") as stream:
            for raw in stream:
                try:
                    row = json.loads(raw)
                except json.JSONDecodeError:
                    warnings.append(f"Incomplete line in {role} rollout skipped")
                    continue
                stamp = instant(row["timestamp"])
                if stamp > cutoff:
                    continue
                source_hash.update(raw)
                payload = row.get("payload", {})
                kind = row.get("type")
                if kind == "turn_context":
                    model = payload.get("model", model)
                if kind == "event_msg" and payload.get("type") == "thread_settings_applied":
                    if payload.get("thread_id", thread_id) == thread_id:
                        settings = payload.get("thread_settings", {})
                        model = settings.get("model", model)
                        tier = settings.get("service_tier", tier)
                record = payload if kind == "token_usage_record" else None
                if kind == "compacted":
                    record = payload.get("latest_token_usage_record")
                    counts["compactions"] += 1
                    if record:
                        compaction_ids.add(record.get("response_id"))
                if record and record.get("thread_id") == thread_id:
                    response_id = record.get("response_id")
                    if not response_id:
                        raise ValueError("Cannot safely deduplicate a response without response_id")
                    own_ids.add(response_id)
                    latest_record = record.get("thread_token_usage", latest_record)
                    if response_id not in seen:
                        seen.add(response_id)
                        usage = {field: record["usage"].get(field, 0) for field in FIELDS}
                        if usage["input_tokens"] + usage["output_tokens"] != usage["total_tokens"]:
                            raise ValueError("Token total mismatch")
                        if usage["reasoning_output_tokens"] > usage["output_tokens"]:
                            raise ValueError("Reasoning must be a subset of output")
                        estimate(usage, model)
                        totals.update(usage)
                        counts["responses"] += 1
                        calls.append(dict(timestamp=stamp.isoformat(), session=index, role=role,
                                          model=model, configured_tier=tier, **usage))
                if kind == "response_item" and payload.get("type") in ("custom_tool_call", "function_call"):
                    # Forked history can contain copied calls. Count only calls after
                    # an attributable task start, rather than presenting a billing count.
                    if starts:
                        counts["tool_invocations"] += 1
                if kind != "event_msg":
                    continue
                event = payload.get("type")
                if event == "token_count":
                    latest_counter = (payload.get("info") or {}).get("total_token_usage", {})
                turn = payload.get("turn_id")
                if event == "task_started":
                    # Fork files include a copied parent task_started/complete pair.
                    # Its payload started_at predates this child's creation.
                    if payload.get("started_at", stamp.timestamp()) < int(created.timestamp()):
                        continue
                    starts.setdefault(turn, stamp)
                    counts["started_turns"] += 1
                if event in ("task_complete", "turn_aborted") and turn in starts:
                    if turn not in finished:
                        intervals.append((starts[turn], stamp))
                        finished.add(turn)
                        counts["completed_turns" if event == "task_complete" else "aborted_turns"] += 1
        closed_seconds = union_seconds(intervals)
        open_count = 0
        for turn, start in starts.items():
            if turn not in finished:
                intervals.append((start, cutoff))
                open_count += 1
        if latest_record and totals["total_tokens"] != latest_record["total_tokens"]:
            warnings.append(f"{role} cumulative usage differs from response records; inspect coverage")
        sessions.append(dict(label="primary" if role == "primary" else f"{role}-{index}",
            role=role, created_utc=created.isoformat(), source_prefix_sha256=source_hash.hexdigest(),
            usage=dict(totals), latest_record_cumulative=latest_record,
            latest_ui_counter=latest_counter, counts=dict(counts), open_turns=open_count,
            completed_turn_elapsed_seconds=closed_seconds,
            observed_task_elapsed_seconds=union_seconds(intervals),
            task_intervals=[(a.isoformat(), b.isoformat()) for a,b in intervals],
            compaction_responses=len(compaction_ids & own_ids)))
    group = {}
    for role in ("primary", "helper", "internal_review"):
        matching = [s for s in sessions if s["role"] == role]
        usage = {f: sum(s["usage"][f] for s in matching) for f in FIELDS}
        usage["ordinary_input_tokens"] = usage["input_tokens"]-usage["cached_input_tokens"]-usage["cache_write_input_tokens"]
        group[role] = dict(sessions=len(matching), usage=usage)
    development = [s for s in sessions if s["role"] != "internal_review"]
    usage = {f: sum(s["usage"][f] for s in development) for f in FIELDS}
    usage["ordinary_input_tokens"] = usage["input_tokens"]-usage["cached_input_tokens"]-usage["cache_write_input_tokens"]
    selected_calls = [c for c in calls if c["role"] != "internal_review"]
    prices = [estimate(c, c["model"]) for c in selected_calls]
    if any(value is None for value in prices):
        warnings.append("Unpriced model in development calls")
    all_intervals = [(instant(a), instant(b)) for s in development for a,b in s["task_intervals"]]
    start = min(instant(s["created_utc"]) for s in development)
    result = dict(schema=1, cutoff_utc=cutoff.isoformat(), project_started_utc=start.isoformat(),
        elapsed_calendar_hours=(cutoff-start).total_seconds()/3600,
        development_usage=usage, groups=group,
        observed_development_task_hours=union_seconds(all_intervals)/3600,
        task_hours_note="Union of task start/end intervals, includes tools/waits and cutoff-clipped open turns; excludes gaps between turns. Not model GPU time or human labour.",
        estimated_development_token_cost_usd={
            "standard": round(sum(p for p in prices if p is not None), 6),
            "fast_scenario": round(2*sum(p for p in prices if p is not None), 6),
            "ultrafast_scenario": round(6*sum(p for p in prices if p is not None), 6)},
        pricing=dict(source=PRICE_SOURCE, checked_date="2026-10-20", model="gpt-6.1-sol",
            short_rates_per_million=dict(input=2, cached=.1, cache_write=2.5, output=10),
            long_rates_per_million=dict(input=4, cached=.2, cache_write=5, output=15),
            long_if_input_exceeds=272000,
            assumptions="Current published API rates applied per request. Configured tier is not verified served tier. No taxes, discounts, tool charges, internal reviews or invoice reconciliation. Not actual billed spend."),
        model_response_counts=dict(collections.Counter(c["model"] for c in selected_calls)),
        configured_tier_response_counts=dict(collections.Counter(c["configured_tier"] for c in selected_calls)),
        long_context_response_count=sum(c["input_tokens"]>272000 for c in selected_calls),
        warnings=warnings, sessions=sessions)
    return result, calls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-rollout", required=True, type=Path)
    parser.add_argument("--state-db", type=Path, help="Optional Codex state SQLite database; opened read-only")
    parser.add_argument("--cutoff", required=True, help="Fixed UTC ISO timestamp makes a report reproducible")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    cutoff = instant(args.cutoff)
    if cutoff.tzinfo is None:
        parser.error("--cutoff must include a timezone")
    result, calls = audit(discover(args.root_rollout, args.state_db), cutoff)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    args.output.with_suffix(".md").write_text(markdown_report(result), encoding="utf-8")
    with args.output.with_suffix(".csv").open("w", newline="", encoding="utf-8") as stream:
        fields = ["timestamp", "session", "role", "model", "configured_tier", *FIELDS]
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(calls)
    print(json.dumps({k: result[k] for k in ("cutoff_utc", "development_usage", "observed_development_task_hours", "elapsed_calendar_hours", "estimated_development_token_cost_usd", "warnings")}, indent=2))


def markdown_report(report):
    u = report["development_usage"]
    primary = report["groups"]["primary"]["usage"]
    helpers = report["groups"]["helper"]["usage"]
    review = report["groups"]["internal_review"]["usage"]
    cost = report["estimated_development_token_cost_usd"]
    root = next(s for s in report["sessions"] if s["role"] == "primary")
    cache_percent = 100*u["cached_input_tokens"]/u["input_tokens"]
    difference = root["usage"]["total_tokens"]-root["latest_ui_counter"].get("total_tokens", 0)
    return f'''# AI development metrics: reproducible local snapshot

Cutoff: **{report["cutoff_utc"]}**. This excludes later work, the unfinished
response at the cutoff, and activity outside the available local project records.
The user identified the billing method as **OpenAI API with token billing**.

## Recorded development usage

Scope: the original project session and its five recorded helper descendants.
These counts come from per-response usage telemetry, not a tokenizer estimate
of visible chat text. All recorded development calls identify `gpt-6.1-sol`.

| Metric | Recorded value |
| --- | ---: |
| Input tokens, including cached input | {u["input_tokens"]:,} |
| Cached input, already included above | {u["cached_input_tokens"]:,} |
| Ordinary uncached input | {u["ordinary_input_tokens"]:,} |
| Explicit cache-write input | {u["cache_write_input_tokens"]:,} |
| Output tokens, including reasoning output | {u["output_tokens"]:,} |
| Reasoning output, already included above | {u["reasoning_output_tokens"]:,} |
| **Input + output total** | **{u["total_tokens"]:,}** |
| Cached share of input | {cache_percent:.2f}% |
| Primary session input + output | {primary["total_tokens"]:,} |
| Five helpers input + output | {helpers["total_tokens"]:,} |
| Unique development response records | {sum(report["model_response_counts"].values()):,} |
| Requests over 272,000 input tokens | {report["long_context_response_count"]:,} |

Input includes repeated context, instructions, tool results and history supplied
on multiple model calls. It is **not unique project text or lines of code**.
Reasoning output is not necessarily visible text. Subsets are never added twice.

## Time

The recorded Codex project session was created at **{report["project_started_utc"]}**.

| Time measure | Value at cutoff |
| --- | ---: |
| Calendar span from session creation | {report["elapsed_calendar_hours"]:.3f} hours |
| Union of observed development task intervals | {report["observed_development_task_hours"]:.3f} hours |
| Primary tasks started | {root["counts"].get("started_turns",0)} |
| Primary tasks completed / aborted / still open | {root["counts"].get("completed_turns",0)} / {root["counts"].get("aborted_turns",0)} / {root["open_turns"]} |

Task intervals include model/tool execution and waits within a turn. Gaps between
turns are excluded. Parallel helper intervals are merged with primary intervals,
not added twice; an open interval is clipped at the cutoff. This is **not model
GPU time, developer labour, or a stopwatch measurement of human work**. User
testing, breaks and other activity cannot be recovered accurately from these
records. Copied parent events in helper histories are excluded by attribution
and creation-time checks. Event timestamps, not copied cumulative durations,
define the intervals.

## API token-cost estimate, not an invoice

Configured service tier is `default` in the development records. Actual served
tier, account discounts, regional premiums, tool billing and historical invoice
line items are unavailable in this audit. Apply the following as a transparent
**current-rate estimate**, not the actual amount paid.

| Scenario | Estimated development token cost, USD |
| --- | ---: |
| Standard rates | **${cost["standard"]:.2f}** |
| Fast rates, if applicable | ${cost["fast_scenario"]:.2f} |
| Ultrafast rates, if applicable | ${cost["ultrafast_scenario"]:.2f} |

Rates checked **20 October 2026**: Standard short-context input/cached/cache-write/
output is $2 / $0.10 / $2.50 / $10 per million tokens. For a request exceeding
272,000 input tokens, the entire request uses $4 / $0.20 / $5 / $15. Fast and
Ultrafast scenarios use 2× and 6× respectively.
[Official model pricing](https://developers.openai.com/api/docs/models/gpt-6.1-sol).

Each request is priced separately:

```
ordinary_input = input - cached_input - cache_write_input
estimated_USD = (ordinary_input * input_rate
               + cached_input * cached_rate
               + cache_write_input * cache_write_rate
               + output * output_rate) / 1,000,000
```

An actual project cost requires reconciling the selected project/time range in
the API usage/billing export with these calls, including tool charges and any
applicable credits, discounts, taxes or other adjustments. This audit does not
read API keys, sign in to billing, or infer spend from account balance changes.

## Counter reconciliation and review telemetry

Never sum repeated `total_token_usage` snapshots. The audit deduplicates
`token_usage_record.usage` by response ID, including compaction records embedded
inside `compacted` events. Forked parent records are excluded by thread identity.

The primary per-response total equals its latest detailed cumulative record.
Its UI `token_count` stream is lower by **{difference:,} tokens** in this
snapshot. Those tokens correspond exactly to the twelve recorded compaction
responses. Reporting only the UI counter would omit them; adding compaction
records a second time would overcount them.

Same-workspace automatic review telemetry contains **{review["total_tokens"]:,}
additional recorded input/output tokens** under `codex-auto-review`. It is
separate from the development subtotal and cost estimate: its published rate
and billing attribution are unknown, and some review cumulative counters exceed
the available per-response records. These supplemental counts are incomplete
for that subsystem. No missing usage is fabricated or assigned a guessed price.

The JSON retains per-session aggregates, anonymous role labels, cutoff-bounded
source hashes, diagnostic counters and coverage warnings. Its CSV contains
timestamps/model/tier/token columns per response. Neither export contains prompts,
raw tool results, user/account identifiers, API keys, or ROM/BIOS data.

## Reproduce and cite in a blog

Run `tools/project_metrics.py --help` for parameters. Example from the workspace:

```powershell
python GBAPrototype/tools/project_metrics.py `
  --root-rollout <original-project-rollout.jsonl> `
  --state-db <Codex-state_5.sqlite> `
  --cutoff {report["cutoff_utc"]} `
  --output GBAPrototype/output/project-metrics-2026-10-20.json
```

The database is opened read-only and used to discover recorded descendants;
unrelated chats are excluded. Keep raw rollout files private. Outputs are
aggregate JSON, per-response CSV and this Markdown summary. Re-running with a
later cutoff intentionally changes totals; archive the cutoff used by the blog.
Four accounting regression tests cover cache/reasoning subsets, long-context
pricing, overlapping intervals, copied history, response deduplication and cutoff.

Suggested wording:

> As of {report["cutoff_utc"]}, the local development telemetry recorded
> {u["total_tokens"]/1e6:.2f} million input/output tokens, including
> {u["cached_input_tokens"]/1e6:.2f} million cached input tokens, across the main
> agent and five helper sessions. Current Standard API token rates estimate
> US${cost["standard"]:.2f}; actual billed spend has not been reconciled.
> Recorded task intervals cover {report["observed_development_task_hours"]:.2f}
> hours within {report["elapsed_calendar_hours"]:.2f} calendar hours.
'''


if __name__ == "__main__":
    main()
