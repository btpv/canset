
from datetime import datetime
import json
import random
from time import time
import tkinter as tk
from tkinter import ttk
from matplotlib.axes import Axes
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import serial
import sys
from collections import deque

try:
    with open(sys.argv[1]) as f:
        logfilein = f.readlines()
    serialMode = False
except:
    serialMode = True

with open("config.json") as f:
    configjson = json.loads(f.read())
    serial_port = configjson["serial_port"]
    baud_rate = configjson["baud_rate"]
    sender_id = configjson["sender_id"]
    graphItems: list[dict[str, str | list[str]]] = configjson["graphs"]


class GraphItem:
    def __init__(self, canvas_frame: tk.Frame, name: str, item_pos: int, jsonkey: list[str], max_size: int = 1000, label: list[str] = [""], color: list[str] = ["black"], unit: str = ""):
        self.canvas_frame = canvas_frame
        self.max_size = max_size
        self.name = name
        self.jsonkey = jsonkey
        self.label = label
        self.color = color
        self.items = len(jsonkey)
        self.data: list[deque[tuple[float, float]]] = [deque(maxlen=self.max_size) for _ in range(self.items)]
        self.fig = Figure(figsize=(4.75, 3), dpi=100)
        self.ax: Axes = self.fig.add_subplot(111)
        self.ax.set_xlim(0, max_size)
        self.ax.set_ylabel(unit)
        self.ax.set_title(name)
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.canvas_frame)
        self.canvas.get_tk_widget().grid(row=item_pos // 4, column=item_pos % 4)

    def push(self, jsondata: dict[str, float | int | str]):
        for i, key in enumerate(self.jsonkey):
            if key in jsondata:
                try:
                    value = jsondata[key]
                    timestamp = jsondata.get("TS", time())
                    self.data[i].append((timestamp, value))
                except ValueError:
                    print(f"Invalid data format for {key}: {jsondata[key]}")

    def draw(self):
        self.ax.clear()
        for i, data in enumerate(self.data):
            self.ax.plot([j[0] for j in data], [j[1] for j in data], label=self.label[i], color=self.color[i])
        self.ax.legend()
        self.canvas.draw()

class GraphApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("canset")
        self.root.geometry("{0}x{1}+0+0".format(root.winfo_screenwidth(), root.winfo_screenheight()))
        self.send_data = {}

        self.canvas_frame = tk.Frame(root)
        self.canvas_frame.pack()
        if serialMode:
            self.serial = serial.Serial(serial_port, baud_rate, timeout=1)
        self.items: list[GraphItem] = [GraphItem(self.canvas_frame, item_pos=i, **v) for i, v in enumerate(graphItems)]
        
        self.table_frame = tk.Frame(root,width=50)
        self.table_frame.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.tree = ttk.Treeview(self.table_frame, columns=("Key", "Value"), show="headings")
        self.tree.heading("Key", text="Key")
        self.tree.heading("Value", text="Value")
        self.tree.pack(fill=tk.BOTH, expand=True)
        
        self.text_input = tk.Entry(root, width=200)
        self.text_input.pack()
        self.text_input.bind("<Delete>", lambda x: self.text_input.delete(0, tk.END))
        self.text_input.bind("<Return>", self.update_message)

        self.msg_listbox = tk.Listbox(root, height=10, width=200)
        self.msg_listbox.pack()
        
        self.json_listbox = tk.Listbox(root, height=10, width=200)
        self.json_listbox.pack(side=tk.LEFT, fill=tk.BOTH)
        
        
        
        self.lastmillis = 0
        self.update()
        if serialMode:
            self.read_serial()
            # self.send_serial()
        else:
            self.read_file(int(sys.argv[2]) if len(sys.argv) > 2 else 0)

    def update_message(self, event=None) -> None:
        self.send_data["MSG"] = self.text_input.get()

    def parse_input(self, raw_data) -> bool:
        try:
            self.json_data: dict = json.loads(raw_data)
            self.json_listbox.insert(0, str(self.json_data))
            if self.json_data["senderID"] != 0:
                return True
            elif "ogSenderID" in self.json_data and self.json_data['ogSenderID'] == sender_id:
                if "MSG" in self.json_data:
                    self.msg_listbox.insert(0, f"<<<< {self.json_data['MSG']}")
                self.send_data = {}
            for item in self.items:
                item.push(self.json_data)
            # update right list
            self.tree.delete(*self.tree.get_children())
            for key, value in self.json_data.items():
                self.tree.insert("", tk.END, values=(key, value))
                
            if "MSG" in self.json_data and "ogSenderID" in self.json_data and self.json_data['ogSenderID'] != sender_id:
                self.msg_listbox.insert(0, f">{self.json_data['ogSenderID']:03d}> {self.json_data['MSG']}")
                print(self.json_data["MSG"])
            return True
        except json.JSONDecodeError:
            print(f"Invalid JSON data received: {raw_data}")
            return False

    def read_file(self, delay=0):
        if len(logfilein) == 0:
            return
        self.parse_input(logfilein[0])
        logfilein.pop(0)
        self.root.after(delay, self.read_file, (delay,))

    def send_serial(self):
        # self.root.after(500, self.send_serial)
        if self.send_data == {}:
            return
        self.send_data["senderID"] = sender_id
        if self.serial.in_waiting > 0:
            return
        self.serial.write((json.dumps(self.send_data) + "\n").encode())
        print(json.dumps(self.send_data))
        # self.send_data = {}

    def read_serial(self):
        self.root.after(50, self.read_serial)
        if self.serial.in_waiting == 0:
            return
        raw_data = self.serial.readline().decode('utf-8', errors='ignore')
        logfile.write(raw_data)
        self.parse_input(raw_data)
        self.root.after(random.randint(5,30), self.send_serial)

    def update(self):
        for item in self.items:
            item.draw()
        self.root.after(500, self.update)

if __name__ == "__main__":
    try:
        if serialMode:
            logfile = open(f"./{datetime.now().strftime('%Y-%m-%d--%H-%M-%S')}-cansat.log", "w")
        root = tk.Tk()
        app = GraphApp(root)
        root.mainloop()
    except Exception as e:
        print(f"Error: {e}")
        if serialMode:
            logfile.close()
