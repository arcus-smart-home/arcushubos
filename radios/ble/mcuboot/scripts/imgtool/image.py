# Copyright 2018 Nordic Semiconductor ASA
# Copyright 2017 Linaro Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Image signing and management.
"""

from . import version as versmod
from intelhex import IntelHex
import hashlib
import struct
import os.path

IMAGE_MAGIC = 0x96f3b83d
IMAGE_HEADER_SIZE = 32
BIN_EXT = "bin"
INTEL_HEX_EXT = "hex"

# Image header flags.
IMAGE_F = {
        'PIC':                   0x0000001,
        'NON_BOOTABLE':          0x0000010, }

TLV_VALUES = {
        'KEYHASH': 0x01,
        'SHA256': 0x10,
        'RSA2048': 0x20,
        'ECDSA224': 0x21,
        'ECDSA256': 0x22, }

TLV_INFO_SIZE = 4
TLV_INFO_MAGIC = 0x6907
TLV_HEADER_SIZE = 4

# Sizes of the image trailer, depending on flash write size.
trailer_sizes = {
    write_size: 128 * 3 * write_size + 8 * 2 + 16
    for write_size in [1, 2, 4, 8]
}

boot_magic = bytes([
    0x77, 0xc2, 0x95, 0xf3,
    0x60, 0xd2, 0xef, 0x7f,
    0x35, 0x52, 0x50, 0x0f,
    0x2c, 0xb6, 0x79, 0x80, ])

class TLV():
    def __init__(self):
        self.buf = bytearray()

    def add(self, kind, payload):
        """Add a TLV record.  Kind should be a string found in TLV_VALUES above."""
        buf = struct.pack('<BBH', TLV_VALUES[kind], 0, len(payload))
        self.buf += buf
        self.buf += payload

    def get(self):
        header = struct.pack('<HH', TLV_INFO_MAGIC, TLV_INFO_SIZE + len(self.buf))
        return header + bytes(self.buf)

class Image():
    @classmethod
    def load(cls, path, included_header=False, **kwargs):
        """Load an image from a given file"""
        ext = os.path.splitext(path)[1][1:].lower()
        if ext == INTEL_HEX_EXT:
            cls = HexImage
        else:
            cls = BinImage

        obj = cls(**kwargs)
        obj.payload, obj.base_addr = obj.load(path)

        # Add the image header if needed.
        if not included_header and obj.header_size > 0:
            obj.payload = (b'\000' * obj.header_size) + obj.payload

        obj.check()
        return obj

    def __init__(self, version=None, header_size=IMAGE_HEADER_SIZE, pad=0):
        self.version = version or versmod.decode_version("0")
        self.header_size = header_size or IMAGE_HEADER_SIZE
        self.pad = pad

    def __repr__(self):
        return "<Image version={}, header_size={}, base_addr={}, pad={}, \
                format={}, payloadlen=0x{:x}>".format(
                self.version,
                self.header_size,
                self.base_addr if self.base_addr is not None else "N/A",
                self.pad,
                self.__class__.__name__,
                len(self.payload))

    def check(self):
        """Perform some sanity checking of the image."""
        # If there is a header requested, make sure that the image
        # starts with all zeros.
        if self.header_size > 0:
            if any(v != 0 for v in self.payload[0:self.header_size]):
                raise Exception("Padding requested, but image does not start with zeros")

    def sign(self, key):
        self.add_header(key)

        tlv = TLV()

        # Note that ecdsa wants to do the hashing itself, which means
        # we get to hash it twice.
        sha = hashlib.sha256()
        sha.update(self.payload)
        digest = sha.digest()

        tlv.add('SHA256', digest)

        if key is not None:
            pub = key.get_public_bytes()
            sha = hashlib.sha256()
            sha.update(pub)
            pubbytes = sha.digest()
            tlv.add('KEYHASH', pubbytes)

            sig = key.sign(bytes(self.payload))
            tlv.add(key.sig_tlv(), sig)

        self.payload += tlv.get()

    def add_header(self, key):
        """Install the image header.

        The key is needed to know the type of signature, and
        approximate the size of the signature."""

        flags = 0
        tlvsz = 0
        if key is not None:
            tlvsz += TLV_HEADER_SIZE + key.sig_len()

        tlvsz += 4 + hashlib.sha256().digest_size
        tlvsz += 4 + hashlib.sha256().digest_size

        fmt = ('<' +
            # type ImageHdr struct {
            'I' +   # Magic uint32
            'H' +   # TlvSz uint16
            'B' +   # KeyId uint8
            'B' +   # Pad1  uint8
            'H' +   # HdrSz uint16
            'H' +   # Pad2  uint16
            'I' +   # ImgSz uint32
            'I' +   # Flags uint32
            'BBHI' + # Vers  ImageVersion
            'I'     # Pad3  uint32
            ) # }
        assert struct.calcsize(fmt) == IMAGE_HEADER_SIZE
        header = struct.pack(fmt,
                IMAGE_MAGIC,
                tlvsz, # TlvSz
                0, # KeyId (TODO: allow other ids)
                0,  # Pad1
                self.header_size,
                0, # Pad2
                len(self.payload) - self.header_size, # ImageSz
                flags, # Flags
                self.version.major,
                self.version.minor or 0,
                self.version.revision or 0,
                self.version.build or 0,
                0) # Pad3
        self.payload = bytearray(self.payload)
        self.payload[:len(header)] = header

    def pad_to(self, size, align):
        """Pad the image to the given size, with the given flash alignment."""
        tsize = trailer_sizes[align]
        padding = size - (len(self.payload) + tsize)
        if padding < 0:
            msg = "Image size (0x{:x}) + trailer (0x{:x}) exceeds requested size 0x{:x}".format(
                    len(self.payload), tsize, size)
            raise Exception(msg)
        pbytes  = b'\xff' * padding
        pbytes += b'\xff' * (tsize - len(boot_magic))
        pbytes += boot_magic
        self.payload += pbytes

class HexImage(Image):

    def load(self, path):
        ih = IntelHex(path)
        return ih.tobinarray(), ih.minaddr()

    def save(self, path):
        h = IntelHex()
        h.frombytes(bytes = self.payload, offset = self.base_addr)
        h.tofile(path, 'hex')

class BinImage(Image):

    def load(self, path):
        with open(path, 'rb') as f:
            return f.read(), None

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(self.payload)
