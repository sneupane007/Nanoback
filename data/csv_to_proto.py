#!/usr/bin/env python3
"""Convert a nanoback CSV file to a length-delimited protobuf binary (.pb).

Usage:
    # First generate the Python bindings (run once from project root):
    #   protoc --python_out=data proto/ohlcv.proto
    #
    # Then convert:
    #   python3 data/csv_to_proto.py data/AAPL.csv AAPL data/AAPL.pb

The output format is the standard protobuf length-delimited stream:
each record is a varint byte-length followed by a serialized OhlcvBar.
ProtoDataHandler reads this format directly.
"""

import csv
import struct
import sys
import os

# Adjust path so we can import the generated ohlcv_pb2 module from data/
sys.path.insert(0, os.path.dirname(__file__))

try:
    import ohlcv_pb2
except ModuleNotFoundError:
    print("ERROR: ohlcv_pb2.py not found.")
    print("Generate it first:")
    print("  protoc --python_out=data proto/ohlcv.proto")
    sys.exit(1)

from google.protobuf.internal.encoder import _EncodeVarint


def convert(csv_path: str, ticker: str, out_path: str) -> None:
    # Column name aliases matching the C++ DataHandler
    date_keys   = {"date", "timestamp", "time", "datetime", "open_time"}
    open_keys   = {"open"}
    high_keys   = {"high"}
    low_keys    = {"low"}
    close_keys  = {"close", "adj close", "adj_close", "adjusted_close"}
    volume_keys = {"volume", "vol"}

    def find_col(header: list[str], keys: set[str]) -> str | None:
        for h in header:
            if h.strip().lower() in keys:
                return h
        return None

    bars_written = 0
    with open(csv_path, newline="") as f_in, open(out_path, "wb") as f_out:
        reader = csv.DictReader(f_in)
        header = reader.fieldnames or []

        col_date   = find_col(header, date_keys)
        col_open   = find_col(header, open_keys)
        col_high   = find_col(header, high_keys)
        col_low    = find_col(header, low_keys)
        col_close  = find_col(header, close_keys)
        col_volume = find_col(header, volume_keys)

        for row in reader:
            bar = ohlcv_pb2.OhlcvBar()
            bar.ticker = ticker

            if col_open:
                try:
                    bar.open  = float(row[col_open])
                    bar.high  = float(row[col_high])
                    bar.low   = float(row[col_low])
                    bar.close = float(row[col_close])
                    bar.volume = int(float(row[col_volume]))
                except (ValueError, TypeError):
                    continue  # skip malformed rows, same as C++ DataHandler

            # timestamp: integer epoch or 0 for date strings
            if col_date:
                raw = row[col_date].strip()
                try:
                    bar.timestamp = int(raw)
                except ValueError:
                    bar.timestamp = 0  # date strings like "2024-01-02" → 0

            data = bar.SerializeToString()
            _EncodeVarint(f_out.write, len(data), None)
            f_out.write(data)
            bars_written += 1

    in_bytes  = os.path.getsize(csv_path)
    out_bytes = os.path.getsize(out_path)
    ratio     = 100.0 * out_bytes / in_bytes if in_bytes else 0
    print(f"Wrote {bars_written} bars to {out_path}")
    print(f"Size: {in_bytes:,} bytes (CSV) → {out_bytes:,} bytes (.pb)  ({ratio:.1f}%)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <csv_path> <ticker> <out_path>")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2], sys.argv[3])
