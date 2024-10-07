# Node manager

Simple command line tool to interact with ESP32 nodes.
Since all nodes actively broadcast their availability, the tool listens for their advertisements at the start
and uses their details for distributing tasks and receiving outputs.

```
$ python3 tessie.py -l
Listening for node broadcasts...
Node found: TESSIE_NODE (192.168.1.56), Status: available, Free SPIFFS: 868711, RSSI: -38
Node found: TESSIE_NODE (192.168.1.9), Status: available, Free SPIFFS: 603655, RSSI: -37
Node found: TESSIE_NODE (192.168.1.16), Status: available, Free SPIFFS: 904102, RSSI: -54
Node found: TESSIE_NODE (192.168.1.56), Status: available, Free SPIFFS: 868711, RSSI: -38
Listening completed
```


# Usage
Call `python3 tessie.py --help` for examples 

```
usage: tessie.py [-h] [-b BINARY] [-f PAYLOADS [PAYLOADS ...]] [-a ARGUMENTS [ARGUMENTS ...]] [-r] [-l]

Tessie Node Manager

Examples of usage:

1. Listen for active nodes:
   python3 tessie.py -l

2. Submit a task without payload and argument:
   python3 tessie.py -b task.elf

3. Submit a task with payload but without argument:
   python3 tessie.py -b task.elf -f logfile.csv

4. Submit a task with payload and argument:
   python3 tessie.py -b task.elf -f logfile.csv -a "AA:BB:CC:DD:EE:FF"

5. Submit a task without payload but with argument:
   python3 tessie.py -b task.elf -a "AA:BB:CC:DD:EE:FF"

6. Submit a task with many payloads and no arguments:
   python3 tessie.py -b task.elf -f logfile1.csv logfile2.csv logfile3.csv

7. Submit a task with no payloads and multiple arguments:
   python3 tessie.py -b task.elf -a "AA:BB:CC:DD:EE:FF" "GG:HH:EE:FF:AA:CC"

8. Submit a task with multiple payloads, each with a different argument:
   python3 tessie.py -b task.elf -f logfile1.csv logfile2.csv -a "A" "B"

options:
  -h, --help            show this help message and exit
  -b BINARY, --binary BINARY
                        Path to the binary file to submit (e.g., task.elf).
  -f PAYLOADS [PAYLOADS ...], --payloads PAYLOADS [PAYLOADS ...]
                        List of payload files to submit (e.g., logfile.csv). Supports multiple files.
  -a ARGUMENTS [ARGUMENTS ...], --arguments ARGUMENTS [ARGUMENTS ...]
                        List of additional arguments (e.g., MAC address, config values). Supports multiple values.
  -r, --retrieve        Retrieve task outputs from nodes.
  -l, --listen          Only listen for nodes and report their status.

```
