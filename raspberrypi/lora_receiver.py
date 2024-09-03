import time
from SX127x.LoRa import *
from SX127x.board_config import BOARD
from datetime import datetime
from flask import Flask, jsonify, render_template_string
import threading
import paho.mqtt.client as mqtt

# Initialize the board (Raspberry Pi) settings
BOARD.setup()

# Flask app setup
app = Flask(__name__)

class LoRaRcvCont(LoRa):
    def __init__(self, verbose=False, mqtt_enabled=False):
        super(LoRaRcvCont, self).__init__(verbose)
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)
        self.latest_data = {
            'battery_voltage': 0,
            'temperature': 0,
            'distance': 0
        }
        self.mqtt_enabled = mqtt_enabled
        if self.mqtt_enabled:
            self.mqtt_client = mqtt.Client()
            self.mqtt_client.connect("192.168.0.1", 8883, 60)

    def start(self):
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)
        while True:
            if self.get_irq_flags()['rx_done']:
                self.clear_irq_flags(RxDone=1)
                payload = self.read_payload(nocheck=True)
                self.process_payload(payload)
            time.sleep(0.5)

    def process_payload(self, payload):
        if len(payload) < 4:
            print("Received incomplete message, ignoring...")
            return

        try:
            receiver_addr = payload[0]
            sender_addr = payload[1]
            message_length = payload[2]
            message = payload[3:3 + message_length]
            message_text = bytes(message).decode('utf-8', 'ignore')
            splitmessage = message_text.split(':')

            vBat = (1.1/1024) * float(splitmessage[3]) + (1.1/1024) * float(splitmessage[3]) * 6.6
            vBat = round(vBat, 2)
            realTemp = float(splitmessage[1]) * 0.0078125
            distance = float(splitmessage[5])

            # Store the latest data
            self.latest_data = {
                'battery_voltage': vBat,
                'temperature': realTemp,
                'distance': distance
            }

            # Optionally publish to MQTT
            if self.mqtt_enabled:
                self.mqtt_client.publish("lora/data", str(self.latest_data))
            
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print(f"[{timestamp}|{sender_addr:02X}|{receiver_addr:02X}|{message_length}] {message_text}")
            print(f"Battery voltage: {vBat}V")
            print(f"Temperature: {realTemp}°C")
            print(f"Distance: {distance}mm")
        except:
            print(f"An error occured while parsing the message")

# Flask routes
@app.route('/api/latest-data', methods=['GET'])
def get_latest_data():
    return jsonify(lora.latest_data)

@app.route('/')
def index():
    html_template = """
    <!DOCTYPE html>
    <html>
    <head><title>LoRa Receiver Data</title></head>
    <body>
        <h1>Latest LoRa Data</h1>
        <p><strong>Battery Voltage:</strong> {{ data.battery_voltage }} V</p>
        <p><strong>Temperature:</strong> {{ data.temperature }} °C</p>
        <p><strong>Distance:</strong> {{ data.distance }} mm</p>
    </body>
    </html>
    """
    return render_template_string(html_template, data=lora.latest_data)

# Start the Flask app in a separate thread
def start_flask():
    app.run(host='0.0.0.0', port=5000)

if __name__ == '__main__':
    # Create LoRa object
    lora = LoRaRcvCont(verbose=False, mqtt_enabled=False)
    lora.set_pa_config(pa_select=1)
    lora.set_rx_crc(True)
    lora.set_freq(433.0)
    lora.set_spreading_factor(7)
    lora.set_bw(BW.BW125)
    lora.set_coding_rate(CODING_RATE.CR4_5)
    lora.set_preamble(8)

    # Start Flask in a new thread
    threading.Thread(target=start_flask).start()

    try:
        print("Starting LoRa Receiver...")
        lora.start()
    except KeyboardInterrupt:
        print("Receiver interrupted by user")
    finally:
        lora.set_mode(MODE.SLEEP)
        BOARD.teardown()
