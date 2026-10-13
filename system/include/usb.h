#ifndef PROOTOS_USB_H
#define PROOTOS_USB_H

#include "types.h"

/* bmRequestType */
#define USB_DIR_OUT          0x00
#define USB_DIR_IN           0x80
#define USB_TYPE_STANDARD    0x00
#define USB_TYPE_CLASS       0x20
#define USB_TYPE_VENDOR      0x40
#define USB_RECIP_DEVICE     0x00
#define USB_RECIP_INTERFACE  0x01
#define USB_RECIP_ENDPOINT   0x02

/* bRequest */
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09

/* Descriptor types */
#define USB_DESC_DEVICE        0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING        0x03
#define USB_DESC_INTERFACE     0x04
#define USB_DESC_ENDPOINT      0x05

/* Device classes */
#define USB_CLASS_MASS_STORAGE 0x08

/* Mass Storage subclasses */
#define USB_MS_SCSI            0x06
/* Mass Storage protocols */
#define USB_MS_PROTO_BOT       0x50   /* Bulk-Only Transport */

/* Endpoint attributes */
#define USB_EP_TYPE_CONTROL    0
#define USB_EP_TYPE_ISO        1
#define USB_EP_TYPE_BULK       2
#define USB_EP_TYPE_INTERRUPT  3

/* ─── Descriptors ─────────────────────────────────── */

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u16 bcdUSB;
    u8  bDeviceClass;
    u8  bDeviceSubClass;
    u8  bDeviceProtocol;
    u8  bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8  iManufacturer;
    u8  iProduct;
    u8  iSerialNumber;
    u8  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u16 wTotalLength;
    u8  bNumInterfaces;
    u8  bConfigurationValue;
    u8  iConfiguration;
    u8  bmAttributes;
    u8  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u8  bInterfaceNumber;
    u8  bAlternateSetting;
    u8  bNumEndpoints;
    u8  bInterfaceClass;
    u8  bInterfaceSubClass;
    u8  bInterfaceProtocol;
    u8  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u8  bEndpointAddress;
    u8  bmAttributes;
    u16 wMaxPacketSize;
    u8  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

/* Setup packet */
typedef struct {
    u8  bmRequestType;
    u8  bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} __attribute__((packed)) usb_setup_packet_t;

#endif
