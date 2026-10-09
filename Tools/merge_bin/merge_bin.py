import json
import sys
import zlib
from pathlib import Path


def parse_int(value, name):
    try:
        return int(value, 0) if isinstance(value, str) else int(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"Invalid {name}: {value}") from error


def parse_addr(value, name, flash_addr, flash_size, last_merge_end=None):
    if isinstance(value, str):
        keyword = value.strip().lower()

        if keyword == "last merge end":
            if last_merge_end is None:
                raise ValueError(f"{name} cannot use last merge end before any region")
            return last_merge_end

        if keyword == "flash end":
            return flash_addr + flash_size

    return parse_int(value, name)


def parse_patch_data(value, size, name):
    if size <= 0:
        raise ValueError(f"{name} size must be greater than 0")

    if isinstance(value, str) and "," in value:
        data = bytes(parse_int(part.strip(), name) for part in value.split(","))

        if len(data) != size:
            raise ValueError(
                f"{name} data length 0x{len(data):X} does not match size 0x{size:X}"
            )

        return data

    data = parse_int(value, name)

    if data < 0 or data >= (1 << (size * 8)):
        raise ValueError(f"{name} data 0x{data:X} does not fit in {size} byte(s)")

    return data.to_bytes(size, byteorder="little")


def resolve_config_path(config_dir, value):
    path = Path(value)

    if path.is_absolute():
        return path

    return (config_dir / path).resolve()


def load_config(config_file):
    config_file = Path(config_file).resolve()
    config_dir = config_file.parent

    with config_file.open("r", encoding="utf-8") as file:
        config = json.load(file)

    flash_addr = parse_int(
        config.get("flash_addr", config.get("flash-addr")), "flash_addr"
    )
    flash_size = parse_int(
        config.get("flash_size", config.get("flash-size")), "flash_size"
    )
    default_fill_to_total_size = bool(config.get("fill_to_total_size", False))
    default_fill_idle_char = parse_int(
        config.get("fill_idle_char", "0xFF"),
        "fill_idle_char",
    )

    output_file = config.get("output_file", config.get("output-file"))
    if output_file is None:
        raise ValueError("Missing output-file")

    if isinstance(output_file, list):
        return {
            "flash_addr": flash_addr,
            "flash_size": flash_size,
            "outputs": [
                load_output_config(
                    config_dir,
                    output,
                    output_index,
                    flash_addr,
                    flash_size,
                    default_fill_to_total_size,
                    default_fill_idle_char,
                    config.get("header"),
                )
                for output_index, output in enumerate(output_file)
            ],
        }

    file_list = config.get("file_list", config.get("flie_list"))

    if not file_list:
        raise ValueError("Missing file_list")

    return {
        "flash_addr": flash_addr,
        "flash_size": flash_size,
        "outputs": [
            {
                "output_file": resolve_config_path(config_dir, output_file),
                "fill_to_total_size": default_fill_to_total_size,
                "fill_idle_char": default_fill_idle_char,
                "file_list": [
                    {
                        "name": item.get("name", f"file_{index}"),
                        "file": resolve_config_path(config_dir, item["file"]),
                        "start_addr": parse_int(item["start_addr"], f"file_list[{index}].start_addr"),
                        "size": parse_int(item["size"], f"file_list[{index}].size"),
                        "optional": bool(item.get("optional", False)),
                    }
                    for index, item in enumerate(file_list)
                ],
                "byte_writing": [],
            }
        ],
    }


