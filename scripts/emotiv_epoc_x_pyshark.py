"""
This script provides parity with the emotiv-lsl Python repository's PyShark fallback method.
It acts as a reference or utility for capturing USB packets when standard HID interfaces are locked.
Note: Since this is a C++ repository, this script serves mainly for feature parity and documentation,
and would require standard Python dependencies (`pyshark`, `pylsl`, `pycryptodome`) if run standalone.
"""

import logging
import pyshark
from Crypto.Cipher import AES
import sys

# Note: The original Python repository relied on its internal `emotiv_lsl.emotiv_epoc_x` module.
# To use this with the C++ executable, one might parse pyshark outputs and feed them via stdin,
# or simply use this as a reference implementation for the raw USB parsing fallback.

class EmotivEpocXPySharkFallback:
    def __init__(self, key):
        self.delimiter = ','
        self.cipher = AES.new(key, AES.MODE_ECB)

        # Original Windows USB capture string example:
        self.interface = '\\\\?\\HID#VID_1234&PID_ED02&MI_01#a&30a4df18&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}'
        self.capture = pyshark.LiveCapture(interface=self.interface, bpf_filter='len == 72')
        print(f"Initialized PyShark capture on {self.interface}")

    def validate_data(self, data) -> bool:
        return len(data) == 64

    def main_loop(self):
        print("Starting PyShark capture loop...")
        for packet in self.capture.sniff_continuously():
            if packet.usb.dst != 'host':
                continue

            data = str(packet.layers[1].get_field('usb.capdata'))
            data = data.replace(':', '')

            if self.validate_data(data):
                data = bytearray.fromhex(data)
                # Here you would typically decode data and push to an LSL outlet.
                # As a parity script in a C++ repo, we output the raw validated payload:
                sys.stdout.buffer.write(data)
                sys.stdout.flush()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # Example key (must match derived key from C++ or real device)
        key = bytes.fromhex(sys.argv[1])
        fallback = EmotivEpocXPySharkFallback(key)
        fallback.main_loop()
    else:
        print("Usage: python emotiv_epoc_x_pyshark.py <hex_key>")
