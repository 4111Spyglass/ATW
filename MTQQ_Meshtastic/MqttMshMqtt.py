import base64
import collections
import hashlib
import json
import random
import time
import threading
import warnings
import paho.mqtt.client as mqtt

# Meshtastic Protobufs
from meshtastic.protobuf import mqtt_pb2, mesh_pb2, portnums_pb2

# Cryptography
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESCCM
from cryptography.hazmat.backends import default_backend

warnings.filterwarnings("ignore", category=RuntimeWarning)


# =====================================================================
# 1. Cryptographic Engine Helper
# =====================================================================
class MeshtasticCrypto:
    """Encapsulates all byte-packing and cipher routines for Meshtastic."""

    LONGFAST_KEY = bytes.fromhex("d4f1bb3a20290759f0bcffabcf4e6901")
    LONGFAST_HASH = 8

    @staticmethod
    def derive_pki_key(private_key: x25519.X25519PrivateKey, peer_public_key: x25519.X25519PublicKey) -> bytes:
        """Derives a 32-byte AES key via Curve25519 ECDH + SHA-256."""
        shared_secret = private_key.exchange(peer_public_key)
        return hashlib.sha256(shared_secret).digest()

    @staticmethod
    def build_ccm_nonce_13(packet_id: int, extra_nonce: int, from_node: int) -> bytes:
        """Constructs the exact 13-byte Nonce expected by Meshtastic AES-CCM."""
        return (
            packet_id.to_bytes(4, "little")
            + extra_nonce.to_bytes(4, "little")
            + from_node.to_bytes(4, "little")
            + bytes(1)
        )

    @classmethod
    def encrypt_channel_ctr(cls, plaintext: bytes, packet_id: int, from_node: int, key: bytes = None) -> bytes:
        """Encrypts data for shared channels using AES-128-CTR with 8+8 byte nonce."""
        key = key or cls.LONGFAST_KEY
        nonce = packet_id.to_bytes(8, "little") + from_node.to_bytes(8, "little")
        cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
        enc = cipher.encryptor()
        return enc.update(plaintext) + enc.finalize()

    @classmethod
    def decrypt_channel_ctr(cls, ciphertext: bytes, packet_id: int, from_node: int, key: bytes = None) -> bytes:
        """Decrypts data from shared channels using AES-128-CTR with 8+8 byte nonce."""
        key = key or cls.LONGFAST_KEY
        nonce = packet_id.to_bytes(8, "little") + from_node.to_bytes(8, "little")
        cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
        dec = cipher.decryptor()
        try:
            return dec.update(ciphertext) + dec.finalize()
        except Exception:
            return None

    @classmethod
    def encrypt_pki_ccm(cls, plaintext: bytes, packet_id: int, from_node: int,
                        private_key: x25519.X25519PrivateKey, peer_public_key: x25519.X25519PublicKey) -> bytes:
        """Encrypts a direct message using Curve25519 + AES-256-CCM."""
        extra_nonce = random.randint(0, 0xFFFFFFFF)
        nonce_13 = cls.build_ccm_nonce_13(packet_id, extra_nonce, from_node)
        aes_key = cls.derive_pki_key(private_key, peer_public_key)

        aesccm = AESCCM(aes_key, tag_length=8)
        ciphertext_and_tag = aesccm.encrypt(nonce_13, plaintext, associated_data=None)
        return ciphertext_and_tag + extra_nonce.to_bytes(4, "little")

    @classmethod
    def decrypt_pki_ccm(cls, encrypted_bytes: bytes, packet_id: int, from_node: int,
                        private_key: x25519.X25519PrivateKey, peer_public_key: x25519.X25519PublicKey) -> bytes:
        """Decrypts a direct message and verifies the 8-byte authentication tag."""
        if len(encrypted_bytes) < 12:
            return None

        extra_nonce = int.from_bytes(encrypted_bytes[-4:], "little")
        ciphertext_and_tag = encrypted_bytes[:-4]

        nonce_13 = cls.build_ccm_nonce_13(packet_id, extra_nonce, from_node)
        aes_key = cls.derive_pki_key(private_key, peer_public_key)

        aesccm = AESCCM(aes_key, tag_length=8)
        try:
            return aesccm.decrypt(nonce_13, ciphertext_and_tag, associated_data=None)
        except Exception:
            return None


