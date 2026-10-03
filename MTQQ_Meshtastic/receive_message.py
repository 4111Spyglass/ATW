from MqttMshMqtt import MqttMshReceiver

HELTEC_ID = 0x8FA0DE68
HELTEC_PUBKEY = "aLMcPAE65mZeQz+Bv37gl/6V11Zc0Ijj08/0apQJEm8="
TEST_TOPIC = "msh/US/TX"

receiver = MqttMshReceiver(root_topic=TEST_TOPIC)
receiver.register_peer_key(HELTEC_ID, HELTEC_PUBKEY)

# =====================================================================
# Usage Examples
# =====================================================================
if __name__ == "__main__":
    import sys

    receiver = MqttMshReceiver(root_topic=TEST_TOPIC)
    receiver.register_peer_key(HELTEC_ID, HELTEC_PUBKEY)
    receiver.start_listening()