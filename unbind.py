#!/usr/bin/python3

import os

USBHID_DIR = '/sys/bus/usb/drivers/usbhid/'

DEVICE_IDS = [
    [0x28BD, 0x0042]
]

def main():
    hids = list_dirs(USBHID_DIR)
    hids.remove('module')
    uevs = [get_uevent(os.path.join(USBHID_DIR, hid, 'uevent')) for hid in hids]
    dids = [get_device_id(uev['PRODUCT']) for uev in uevs]
    for hid, did in zip(hids, dids):
        if did in DEVICE_IDS:
            print('{0} ({1:04X}:{2:04X}) : unbound from usbhid'.format(hid, did[0], did[1]))
            unbind(hid)

def unbind(hid):
    path = os.path.join(USBHID_DIR, 'unbind')
    with open(path, mode='w', encoding='ascii') as fout:
        fout.write(hid)

def get_device_id(product):
    return [int(x, base=16) for x in product.split('/')[0:2]]

def get_uevent(uevent):
    with open(uevent, mode='r', encoding='ascii') as fin:
        doc = fin.read()
    return dict([line.split('=') for line in doc.splitlines()])

def list_dirs(path):
    return [x for x in os.listdir(path) if os.path.isdir(os.path.join(path, x))]

if __name__ == '__main__':
    
