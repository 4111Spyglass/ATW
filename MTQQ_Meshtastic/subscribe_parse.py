import base64
import traceback

import paho.mqtt.client as mqtt

from meshtastic.protobuf import mqtt_pb2
from meshtastic.protobuf import mesh_pb2
from meshtastic.protobuf import portnums_pb2
from meshtastic.protobuf import telemetry_pb2

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend


# ---------------------------------------------------------------------
# LongFast Public Key
# In Meshtastic, PSK "AQ==" (0x01) maps to this default 128-bit AES key
# ---------------------------------------------------------------------

PUBLIC_KEY = bytes.fromhex("d4f1bb3a20290759f0bcffabcf4e6901")


# ---------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------

def get_protobuf_field(proto_msg, field_name):
    """
    Safely extract protobuf fields such as 'from' which collide with
    Python keywords.
    """
    for field_descriptor, value in proto_msg.ListFields():
        if field_descriptor.name == field_name:
            return value

    return 0


def decrypt_packet(packet, key, sender_id):
    """
    Attempt Meshtastic AES-CTR decryption.
    """
    try:
        packet_id = packet.id

        nonce = (
            packet_id.to_bytes(8, "little")
            + sender_id.to_bytes(8, "little")
        )

        cipher = Cipher(
            algorithms.AES(key),
            modes.CTR(nonce),
            backend=default_backend()
        )

        decryptor = cipher.decryptor()

        plaintext = (
            decryptor.update(packet.encrypted)
            + decryptor.finalize()
        )

        return plaintext

    except Exception as e:
        print(f"❌ Crypto failure: {e}")
        return None


# ---------------------------------------------------------------------
# MQTT & Payload Parsing
# ---------------------------------------------------------------------

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("✅ Connected to MQTT broker")
        client.subscribe("msh/US/TX/#")
        print("📡 Subscribed to msh/US/TX/#")
    else:
        print(f"❌ MQTT connect failed rc={rc}")


def decode_payload(payload_data, from_id):
    print(
        f"🔢 Portnum={payload_data.portnum} "
        f"PayloadLength={len(payload_data.payload)}"
    )

    try:
        if payload_data.portnum == portnums_pb2.PortNum.TEXT_MESSAGE_APP:
            text = payload_data.payload.decode("utf-8", errors="ignore")
            print(f"💬 MESSAGE Node=!{from_id:08x}: '{text}'")

        elif payload_data.portnum == portnums_pb2.PortNum.NODEINFO_APP:
            user_info = mesh_pb2.User()
            user_info.ParseFromString(payload_data.payload)
            print(
                f"👤 NODEINFO !{from_id:08x} "
                f"Long='{user_info.long_name}' Short='{user_info.short_name}'"
            )

        elif payload_data.portnum == portnums_pb2.PortNum.POSITION_APP:
            position = mesh_pb2.Position()
            position.ParseFromString(payload_data.payload)
            print("📍 POSITION")
            print(position)

        elif payload_data.portnum == portnums_pb2.PortNum.TELEMETRY_APP:
            telemetry = telemetry_pb2.Telemetry()
            telemetry.ParseFromString(payload_data.payload)
            print("📊 TELEMETRY")
            print(telemetry)

        else:
            print(f"📦 OTHER PORTNUM: {payload_data.portnum}")

    except Exception as e:
        print(f"⚠️ Payload decode failed: {e}")


def on_message(client, userdata, msg):
    try:
        envelope = mqtt_pb2.ServiceEnvelope()

        try:
            envelope.ParseFromString(msg.payload)
        except Exception as e:
            print(f"Envelope parse failure: {e}")
            return

        print("=================== RAW ENVELOPE DUMP ===================")
        print(envelope)
        print("=========================================================")

        packet = envelope.packet
        from_id = get_protobuf_field(packet, "from")
        to_id = get_protobuf_field(packet, "to")

        is_longfast = "LongFast" in msg.topic

        # Ignore metadata-only packets
        if not packet.HasField("encrypted") and not packet.HasField("decoded"):
            return

        print("\n" + "=" * 80)
        print(f"TOPIC: {msg.topic}")
        print(f"FROM: !{from_id:08x} -> TO: !{to_id:08x} | ID: {packet.id}")

        # Already decoded by MQTT gateway
        if packet.HasField("decoded"):
            print("✅ Already decoded by gateway")
            decode_payload(packet.decoded, from_id)
            return

        # Encrypted payload
        if packet.HasField("encrypted"):
            if not is_longfast:
                print("🔒 Non-LongFast topic, skipping decrypt")
                return

            if getattr(packet, "pki_encrypted", False):
                print("🔒 PKI direct message, skipping channel decrypt")
                return

            decrypted = decrypt_packet(packet, PUBLIC_KEY, from_id)
            if not decrypted:
                return

            payload_data = mesh_pb2.Data()
            try:
                payload_data.ParseFromString(decrypted)
                print("✅ Data protobuf decrypted & parsed successfully")
                decode_payload(payload_data, from_id)
            except Exception as e:
                print(f"⚠️ Data protobuf parse failed: {e}")
                print(f"RAW DECRYPTED HEX: {decrypted.hex()}")

    except Exception:
        print("❌ Unhandled exception")
        traceback.print_exc()


# ---------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------

client = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2
)

client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set(
    "meshdev",
    "large4cats"
)

print("Connecting to Meshtastic MQTT...")

client.connect(
    "mqtt.meshtastic.org",
    1883,
    60
)

client.loop_forever()