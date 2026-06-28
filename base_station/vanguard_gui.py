# ==============================================================================
# PROJECT VANGUARD - BASE STATION GUI (v2.0 - TACTICAL EDITION)
# Vision subsystem routed to Chrome. This GUI acts purely as the command, 
# control, and telemetry hub. Lightweight, multi-threaded, and zero-latency.
# ==============================================================================

import customtkinter as ctk
import threading
import time
import math
import socket
import datetime
import os

# --- Setup Modern Theme ---
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("dark-blue") 

class VanguardCockpit(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Project VANGUARD - BASE STATION (Telemetry & Control)")
        self.geometry("1400x900")
        self.obstacle_history = []

        # --- State Variables ---
        self.running = True               
        self.current_drive_cmd = None     
        self.obstacle_graphic = None      
        self.sound_arrow = None           
        self.arrow_timer = None

        # --- Radar Geometry Settings ---
        self.cw = 500       # Canvas Width
        self.ch = 500       # Canvas Height
        self.cx = self.cw/2 # Center X
        self.cy = self.ch/2 # Center Y
        self.max_r = 220    # Max Radar Radius

        # --- Main Grid Layout ---
        self.grid_columnconfigure(0, weight=4) 
        self.grid_columnconfigure(1, weight=3)
        self.grid_rowconfigure(0, weight=1)

        # ==========================================
        # LEFT PANEL: TACTICAL RADAR & TELEMETRY
        # ==========================================
        self.left_panel = ctk.CTkFrame(self, fg_color="#121212", corner_radius=15)
        self.left_panel.grid(row=0, column=0, padx=15, pady=15, sticky="nsew")
        self.left_panel.grid_rowconfigure(0, weight=3) 
        self.left_panel.grid_rowconfigure(1, weight=1) 

        # 1. Minimap (Sound & ToF)
        self.map_label_title = ctk.CTkLabel(self.left_panel, text="LIDAR & ACOUSTIC RADAR", font=("Consolas", 20, "bold"), text_color="#00a8e8")
        self.map_label_title.grid(row=0, column=0, pady=(20, 0), sticky="n")
        
        self.radar_canvas = ctk.CTkCanvas(self.left_panel, width=self.cw, height=self.ch, bg="#0a0a0a", highlightthickness=2, highlightbackground="#00a8e8")
        self.radar_canvas.grid(row=0, column=0, pady=(60, 20))
        self.draw_radar_grid() 

        # 2. Live Telemetry Console
        self.console_frame = ctk.CTkFrame(self.left_panel, fg_color="#1a1a1a", corner_radius=10)
        self.console_frame.grid(row=1, column=0, padx=20, pady=20, sticky="nsew")
        self.console_frame.grid_columnconfigure(0, weight=1)
        self.console_frame.grid_rowconfigure(1, weight=1)

        ctk.CTkLabel(self.console_frame, text="LIVE SYSTEM TERMINAL", font=("Consolas", 14, "bold"), text_color="#707070").grid(row=0, column=0, pady=(10,0), sticky="w", padx=15)
        
        self.terminal = ctk.CTkTextbox(self.console_frame, fg_color="#0d0d0d", text_color="#00ff00", font=("Consolas", 13), wrap="word")
        self.terminal.grid(row=1, column=0, padx=15, pady=15, sticky="nsew")
        self.terminal.insert("end", ">> VANGUARD COMMAND SYSTEM INITIALIZED.\n")
        self.terminal.configure(state="disabled") # Prevent user typing

        # ==========================================
        # RIGHT PANEL: ACTUATOR CONTROLS
        # ==========================================
        self.right_panel = ctk.CTkFrame(self, fg_color="#121212", corner_radius=15)
        self.right_panel.grid(row=0, column=1, padx=15, pady=15, sticky="nsew")
        self.right_panel.grid_columnconfigure(0, weight=1)

        # --- 1. Robotic Arm Controls ---
        self.arm_frame = ctk.CTkFrame(self.right_panel, fg_color="#1e1e1e", corner_radius=10)
        self.arm_frame.grid(row=0, column=0, padx=20, pady=20, sticky="ew")
        
        ctk.CTkLabel(self.arm_frame, text="ROBOTIC ARM MANIPULATORS", font=("Consolas", 18, "bold"), text_color="#ff9f1c").pack(pady=15)
        
        self.create_slider(self.arm_frame, "Shoulder Joint")
        self.create_slider(self.arm_frame, "Elbow Joint")
        self.create_slider(self.arm_frame, "Claw Actuation") 

        # # --- 2. Flipper Controls ---
        # self.flipper_frame = ctk.CTkFrame(self.right_panel, fg_color="#1e1e1e", corner_radius=10)
        # self.flipper_frame.grid(row=1, column=0, padx=20, pady=20, sticky="ew")
        
        # ctk.CTkLabel(self.flipper_frame, text="FLIPPER SYNC SYSTEM", font=("Consolas", 18, "bold"), text_color="#ff9f1c").pack(pady=15)
        # self.create_slider(self.flipper_frame, "Flipper Angle")

        # --- 3. D-Pad Drive Controls ---
        self.drive_frame = ctk.CTkFrame(self.right_panel, fg_color="#1e1e1e", corner_radius=10)
        self.drive_frame.grid(row=2, column=0, padx=20, pady=20, sticky="ew")
        self.drive_frame.grid_columnconfigure((0,1,2), weight=1)
        
        ctk.CTkLabel(self.drive_frame, text="CHASSIS DRIVE SYSTEM", font=("Consolas", 18, "bold"), text_color="#e71d36").grid(row=0, column=0, columnspan=3, pady=20)
        
        btn_kwargs = {"width": 110, "height": 70, "font": ("Consolas", 18, "bold"), "corner_radius": 10}
        
        ctk.CTkButton(self.drive_frame, text="FWD (W)", command=lambda: self.send_cmd("DRIVE_FWD"), **btn_kwargs).grid(row=1, column=1, pady=10)
        ctk.CTkButton(self.drive_frame, text="LEFT (A)", command=lambda: self.send_cmd("DRIVE_LEFT"), **btn_kwargs).grid(row=2, column=0, padx=10)
        ctk.CTkButton(self.drive_frame, text="STOP", fg_color="#e71d36", hover_color="#bd172c", command=lambda: self.send_cmd("DRIVE_STOP"), **btn_kwargs).grid(row=2, column=1, padx=10)
        ctk.CTkButton(self.drive_frame, text="RIGHT (D)", command=lambda: self.send_cmd("DRIVE_RIGHT"), **btn_kwargs).grid(row=2, column=2, padx=10)
        ctk.CTkButton(self.drive_frame, text="REV (S)", command=lambda: self.send_cmd("DRIVE_BACK"), **btn_kwargs).grid(row=3, column=1, pady=10)

        # ==========================================
        # NETWORK AND THREADING SETUP
        # ==========================================
        
        # --- UDP Server Setup for Listening (Receiving data FROM the robot) ---
        self.udp_ip = "0.0.0.0" 
        self.udp_port = 5005
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.udp_ip, self.udp_port))
        
        self.udp_thread = threading.Thread(target=self.listen_for_telemetry, daemon=True)
        self.udp_thread.start()

        # --- UDP Setup for Sending (Sending WASD commands TO the robot) ---
        self.esp32_ip = "10.133.166.214" # <--- IMPORTANT: Update to Node 2 (WROOM) IP Address
        self.cmd_port = 4210
        self.cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        self.setup_keyboard_bindings()


    # ==========================================
    # TELEMETRY AND MAPPING FUNCTIONS
    # ==========================================

    def log_terminal(self, message):
        """ Prints messages to the GUI terminal with a timestamp """
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        self.terminal.configure(state="normal")
        self.terminal.insert("end", f"[{ts}] {message}\n")
        self.terminal.see("end") # Auto-scroll to bottom
        self.terminal.configure(state="disabled")

    def listen_for_telemetry(self):
        """ THREAD 1: Background thread that constantly listens for UDP sensor data """
        while self.running:
            try:
                data, addr = self.sock.recvfrom(1024) 
                message = data.decode('utf-8').strip()
                if message == "VOICE":

                    self.after(
                        0,
                        self.log_terminal,
                        "POSSIBLE HUMAN VOICE DETECTED"
                    )

                    self.after(
                        0,
                        self.write_mission_log,
                        "VOICE DETECTED"
                    ) 
                if message.startswith("GAS:"):

                    gasValue = int(
                        message.replace(
                            "GAS:",
                            ""
                        )
                    )

                    self.after(
                        0,
                        self.log_terminal,
                        f"GAS ALERT -> {gasValue}"
                    )

                    self.after(
                        0,
                        self.write_mission_log,
                        f"GAS ALERT -> {gasValue}"
                    )

                # If it's sound data...
                if message.startswith("TDOA:"):
                    values = message.replace("TDOA:", "").split(',')
                    if len(values) == 2:
                        delay12 = int(values[0])
                        delay13 = int(values[1])
                        self.after(0, self.update_minimap_arrow, delay12, delay13)
                        self.after(0, self.log_terminal, f"ACOUSTIC DETECTED -> Vector: {delay12}, {delay13}")

                # If it's LiDAR data...
                if message.startswith("MAP:"):
                    parts = message.replace("MAP:", "").split(',')
                    if len(parts) == 2:
                        distance_cm = int(parts[0])
                        obs_type = parts[1]
                        print(f"RAW ToF DISTANCE: {distance_cm} cm")    

                        self.after(0, self.update_minimap_obstacle, distance_cm, obs_type)
                        
            except Exception as e:
                pass 


    def update_minimap_obstacle(self, distance_cm, obs_type):
        """ Draws the detected obstacle on the radar canvas """
        self.obstacle_history.append(
            (
                distance_cm,
                obs_type
            )
        )
        
        pixel_dist = (distance_cm / 150.0) * self.max_r
        draw_y = self.cy - pixel_dist
        
        color = "#e71d36" if distance_cm < 50 else "#ff9f1c"

        if obs_type == "CENTER":
            self.obstacle_graphic = self.radar_canvas.create_rectangle(
                self.cx - 20, draw_y - 8, self.cx + 20, draw_y + 8, fill=color, outline=color)
        elif obs_type == "PERIPHERAL":
            self.obstacle_graphic = self.radar_canvas.create_arc(
                self.cx - 60, draw_y - 30, self.cx + 60, draw_y + 30, start=45, extent=90, style=ctk.ARC, outline=color, width=5)
        for distance, typ in self.obstacle_history:

            pixel_dist = (
                distance / 150.0
            ) * self.max_r

            draw_y = (
                self.cy - pixel_dist
            )

            self.radar_canvas.create_oval(
                self.cx - 2,
                draw_y - 2,
                self.cx + 2,
                draw_y + 2,
                fill="#00ff00",
                outline=""
            )

    def update_minimap_arrow(self, d12, d13):
        """ Converts the sample delays into a 2D vector and draws the sound arrow """
        arrow_length = 130
        
        angle_rad = math.atan2(d13, d12)
        end_x = self.cx + (arrow_length * math.cos(angle_rad))
        end_y = self.cy - (arrow_length * math.sin(angle_rad)) 
        
        if self.sound_arrow:
            self.radar_canvas.delete(self.sound_arrow)
            
        self.sound_arrow = self.radar_canvas.create_line(
            self.cx, self.cy, end_x, end_y, fill="#ff3333", width=5, arrow=ctk.LAST)
            
        if self.arrow_timer:
            self.after_cancel(self.arrow_timer)
            
        self.arrow_timer = self.after(2000, self.clear_arrow)

    def clear_arrow(self):
        """ Deletes the arrow from the minimap when the sound is old """
        if self.sound_arrow:
            self.radar_canvas.delete(self.sound_arrow)
            self.sound_arrow = None
        

    # ==========================================
    # HELPER UI FUNCTIONS
    # ==========================================

    def create_slider(self, parent, label_text):
        """ A reusable function to generate nice looking sliders """
        frame = ctk.CTkFrame(parent, fg_color="transparent")
        frame.pack(fill="x", padx=15, pady=12)
        ctk.CTkLabel(frame, text=label_text, width=150, anchor="w", font=("Consolas", 15)).pack(side="left")
        
        cmd_prefix = label_text.split()[0].upper()
        
        slider = ctk.CTkSlider(frame, from_=0, to=180, number_of_steps=180, 
                               button_color="#00a8e8", button_hover_color="#007ea7", height=20,
                               command=lambda val, p=cmd_prefix: self.send_cmd(f"{p}:{int(val)}"))
        slider.set(90)
        slider.pack(side="left", fill="x", expand=True, padx=15)


    def draw_radar_grid(self):
        """ Draws the static green grid lines on the minimap """
        self.radar_canvas.delete("all")
        r = self.max_r
        
        for radius in [r, r*0.75, r*0.5, r*0.25]:
            self.radar_canvas.create_oval(self.cx-radius, self.cy-radius, self.cx+radius, self.cy+radius, outline="#003f5c", width=2)
            
        self.radar_canvas.create_line(self.cx, self.cy-r, self.cx, self.cy+r, fill="#003f5c", dash=(4, 4))
        self.radar_canvas.create_line(self.cx-r, self.cy, self.cx+r, self.cy, fill="#003f5c", dash=(4, 4))
        
        self.radar_canvas.create_rectangle(self.cx-12, self.cy-18, self.cx+12, self.cy+18, fill="#00a8e8", outline="#ffffff")


    # ==========================================
    # INPUT & COMMAND SENDING FUNCTIONS
    # ==========================================

    def setup_keyboard_bindings(self):
        """ Tells the window to listen for keyboard buttons """
        self.bind('<KeyPress-w>', lambda e: self.handle_drive("DRIVE_FWD"))
        self.bind('<KeyPress-s>', lambda e: self.handle_drive("DRIVE_BACK"))
        self.bind('<KeyPress-a>', lambda e: self.handle_drive("DRIVE_LEFT"))
        self.bind('<KeyPress-d>', lambda e: self.handle_drive("DRIVE_RIGHT"))
        
        self.bind('<KeyRelease-w>', lambda e: self.handle_drive("DRIVE_STOP"))
        self.bind('<KeyRelease-s>', lambda e: self.handle_drive("DRIVE_STOP"))
        self.bind('<KeyRelease-a>', lambda e: self.handle_drive("DRIVE_STOP"))
        self.bind('<KeyRelease-d>', lambda e: self.handle_drive("DRIVE_STOP"))


    def handle_drive(self, new_cmd):
        """ Ensures we only send the command over WiFi ONCE when it changes. """
        if self.current_drive_cmd != new_cmd:
            self.current_drive_cmd = new_cmd
            self.send_cmd(new_cmd)


    def send_cmd(self, command):
        """ Packages text commands into UDP packets and fires them over WiFi """
        # Log it to the terminal instead of the hidden python console
        self.log_terminal(f"TX >> {command}")
        try:
            self.cmd_sock.sendto(command.encode('utf-8'), (self.esp32_ip, self.cmd_port))
        except Exception as e:
            self.log_terminal(f"ERROR: Failed to send -> {e}")


    def on_closing(self):
        """ Safely shuts down the background threads """
        self.running = False
        self.destroy()

    def write_mission_log(self, event):

        with open(
            "mission_log.txt",
            "a"
        ) as file:

            timestamp = datetime.datetime.now()

            file.write(
                f"{timestamp} : {event}\n"
            )

if __name__ == "__main__":
    app = VanguardCockpit()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()