# =====================================================================
# 2. Base Station Class
# =====================================================================
class MqttMshBase:
    DEFAULT_BROKER = "mqtt.meshtastic.org"
    DEFAULT_PORT = 1883
    DEFAULT_USER = "meshdev"
    DEFAULT_PASS = "large4cats"
    DEFAULT_ROOT = "msh/US/TX"

    def __init__(
        self,
        node_id: int = 0x11223344,
        node_name: str = "Python Station",
        node_short: str = "PYST",
        seed_hex: str = "11" * 32,
        root_topic: str = DEFAULT_ROOT,
        broker: str = DEFAULT_BROKER,
        port: int = DEFAULT_PORT,
        user: str = DEFAULT_USER,
        password: str = DEFAULT_PASS
    ):
        self.node_id = node_id
        self.node_name = node_name
        self.node_short = node_short
        self.root_topic = root_topic
        self.broker = broker
        self.port = port
        self.user = user
        self.password = password

        self.private_key = x25519.X25519PrivateKey.from_private_bytes(bytes.fromhex(seed_hex))
        self.public_key = self.private_key.public_key().public_bytes_raw()
        self.known_public_keys: dict[int, x25519.X25519PublicKey] = {}

    def register_peer_key(self, node_id: int, pubkey_bytes_or_b64: bytes | str):
        if isinstance(pubkey_bytes_or_b64, str):
            raw_bytes = base64.b64decode(pubkey_bytes_or_b64)
        else:
            raw_bytes = pubkey_bytes_or_b64
        self.known_public_keys[node_id] = x25519.X25519PublicKey.from_public_bytes(raw_bytes)

    def _create_mqtt_client(self, client_id_prefix: str) -> mqtt.Client:
        client_id = f"{client_id_prefix}-{random.randint(1000, 9999)}"
        client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        client.username_pw_set(self.user, self.password)
        return client

    def _wrap_envelope(self, packet: mesh_pb2.MeshPacket, channel_id: str) -> mqtt_pb2.ServiceEnvelope:
        return mqtt_pb2.ServiceEnvelope(
            packet=packet,
            channel_id=channel_id,
            gateway_id=f"!{self.node_id:08x}"
        )


