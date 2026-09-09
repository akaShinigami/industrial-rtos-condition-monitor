#!/usr/bin/env python3
"""Render production telemetry CSV as self-contained HTML with inline SVG.

Usage: python3 tools/render_telemetry.py build/visualization/scenario.csv \
           build/visualization/scenario.html
Only the Python standard library is used. No controller decisions are simulated.
"""

import argparse
import csv
from html import escape
from pathlib import Path


THRESHOLDS = (
    "temperature_warning_mdeg_c", "temperature_fault_mdeg_c",
    "vibration_warning_um_s", "vibration_fault_um_s",
    "current_warning_ma", "current_fault_ma",
)
NUMERIC = ("time_ms", "temperature_mdeg_c", "vibration_um_s", "current_ma",
           "fault_mask", "injection_mask", "sensor_valid", "stale_services",
           "watchdog_fault") + THRESHOLDS
COLUMNS = set(NUMERIC) | {"actuator", "health", "event"}
EVENTS = {
    "safe_startup": "Safe startup",
    "start_requested": "Start requested",
    "running": "Nominal running",
    "overtemperature_injected_and_trip": "Overtemperature injected / safety trip",
    "injection_cleared_health_recovered": "Injection cleared / health recovered",
    "faulted_latch_retained": "FAULTED latch retained",
    "explicit_safe_reset": "Explicit safe reset",
}
COLORS = {"STOPPED": "#64748b", "STARTING": "#a78bfa", "RUNNING": "#0f766e",
          "STOPPING": "#a78bfa", "FAULTED": "#be123c", "HEALTHY": "#0f766e",
          "WARNING": "#b45309", "FAULT": "#be123c", "EMERGENCY_STOP": "#881337"}


def read_samples(path):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if (reader.fieldnames is None or not COLUMNS.issubset(reader.fieldnames)
                or len(reader.fieldnames) != len(set(reader.fieldnames))):
            raise ValueError("CSV requires one header with all telemetry columns")
        samples = []
        for line, row in enumerate(reader, 2):
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"line {line}: inconsistent column count")
            try:
                for column in NUMERIC:
                    # Only masks use hexadecimal; decimal columns allow leading zeroes.
                    base = 16 if column in {"fault_mask", "injection_mask", "stale_services"} and row[column].startswith("0x") else 10
                    row[column] = int(row[column], base)
            except ValueError as exc:
                raise ValueError(f"line {line}: invalid integer") from exc
            if any(row[key] < 0 for key in NUMERIC if key != "temperature_mdeg_c"):
                raise ValueError(f"line {line}: negative time, mask, or unsigned measurement")
            if any(row[key] not in (0, 1) for key in ("sensor_valid", "watchdog_fault")):
                raise ValueError(f"line {line}: status must be 0 or 1")
            if row["actuator"] not in {"STOPPED", "STARTING", "RUNNING", "STOPPING", "FAULTED"} or row["health"] not in {"HEALTHY", "WARNING", "FAULT", "EMERGENCY_STOP"}:
                raise ValueError(f"line {line}: unknown actuator/health state")
            if samples and row["time_ms"] <= samples[-1]["time_ms"]:
                raise ValueError("sample timestamps must be strictly increasing")
            if samples and any(row[key] != samples[0][key] for key in THRESHOLDS):
                raise ValueError("thresholds must stay constant within a trace")
            if any(row[THRESHOLDS[i]] >= row[THRESHOLDS[i + 1]] for i in (0, 2, 4)):
                raise ValueError("warning thresholds must be below fault thresholds")
            samples.append(row)
    if len(samples) < 2:
        raise ValueError("at least two samples are required")
    return samples


def event_name(row):
    return EVENTS.get(row["event"], row["event"].replace("_", " "))


def x_position(samples, time_ms):
    return 76 + 820 * (time_ms - samples[0]["time_ms"]) / (samples[-1]["time_ms"] - samples[0]["time_ms"])


def spans(samples, key):
    start = samples[0]["time_ms"]
    value = samples[0][key]
    for row in samples[1:]:
        if row[key] != value:
            yield start, row["time_ms"], value
            start, value = row["time_ms"], row[key]
    yield start, samples[-1]["time_ms"], value