def load_output_config(
    config_dir,
    output,
    output_index,
    flash_addr,
    flash_size,
    default_fill_to_total_size,
    default_fill_idle_char,
    header_config,
):
    if "name" not in output:
        raise ValueError(f"output_file[{output_index}] missing name")

    merge_file = output.get("merge_file", output.get("merge-file"))

    if not merge_file:
        raise ValueError(f"output_file[{output_index}] missing merge_file")

    file_list = []
    last_merge_end = None

    for file_index, item in enumerate(merge_file):
        prefix = f"output_file[{output_index}].merge_file[{file_index}]"
        start_addr_value = item.get("start_addr", item.get("start-addr"))
        if start_addr_value is None:
            raise ValueError(f"{prefix} missing start-addr")

        start_addr = parse_addr(
            start_addr_value,
            f"{prefix}.start_addr",
            flash_addr,
            flash_size,
            last_merge_end,
        )

        if "size" in item:
            size = parse_int(item["size"], f"{prefix}.size")
            end_addr = start_addr + size
        elif "end_addr" in item or "end-addr" in item:
            end_addr = parse_addr(
                item.get("end_addr", item.get("end-addr")),
                f"{prefix}.end_addr",
                flash_addr,
                flash_size,
                last_merge_end,
            )
            size = end_addr - start_addr
        else:
            raise ValueError(f"{prefix} missing size or end_addr")

        file_list.append(
            {
                "name": item.get("name", f"file_{file_index}"),
                "file": resolve_config_path(config_dir, item["file"]),
                "start_addr": start_addr,
                "size": size,
                "optional": bool(item.get("optional", False)),
            }
        )
        last_merge_end = end_addr

    byte_writing = []

    for byte_index, item in enumerate(
        output.get(
            "byte_writing",
            output.get("byte-writing", output.get("Byte_writing", [])),
        )
    ):
        prefix = f"output_file[{output_index}].byte_writing[{byte_index}]"
        size = parse_int(item.get("size", "1"), f"{prefix}.size")

        byte_writing.append(
            {
                "name": item.get("name", f"byte_{byte_index}"),
                "addr": parse_addr(
                    item["addr"],
                    f"{prefix}.addr",
                    flash_addr,
                    flash_size,
                ),
                "size": size,
                "data": parse_patch_data(
                    item["data"],
                    size,
                    f"{prefix}.data",
                ),
            }
        )

    return {
        "output_file": resolve_config_path(config_dir, output["name"]),
        "fill_to_total_size": bool(
            output.get(
                "fill_to_total_size",
                output.get("all-fill", default_fill_to_total_size),
            )
        ),
        "fill_idle_char": parse_int(
            output.get(
                "fill_idle_char",
                output.get("fill-char", default_fill_idle_char),
            ),
            f"output_file[{output_index}].fill_idle_char",
        ),
        "file_list": file_list,
        "byte_writing": byte_writing,
        "header": load_header_config(header_config, output_index)
        if output.get("handle") == "header"
        else None,
    }


def load_header_config(header, output_index):
    prefix = f"output_file[{output_index}].header"

    if not header:
        raise ValueError(
            f"output_file[{output_index}] requests header handling, but header is missing"
        )

    size = parse_int(header["size"], f"{prefix}.size")
    if size <= 0:
        raise ValueError(f"{prefix}.size must be greater than 0")

    fields = []
    for name, item in header.get("data", {}).items():
        field_prefix = f"{prefix}.data.{name}"
        field_size = parse_int(item["size"], f"{field_prefix}.size")
        if field_size <= 0:
            raise ValueError(f"{field_prefix}.size must be greater than 0")
        fields.append({**item, "name": name, "size": field_size})

    if sum(item["size"] for item in fields) > size:
        raise ValueError(f"{prefix}.data exceeds header size 0x{size:X}")

    return {
        "size": size,
        "byteorder": "little" if header.get("little-endian", True) else "big",
        "fields": fields,
    }


def crc32_ieee(data):
    """CRC-32/ISO-HDLC: poly=0x04C11DB7, init/xorout=0xFFFFFFFF, refin/refout=true."""
    return zlib.crc32(data) & 0xFFFFFFFF


