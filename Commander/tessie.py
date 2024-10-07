# Tesselator commander 
# https://github.com/invpe/Tesselator
import time
import socket
import json
import requests
import argparse
from collections import deque
import os

BROADCAST_PORT = 1911  # The port the ESP32 nodes are broadcasting on
LISTEN_TIMEOUT = 5  # Timeout for listening to new broadcasts (seconds)
AVAILABLE_NODES = {}  # Dictionary to hold available nodes
TASK_QUEUE = deque()  # Queue to hold pending tasks
POLL_INTERVAL = 1  # Polling interval for checking task output (seconds)

class Task:
    def __init__(self, binary_data, payload_files=None, argument=None):
        self.binary_data = binary_data

        # Optional payload files (can be None)
        self.payloads = {}
        if payload_files:
            for payload_file in payload_files:
                file_content = self.read_file(payload_file)
                self.payloads[payload_file] = file_content

        # Optional argument (can be None or empty string)
        self.argument = argument

    def read_file(self, file_path):
        """Utility to read and return file content in binary mode."""
        if os.path.isfile(file_path):
            with open(file_path, 'rb') as f:
                return f.read()
        raise ValueError(f"File {file_path} not found!")

    def __repr__(self):
        return f"<Task with {len(self.payloads)} payloads and argument: {self.argument}>"

