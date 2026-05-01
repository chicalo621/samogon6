#!/usr/bin/env python3
#py .\mqtt_ota.py --broker vmi516392.contaboserver.net --port 9001 --user samovar --pass ATxRDyJ3R3ad --topic "380673545661/1/sam/cmd" --pub-topic "380673545661/1/sam/data" --file .\samogon.ino.bin --delay 0.1 --wait-final 180

import paho.mqtt.client as mqtt
import time
import argparse
import os
import sys

status = None
progress = None
pub_status_topic = None

def on_connect(client, userdata, flags, rc):
    print("Connected to broker, rc=", rc)
    if pub_status_topic:
        client.subscribe(pub_status_topic + "/ota/status", qos=1)
        client.subscribe(pub_status_topic + "/ota/progress", qos=0)

def on_message(client, userdata, msg):
    global status, progress
    try:
        s = msg.payload.decode('utf-8', errors='ignore')
    except:
        s = str(msg.payload)
    print(f"[MQTT RX] {msg.topic} -> {s}")
    if msg.topic.endswith("/ota/status"):
        status = s
    elif msg.topic.endswith("/ota/progress"):
        try:
            progress = int(s)
        except:
            progress = s

def publish_with_retry(client, topic, payload, qos, retries=3, wait=0.2):
    for attempt in range(1, retries+1):
        info = client.publish(topic, payload, qos=qos)
        if info.rc == mqtt.MQTT_ERR_SUCCESS:
            return True
        print(f"Publish failed (rc={info.rc}), attempt {attempt}/{retries})")
        time.sleep(wait)
    return False

def main():
    global pub_status_topic
    parser = argparse.ArgumentParser(description="MQTT OTA uploader (chunked)")
    parser.add_argument("--broker", required=True, help="MQTT broker address")
    parser.add_argument("--port", type=int, default=1883, help="MQTT port")
    parser.add_argument("--file", required=True, help="Path to firmware .bin")
    parser.add_argument("--topic", required=True, help="Base sub_topic (device listens on {topic}/ota/...)")
    parser.add_argument("--pub-topic", default=None, help="Base pub_topic to read status/progress")
    parser.add_argument("--user", default=None, help="MQTT username (optional)")
    parser.add_argument("--pass", dest="password", default=None, help="MQTT password (optional)")
    parser.add_argument("--chunk", type=int, default=4096, help="Chunk size in bytes (<=4096)")
    parser.add_argument("--delay", type=float, default=0.05, help="Delay between chunks (seconds)")
    parser.add_argument("--wait-start", type=int, default=5, help="Seconds to wait for 'started' status after begin")
    parser.add_argument("--wait-final", type=int, default=30, help="Seconds to wait for final status after end")
    parser.add_argument("--ws", action="store_true", help="Use WebSocket transport")
    args = parser.parse_args()

    broker = args.broker
    port = args.port
    firmware_path = args.file
    sub_topic = args.topic.rstrip('/')
    pub_status_topic = args.pub_topic.rstrip('/') if args.pub_topic else sub_topic
    username = args.user
    password = args.password
    chunk_size = args.chunk
    delay_between = args.delay
    use_ws = args.ws

    if chunk_size > 4096:
        print("Warning: chunk_size > 4096 may be incompatible with device.")
    if not os.path.isfile(firmware_path):
        print("Firmware file not found:", firmware_path)
        sys.exit(2)

    data = open(firmware_path, "rb").read()
    total = len(data)
    print(f"Firmware: {firmware_path} ({total} bytes)")

    client = mqtt.Client(transport="websockets") if use_ws else mqtt.Client()
    if username:
        client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(broker, port, keepalive=60)
    except Exception as e:
        print("Cannot connect to broker:", e)
        sys.exit(3)

    client.loop_start()

    begin_topic = f"{sub_topic}/ota/begin"
    if not publish_with_retry(client, begin_topic, str(total), qos=1):
        print("Failed to publish begin"); client.loop_stop(); client.disconnect(); sys.exit(4)

    waited = 0.0
    while waited < args.wait_start:
        if status and "started" in str(status).lower():
            print("Device reported started"); break
        time.sleep(0.1); waited += 0.1

    data_topic = f"{sub_topic}/ota/data"
    for offset in range(0, total, chunk_size):
        chunk = data[offset: offset + chunk_size]
        if not publish_with_retry(client, data_topic, chunk, qos=0, retries=2):
            print("Failed to publish chunk at offset", offset); client.loop_stop(); client.disconnect(); sys.exit(5)
        sent = min(offset + len(chunk), total)
        print(f"Sent {sent}/{total} ({sent*100//total}%)")
        time.sleep(delay_between)

    end_topic = f"{sub_topic}/ota/end"
    if not publish_with_retry(client, end_topic, "done", qos=1):
        print("Failed to publish end"); client.loop_stop(); client.disconnect(); sys.exit(6)

    final_status = None; waited = 0.0
    print("Waiting for final status...")
    while waited < args.wait_final:
        if status:
            final_status = status
            if "success" in str(final_status).lower() or "error" in str(final_status).lower():
                break
        time.sleep(0.5); waited += 0.5

    client.loop_stop(); client.disconnect()
    print("Final status:", final_status)
    sys.exit(0 if final_status and "success" in str(final_status).lower() else 7)

if __name__ == "__main__":
    main()