def chart(samples, column, title, unit, warning_key, fault_key):
    values = [row[column] / 1000 for row in samples]
    warning = samples[0][warning_key] / 1000
    fault = samples[0][fault_key] / 1000
    low = min(0, min(values))
    high = max(max(values), fault) * 1.12
    y = lambda value: 172 - 132 * (value - low) / (high - low)
    parts = [f'<section class="panel"><h2>{escape(title)} <span>{escape(unit)}</span></h2>',
             f'<svg viewBox="0 0 1100 220" role="img" aria-label="{escape(title)} versus time">',
             f'<title>{escape(title)} versus time, with warning and fault thresholds</title>']
    for start, end, health in spans(samples, "health"):
        if health in {"FAULT", "EMERGENCY_STOP"}:
            left, right = x_position(samples, start), x_position(samples, end)
            parts.append(f'<rect x="{left:.2f}" y="40" width="{right-left:.2f}" height="132" fill="#fff1f2"/>')
    for index in range(5):
        value = low + (high - low) * index / 4
        ordinate = y(value)
        parts.append(f'<path d="M76 {ordinate:.2f} H896" class="grid"/><text x="62" y="{ordinate+4:.2f}" text-anchor="end">{value:.1f}</text>')
    for value, label, color in ((warning, "Warning", "#b45309"), (fault, "Fault", "#be123c")):
        ordinate = y(value)
        parts.append(f'<path d="M76 {ordinate:.2f} H896" stroke="{color}" stroke-dasharray="6 5" fill="none"/><text x="912" y="{ordinate+4:.2f}" fill="{color}">{label} {value:g} {unit}</text>')
    # Step lines: each sample applies until the next observation; no invented ramp.
    path = f'M{x_position(samples, samples[0]["time_ms"]):.2f} {y(values[0]):.2f}'
    for row, value in zip(samples[1:], values[1:]):
        path += f' H{x_position(samples, row["time_ms"]):.2f} V{y(value):.2f}'
    parts.append(f'<path class="trace" d="{path}"/>')
    for row, value in zip(samples, values):
        parts.append(f'<circle cx="{x_position(samples, row["time_ms"]):.2f}" cy="{y(value):.2f}" r="2.4" fill="#0f766e"><title>{row["time_ms"]} ms: {value:g} {unit}</title></circle>')
    for row in samples:
        if row["event"] in {"overtemperature_injected_and_trip", "injection_cleared_health_recovered", "explicit_safe_reset"}:
            abscissa = x_position(samples, row["time_ms"])
            parts.append(f'<path d="M{abscissa:.2f} 32 V178" class="event-line"><title>{escape(event_name(row))} at {row["time_ms"]} ms</title></path>')
    for index in range(6):
        time = samples[0]["time_ms"] + (samples[-1]["time_ms"] - samples[0]["time_ms"]) * index / 5
        parts.append(f'<text x="{x_position(samples,time):.2f}" y="196" text-anchor="middle">{time/1000:.2f}</text>')
    parts.append('<text x="912" y="196">Time (s)</text></svg></section>')
    return "".join(parts)


def timeline(samples):
    parts = ['<section class="panel"><h2>State timeline <span>Observed after each cycle</span></h2>',
             '<svg viewBox="0 0 1100 190" role="img" aria-label="Actuator health and fault timeline">']
    for index, (key, label) in enumerate((("actuator", "Actuator"), ("health", "Health"), ("fault_mask", "Fault bits"))):
        top = 24 + 48 * index
        parts.append(f'<text x="76" y="{top-6}">{label}</text>')
        for start, end, value in spans(samples, key):
            left, right = x_position(samples, start), x_position(samples, end)
            color = ("#be123c" if value else "#64748b") if key == "fault_mask" else COLORS[value]
            text = f"0x{value:02x}" if key == "fault_mask" else value
            parts.append(f'<rect x="{left:.2f}" y="{top}" width="{right-left:.2f}" height="26" fill="{color}"><title>{escape(text)}: {start}–{end} ms</title></rect>')
            if right-left > 70:
                parts.append(f'<text x="{(left+right)/2:.2f}" y="{top+18}" text-anchor="middle" fill="white">{escape(text)}</text>')
    parts.append(f'<text x="76" y="183">{samples[0]["time_ms"]/1000:g} s</text><text x="896" y="183" text-anchor="end">{samples[-1]["time_ms"]/1000:g} s</text></svg>')
    parts.append('<p class="note">Red plot shading marks FAULT health. Notice where health recovers while the actuator stays FAULTED. Fault bit 0x01 is overtemperature.</p></section>')
    return "".join(parts)


