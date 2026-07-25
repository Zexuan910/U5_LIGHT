#!/usr/bin/env python3
"""Replay the firmware WALK heuristic against exported six-axis recordings."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Profile:
    min_interval_ms: int = 300
    max_interval_ms: int = 1800
    gait_reset_ms: int = 2200
    event_timeout_ms: int = 700
    peak_fall_min_ms: int = 80
    default_interval_ms: int = 600
    default_stride_m: float = 0.62
    min_stride_m: float = 0.42
    max_stride_m: float = 0.88
    accel_motion_floor_g: float = 0.06
    accel_peak_floor_g: float = 0.14
    gyro_gate_floor_rad_s: float = 0.10
    gyro_peak_floor_rad_s: float = 0.42
    cadence_reference_spm: float = 100.0
    cadence_stride_gain: float = 0.0015
    peak_stride_gain: float = 0.35
    consistency_fraction: float = 0.55
    consistency_floor_ms: int = 240
    recover_missed_steps: bool = True


@dataclass
class State:
    gravity_g: float = 1.0
    noise_dynamic_g: float = 0.025
    noise_gyro_rad_s: float = 0.04
    filtered_dynamic_g: float = 0.0
    filtered_gyro_rad_s: float = 0.0
    last_sample_ms: int = 0
    last_motion_ms: int = 0
    last_candidate_ms: int = 0
    last_candidate_interval_ms: int = 0
    step_start_ms: int = 0
    peak_ms: int = 0
    peak_dynamic_g: float = 0.0
    peak_gyro_rad_s: float = 0.0
    step_active: bool = False
    gait_confirmed: bool = False
    candidate_intervals: list[int] = field(default_factory=list)
    candidate_strides: list[float] = field(default_factory=list)
    candidate_peaks: list[float] = field(default_factory=list)
    committed_intervals: list[int] = field(default_factory=list)
    committed_peaks: list[float] = field(default_factory=list)
    steps: int = 0
    average_stride_m: float = 0.62
    instant_speed_mps: float = 0.0
    distance_m: float = 0.0


def clamp(value: float, low: float, high: float) -> float:
    return min(max(value, low), high)


def alpha(tau_ms: float, dt_ms: int) -> float:
    dt = float(dt_ms if dt_ms else 1)
    return tau_ms / (tau_ms + dt)


def estimate_stride(profile: Profile, interval_ms: int, peak_g: float) -> float:
    cadence = (
        60000.0 / interval_ms
        if 0 < interval_ms <= profile.max_interval_ms
        else profile.cadence_reference_spm
    )
    stride = profile.default_stride_m
    stride += (cadence - profile.cadence_reference_spm) * profile.cadence_stride_gain
    stride += (peak_g - profile.accel_peak_floor_g) * profile.peak_stride_gain
    return clamp(stride, profile.min_stride_m, profile.max_stride_m)


def clear_chain(state: State) -> None:
    state.gait_confirmed = False
    state.candidate_intervals.clear()
    state.candidate_strides.clear()
    state.candidate_peaks.clear()
    state.last_candidate_ms = 0
    state.last_candidate_interval_ms = 0


def consistent(profile: Profile, interval_ms: int, previous_ms: int) -> bool:
    if interval_ms < profile.min_interval_ms or interval_ms > profile.max_interval_ms:
        return previous_ms == 0
    if previous_ms == 0:
        return True
    tolerance = max(
        int(previous_ms * profile.consistency_fraction),
        profile.consistency_floor_ms,
    )
    return abs(interval_ms - previous_ms) <= tolerance


def commit(state: State, interval_ms: int, stride_m: float, peak_g: float) -> None:
    raw_speed = stride_m * 1000.0 / interval_ms
    if state.steps == 0:
        state.average_stride_m = stride_m
        state.instant_speed_mps = raw_speed
    else:
        state.average_stride_m = state.average_stride_m * 0.75 + stride_m * 0.25
        state.instant_speed_mps = state.instant_speed_mps * 0.65 + raw_speed * 0.35
    state.distance_m += state.average_stride_m
    state.steps += 1
    state.committed_intervals.append(interval_ms)
    state.committed_peaks.append(peak_g)


def start_chain(
    state: State,
    profile: Profile,
    tick_ms: int,
    interval_ms: int,
    stride_m: float,
    peak_g: float,
) -> None:
    clear_chain(state)
    state.candidate_intervals.append(interval_ms)
    state.candidate_strides.append(stride_m)
    state.candidate_peaks.append(peak_g)
    state.last_candidate_ms = tick_ms


def accept_candidate(state: State, profile: Profile, tick_ms: int, peak_g: float) -> None:
    if (
        state.last_candidate_ms == 0
        or tick_ms - state.last_candidate_ms > profile.max_interval_ms
    ):
        start_chain(
            state,
            profile,
            tick_ms,
            profile.default_interval_ms,
            estimate_stride(profile, profile.default_interval_ms, peak_g),
            peak_g,
        )
        return

    interval_ms = tick_ms - state.last_candidate_ms
    stride_m = estimate_stride(profile, interval_ms, peak_g)

    if state.gait_confirmed:
        if profile.min_interval_ms <= interval_ms <= profile.max_interval_ms:
            previous = state.last_candidate_interval_ms
            if (
                profile.recover_missed_steps
                and previous
                and previous * 8 // 5 < interval_ms < previous * 5 // 2
            ):
                recovered = interval_ms // 2
                recovered_stride = estimate_stride(profile, recovered, peak_g)
                commit(state, recovered, recovered_stride, peak_g)
                commit(state, recovered, recovered_stride, peak_g)
                state.last_candidate_interval_ms = recovered
            else:
                commit(state, interval_ms, stride_m, peak_g)
                state.last_candidate_interval_ms = interval_ms
            state.last_candidate_ms = tick_ms
            return

        start_chain(
            state,
            profile,
            tick_ms,
            profile.default_interval_ms,
            estimate_stride(profile, profile.default_interval_ms, peak_g),
            peak_g,
        )
        return

    if not consistent(profile, interval_ms, state.last_candidate_interval_ms):
        start_chain(state, profile, tick_ms, interval_ms, stride_m, peak_g)
        return

    state.last_candidate_ms = tick_ms
    state.last_candidate_interval_ms = interval_ms
    if len(state.candidate_intervals) < 3:
        state.candidate_intervals.append(interval_ms)
        state.candidate_strides.append(stride_m)
        state.candidate_peaks.append(peak_g)

    if len(state.candidate_intervals) >= 3:
        for candidate_interval, candidate_stride, candidate_peak in zip(
            state.candidate_intervals,
            state.candidate_strides,
            state.candidate_peaks,
        ):
            commit(state, candidate_interval, candidate_stride, candidate_peak)
        state.gait_confirmed = True


def replay(rows: list[list[float]], profile: Profile) -> State:
    state = State(average_stride_m=profile.default_stride_m)
    for index, row in enumerate(rows):
        tick_ms = index * 20
        dt_ms = tick_ms - state.last_sample_ms
        if dt_ms == 0 or dt_ms > 200:
            dt_ms = 20

        accel_norm = math.sqrt(sum(value * value for value in row[:3]))
        gyro_norm = math.sqrt(sum(value * value for value in row[3:6]))
        gravity_alpha = alpha(480.0, dt_ms)
        signal_alpha = alpha(55.0, dt_ms)
        state.gravity_g = gravity_alpha * state.gravity_g + (1.0 - gravity_alpha) * accel_norm
        dynamic_g = accel_norm - state.gravity_g
        state.filtered_dynamic_g = (
            signal_alpha * state.filtered_dynamic_g + (1.0 - signal_alpha) * dynamic_g
        )
        state.filtered_gyro_rad_s = (
            signal_alpha * state.filtered_gyro_rad_s + (1.0 - signal_alpha) * gyro_norm
        )
        abs_dynamic_g = abs(state.filtered_dynamic_g)

        if (
            not state.step_active
            and abs_dynamic_g < 0.08
            and state.filtered_gyro_rad_s < 0.18
        ):
            noise_alpha = alpha(2000.0, dt_ms)
            state.noise_dynamic_g = clamp(
                noise_alpha * state.noise_dynamic_g
                + (1.0 - noise_alpha) * abs_dynamic_g,
                0.015,
                0.05,
            )
            state.noise_gyro_rad_s = clamp(
                noise_alpha * state.noise_gyro_rad_s
                + (1.0 - noise_alpha) * state.filtered_gyro_rad_s,
                0.025,
                0.10,
            )

        accel_motion_threshold = max(
            profile.accel_motion_floor_g, state.noise_dynamic_g * 3.0
        )
        accel_peak_threshold = max(
            profile.accel_peak_floor_g, state.noise_dynamic_g * 5.0
        )
        gyro_gate_threshold = max(
            profile.gyro_gate_floor_rad_s, state.noise_gyro_rad_s * 3.0
        )
        gyro_peak_threshold = max(
            profile.gyro_peak_floor_rad_s, state.noise_gyro_rad_s * 5.0
        )

        if tick_ms < 400:
            state.last_sample_ms = tick_ms
            continue

        if (
            abs_dynamic_g >= accel_motion_threshold
            or state.filtered_gyro_rad_s >= gyro_gate_threshold
        ):
            state.last_motion_ms = tick_ms

        if (
            state.last_candidate_ms
            and tick_ms - state.last_candidate_ms > profile.gait_reset_ms
        ):
            clear_chain(state)

        strong = (
            abs_dynamic_g >= accel_peak_threshold
            and state.filtered_gyro_rad_s >= gyro_gate_threshold
        ) or (
            state.filtered_gyro_rad_s >= gyro_peak_threshold
            and abs_dynamic_g >= accel_motion_threshold
        )

        if (
            not state.step_active
            and strong
            and (
                state.last_candidate_ms == 0
                or tick_ms - state.last_candidate_ms >= profile.min_interval_ms
            )
        ):
            state.step_active = True
            state.step_start_ms = tick_ms
            state.peak_ms = tick_ms
            state.peak_dynamic_g = abs_dynamic_g
            state.peak_gyro_rad_s = state.filtered_gyro_rad_s
        elif state.step_active:
            if abs_dynamic_g > state.peak_dynamic_g:
                state.peak_dynamic_g = abs_dynamic_g
                state.peak_ms = tick_ms
            state.peak_gyro_rad_s = max(
                state.peak_gyro_rad_s, state.filtered_gyro_rad_s
            )

            fell = (
                tick_ms - state.step_start_ms >= profile.peak_fall_min_ms
                and (
                    (
                        state.peak_gyro_rad_s >= gyro_peak_threshold
                        and state.filtered_gyro_rad_s <= state.peak_gyro_rad_s * 0.72
                    )
                    or (
                        state.peak_dynamic_g >= accel_peak_threshold
                        and abs_dynamic_g <= state.peak_dynamic_g * 0.62
                    )
                )
            ) or (
                abs_dynamic_g <= max(0.10, accel_motion_threshold)
                and state.filtered_gyro_rad_s
                <= max(0.30, gyro_gate_threshold * 1.5)
            )

            if fell or tick_ms - state.step_start_ms > profile.event_timeout_ms:
                accept_candidate(
                    state, profile, state.peak_ms, state.peak_dynamic_g
                )
                state.step_active = False
                state.peak_dynamic_g = 0.0
                state.peak_gyro_rad_s = 0.0

        state.last_sample_ms = tick_ms
    return state


def load_metadata(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    with path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.reader(stream))
    if rows and [item.lower() for item in rows[0][:2]] == ["field", "value"]:
        rows = rows[1:]
    for row in rows:
        if len(row) >= 2:
            result[row[0]] = row[1]
    return result


def load_samples(path: Path) -> list[list[float]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return [[float(value) for value in row[:6]] for row in csv.reader(stream) if row]


def float_value(metadata: dict[str, str], *keys: str) -> float | None:
    for key in keys:
        value = metadata.get(key)
        if value not in (None, ""):
            return float(value)
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("roots", nargs="+", type=Path)
    parser.add_argument("--min-interval-ms", type=int, default=300)
    parser.add_argument("--max-interval-ms", type=int, default=1800)
    parser.add_argument("--consistency-fraction", type=float, default=0.55)
    parser.add_argument("--consistency-floor-ms", type=int, default=240)
    parser.add_argument("--no-recovery", action="store_true")
    parser.add_argument("--features", action="store_true")
    parser.add_argument("--default-stride-m", type=float, default=0.62)
    parser.add_argument("--min-stride-m", type=float, default=0.42)
    parser.add_argument("--max-stride-m", type=float, default=0.88)
    parser.add_argument("--cadence-reference-spm", type=float, default=100.0)
    parser.add_argument("--cadence-stride-gain", type=float, default=0.0015)
    parser.add_argument("--peak-stride-gain", type=float, default=0.35)
    args = parser.parse_args()
    profile = Profile(
        min_interval_ms=args.min_interval_ms,
        max_interval_ms=args.max_interval_ms,
        consistency_fraction=args.consistency_fraction,
        consistency_floor_ms=args.consistency_floor_ms,
        recover_missed_steps=not args.no_recovery,
        default_stride_m=args.default_stride_m,
        min_stride_m=args.min_stride_m,
        max_stride_m=args.max_stride_m,
        cadence_reference_spm=args.cadence_reference_spm,
        cadence_stride_gain=args.cadence_stride_gain,
        peak_stride_gain=args.peak_stride_gain,
    )

    total_abs_step_error = 0.0
    total_abs_distance_error = 0.0
    evaluated = 0
    header = (
        "dataset,actual_steps,pred_steps,step_error_pct,"
        "actual_distance_m,pred_distance_m,distance_error_pct"
    )
    if args.features:
        header += ",median_cadence_spm,mean_peak_g,actual_stride_m"
    print(header)
    for root in args.roots:
        for directory in sorted(path for path in root.iterdir() if path.is_dir()):
            continuous = next(directory.glob("*_continuous.csv"), None)
            metadata_path = next(directory.glob("*metadata*.csv"), None)
            if continuous is None or metadata_path is None:
                continue
            metadata = load_metadata(metadata_path)
            actual_steps = float_value(metadata, "actual_steps", "true_steps")
            actual_distance = float_value(metadata, "distance_m")
            exported_duration = float_value(
                metadata, "raw_duration_s", "exported_duration_s"
            )
            reference_duration = float_value(
                metadata, "reported_duration_s", "recorded_duration_s"
            )
            if reference_duration is None:
                speed = float_value(metadata, "avg_speed_mps", "average_speed_mps")
                if speed and actual_distance:
                    reference_duration = actual_distance / speed
            if (
                actual_steps is None
                or actual_distance is None
                or (
                    exported_duration
                    and reference_duration
                    and exported_duration < reference_duration * 0.85
                )
            ):
                continue

            state = replay(load_samples(continuous), profile)
            step_error = (state.steps - actual_steps) * 100.0 / actual_steps
            distance_error = (
                (state.distance_m - actual_distance) * 100.0 / actual_distance
            )
            line = (
                f"{directory.name},{actual_steps:.0f},{state.steps},"
                f"{step_error:.2f},{actual_distance:.2f},{state.distance_m:.2f},"
                f"{distance_error:.2f}"
            )
            if args.features:
                median_cadence = statistics.median(
                    60000.0 / value for value in state.committed_intervals
                )
                mean_peak = statistics.fmean(state.committed_peaks)
                line += (
                    f",{median_cadence:.2f},{mean_peak:.5f},"
                    f"{actual_distance / actual_steps:.5f}"
                )
            print(line)
            total_abs_step_error += abs(step_error)
            total_abs_distance_error += abs(distance_error)
            evaluated += 1

    if evaluated:
        print(
            f"MEAN_ABS_ERROR,,,{total_abs_step_error / evaluated:.2f},,,"
            f"{total_abs_distance_error / evaluated:.2f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
