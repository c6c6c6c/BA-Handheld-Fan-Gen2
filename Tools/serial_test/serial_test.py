#!/usr/bin/env python3
"""Handheld fan serial protocol tester."""

from __future__ import annotations

import argparse
import struct
import sys
import time
import unicodedata
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional


SOF = b"\x55\xAA"
RESPONSE_TIMEOUT = 1.0
COMMON_BAUDRATES = (115200, 9600, 19200, 38400, 57600, 230400, 460800)

PROTO_INFO_REQUEST = 0x81
PROTO_SYSTEM_REQUEST = 0x82
PROTO_BATTERY_INFO_REQUEST = 0x83
PROTO_ENTER_OTA_REQUEST = 0x86
PROTO_OTA_UPDATE_REQUEST = 0x87
PROTO_OTA_COMPLETION_REQUEST = 0x88
PROTO_INFO_RESPONSE = 0x01
PROTO_SYSTEM_RESPONSE = 0x02
PROTO_BATTERY_INFO_RESPONSE = 0x03
PROTO_ENTER_OTA_RESPONSE = 0x06
PROTO_OTA_UPDATE_RESPONSE = 0x07
PROTO_OTA_COMPLETION_RESPONSE = 0x08
PROTO_ERR = 0xFF

OTA_PAGE_SIZE = 128
OTA_APP_BASE_ADDRESS = 0x08004000
OTA_APP_MAX_SIZE = 0xBE00
OTA_UPDATE_ACTION = "PROTO_OTA_UPDATE_REQUEST"
DEFAULT_FIRMWARE = (
    Path(__file__).resolve().parents[2] / "Projects" / "Output" / "ba_fan_app.bin"
)


class ProtocolError(Exception):
    """Raised when a received protocol frame is invalid."""


@dataclass(frozen=True)
class Query:
    name: str
    request: int
    response: int
    parser: Callable[[bytes], list[tuple[str, object]]]
    response_optional: bool = False


@dataclass
class OtaSession:
    path: Path
    data: bytes
    next_packet: int = 0

    @property
    def packet_count(self) -> int:
        return (len(self.data) + OTA_PAGE_SIZE - 1) // OTA_PAGE_SIZE


def terminal_color(text: object, code: str, enabled: bool) -> str:
    """在支持 ANSI 的终端中为文本添加颜色。"""
    value = str(text)
    return f"\033[{code}m{value}\033[0m" if enabled else value


def display_width(text: object) -> int:
    """返回文本的终端显示宽度，中文字符按两列计算。"""
    return sum(
        2 if unicodedata.east_asian_width(char) in ("W", "F") else 1
        for char in str(text)
    )


def pad_display(text: object, width: int) -> str:
    value = str(text)
    return value + " " * max(0, width - display_width(value))