def listen_for_nodes():
    """Listen for node advertisements and update the available nodes list."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Enable reuse of the address to avoid errors
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # Enable broadcasting mode
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind to all interfaces and the broadcast port
    try:
        sock.bind(('', BROADCAST_PORT))  # Bind to the broadcast port
        sock.settimeout(LISTEN_TIMEOUT)  # Set a timeout for listening
    except OSError as e:
        print(f"Failed to bind to port {BROADCAST_PORT}: {e}")
        return

    start_time = time.time()  # Track the start time to manage timeout manually

    try:
        while True:
            try:
                # Check if we've reached the timeout period
                if time.time() - start_time > LISTEN_TIMEOUT:
                    print(f"Listening completed")
                    break

                # Increase buffer size in case the message is larger than 1024 bytes
                message, addr = sock.recvfrom(4096)
                ip_address = addr[0]

                try:
                    # Parse the incoming JSON message
                    node_info = json.loads(message.decode())
                    node_name = node_info.get("node", "Unknown")
                    mac_address = node_info.get("mac", "Unknown")
                    total_executed = node_info.get("total_executed", 0)
                    status = node_info.get("status", "unknown")
                    free_spiffs_bytes = node_info.get("free_spiffs_bytes", "unknown")
                    rssi = node_info.get("rssi", "unknown")

                    # Update the available nodes dictionary
                    AVAILABLE_NODES[ip_address] = {
                        "node_name": node_name,
                        "mac": mac_address,
                        "total_executed": total_executed,
                        "status": status,
                        "free_spiffs_bytes": free_spiffs_bytes,
                        "rssi": rssi
                    }
                    print(f"Node found: {node_name} ({ip_address}), Status: {status}, Free SPIFFS: {free_spiffs_bytes}, RSSI: {rssi}")

                except json.JSONDecodeError:
                    print(f"Received invalid JSON from {ip_address}: {message.decode()}")

            except socket.timeout:
                # Timeout ends the listening period
                print("Socket timed out")
                break

    except OSError as e:
        print(f"Socket error occurred: {e}")

    finally:
        sock.close()

def check_node_status(node_url):
    """Check the availability of a node using the /status endpoint."""
    try:
        response = requests.get(f"{node_url}/status")
        if response.status_code == 200:
            data = response.json()
            return data.get("status") == "available"
    except requests.RequestException as e:
        print(f"Error checking status for {node_url}: {e}")
    return False

def upload_file(node_url, file_data, endpoint):
    """Upload binary or payload to the node using the respective endpoint."""
    try:
        files = {'file': file_data}
        response = requests.post(f"{node_url}/{endpoint}", files=files)        

        if response.status_code == 200:            
            return True
        else:
            print(f"Failed to upload file to {endpoint}: {response.status_code}")
            return False
    except requests.RequestException as e:
        print(f"Error uploading file to {node_url}/{endpoint}: {e}")
        return False

def submit_task(node_url, task):
    print(f"Submitting task to {node_url}")

    try:
        # Step 1: Upload binary file
        binary_success = upload_file(node_url, task.binary_data, 'uploadbin')
        if not binary_success:
            return False

        # Step 2: Upload payload files (if any)
        if task.payloads:
            for filename, file_data in task.payloads.items():
                payload_success = upload_file(node_url, file_data, f'uploadpayload?filename={filename}')
                if not payload_success:
                    return False

        # Step 3: Send argument (MAC address or other parameters)
        if task.argument:
            response = requests.post(f"{node_url}/arg", data=task.argument)
            if response.status_code != 200:
                print(f"Failed to send argument to {node_url}: {response.status_code}")
                return False

        # Step 4: Execute the task on the ESP32 node
        response = requests.post(f"{node_url}/execute")
        print(f"{response.text}")

        if response.status_code == 200:
            print(f"Task started successfully on {node_url}")
            return True
        else:
            print(f"Failed to start task on {node_url}: {response.status_code}")
            return False

    except requests.RequestException as e:
        print(f"Error submitting task to {node_url}: {e}")
        return False

def get_task_output(node_url):
    """Retrieve the task output file from the node using the /output endpoint."""
    try:
        # Make a GET request to the /output endpoint
        response = requests.get(f"{node_url}/output", stream=True)
        if response.status_code == 200:
            # Use node IP and task ID or timestamp to generate a unique filename
            output_file_path = f"output_from_{node_url.replace('http://', '').replace('.', '_')}_{int(time.time())}.txt"
            with open(output_file_path, "wb") as output_file:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        output_file.write(chunk)
            return output_file_path
        else:
            print(f"Failed to get output from {node_url}: {response.status_code}")
    except requests.RequestException as e:
        print(f"Error retrieving output from {node_url}: {e}")
    return None

def manage_nodes():
    """Just listen for node advertisements and display available nodes."""
    print("Listening for node broadcasts...")
    listen_for_nodes()

    print("\nAvailable Nodes:")
    for ip, info in AVAILABLE_NODES.items():
        print(f"IP: {ip}, Node: {info['node_name']}, MAC: {info['mac']}, Total Executed: {info['total_executed']}, Status: {info['status']}")
    print("\n")

def queue_tasks(binary_file, payload_files, arguments):
    """Submit tasks to available nodes with optional payload files and arguments."""
    print("Listening for node broadcasts...")
    listen_for_nodes()

    # Read the binary file in binary mode
    with open(binary_file, "rb") as bin_file:
        binary_data = bin_file.read()

    # Scenario: Matching number of payloads and arguments
    if payload_files and arguments and len(payload_files) == len(arguments):
        # Create a task for each payload-argument pair
        for i in range(len(payload_files)):
            task = Task(binary_data=binary_data, payload_files=[payload_files[i]], argument=arguments[i])
            TASK_QUEUE.append(task)
    
    # Scenario: Multiple payloads but no arguments
    elif payload_files and not arguments:
        for payload_file in payload_files:
            task = Task(binary_data=binary_data, payload_files=[payload_file], argument=None)
            TASK_QUEUE.append(task)

    # Scenario: Multiple arguments but no payloads
    elif arguments and not payload_files:
        for argument in arguments:
            task = Task(binary_data=binary_data, payload_files=None, argument=argument)
            TASK_QUEUE.append(task)

    # Scenario: Mismatched number of payloads and arguments or other scenarios
    elif payload_files and arguments and len(payload_files) != len(arguments):
        print("Error: Mismatched number of payloads and arguments. Please provide equal numbers of each.")
        return

    # Scenario: Neither payloads nor arguments provided
    else:
        task = Task(binary_data=binary_data, payload_files=None, argument=None)
        TASK_QUEUE.append(task)

    # Submit tasks to available nodes
    manage_task_submission()


def manage_task_submission():
    """Assign tasks to available nodes and manage the task queue."""
    active_tasks = {}

    # Continue running as long as there are tasks in the queue or nodes still processing tasks
    while TASK_QUEUE or active_tasks:
        # Poll each node's status and try to submit tasks when available
        for ip_address, node_info in AVAILABLE_NODES.items():
            node_url = f"http://{ip_address}"

            # Check if the node is busy, if yes, check its status periodically
            if ip_address not in active_tasks:  # Only check nodes not already running a task
                node_status = node_info["status"]

                if node_status == "available":
                    if TASK_QUEUE:  # Submit a task if there's one in the queue
                        task = TASK_QUEUE.popleft()
                        success = submit_task(node_url, task)

                        if success:
                            active_tasks[ip_address] = task  # Track active task on the node
                        else:
                            print(f"Failed to submit task to {ip_address}. Requeuing task.")
                            TASK_QUEUE.appendleft(task)  # Requeue the task on failure

                elif node_status == "busy":
                    # Periodically poll the node for availability
                    if check_node_status(node_url):  # If the node is now available, submit a task
                        if TASK_QUEUE:
                            task = TASK_QUEUE.popleft()
                            success = submit_task(node_url, task)

                            if success:
                                active_tasks[ip_address] = task  # Track active task on the node
                            else:
                                print(f"Failed to submit task to {ip_address}. Requeuing task.")
                                TASK_QUEUE.appendleft(task)  # Requeue the task on failure

        # Check for completed tasks and free up nodes
        for ip_address in list(active_tasks.keys()):
            node_url = f"http://{ip_address}"
            output = get_task_output(node_url)

            if output:
                print(f"Output from {ip_address}: {output}")
                del active_tasks[ip_address]  # Remove from active tasks, node is now free

        # Wait for a short interval before polling again to avoid busy-waiting
        time.sleep(POLL_INTERVAL)

def retrieve_outputs():
    """Retrieve task outputs from available nodes."""
    print("Listening for node broadcasts...")
    listen_for_nodes()

    for ip_address, node_info in AVAILABLE_NODES.items():
        node_url = f"http://{ip_address}"

        # Check for task output
        output = get_task_output(node_url)
        if output:
            print(f"Output from {node_url}: {output}")
        else:
            print(f"No output available yet from {node_url}")

if __name__ == "__main__":
    # Enriching the description with more details and examples
    parser = argparse.ArgumentParser(
        description="""
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
""",
        formatter_class=argparse.RawTextHelpFormatter  # Ensures text formatting is preserved
    )

    # Mandatory binary file (e.g., task.bin)
    parser.add_argument(
        "-b", "--binary", 
        type=str, 
        help="Path to the binary file to submit (e.g., task.elf).", 
        required=False
    )

    # Optional payload files (e.g., .csv, .txt, etc.)
    parser.add_argument(
        "-f", "--payloads", 
        nargs='+', 
        help="List of payload files to submit (e.g., logfile.csv). Supports multiple files.", 
        default=[]
    )

    # Optional arguments (e.g., MAC addresses, configuration parameters)
    parser.add_argument(
        "-a", "--arguments", 
        nargs='+', 
        help="List of additional arguments (e.g., MAC address, config values). Supports multiple values.", 
        default=[]
    )

    # Retrieve outputs
    parser.add_argument(
        "-r", "--retrieve", 
        action="store_true", 
        help="Retrieve task outputs from nodes."
    )

    # Listen for nodes only (new option)
    parser.add_argument(
        "-l", "--listen", 
        action="store_true", 
        help="Only listen for nodes and report their status."
    )

    args = parser.parse_args()

    # Check if we are only listening for nodes
    if args.listen:
        manage_nodes()
    elif args.binary:
        queue_tasks(args.binary, args.payloads, args.arguments)
    elif args.retrieve:
        retrieve_outputs()
    else:
        print("No valid options provided. Use -b for submitting tasks, -r for retrieving outputs, or -l for listening to nodes.")