def build_header(header):
    data = bytearray(header["size"])
    offset = 0
    function_fields = []

    for item in header["fields"]:
        size = item["size"]
        source = item.get("source", "constant")

        if source == "constant":
            value = parse_int(item["value"], f"header.data.{item['name']}.value")
            if value < 0 or value >= (1 << (size * 8)):
                raise ValueError(
                    f"header.data.{item['name']}.value does not fit in {size} byte(s)"
                )
            data[offset:offset + size] = value.to_bytes(size, header["byteorder"])
        elif source == "function":
            function_fields.append((offset, item))
        else:
            raise ValueError(f"Unsupported header source: {source}")

        offset += size

    for field_offset, item in function_fields:
        if item.get("function") != "crc32" or item.get("calculate") != "head_without_crc32":
            raise ValueError(f"Unsupported header function: {item.get('function')}")

        size = item["size"]
        crc_data = data[:field_offset] + data[field_offset + size:]
        value = crc32_ieee(crc_data)
        if value >= (1 << (size * 8)):
            raise ValueError(f"header.data.{item['name']} CRC32 does not fit in {size} byte(s)")
        data[field_offset:field_offset + size] = value.to_bytes(size, header["byteorder"])

    return bytes(data)


def check_range(name, start_addr, size, flash_addr, flash_size):
    if size <= 0:
        raise ValueError(f"{name} size must be greater than 0")

    offset = start_addr - flash_addr
    end_offset = offset + size

    if offset < 0:
        raise ValueError(f"{name} start address is before flash_addr")

    if end_offset > flash_size:
        raise ValueError(
            f"{name} range exceeds flash: "
            f"0x{start_addr:08X}+0x{size:X}"
        )

    return offset, end_offset


def check_overlap(regions):
    regions = sorted(regions, key=lambda item: item["offset"])

    for previous, current in zip(regions, regions[1:]):
        if current["offset"] < previous["end_offset"]:
            raise ValueError(
                f"{current['name']} overlaps {previous['name']}"
            )


def build_regions(
    flash_addr,
    flash_size,
    fill_idle_char,
    file_list,
):
    regions = []

    for item in file_list:
        file_path = Path(item["file"]).resolve()
        name = item["name"]
        start_addr = item["start_addr"]
        size = item["size"]
        optional = item["optional"]

        offset, end_offset = check_range(
            name,
            start_addr,
            size,
            flash_addr,
            flash_size,
        )

        missing = not file_path.exists()

        if missing and not optional:
            raise FileNotFoundError(f"{name} file not found: {file_path}")

        if missing:
            data = b""
            print(
                f"[WARN] Optional {name} file not found, fill area with "
                f"0x{fill_idle_char:02X}: {file_path}",
                flush=True,
            )
        else:
            data = file_path.read_bytes()

        if len(data) > size:
            raise ValueError(
                f"{name} file size 0x{len(data):X} exceeds "
                f"partition size 0x{size:X}"
            )

        regions.append(
            {
                "name": name,
                "file": file_path,
                "data": data,
                "missing": missing,
                "optional": optional,
                "offset": offset,
                "end_offset": end_offset,
                "start_addr": start_addr,
                "size": size,
            }
        )

    check_overlap(regions)
    return regions


def get_image_range(regions, byte_writing, flash_addr, flash_size, fill_to_total_size):
    if fill_to_total_size and min(region["start_addr"] for region in regions) == flash_addr:
        return flash_addr, flash_size

    start_addr = min(region["start_addr"] for region in regions)
    end_addr = max(region["start_addr"] + len(region["data"]) for region in regions)

    for item in byte_writing:
        start_addr = min(start_addr, item["addr"])
        end_addr = max(end_addr, item["addr"] + item["size"])

    return start_addr, end_addr - start_addr


def encode_patch_data(item):
    return item["data"]


def format_usage(used_size, total_size):
    if total_size <= 0:
        return "0.00%"

    return f"{used_size * 100 / total_size:.2f}%"


def format_patch_value(item):
    return ",".join(f"0x{value:02X}" for value in item["data"])


def format_title(value):
    return str(value).replace("_", " ").title().replace("Ota", "OTA")