# =====================================================================
# 3. Msh Transmitter / Sender
# =====================================================================
class MqttMshSender(MqttMshBase):

    def _publish_packet(self, topic: str, channel_id: str, packet: mesh_pb2.MeshPacket):
        envelope = self._wrap_envelope(packet, channel_id)
        client = self._create_mqtt_client("py-sender")

        connected = threading.Event()
        client.on_connect = lambda c, u, f, rc, p=None: connected.set() if rc == 0 else None

        client.connect(self.broker, self.port, 60)
        client.loop_start()
        connected.wait(10.0)

        client.publish(topic, envelope.SerializeToString(), qos=1).wait_for_publish(5.0)
        time.sleep(0.5)
        client.loop_stop()
        client.disconnect()

    def send_nodeinfo(self):
        user = mesh_pb2.User(
            id=f"!{self.node_id:08x}",
            long_name=self.node_name,
            short_name=self.node_short,
            macaddr=bytes([0x11, 0x22, 0x33, 0x44, 0x55, 0x66]),
            hw_model=mesh_pb2.HardwareModel.PRIVATE_HW,
            public_key=self.public_key
        )
        data = mesh_pb2.Data(portnum=portnums_pb2.PortNum.NODEINFO_APP, payload=user.SerializeToString())
        packet_id = random.randint(1, 0xFFFFFFFF)
        encrypted_payload = MeshtasticCrypto.encrypt_channel_ctr(data.SerializeToString(), packet_id, self.node_id)

        packet = mesh_pb2.MeshPacket(
            to=0xFFFFFFFF,
            id=packet_id,
            channel=MeshtasticCrypto.LONGFAST_HASH,
            encrypted=encrypted_payload,
            rx_time=int(time.time()),
            hop_limit=3,
            hop_start=3,
            want_ack=False,
            priority=mesh_pb2.MeshPacket.Priority.RELIABLE
        )
        setattr(packet, "from", self.node_id)

        topic = f"{self.root_topic}/2/e/LongFast/!{self.node_id:08x}"
        print(f"📡 Announcing {self.node_name} ({self.node_short}) NodeInfo...")
        self._publish_packet(topic, "LongFast", packet)
        print("✅ NodeInfo announced!")

    def send_broadcast(self, text: str):
        data = mesh_pb2.Data(portnum=portnums_pb2.PortNum.TEXT_MESSAGE_APP, payload=text.encode("utf-8"))
        packet_id = random.randint(1, 0xFFFFFFFF)
        encrypted_payload = MeshtasticCrypto.encrypt_channel_ctr(data.SerializeToString(), packet_id, self.node_id)

        packet = mesh_pb2.MeshPacket(
            to=0xFFFFFFFF,
            id=packet_id,
            channel=MeshtasticCrypto.LONGFAST_HASH,
            encrypted=encrypted_payload,
            rx_time=int(time.time()),
            hop_limit=3,
            hop_start=3,
            want_ack=False,
            priority=mesh_pb2.MeshPacket.Priority.RELIABLE
        )
        setattr(packet, "from", self.node_id)

        topic = f"{self.root_topic}/2/e/LongFast/!{self.node_id:08x}"
        print(f"📢 Broadcasting to LongFast: '{text}'")
        self._publish_packet(topic, "LongFast", packet)
        print("✅ Broadcast delivered!")

    def send_direct_message(self, text: str, destination_node: int):
        peer_pubkey = self.known_public_keys.get(destination_node)
        if not peer_pubkey:
            raise ValueError(f"Cannot send DM: Public key for node !{destination_node:08x} is not registered.")

        data = mesh_pb2.Data(
            portnum=portnums_pb2.PortNum.TEXT_MESSAGE_APP,
            payload=text.encode("utf-8"),
            dest=destination_node,
            source=self.node_id
        )
        packet_id = random.randint(1, 0xFFFFFFFF)
        encrypted_payload = MeshtasticCrypto.encrypt_pki_ccm(
            data.SerializeToString(), packet_id, self.node_id, self.private_key, peer_pubkey
        )

        packet = mesh_pb2.MeshPacket(
            to=destination_node,
            id=packet_id,
            channel=0,
            pki_encrypted=True,
            encrypted=encrypted_payload,
            rx_time=int(time.time()),
            hop_limit=3,
            hop_start=3,
            want_ack=False,
            priority=mesh_pb2.MeshPacket.Priority.RELIABLE
        )
        setattr(packet, "from", self.node_id)

        topic = f"{self.root_topic}/2/e/PKI/!{self.node_id:08x}"
        print(f"🔒 Sending PKI DM to !{destination_node:08x}: '{text}'")
        self._publish_packet(topic, "PKI", packet)
        print("✅ Private DM delivered!")


