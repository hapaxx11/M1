/* See COPYING.txt for license details. */

#ifndef __USBD_HID_H
#define __USBD_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"

/*---------- HID Configuration ----------*/

#define HID_EPIN_SIZE                    8U
#define HID_FS_BINTERVAL                 10U    /* 10ms polling */
#define HID_HS_BINTERVAL                 10U

#define HID_DESCRIPTOR_TYPE              0x21U
#define HID_REPORT_DESCRIPTOR_TYPE       0x22U

#define HID_REQ_SET_PROTOCOL             0x0BU
#define HID_REQ_GET_PROTOCOL             0x03U
#define HID_REQ_SET_IDLE                 0x0AU
#define HID_REQ_GET_IDLE                 0x02U
#define HID_REQ_SET_REPORT               0x09U
#define HID_REQ_GET_REPORT               0x01U

/* Keyboard boot protocol report: 8 bytes */
#define HID_KEYBOARD_REPORT_SIZE         8U

/* Size of the HID keyboard report descriptor (see usbd_hid.c) */
#define HID_KEYBOARD_REPORT_DESC_SIZE    63U

/* Alias used by the Composite Builder's HIDMouseDesc function */
#define HID_MOUSE_REPORT_DESC_SIZE       HID_KEYBOARD_REPORT_DESC_SIZE

/*---------- HID Descriptor Type (for Composite Builder) ----------*/

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bHIDDescriptorType;
    uint16_t wItemLength;
} USBD_HIDDescTypeDef;

/*---------- HID Handle ----------*/

typedef enum {
    HID_IDLE = 0,
    HID_BUSY,
} HID_StateTypeDef;

typedef struct {
    uint8_t           report[HID_KEYBOARD_REPORT_SIZE];
    uint8_t           idle;
    uint8_t           protocol;         /* 0 = Boot, 1 = Report */
    uint8_t           altSetting;
    HID_StateTypeDef  state;
} USBD_HID_HandleTypeDef;

/*---------- Exported Class ----------*/

extern USBD_ClassTypeDef USBD_HID;
#define USBD_HID_CLASS  &USBD_HID

/*---------- Public API ----------*/

uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_HID_H */