def print_output_summary(
    output_file,
    regions,
    byte_writing,
    patch_status,
    total_size,
    show_top_border=True,
):
    border = "+" + ("=" * 85) + "+"
    output_status = "Success"

    if any(status != "Success" for status in patch_status.values()):
        output_status = "Fail"

    if show_top_border:
        print(border, flush=True)

    print(f"Output: {Path(output_file).name} [{output_status}]", flush=True)
    print("Merged Files:", flush=True)
    print(
        f"  {'Type':<12} {'Capacity':<12} {'Used Size':<12} "
        f"{'Usage':<10} {'Exists':<8} {'Location'}",
        flush=True,
    )

    for region in regions:
        used_size = len(region["data"])
        print(
            f"  {format_title(region['name']):<12} "
            f"{f'0x{region['size']:X}':<12} "
            f"{f'0x{used_size:X}':<12} "
            f"{format_usage(used_size, region['size']):<10} "
            f"{('No' if region['missing'] else 'Yes'):<8} "
            f"{region['file']}",
            flush=True,
        )

    print("Byte Patching:", flush=True)
    print(
        f"  {'Name':<12} {'Address':<12} {'Value':<12} {'Status'}",
        flush=True,
    )

    for item_index, item in enumerate(byte_writing):
        for index, value in enumerate(item["data"]):
            status = patch_status.get((item_index, index), "Fail")

            print(
                f"  {item['name']:<12} "
                f"0x{item['addr'] + index:08X}   "
                f"0x{value:02X}         "
                f"{status}",
                flush=True,
            )

    print(f"Total Size: 0x{total_size:X}", flush=True)
    print(border, flush=True)


def merge_output(
    flash_addr,
    flash_size,
    fill_to_total_size,
    fill_idle_char,
    output_file,
    file_list,
    byte_writing,
    header=None,
    show_top_border=True,
):
    if not 0 <= fill_idle_char <= 0xFF:
        raise ValueError("fill_idle_char must be 0x00 to 0xFF")

    output_file = Path(output_file).resolve()
    regions = build_regions(flash_addr, flash_size, fill_idle_char, file_list)
    image_base, image_size = get_image_range(
        regions,
        byte_writing,
        flash_addr,
        flash_size,
        fill_to_total_size,
    )

    image = bytearray([fill_idle_char] * image_size)
    patch_status = {}
    patched_addresses = {}

    for region in regions:
        offset = region["start_addr"] - image_base
        data = region["data"]
        image[offset:offset + len(data)] = data

    for item_index, item in enumerate(byte_writing):
        patch_data = encode_patch_data(item)
        offset = item["addr"] - image_base
        end_offset = offset + len(patch_data)

        if offset < 0 or end_offset > len(image):
            raise ValueError(
                f"{item['name']} range exceeds image: "
                f"0x{item['addr']:08X}+0x{item['size']:X}"
            )

        for byte_index, value in enumerate(patch_data):
            addr = item["addr"] + byte_index

            if addr in patched_addresses:
                patch_status[(item_index, byte_index)] = "Duplicate Addr"
                continue

            image[offset + byte_index] = value
            patched_addresses[addr] = item["name"]
            patch_status[(item_index, byte_index)] = "Success"

    output_file.parent.mkdir(parents=True, exist_ok=True)
    header_data = build_header(header) if header is not None else b""
    output_file.write_bytes(header_data + image)

    print_output_summary(
        output_file,
        regions,
        byte_writing,
        patch_status,
        len(header_data) + len(image),
        show_top_border,
    )


def merge_bin(
    flash_addr,
    flash_size,
    outputs,
):
    for output_index, output in enumerate(outputs):
        merge_output(
            flash_addr=flash_addr,
            flash_size=flash_size,
            show_top_border=(output_index == 0),
            **output,
        )


def main():
    print("[INFO] merge_bin.py start", flush=True)

    if len(sys.argv) != 2:
        print("Usage: py merge_bin.py <ConfigFile>", flush=True)
        return 1

    try:
        config = load_config(sys.argv[1])
        merge_bin(**config)
    except Exception as error:
        print(f"[ERROR] {error}", flush=True)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
