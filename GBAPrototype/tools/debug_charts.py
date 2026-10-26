"""Create an offline browser report from decoded GBA frame telemetry."""
from pathlib import Path
import json

def write_report(path: Path, header: dict, frames: list[dict], profiles: list[dict], summary: dict) -> Path:
    cumulative = 0.0
    plotted = []
    for frame in frames:
        cumulative += frame["wall_us"] / 1_000_000
        plotted.append(dict(frame, t=round(cumulative, 6)))
    origin_ms = frames[0]["uptime_ms"] - frames[0]["wall_us"] / 1000 if frames else 0
    frame_times = {frame["frame"]: frame["t"] for frame in plotted}
    cpu = [dict(profile, t=frame_times.get(profile["frame"], round((profile["uptime_ms"] - origin_ms) / 1000, 6))) for profile in profiles]
    payload = dict(header=header, frames=plotted, profiles=cpu, summary=summary)
    data = json.dumps(payload, separators=(",", ":"), ensure_ascii=True).replace("<", "\\u003c")
    template = Path(__file__).with_name("debug_chart.html").read_text(encoding="utf-8")
    path.write_text(template.replace("__GBA_LOG_DATA__", data), encoding="utf-8")
    return path