def render(samples):
    events = [row for row in samples if row["event"]]
    event_rows = []
    for row in events:
        event_rows.append(f'<tr><td>{row["time_ms"]/1000:.2f} s</td><td>{escape(event_name(row))}</td><td>{row["actuator"]}</td><td>{row["health"]}</td><td>0x{row["fault_mask"]:02x}</td><td>0x{row["injection_mask"]:02x}</td><td>{"valid" if row["sensor_valid"] else "INVALID"}</td><td>{"FAULT" if row["watchdog_fault"] else "healthy"} / 0x{row["stale_services"]:02x}</td></tr>')
    plots = chart(samples, "temperature_mdeg_c", "Temperature", "°C", THRESHOLDS[0], THRESHOLDS[1])
    plots += chart(samples, "vibration_um_s", "Vibration", "mm/s", THRESHOLDS[2], THRESHOLDS[3])
    plots += chart(samples, "current_ma", "Current", "A", THRESHOLDS[4], THRESHOLDS[5])
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Condition monitor · simulation telemetry</title>
<style>
*{{box-sizing:border-box}}body{{margin:0;background:#f1f5f9;color:#172b3a;font:15px/1.5 system-ui,sans-serif}}
main{{max-width:1240px;margin:auto;padding:40px 28px}}header{{border-top:5px solid #0f766e;padding:24px 0}}
.eyebrow{{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:#0f766e;font-weight:700}}
h1{{font-size:clamp(26px,4vw,42px);margin:8px 0;letter-spacing:-.04em}}header p{{max-width:850px;color:#475569}}
.stats{{display:flex;gap:12px;flex-wrap:wrap;margin:20px 0}}.stat{{background:white;border:1px solid #dbe4eb;border-radius:8px;padding:12px 20px;min-width:170px}}.stat strong{{display:block;font-size:23px}}.stat span{{color:#64748b;font-size:12px}}
.panel{{background:white;border:1px solid #dbe4eb;border-radius:10px;margin:16px 0;padding:18px 20px}}h2{{font-size:17px;margin:0 0 6px}}h2 span{{font-weight:400;color:#64748b;font-size:13px;margin-left:10px}}
svg{{width:100%;height:auto;display:block}}svg text{{font:12px system-ui,sans-serif}}.grid{{stroke:#e2e8f0;fill:none}}.trace{{stroke:#0f766e;stroke-width:2.5;fill:none}}.event-line{{stroke:#94a3b8;stroke-dasharray:3 4;fill:none}}
.note,footer{{font-size:13px;color:#64748b}}.table-wrap{{overflow-x:auto}}table{{width:100%;border-collapse:collapse;font-size:12px;text-align:left}}th,td{{padding:11px 9px;border-bottom:1px solid #e2e8f0}}th{{color:#475569;background:#f8fafc}}td:first-child{{white-space:nowrap;font-variant-numeric:tabular-nums}}
@media(max-width:600px){{main{{padding:20px 10px}}.panel{{padding:12px 6px}}svg{{min-width:650px}}.panel{{overflow-x:auto}}}}
@media print{{body{{background:white}}main{{padding:0}}.panel{{break-inside:avoid}}}}
</style></head><body><main>
<header><div class="eyebrow">Zephyr RTOS / deterministic simulation</div><h1>Observe the trip. Verify the recovery.</h1>
<p>Production C telemetry from a motor/pump simulation. Follow the measurements alongside the controller state: clearing an abnormal condition can restore health while the actuator remains latched until an explicit safe reset.</p>
<div class="stats"><div class="stat"><strong>{len(samples)}</strong><span>production samples</span></div><div class="stat"><strong>{(samples[-1]['time_ms']-samples[0]['time_ms'])/1000:g} s</strong><span>logical simulation duration</span></div><div class="stat"><strong>{max(row['temperature_mdeg_c'] for row in samples)/1000:g} °C</strong><span>peak effective temperature</span></div></div></header>
{timeline(samples)}{plots}
<section class="panel"><h2>Recorded events</h2><div class="table-wrap"><table><thead><tr><th>Time</th><th>Event</th><th>Actuator</th><th>Health</th><th>Faults</th><th>Injections</th><th>Sensor</th><th>Watchdog / stale mask</th></tr></thead><tbody>{''.join(event_rows)}</tbody></table></div></section>
<footer>Offline, self-contained SVG · Thresholds come from the CSV emitted by the C application. Step lines hold each observed value until the next sample; dots mark observations. Measurements on the trip cycle precede the resulting actuator transition. Logical time is independent of host execution time. Simulation thresholds are not universal industrial limits.</footer>
</main></body></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("html_path", type=Path)
    args = parser.parse_args()
    try:
        document = render(read_samples(args.csv_path))
        args.html_path.parent.mkdir(parents=True, exist_ok=True)
        args.html_path.write_text(document, encoding="utf-8")
    except (OSError, ValueError, csv.Error) as exc:
        parser.exit(1, f"render_telemetry: {exc}\n")
    print(f"Wrote {args.html_path} ({args.html_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
