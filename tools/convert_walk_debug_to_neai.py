#!/usr/bin/env python3
import argparse
import csv
import struct
from pathlib import Path


RECORD_SIZE = 16
ACCEL_LSB_PER_G = 16384.0
GYRO_LSB_PER_DPS = 65.5
DEG_TO_RAD = 0.017453292519943295


def read_meta(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) >= 8:
        return struct.unpack_from("<II", data, 0)
    if len(data) < 4:
        raise ValueError(f"metadata file is too small: {path}")
    return struct.unpack_from("<HH", data, 0)


def read_records(path: Path, capacity: int) -> list[tuple[int, int, int, int, int, int, int]]:
    data = path.read_bytes()
    expected = capacity * RECORD_SIZE
    if len(data) < expected:
        raise ValueError(f"sample file has {len(data)} bytes, expected {expected}")

    records = []
    for index in range(capacity):
        records.append(struct.unpack_from("<I6h", data, index * RECORD_SIZE))
    return records


def read_linear_records(path: Path):
    data = path.read_bytes()
    if len(data) % RECORD_SIZE != 0:
        raise ValueError(f"linear sample file size is not a multiple of {RECORD_SIZE}: {path}")
    return [
        struct.unpack_from("<I6h", data, offset)
        for offset in range(0, len(data), RECORD_SIZE)
    ]


def ordered_records(records, write_index: int, count: int):
    capacity = len(records)
    count = min(count, capacity)
    start = (write_index + capacity - count) % capacity
    ordered = []
    for offset in range(count):
        record = records[(start + offset) % capacity]
        if record[0] != 0:
            ordered.append(record)
    return ordered


def sample_to_float_row(record):
    _, ax, ay, az, gx, gy, gz = record
    return [
        ax / ACCEL_LSB_PER_G,
        ay / ACCEL_LSB_PER_G,
        az / ACCEL_LSB_PER_G,
        (gx / GYRO_LSB_PER_DPS) * DEG_TO_RAD,
        (gy / GYRO_LSB_PER_DPS) * DEG_TO_RAD,
        (gz / GYRO_LSB_PER_DPS) * DEG_TO_RAD,
    ]


def write_continuous_csv(path: Path, records):
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        for record in records:
            writer.writerow(sample_to_float_row(record))


def write_window_csv(path: Path, records, window: int, stride: int):
    rows = 0
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        if len(records) >= window:
            for start in range(0, len(records) - window + 1, stride):
                row = []
                for record in records[start:start + window]:
                    row.extend(sample_to_float_row(record))
                writer.writerow(row)
                rows += 1
    return rows


def main():
    parser = argparse.ArgumentParser(description="Convert U5 walk debug storage to NanoEdge CSV files.")
    parser.add_argument("--samples-bin", required=True, type=Path)
    parser.add_argument("--meta-bin", required=True, type=Path)
    parser.add_argument("--label", required=True)
    parser.add_argument("--out-dir", default=Path("Dataset"), type=Path)
    parser.add_argument("--capacity", default=4096, type=int)
    parser.add_argument("--window", default=64, type=int)
    parser.add_argument("--stride", default=16, type=int)
    parser.add_argument("--linear", action="store_true",
                        help="sample file is already in chronological order")
    args = parser.parse_args()

    write_index, count = read_meta(args.meta_bin)
    if args.linear:
        ordered = [record for record in read_linear_records(args.samples_bin) if record[0] != 0]
    else:
        records = read_records(args.samples_bin, args.capacity)
        ordered = ordered_records(records, write_index, count)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    continuous_path = args.out_dir / f"{args.label}_continuous.csv"
    windows_path = args.out_dir / f"{args.label}.csv"

    write_continuous_csv(continuous_path, ordered)
    window_rows = write_window_csv(windows_path, ordered, args.window, args.stride)

    print(f"label={args.label}")
    print(f"samples={len(ordered)} duration_s={len(ordered) / 50.0:.2f}")
    print(f"continuous_csv={continuous_path}")
    print(f"nanoedge_csv={windows_path} windows={window_rows}")


if __name__ == "__main__":
    main()
