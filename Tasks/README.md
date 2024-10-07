# Tasks

Tasks are the core of the Tesselator system, representing the workloads executed on each ESP32 node. These tasks are designed to perform the heavy lifting and should be compact, efficient, and well-optimized. Each task is a separate binary, meaning you can create multiple tasks for different purposes or design a single task capable of handling multiple functions, depending on the arguments passed to it.

Tasks are sent to the distributed nodes, where they are executed independently. Whether the task returns a result for later use is entirely up to you, providing flexibility in how tasks are utilized and processed.

## Specification

1. Tasks are stored on Nodes as `/spiffs/task_binary`
2. Tasks get started by main sketch which is calling a function pointer to ```void local_main(const char* arg, size_t len)```
3. If you pass a payload file, you can read it from  `/spiffs/task_input`
4. If you want to return an output from the task, you should store it in `/spiffs/task_output`



## Writing tasks

Every task requires a starting point, so you should implement it:

```
void local_main(const char* arg, size_t len) {
    printf("Starting task\n");
    printf("Job done\n");
}
```

- The `arg` parameter is an argument you pass to a task with `-a` using python script
- Tasks do not return any ret code, simply `void`


## Customizing

If you want to customize anything feel free to do so in the main skech,
there you can declare the function prototype you're calling as a starting point for task

```
  // Execute the function
    typedef void (*func_t)(const char*, size_t len);
    func_t func = (func_t)ctx->exec;
    func(strArgument.c_str(), strArgument.length());
```

Or export function addresses if required

```
    const ELFLoaderSymbol_t exports[] = {
      { "puts", (void*)puts },
      { "printf", (void*)printf },
      { "fgets", (void*)fgets },
      { "fread", (void*)fread },
      { "fwrite", (void*)fwrite },
      { "fopen", (void*)fopen },
      { "fclose", (void*)fclose },
      { "fprintf", (void*)fprintf },
      { "fseek", (void*)fseek },
      { "ftell", (void*)ftell },
      { "fflush", (void*)fflush },
      { "fscanf", (void*)fscanf }
    };
```



## Compilation


A simple shell script `build.sh` to compile the task source code to binary for use with Tessie.
The script uses my Arduino environment, but you can easily update it to fit your needs.
The version of ESP32 i use with my arduino is: `2.0.10`, remember you're working with a ULP device there are constraints here and there.

Few important notes on compilation process:

- When compiling, remember to keep the binary size to minimum
- Linker `.ld` script files are taken from Arduino

## Example

If you're using my `build.sh` script, simply call `./build.sh file_name` you want to build.

```
./build.sh tessie_chaos clean && ./build.sh tessie_chaos
Compiling to tessie_chaos.elf...
Stripping unneeded sections from tessie_chaos.elf...
Generating objdump...
Generating readelf...
Converting tessie_chaos.elf to a C++ hex array...
Encoding tessie_chaos.elf to base64...
Build complete!
Deploy task to Grid tessie_chaos.bin
Test the task with runner tessie_chaos.hex
Send to tesselator tessie_chaos.elf
-rw-rw-r-- 1 invpe invpe  18K Oct  7 11:21 tessie_chaos.bin
-rw-rw-r-- 1 invpe invpe 7,8K Oct  6 15:02 tessie_chaos.c
-rw-rw-r-- 1 invpe invpe  14K Oct  7 11:21 tessie_chaos.elf
-rw-rw-r-- 1 invpe invpe  81K Oct  7 11:21 tessie_chaos.hex
```

Take the `.elf` file for execution on nodes.

# Submitting a task

Once you have a task `.elf` file you want to run, simply follow the steps to run your task on any available node.
You can queue more tasks at once, follow the `--help` for examples.

Let's submit a simple task to any of my free nodes in the network.

1. Let's see what nodes i have available

```
python3 tessie.py -l
Listening for node broadcasts...
Node found: TESSIE_NODE (192.168.1.56), Status: available, Free SPIFFS: 868711, RSSI: -39
Node found: TESSIE_NODE (192.168.1.9), Status: available, Free SPIFFS: 603655, RSSI: -40
Node found: TESSIE_NODE (192.168.1.16), Status: available, Free SPIFFS: 904102, RSSI: -53
Node found: TESSIE_NODE (192.168.1.56), Status: available, Free SPIFFS: 868711, RSSI: -41
Listening completed

Available Nodes:
IP: 192.168.1.56, Node: TESSIE_NODE, MAC: 4C:75:25:CE:AA:B4, Total Executed: 4, Status: available
IP: 192.168.1.9, Node: TESSIE_NODE, MAC: 4C:75:25:E9:CE:80, Total Executed: 4, Status: available
IP: 192.168.1.16, Node: TESSIE_NODE, MAC: 4C:75:25:E9:CC:B0, Total Executed: 10, Status: available
```

Ok few nodes there, let's compile the task

```
./build.sh tessie_simple
...
6,2K Oct  7 11:38 tessie_simple.elf
```

And submit it

```
python3 tessie.py -b ../ELF/Tasks/tessie_simple.elf -a "Hello"

Listening for node broadcasts...
Node found: TESSIE_NODE (192.168.1.16), Status: available, Free SPIFFS: 904102, RSSI: -53
Node found: TESSIE_NODE (192.168.1.56), Status: available, Free SPIFFS: 868711, RSSI: -41
Node found: TESSIE_NODE (192.168.1.9), Status: available, Free SPIFFS: 603655, RSSI: -38
Node found: TESSIE_NODE (192.168.1.16), Status: available, Free SPIFFS: 904102, RSSI: -54
Listening completed

Submitting task to http://192.168.1.16
Task started successfully on http://192.168.1.16

Output from 192.168.1.16: output_from_192_168_1_16_1728293966.txt
```

Done, now the output of the task executed on node 192.168.1.16 is

```
cat output_from_192_168_1_16_1728293966.txt

This is output
Argument given: Hello
```






