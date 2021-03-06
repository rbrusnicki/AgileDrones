"""LCM type definitions
This file automatically generated by lcm.
DO NOT MODIFY BY HAND!!!!
"""

try:
    import cStringIO.StringIO as BytesIO
except ImportError:
    from io import BytesIO
import struct

class pose_t(object):
    __slots__ = ["timestamp", "position", "orientation"]

    def __init__(self):
        self.timestamp = 0
        self.position = [ 0.0 for dim0 in range(3) ]
        self.orientation = [ 0.0 for dim0 in range(4) ]

    def encode(self):
        buf = BytesIO()
        buf.write(pose_t._get_packed_fingerprint())
        self._encode_one(buf)
        return buf.getvalue()

    def _encode_one(self, buf):
        buf.write(struct.pack(">q", self.timestamp))
        buf.write(struct.pack('>3d', *self.position[:3]))
        buf.write(struct.pack('>4d', *self.orientation[:4]))

    def decode(data):
        if hasattr(data, 'read'):
            buf = data
        else:
            buf = BytesIO(data)
        if buf.read(8) != pose_t._get_packed_fingerprint():
            raise ValueError("Decode error")
        return pose_t._decode_one(buf)
    decode = staticmethod(decode)

    def _decode_one(buf):
        self = pose_t()
        self.timestamp = struct.unpack(">q", buf.read(8))[0]
        self.position = struct.unpack('>3d', buf.read(24))
        self.orientation = struct.unpack('>4d', buf.read(32))
        return self
    _decode_one = staticmethod(_decode_one)

    _hash = None
    def _get_hash_recursive(parents):
        if pose_t in parents: return 0
        tmphash = (0x23d7a1a628873b7e) & 0xffffffffffffffff
        tmphash  = (((tmphash<<1)&0xffffffffffffffff)  + (tmphash>>63)) & 0xffffffffffffffff
        return tmphash
    _get_hash_recursive = staticmethod(_get_hash_recursive)
    _packed_fingerprint = None

    def _get_packed_fingerprint():
        if pose_t._packed_fingerprint is None:
            pose_t._packed_fingerprint = struct.pack(">Q", pose_t._get_hash_recursive([]))
        return pose_t._packed_fingerprint
    _get_packed_fingerprint = staticmethod(_get_packed_fingerprint)

