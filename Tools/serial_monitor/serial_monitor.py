#!/usr/bin/env python3
"""Serial-port monitor with timestamps and optional log output."""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import BinaryIO, Optional


def timestamp() -> str:
    """Return the local time with millisecond precision."""
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def format_payload(payload: bytes, hex_mode: bool, encoding: str) -> str:
    """Format one received record for display and logging."""
    if hex_mode:
        return " ".join(f"{byte:02X}" for byte in payload)
    return payload.decode(encoding, errors="replace")


def emit(payload: bytes, hex_mode: bool, encoding: str, log_file: Optional[BinaryIO]) -> None:
    """Print one timestamped record and optionally append it to a log file."""
    record = f"[{timestamp()}] {format_payload(payload, hex_mode, encoding)}"
    print(record, flush=True)

    if log_file is not None:
        log_file.write((record + "\n").encode("utf-8"))
        log_file.flush()


def list_ports() -> int:
    """Print available serial ports."""
    try:
        from serial.tools import list_ports as serial_list_ports
    except ImportError:
        print("错误：未安装 pyserial，请执行：python -m pip install pyserial", file=sys.stderr)
        return 2

    ports = sorted(serial_list_ports.comports(), key=lambda item: item.device)
    if not ports:
        print("未发现可用串口。")
        return 0

    for port in ports:
        description = port.description or "未知设备"
        print(f"{port.device:<12} {description}")
    return 0


def select_port(ports: list[object]) -> Optional[str]:
    """Interactively select one port from the detected serial devices."""
    if not ports:
        print("错误：未发现可用串口，请检查设备连接和驱动。", file=sys.stderr)
        return None

    print("可用串口：")
    for index, detected_port in enumerate(ports, start=1):
        description = detected_port.description or "未知设备"
        print(f"  {index}. {detected_port.device:<12} {description}")

    while True:
        try:
            choice = input("请选择串口编号或名称 [1]：").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n已取消串口选择。")
            return None

        if not choice:
            return ports[0].device
        if choice.isdigit():
            selected_index = int(choice) - 1
            if 0 <= selected_index < len(ports):
                return ports[selected_index].device
        else:
            for detected_port in ports:
                if detected_port.device.lower() == choice.lower():
                    return detected_port.device

        print("输入无效，请输入列表中的编号或串口名称。")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="带时间戳的串口监控工具",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("port", nargs="?", help="串口名称，例如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("-b", "--baudrate", type=int, default=115200, help="波特率")
    parser.add_argument("--timeout", type=float, default=0.05, help="串口读取超时，单位秒")
    parser.add_argument(
        "--flush-interval",
        type=float,
        default=0.2,
        help="无换行数据的空闲输出间隔，单位秒；设为 0 表示不自动输出",
    )
    parser.add_argument("--encoding", default="utf-8", help="文本解码格式")
    parser.add_argument("--hex", action="store_true", help="以十六进制显示接收数据")
    parser.add_argument("--log", type=Path, help="将带时间戳的输出追加保存到指定文件")
    parser.add_argument("--list", action="store_true", help="列出可用串口后退出")
    return parser.parse_args()


def monitor(args: argparse.Namespace) -> int:
    try:
        import serial
        from serial.tools import list_ports as serial_list_ports
    except ImportError:
        print("错误：未安装 pyserial，请执行：python -m pip install pyserial", file=sys.stderr)
        return 2

    if args.timeout < 0:
        print("错误：--timeout 不能小于 0。", file=sys.stderr)
        return 2
    if args.flush_interval < 0:
        print("错误：--flush-interval 不能小于 0。", file=sys.stderr)
        return 2

    ports = sorted(serial_list_ports.comports(), key=lambda item: item.device)
    if args.port is None or args.port.lower() == "select":
        args.port = select_port(ports)
        if args.port is None:
            return 1
    elif args.port.lower() == "auto":
        if not ports:
            print("错误：未发现可用串口，请检查设备连接和驱动。", file=sys.stderr)
            return 1
        if len(ports) > 1:
            print("检测到多个串口，请明确指定其中一个：", file=sys.stderr)
            for detected_port in ports:
                description = detected_port.description or "未知设备"
                print(f"  {detected_port.device:<12} {description}", file=sys.stderr)
            return 2

        args.port = ports[0].device
        print(f"[{timestamp()}] 自动选择串口 {args.port}。", flush=True)

    log_file: Optional[BinaryIO] = None
    if args.log is not None:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        log_file = args.log.open("ab")

    try:
        with serial.Serial(args.port, args.baudrate, timeout=args.timeout) as port:
            print(
                f"[{timestamp()}] 已打开 {port.port}，波特率 {port.baudrate}。按 Ctrl+C 退出。",
                flush=True,
            )

            pending = bytearray()
            last_receive_time = time.monotonic()

            while True:
                chunk = port.read(port.in_waiting or 1)
                now = time.monotonic()

                if chunk:
                    pending.extend(chunk)
                    last_receive_time = now

                    while b"\n" in pending:
                        raw_line, _, remainder = pending.partition(b"\n")
                        pending = bytearray(remainder)
                        emit(raw_line.rstrip(b"\r"), args.hex, args.encoding, log_file)
                elif (
                    pending
                    and args.flush_interval > 0
                    and (now - last_receive_time) >= args.flush_interval
                ):
                    emit(bytes(pending), args.hex, args.encoding, log_file)
                    pending.clear()

    except KeyboardInterrupt:
        if "pending" in locals() and pending:
            emit(bytes(pending), args.hex, args.encoding, log_file)
        print(f"\n[{timestamp()}] 监控已停止。")
        return 0
    except serial.SerialException as exc:
        print(f"错误：串口访问失败：{exc}", file=sys.stderr)
        return 1
    finally:
        if log_file is not None:
            log_file.close()


def main() -> int:
    args = parse_args()

    if args.list:
        return list_ports()

    return monitor(args)


if __name__ == "__main__":
    raise SystemExit(main())