def crc16(data: bytes) -> int:
    """Return the CRC-16/MODBUS value used by the firmware."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def build_frame(command: int, payload: bytes = b"") -> bytes:
    if len(payload) > 0xFF:
        raise ValueError("协议数据区不能超过 255 字节")
    frame = SOF + bytes((command, len(payload))) + payload
    return frame + struct.pack("<H", crc16(frame))


def read_exact(port: object, size: int, deadline: float) -> bytes:
    data = bytearray()
    while len(data) < size:
        if time.monotonic() >= deadline:
            raise TimeoutError
        chunk = port.read(size - len(data))
        if chunk:
            data.extend(chunk)
    return bytes(data)


def read_frame(port: object, timeout: float) -> tuple[int, bytes, bytes]:
    """Read one complete frame, resynchronizing on the 55 AA header."""
    deadline = time.monotonic() + timeout
    matched = 0
    while matched < len(SOF):
        byte = read_exact(port, 1, deadline)[0]
        if byte == SOF[matched]:
            matched += 1
        else:
            matched = 1 if byte == SOF[0] else 0

    header_tail = read_exact(port, 2, deadline)
    command, data_length = header_tail
    tail = read_exact(port, data_length + 2, deadline)
    frame = SOF + header_tail + tail
    if crc16(frame) != 0:
        raise ProtocolError(f"CRC 校验失败: {frame.hex(' ').upper()}")
    return command, tail[:-2], frame


def unpack_payload(fmt: str, payload: bytes, response_name: str) -> tuple[object, ...]:
    expected = struct.calcsize(fmt)
    if len(payload) != expected:
        raise ProtocolError(
            f"{response_name} 数据长度错误: 收到 {len(payload)} 字节，预期 {expected} 字节"
        )
    return struct.unpack(fmt, payload)


def parse_info(payload: bytes) -> list[tuple[str, object]]:
    version, uid0, uid1, uid2, year, month, day = unpack_payload(
        "<B3IHBB", payload, "PROTO_INFO_RESPONSE"
    )
    return [
        ("协议版本", f"0x{version:02X}"),
        ("MCU UID", f"{uid0:08X}-{uid1:08X}-{uid2:08X}"),
        ("固件编译日期", f"{year:04d}-{month:02d}-{day:02d}"),
    ]


def parse_system(payload: bytes) -> list[tuple[str, object]]:
    values = unpack_payload(
        "<BdddIffffIIHHBBBIHIIIB", payload, "PROTO_SYSTEM_RESPONSE"
    )
    (
        state, usb_in_v, voltage_v, current_ma, cycle_count,
        soc, static_soc, soh, remaining_capacity_mah,
        charge_mah_total, discharge_mah_total, fault_real, fault_hold,
        power, fan_mode, mist_mode, mist_freq, mist_duty,
        on_time, off_time, mist_time, button,
    ) = values
    state_names = {0: "空闲", 1: "充电", 2: "放电", 3: "满电", 4: "故障"}
    return [
        ("电池状态", f"{state} ({state_names.get(state, '未知')})"),
        ("USB 输入电压", f"{usb_in_v:.3f} V"),
        ("电池电压", f"{voltage_v:.3f} V"),
        ("电池电流", f"{current_ma:.3f} mA"),
        ("循环次数", cycle_count),
        ("SOC", f"{soc:.2f} %"),
        ("静态 SOC", f"{static_soc:.2f} %"),
        ("SOH", f"{soh:.2f} %"),
        ("剩余容量", f"{remaining_capacity_mah:.2f} mAh"),
        ("累计充电量", f"{charge_mah_total} mAh"),
        ("累计放电量", f"{discharge_mah_total} mAh"),
        ("实时故障", f"0x{fault_real:04X}"),
        ("锁存故障", f"0x{fault_hold:04X}"),
        ("电源状态", power),
        ("风扇模式", fan_mode),
        ("雾化模式", mist_mode),
        ("雾化频率", f"{mist_freq} Hz"),
        ("雾化占空比", mist_duty),
        ("开机时间", f"{on_time} ms"),
        ("关机时间", f"{off_time} ms"),
        ("雾化运行时间", f"{mist_time} ms"),
        ("按键状态", f"0x{button:02X}"),
    ]


def parse_battery_info(payload: bytes) -> list[tuple[str, object]]:
    values = unpack_payload("<fffffffI", payload, "PROTO_BATTERY_INFO_RESPONSE")
    (
        design_capacity, full_voltage, cutoff_voltage, rest_current,
        rest_dvdt, charge_efficiency, discharge_efficiency, rest_time,
    ) = values
    return [
        ("设计容量", f"{design_capacity:.2f} mAh"),
        ("满充电压", f"{full_voltage:.3f} V"),
        ("截止电压", f"{cutoff_voltage:.3f} V"),
        ("静置电流阈值", f"{rest_current:.3f} mA"),
        ("静置电压变化率阈值", f"{rest_dvdt:.6f} V/s"),
        ("充电效率", f"{charge_efficiency:.4f}"),
        ("放电效率", f"{discharge_efficiency:.4f}"),
        ("OCV 静置时间", f"{rest_time} ms"),
    ]


def parse_empty_response(payload: bytes) -> list[tuple[str, object]]:
    if payload:
        raise ProtocolError(f"应答数据区应为空，实际收到 {len(payload)} 字节")
    return []


INFO_QUERY = Query(
    "PROTO_INFO_RESPONSE", PROTO_INFO_REQUEST, PROTO_INFO_RESPONSE, parse_info
)
SYSTEM_QUERY = Query(
    "PROTO_SYSTEM_RESPONSE", PROTO_SYSTEM_REQUEST, PROTO_SYSTEM_RESPONSE, parse_system
)
BATTERY_QUERY = Query(
    "PROTO_BATTERY_INFO_RESPONSE", PROTO_BATTERY_INFO_REQUEST,
    PROTO_BATTERY_INFO_RESPONSE, parse_battery_info,
)
ENTER_OTA_QUERY = Query(
    "PROTO_ENTER_OTA_RESPONSE", PROTO_ENTER_OTA_REQUEST,
    PROTO_ENTER_OTA_RESPONSE, parse_empty_response, True,
)
OTA_COMPLETION_QUERY = Query(
    "PROTO_OTA_COMPLETION_RESPONSE", PROTO_OTA_COMPLETION_REQUEST,
    PROTO_OTA_COMPLETION_RESPONSE, parse_empty_response,
)

MENU_ITEMS: tuple[Query | str, ...] = (
    INFO_QUERY,
    SYSTEM_QUERY,
    BATTERY_QUERY,
    ENTER_OTA_QUERY,
    OTA_UPDATE_ACTION,
    OTA_COMPLETION_QUERY,
)


def select_port(ports: list[object]) -> Optional[str]:
    if not ports:
        print("未检测到可用串口，请检查设备连接和驱动。", file=sys.stderr)
        return None
    print("检测到以下串口：")
    for index, port in enumerate(ports, 1):
        print(f"  {index}. {port.device:<10} {port.description or '未知设备'}")
    while True:
        try:
            choice = input("请选择串口 [1]: ").strip()
        except (EOFError, KeyboardInterrupt):
            return None
        if not choice:
            return ports[0].device
        if choice.isdigit() and 1 <= int(choice) <= len(ports):
            return ports[int(choice) - 1].device
        for port in ports:
            if choice.lower() == port.device.lower():
                return port.device
        print("输入无效，请输入串口编号或名称。")


def select_baudrate() -> Optional[int]:
    print("常用波特率：")
    for index, baudrate in enumerate(COMMON_BAUDRATES, 1):
        print(f"  {index}. {baudrate}")
    while True:
        try:
            choice = input("请选择波特率 [1]: ").strip()
        except (EOFError, KeyboardInterrupt):
            return None
        if not choice:
            return COMMON_BAUDRATES[0]
        if choice.isdigit():
            value = int(choice)
            if 1 <= value <= len(COMMON_BAUDRATES):
                return COMMON_BAUDRATES[value - 1]
            if value > 0:
                return value
        print("输入无效，请输入编号或正整数波特率。")


def select_action() -> Optional[Query | str]:
    """交互选择要检测的协议帧，返回 None 表示退出。"""
    print("可检测的协议帧：")
    for index, item in enumerate(MENU_ITEMS, 1):
        name = item.name if isinstance(item, Query) else item
        if item is ENTER_OTA_QUERY:
            name = "PROTO_ENTER_OTA_REQUEST"
        elif item is OTA_COMPLETION_QUERY:
            name = "PROTO_OTA_COMPLETION_REQUEST"
        print(f"  {index}. {name}")
    print("  0. 退出")
    while True:
        try:
            choice = input("请选择要检测的帧 [1]: ").strip()
        except (EOFError, KeyboardInterrupt):
            return None
        if not choice:
            return MENU_ITEMS[0]
        if choice == "0" or choice.lower() in ("q", "quit", "exit"):
            return None
        if choice.isdigit() and 1 <= int(choice) <= len(MENU_ITEMS):
            return MENU_ITEMS[int(choice) - 1]
        for item in MENU_ITEMS:
            name = item.name if isinstance(item, Query) else item
            if item is ENTER_OTA_QUERY:
                name = "PROTO_ENTER_OTA_REQUEST"
            elif item is OTA_COMPLETION_QUERY:
                name = "PROTO_OTA_COMPLETION_REQUEST"
            if choice.upper() == name:
                return item
        print("输入无效，请输入帧编号或完整名称。")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="手持风扇通信协议测试工具")
    parser.add_argument("port", nargs="?", help="串口名；省略时交互选择")
    parser.add_argument("-b", "--baudrate", type=int, help="波特率；省略时交互选择")
    parser.add_argument("--timeout", type=float, default=RESPONSE_TIMEOUT,
                        help="单帧回复超时（秒）")
    parser.add_argument(
        "--firmware", type=Path, default=DEFAULT_FIRMWARE,
        help="OTA 烧录使用的原始 Application BIN 文件",
    )
    parser.add_argument("--list", action="store_true", help="列出串口后退出")
    parser.add_argument("--no-color", action="store_true", help="禁用终端彩色输出")
    return parser.parse_args()


def print_ports(ports: list[object]) -> None:
    if not ports:
        print("未检测到可用串口。")
        return
    for port in ports:
        print(f"{port.device:<10} {port.description or '未知设备'}")


def run_query(port: object, query: Query, timeout: float, color_enabled: bool) -> bool:
    request = build_frame(query.request)
    port.reset_input_buffer()
    port.write(request)
    port.flush()
    request_name = query.name.replace("RESPONSE", "REQUEST")
    print(
        f"\n{terminal_color('发送', '36', color_enabled)} "
        f"{request_name}: {terminal_color(request.hex(' ').upper(), '90', color_enabled)}"
    )
    try:
        command, payload, raw_frame = read_frame(port, timeout)
    except TimeoutError:
        if query.response_optional:
            print(
                f"未收到 {query.name}；应用程序可能已立即复位进入 Bootloader。"
            )
            return True
        print(f"超时：{timeout:g} 秒内未收到 {query.name}", file=sys.stderr)
        return False
    except ProtocolError as exc:
        print(f"协议错误：{exc}", file=sys.stderr)
        return False

    print(
        f"{terminal_color('收到', '33', color_enabled)}: "
        f"{terminal_color(raw_frame.hex(' ').upper(), '90', color_enabled)}"
    )
    if command == PROTO_ERR:
        print("设备返回 PROTO_ERR。", file=sys.stderr)
        return False
    if command != query.response:
        print(f"回复命令错误：收到 0x{command:02X}，预期 0x{query.response:02X}",
              file=sys.stderr)
        return False
    try:
        fields = query.parser(payload)
    except ProtocolError as exc:
        print(f"解析错误：{exc}", file=sys.stderr)
        return False
    print(terminal_color(f"{query.name} 解析结果：", "1;35", color_enabled))
    if not fields:
        print(f"  {terminal_color('应答数据区为空，命令执行成功', '32', color_enabled)}")
        return True
    label_width = max(display_width(name) for name, _ in fields)
    for name, value in fields:
        label = pad_display(name, label_width)
        print(
            f"  {terminal_color(label, '36', color_enabled)} : "
            f"{terminal_color(value, '32', color_enabled)}"
        )
    return True


def load_ota_session(firmware: Path) -> Optional[OtaSession]:
    path = firmware.expanduser().resolve()
    if not path.is_file():
        print(f"OTA 固件文件不存在：{path}", file=sys.stderr)
        return None
    if "upgrade" in path.stem.lower():
        print(
            "OTA 协议应发送原始 Application BIN，不能使用带升级头的 upgrade.bin。",
            file=sys.stderr,
        )
        return None
    data = path.read_bytes()
    if not data:
        print(f"OTA 固件文件为空：{path}", file=sys.stderr)
        return None
    if len(data) > OTA_APP_MAX_SIZE:
        print(
            f"OTA 固件过大：{len(data)} 字节，App 分区最大 {OTA_APP_MAX_SIZE} 字节。",
            file=sys.stderr,
        )
        return None
    return OtaSession(path, data)


def send_ota_packet(
    port: object,
    session: OtaSession,
    timeout: float,
    color_enabled: bool,
    show_request: bool,
) -> bool:
    packet_index = session.next_packet
    start = packet_index * OTA_PAGE_SIZE
    chunk = session.data[start:start + OTA_PAGE_SIZE]
    payload = struct.pack("<H", len(chunk)) + chunk
    frame = build_frame(PROTO_OTA_UPDATE_REQUEST, payload)
    address = OTA_APP_BASE_ADDRESS + start

    port.reset_input_buffer()
    port.write(frame)
    port.flush()
    heading = (
        f"PROTO_OTA_UPDATE_REQUEST {packet_index + 1}/{session.packet_count}, "
        f"地址 0x{address:08X}, {len(chunk)} 字节"
    )
    print(f"\n{terminal_color('发送', '36', color_enabled)} {heading}")
    if show_request:
        print(f"  {terminal_color(frame.hex(' ').upper(), '90', color_enabled)}")

    try:
        command, response_payload, raw_frame = read_frame(port, timeout)
    except TimeoutError:
        print(f"超时：{timeout:g} 秒内未收到 PROTO_OTA_UPDATE_RESPONSE", file=sys.stderr)
        return False
    except ProtocolError as exc:
        print(f"协议错误：{exc}", file=sys.stderr)
        return False

    print(
        f"{terminal_color('收到', '33', color_enabled)}: "
        f"{terminal_color(raw_frame.hex(' ').upper(), '90', color_enabled)}"
    )
    if command == PROTO_ERR:
        print("设备返回 PROTO_ERR，当前帧未计入进度。", file=sys.stderr)
        return False
    if command != PROTO_OTA_UPDATE_RESPONSE:
        print(
            f"回复命令错误：收到 0x{command:02X}，预期 0x{PROTO_OTA_UPDATE_RESPONSE:02X}",
            file=sys.stderr,
        )
        return False
    if response_payload:
        print(
            f"回复数据长度错误：预期 0，收到 {len(response_payload)} 字节",
            file=sys.stderr,
        )
        return False

    session.next_packet += 1
    return True


def run_ota_update(
    port: object,
    session: OtaSession,
    timeout: float,
    color_enabled: bool,
) -> None:
    print(f"\nOTA 固件：{session.path}")
    print(
        f"大小：{len(session.data)} 字节，共 {session.packet_count} 帧，"
        f"每帧最大 {OTA_PAGE_SIZE} 字节。"
    )
    while True:
        if session.next_packet >= session.packet_count:
            print(
                terminal_color(
                    "所有 OTA_UPDATE 数据帧已烧录，请返回主菜单发送 OTA_COMPLETION。",
                    "1;32",
                    color_enabled,
                )
            )
            return

        next_address = OTA_APP_BASE_ADDRESS + session.next_packet * OTA_PAGE_SIZE
        print(
            f"\n当前进度：{session.next_packet}/{session.packet_count}，"
            f"下一帧地址 0x{next_address:08X}"
        )
        print("  [1] 下一帧")
        print("  [2] 完整烧录之后所有剩余帧")
        print("  [0] 退出")
        try:
            choice = input("请选择 [1]: ").strip() or "1"
        except (EOFError, KeyboardInterrupt):
            return

        if choice == "0":
            return
        if choice == "1":
            if not send_ota_packet(port, session, timeout, color_enabled, True):
                print(
                    "OTA 进度可能已不同步；请重新发送 ENTER_OTA 并从第 1 帧开始。",
                    file=sys.stderr,
                )
                return
            continue
        if choice == "2":
            while session.next_packet < session.packet_count:
                if not send_ota_packet(port, session, timeout, color_enabled, False):
                    print(
                        "完整烧录已停止且进度可能已不同步；"
                        "请重新发送 ENTER_OTA 并从第 1 帧开始。",
                        file=sys.stderr,
                    )
                    return
            continue
        print("输入无效，请输入 0、1 或 2。")


def main() -> int:
    args = parse_args()
    color_enabled = not args.no_color and sys.stdout.isatty()
    if args.timeout <= 0:
        print("错误：--timeout 必须大于 0。", file=sys.stderr)
        return 2
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("错误：未安装 pyserial，请执行 python -m pip install pyserial", file=sys.stderr)
        return 2

    ports = sorted(list_ports.comports(), key=lambda item: item.device)
    if args.list:
        print_ports(ports)
        return 0

    port_name = args.port or select_port(ports)
    if port_name is None:
        return 1
    baudrate = args.baudrate if args.baudrate is not None else select_baudrate()
    if baudrate is None:
        return 1
    if baudrate <= 0:
        print("错误：波特率必须大于 0。", file=sys.stderr)
        return 2
    try:
        with serial.Serial(port_name, baudrate, timeout=0.05,
                           write_timeout=1.0) as port:
            print(f"已打开 {port.port}，波特率 {port.baudrate}，回复超时 {args.timeout:g} 秒。")
            ota_session: Optional[OtaSession] = None
            while True:
                print()
                action = select_action()
                if action is None:
                    print("已退出协议测试。")
                    return 0
                if action == OTA_UPDATE_ACTION:
                    if ota_session is None:
                        ota_session = load_ota_session(args.firmware)
                    if ota_session is not None:
                        run_ota_update(port, ota_session, args.timeout, color_enabled)
                    continue

                query = action
                passed = run_query(port, query, args.timeout, color_enabled)
                if query is ENTER_OTA_QUERY and passed:
                    ota_session = None
                elif query is OTA_COMPLETION_QUERY and passed:
                    ota_session = None
                result = terminal_color(
                    "通过" if passed else "失败",
                    "1;32" if passed else "1;31",
                    color_enabled,
                )
                print(f"\n测试结果：{query.name} {result}。")
    except (serial.SerialException, OSError) as exc:
        print(f"串口访问失败：{exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
