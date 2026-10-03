import argparse
from MqttMshMqtt import MqttMshSender

HELTEC_ID = 0x8FA0DE68
HELTEC_PUBKEY = "aLMcPAE65mZeQz+Bv37gl/6V11Zc0Ijj08/0apQJEm8="
TEST_TOPIC = "msh/US/TX"

sender = MqttMshSender(root_topic=TEST_TOPIC)
sender.register_peer_key(HELTEC_ID, HELTEC_PUBKEY)

# =====================================================================
# Usage Examples
# =====================================================================
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send a direct Meshtastic message over MQTT.")
    parser.add_argument(
        "-m", "--message",
        type=str,
        default="Ping",
        help="The public message text to send (default: 'Ping')"
    )

    args = parser.parse_args()

    print(f"Sending: '{args.message}'")

    sender.send_broadcast(args.message)
