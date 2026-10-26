# AI development metrics: reproducible local snapshot

Cutoff: **2026-10-20T00:30:00+00:00**. This excludes later work, the unfinished
response at the cutoff, and activity outside the available local project records.
The user identified the billing method as **OpenAI API with token billing**.

## Recorded development usage

Scope: the original project session and its five recorded helper descendants.
These counts come from per-response usage telemetry, not a tokenizer estimate
of visible chat text. All recorded development calls identify `gpt-6.1-sol`.

| Metric | Recorded value |
| --- | ---: |
| Input tokens, including cached input | 276,442,950 |
| Cached input, already included above | 270,892,544 |
| Ordinary uncached input | 5,550,406 |
| Explicit cache-write input | 0 |
| Output tokens, including reasoning output | 1,391,730 |
| Reasoning output, already included above | 567,650 |
| **Input + output total** | **277,834,680** |
| Cached share of input | 97.99% |
| Primary session input + output | 268,996,473 |
| Five helpers input + output | 8,838,207 |
| Unique development response records | 1,535 |
| Requests over 272,000 input tokens | 321 |

Input includes repeated context, instructions, tool results and history supplied
on multiple model calls. It is **not unique project text or lines of code**.
Reasoning output is not necessarily visible text. Subsets are never added twice.

## Time

The recorded Codex project session was created at **2026-10-18T21:50:23.145000+00:00**.

| Time measure | Value at cutoff |
| --- | ---: |
| Calendar span from session creation | 26.660 hours |
| Union of observed development task intervals | 9.121 hours |
| Primary tasks started | 93 |
| Primary tasks completed / aborted / still open | 91 / 1 / 1 |

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
| Standard rates | **$66.78** |
| Fast rates, if applicable | $133.56 |
| Ultrafast rates, if applicable | $400.69 |

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
Its UI `token_count` stream is lower by **4,116,266 tokens** in this
snapshot. Those tokens correspond exactly to the twelve recorded compaction
responses. Reporting only the UI counter would omit them; adding compaction
records a second time would overcount them.

Same-workspace automatic review telemetry contains **28,384,121
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
  --cutoff 2026-10-20T00:30:00+00:00 `
  --output GBAPrototype/output/project-metrics-2026-10-20.json
```

The database is opened read-only and used to discover recorded descendants;
unrelated chats are excluded. Keep raw rollout files private. Outputs are
aggregate JSON, per-response CSV and this Markdown summary. Re-running with a
later cutoff intentionally changes totals; archive the cutoff used by the blog.
Four accounting regression tests cover cache/reasoning subsets, long-context
pricing, overlapping intervals, copied history, response deduplication and cutoff.

Suggested wording:

> As of 2026-10-20T00:30:00+00:00, the local development telemetry recorded
> 277.83 million input/output tokens, including
> 270.89 million cached input tokens, across the main
> agent and five helper sessions. Current Standard API token rates estimate
> US$66.78; actual billed spend has not been reconciled.
> Recorded task intervals cover 9.12
> hours within 26.66 calendar hours.