# =====================================================================
# 4. Msh Receiver (Handles BOTH DMs and Public Broadcasts)
# =====================================================================
class MqttMshReceiver(MqttMshBase):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._client: mqtt.Client | None = None
        self._on_dm_callback = None
        self._on_broadcast_callback = None
        self._seen_messages = collections.deque(maxlen=1000)

    def _send_ack_async(self, to_node: int, request_id: int):
        def _worker():
            peer_pubkey = self.known_public_keys.get(to_node)
            if not peer_pubkey:
                return
            try:
                packet_id = random.randint(1, 0xFFFFFFFF)
                data = mesh_pb2.Data(
                    portnum=portnums_pb2.PortNum.ROUTING_APP,
                    payload=b"",
                    request_id=request_id,
                    want_response=False,
                )
                encrypted_payload = MeshtasticCrypto.encrypt_pki_ccm(
                    data.SerializeToString(), packet_id, self.node_id, self.private_key, peer_pubkey
                )
                ack_packet = mesh_pb2.MeshPacket(
                    to=to_node,
                    id=packet_id,
                    channel=0,
                    pki_encrypted=True,
                    encrypted=encrypted_payload,
                    rx_time=int(time.time()),
                    hop_limit=3,
                    hop_start=3,
                    want_ack=False,
                    priority=mesh_pb2.MeshPacket.Priority.ACK,
                )
                setattr(ack_packet, "from", self.node_id)

                topic = f"{self.root_topic}/2/e/PKI/!{self.node_id:08x}"
                envelope = self._wrap_envelope(ack_packet, "PKI")

                if self._client and self._client.is_connected():
                    self._client.publish(topic, envelope.SerializeToString(), qos=0)
                    print(f"⚡ Background ACK delivered for request #{request_id} to !{to_node:08x}")
            except Exception as e:
                print(f"⚠️ Failed to send ACK: {e}")

        threading.Thread(target=_worker, daemon=True).start()

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        # Filter non-protobuf topics
        if not ("/2/e/" in topic or "/2/c/" in topic):
            return

        try:
            envelope = mqtt_pb2.ServiceEnvelope()
            envelope.ParseFromString(msg.payload)
            packet = envelope.packet

            from_node = getattr(packet, "from")
            to_node = packet.to
            is_pki = getattr(packet, "pki_encrypted", False)

            # Auto-learn peer public keys from NodeInfo broadcasts
            if packet.HasField("decoded") and packet.decoded.portnum == portnums_pb2.PortNum.NODEINFO_APP:
                user = mesh_pb2.User()
                user.ParseFromString(packet.decoded.payload)
                if len(user.public_key) == 32 and from_node not in self.known_public_keys:
                    self.register_peer_key(from_node, user.public_key)
                    print(f"🔑 Learned Public Key for {user.long_name} (!{from_node:08x})")

            # -------------------------------------------------------------
            # CASE A: Private Direct Message (PKI to us)
            # -------------------------------------------------------------
            if to_node == self.node_id and is_pki:
                peer_pubkey = self.known_public_keys.get(from_node)
                if not peer_pubkey or not packet.encrypted:
                    return

                plaintext = MeshtasticCrypto.decrypt_pki_ccm(
                    packet.encrypted, packet.id, from_node, self.private_key, peer_pubkey
                )
                if not plaintext:
                    return

                data = mesh_pb2.Data()
                data.ParseFromString(plaintext)

                if data.portnum == portnums_pb2.PortNum.ROUTING_APP:
                    return

                text = data.payload.decode("utf-8", errors="ignore") if data.portnum == portnums_pb2.PortNum.TEXT_MESSAGE_APP else None

                dm_dict = {
                    "timestamp": packet.rx_time if packet.rx_time else int(time.time()),
                    "message_id": packet.id,
                    "from_id": f"!{from_node:08x}",
                    "to_id": f"!{to_node:08x}",
                    "portnum": portnums_pb2.PortNum.Name(data.portnum),
                    "text": text,
                    "hop_limit": packet.hop_limit,
                    "channel_id": envelope.channel_id,
                    "gateway_id": envelope.gateway_id,
                    "topic": topic
                }

                if self._on_dm_callback:
                    self._on_dm_callback(dm_dict)

                if packet.want_ack:
                    self._send_ack_async(from_node, packet.id)

            # -------------------------------------------------------------
            # CASE B: Public Channel Broadcast (LongFast)
            # -------------------------------------------------------------
            elif not is_pki and "/LongFast/" in topic:
                data = None

                if packet.HasField("decoded"):
                    data = packet.decoded
                elif packet.HasField("encrypted"):
                    plaintext = MeshtasticCrypto.decrypt_channel_ctr(packet.encrypted, packet.id, from_node)
                    if plaintext:
                        try:
                            cand = mesh_pb2.Data()
                            cand.ParseFromString(plaintext)
                            data = cand
                        except Exception:
                            return

                if not data or data.portnum != portnums_pb2.PortNum.TEXT_MESSAGE_APP:
                    return

                # De-duplicate verified broadcasts
                if (from_node, packet.id) in self._seen_messages:
                    return
                self._seen_messages.append((from_node, packet.id))

                pub_dict = {
                    "timestamp": packet.rx_time if packet.rx_time else int(time.time()),
                    "message_id": packet.id,
                    "from_id": f"!{from_node:08x}",
                    "to": "BROADCAST (^all)" if to_node == 0xFFFFFFFF else f"!{to_node:08x}",
                    "text": data.payload.decode("utf-8", errors="ignore").strip(),
                    "channel": envelope.channel_id or "LongFast",
                    "hop_limit": packet.hop_limit,
                    "rx_snr": packet.rx_snr,
                    "rx_rssi": packet.rx_rssi,
                    "gateway_id": envelope.gateway_id,
                    "topic": topic
                }

                if self._on_broadcast_callback:
                    self._on_broadcast_callback(pub_dict)

        except Exception:
            pass

    def start_listening(self, on_dm=None, on_broadcast=None):
        """Starts listening for both Direct Messages and Public Channel broadcasts."""
        self._on_dm_callback = on_dm or self._default_dm_printer
        self._on_broadcast_callback = on_broadcast or self._default_broadcast_printer
        self._client = self._create_mqtt_client("py-receiver")

        def on_connect(c, userdata, flags, rc, properties=None):
            if rc == 0:
                sub_topic = f"{self.root_topic}/#"
                print(f"✅ Connected to {self.broker}! Subscribing to {sub_topic}...")
                c.subscribe(sub_topic)
                print(f"🎧 Listening for DMs to !{self.node_id:08x} AND public LongFast broadcasts...\n")
            else:
                print(f"❌ Connection failed with code {rc}")

        self._client.on_connect = on_connect
        self._client.on_message = self._on_message

        print(f"Connecting to {self.broker}...")
        self._client.connect(self.broker, self.port, 60)

        try:
            self._client.loop_forever()
        except KeyboardInterrupt:
            print("\nStopping listener...")
            self._client.disconnect()

    @staticmethod
    def _default_dm_printer(dm: dict):
        print("\n" + "=" * 60)
        print(f"📬 DIRECT MESSAGE FROM {dm['from_id']}")
        print("=" * 60)
        print(json.dumps(dm, indent=2))
        print("=" * 60 + "\n")

    @staticmethod
    def _default_broadcast_printer(bcast: dict):
        print("\n" + "=" * 60)
        print(f"📢 PUBLIC BROADCAST FROM {bcast['from_id']}")
        print("=" * 60)
        print(json.dumps(bcast, indent=2))
        print("=" * 60 + "\n")


# =====================================================================
# 5. Usage Examples
# =====================================================================
if __name__ == "__main__":
    import sys

    HELTEC_ID = 0x8FA0DE68
    HELTEC_PUBKEY = "aLMcPAE65mZeQz+Bv37gl/6V11Zc0Ijj08/0apQJEm8="
    TEST_TOPIC = "msh/US/TX"

    # --- Mode 1: Send ---
    if len(sys.argv) > 1 and sys.argv[1].lower() in ("send", "--send", "-s"):
        sender = MqttMshSender(root_topic=TEST_TOPIC)
        sender.register_peer_key(HELTEC_ID, HELTEC_PUBKEY)

        # Broadcast test:
        sender.send_broadcast("Hello from unified MqttMsh!")

        # DM test:
        # sender.send_direct_message("Private hello!", destination_node=HELTEC_ID)

    # --- Mode 2: Listen for BOTH DMs and Broadcasts ---
    else:
        receiver = MqttMshReceiver(root_topic=TEST_TOPIC)
        receiver.register_peer_key(HELTEC_ID, HELTEC_PUBKEY)
        receiver.start_listening()