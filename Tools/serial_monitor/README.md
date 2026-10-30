# 串口监控工具

安装依赖：

```powershell
python -m pip install pyserial
```

查看可用串口：

```powershell
python Tools/serial_monitor/serial_monitor.py --list
```

列出全部可用串口并交互选择：

```powershell
python Tools/serial_monitor/serial_monitor.py select
```

以 115200 波特率监控 `COM3`：

```powershell
python Tools/serial_monitor/serial_monitor.py COM3
```

仅连接一个串口设备时，可以自动选择端口：

```powershell
python Tools/serial_monitor/serial_monitor.py auto
```

保存日志：

```powershell
python Tools/serial_monitor/serial_monitor.py COM3 --log Logs/serial.log
```

以十六进制显示：

```powershell
python Tools/serial_monitor/serial_monitor.py COM3 --hex
```

每条输出均带本地时间戳，例如：

```text
[2026-10-30 15:20:12.123] device ready
```
