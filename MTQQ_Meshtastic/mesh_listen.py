import sys
import time
import argparse
import datetime
import meshtastic
import meshtastic.tcp_interface
from pubsub import pub


def parse_arguments():
    parser = argparse.ArgumentParser(description="Resilient Native Meshtastic Packet Stream Listener")
    parser.add_argument(
        "--host",
        type=str,
        required=True,
        help="The IP address or hostname of the target Meshtastic node"
    )
    return parser.parse_args()


class MeshtasticSupervisor:
    def __init__(self, host):
        self.host = host
        self.grid_connection = None
        self.last_packet_time = time.time()

        # Register the payload parser callback globally
        pub.subscribe(self.on_receive_packet, "meshtastic.receive")

    def connect(self):
        """Attempts to open a clean interface socket to the Heltec node."""
        self.disconnect()
        try:
            print(f"Connecting to node at {self.host}...")
            self.grid_connection = meshtastic.tcp_interface.TCPInterface(hostname=self.host)
            print(f"Connected successfully! Listening for telemetry...\n")

            # Print the data grid headers
            print(
                f"{'TIMESTAMP':<19} | {'SENDER ID':<12} | {'RECEIVER ID':<12} | {'MSG TYPE':<20} | {'HOPS':<4} | {'INTERFACE':<10} | {'SNR':<6} | {'RSSI':<6}")
            print("-" * 111)
            sys.stdout.flush()

            # Reset heartbeat timer upon fresh handshake link
            self.last_packet_time = time.time()
            return True
        except Exception as e:
            print(f"Connection failed: {e}. Retrying shortly...", file=sys.stderr)
            return False

    def disconnect(self):
        """Cleans up the existing connection objects safely."""
        if self.grid_connection:
            try:
                self.grid_connection.close()
            except Exception:
                pass
            self.grid_connection = None

    def on_receive_packet(self, packet, interface=None):
        """Processes incoming payloads and tracks the network heartbeat."""
        try:
            # Update the alive tracker whenever ANY internal/external packet arrives
            self.last_packet_time = time.time()

            sender = packet.get("fromId", "Unknown")
            receiver = packet.get("toId", "Unknown")
            via_mqtt = packet.get("viaMqtt", False)

            via_lora = "rxSnr" in packet or "rxRssi" in packet
            rx_snr = str(packet.get("rxSnr", "N/A"))
            rx_rssi = str(packet.get("rxRssi", "N/A"))

            hop_start = packet.get("hopStart")
            hop_limit = packet.get("hopLimit")

            if hop_start is not None and hop_limit is not None:
                hops_str = str(max(0, hop_start - hop_limit))
            elif via_lora:
                hops_str = "0"
            else:
                hops_str = "N/A"

            decoded = packet.get("decoded", {})
            msg_type = decoded.get("portnum", "UNKNOWN_APP")
            interface_str = "mqtt" if via_mqtt else "lora" if via_lora else "internal"

            unix_time = packet.get("rxTime") or int(time.time())
            time_str = datetime.datetime.fromtimestamp(unix_time).strftime('%Y-%m-%d %H:%M:%S')

            print(
                f"{time_str:<19} | {sender:<12} | {receiver:<12} | {msg_type:<20} | {hops_str:<4} | {interface_str:<10} | {rx_snr:<6} | {rx_rssi:<6}")
            sys.stdout.flush()

        except Exception:
            pass

    def run_supervisor_loop(self):
        """Monitors script link status and self-heals dropped connections."""
        if not self.connect():
            # Initial retry cooldown fallback delay pacing
            time.sleep(5)

        while True:
            try:
                time.sleep(2)

                # Check link integrity. Since your node returns an internal heartbeat telemetry
                # packet every 60 seconds, missing updates for > 75 seconds confirms a lost port connection.
                if time.time() - self.last_packet_time > 75:
                    print(f"\n⚠️ Link timeout detected (Port hijacked or dropped). Recovering connection...",
                          file=sys.stderr)
                    sys.stdout.flush()

                    # Tear down and rebuild interface socket configurations
                    if self.connect():
                        continue
                    time.sleep(8)

            except KeyboardInterrupt:
                print("\nExiting native telemetry stream supervisor.")
                self.disconnect()
                sys.exit(0)
            except Exception as e:
                time.sleep(5)


if __name__ == "__main__":
    args = parse_arguments()
    supervisor = MeshtasticSupervisor(host=args.host)
    supervisor.run_supervisor_loop()
