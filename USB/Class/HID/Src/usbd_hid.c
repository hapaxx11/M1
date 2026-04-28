/* See COPYING.txt for license details. */

#include "usbd_hid.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"

#ifdef USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif

/*---------- HID Keyboard Report Descriptor ----------*/
/*
 * Standard USB HID Boot Keyboard descriptor.
 * Report layout (8 bytes):
 *   Byte 0   : Modifier keys (Ctrl/Shift/Alt/GUI L+R = 8 bits)
 *   Byte 1   : Reserved (0x00)
 *   Bytes 2-7: Key codes (up to 6 simultaneous keys)
 */
static const uint8_t HID_KeyboardReportDesc[HID_KEYBOARD_REPORT_DESC_SIZE] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop)     */
    0x09, 0x06,       /* Usage (Keyboard)                 */
    0xA1, 0x01,       /* Collection (Application)         */

    /* Modifier keys (8 bits) */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad)   */
    0x19, 0xE0,       /*   Usage Minimum (Left Ctrl)      */
    0x29, 0xE7,       /*   Usage Maximum (Right GUI)      */
    0x15, 0x00,       /*   Logical Minimum (0)            */
    0x25, 0x01,       /*   Logical Maximum (1)            */
    0x75, 0x01,       /*   Report Size (1 bit)            */
    0x95, 0x08,       /*   Report Count (8)               */
    0x81, 0x02,       /*   Input (Data, Variable, Abs)    */

    /* Reserved byte */
    0x95, 0x01,       /*   Report Count (1)               */
    0x75, 0x08,       /*   Report Size (8 bits)           */
    0x81, 0x01,       /*   Input (Constant) -- reserved   */

    /* LED output report (5 bits + 3 pad) */
    0x95, 0x05,       /*   Report Count (5)               */
    0x75, 0x01,       /*   Report Size (1 bit)            */
    0x05, 0x08,       /*   Usage Page (LEDs)              */
    0x19, 0x01,       /*   Usage Minimum (Num Lock)       */
    0x29, 0x05,       /*   Usage Maximum (Kana)           */
    0x91, 0x02,       /*   Output (Data, Variable, Abs)   */
    0x95, 0x01,       /*   Report Count (1)               */
    0x75, 0x03,       /*   Report Size (3 bits)           */
    0x91, 0x01,       /*   Output (Constant) -- pad       */

    /* Key array (6 bytes) */
    0x95, 0x06,       /*   Report Count (6)               */
    0x75, 0x08,       /*   Report Size (8 bits)           */
    0x15, 0x00,       /*   Logical Minimum (0)            */
    0x25, 0x65,       /*   Logical Maximum (101)          */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad)   */
    0x19, 0x00,       /*   Usage Minimum (0)              */
    0x29, 0x65,       /*   Usage Maximum (101)            */
    0x81, 0x00,       /*   Input (Data, Array)            */

    0xC0              /* End Collection                   */
};

/*---------- Non-composite descriptors (unused in composite mode, but must exist) ----------*/

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length);
#endif

USBD_ClassTypeDef USBD_HID = {
    USBD_HID_Init,
    USBD_HID_DeInit,
    USBD_HID_Setup,
    NULL,                  /* EP0_TxSent */
    NULL,                  /* EP0_RxReady */
    USBD_HID_DataIn,
    NULL,                  /* DataOut */
    NULL,                  /* SOF */
    NULL,                  /* IsoINIncomplete */
    NULL,                  /* IsoOUTIncomplete */
#ifdef USE_USBD_COMPOSITE
    NULL,                  /* GetFSConfigDescriptor (composite builder handles it) */
    NULL,                  /* GetHSConfigDescriptor */
    NULL,                  /* GetOtherSpeedConfigDescriptor */
    NULL,                  /* GetDeviceQualifierDescriptor */
#else
    USBD_HID_GetFSCfgDesc,
    NULL,
    NULL,
    NULL,
#endif
};

/*---------- Init / DeInit ----------*/

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    UNUSED(cfgidx);
    USBD_HID_HandleTypeDef *hhid;

    /* Use local static buffer (same pattern as MSC -- avoids USBD_static_malloc issue) */
    static uint32_t mem[(sizeof(USBD_HID_HandleTypeDef) / 4) + 1];
    hhid = (USBD_HID_HandleTypeDef *)mem;

    if (hhid == NULL) {
        pdev->pClassDataCmsit[pdev->classId] = NULL;
        return (uint8_t)USBD_EMEM;
    }

    pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
    pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

#ifdef USE_USBD_COMPOSITE
    /* Get endpoint address from composite builder */
    uint8_t ep_in = ((USBD_CompositeElementTypeDef *)pdev->pClassDataCmsit[pdev->classId - 1U] != NULL)
                  ? pdev->tclasslist[pdev->classId].Eps[0].add
                  : 0x84U;
    /* Fix: just read from the class list directly */
    ep_in = pdev->tclasslist[pdev->classId].Eps[0].add;
#else
    uint8_t ep_in = 0x81U;
#endif

    (void)memset(hhid, 0, sizeof(USBD_HID_HandleTypeDef));
    hhid->state    = HID_IDLE;
    hhid->protocol = 1U; /* Report protocol by default */
    hhid->idle     = 0U;

    /* Open IN endpoint */
    (void)USBD_LL_OpenEP(pdev, ep_in, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
    pdev->ep_in[ep_in & 0xFU].is_used = 1U;
    pdev->ep_in[ep_in & 0xFU].bInterval = HID_FS_BINTERVAL;

    return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    UNUSED(cfgidx);

#ifdef USE_USBD_COMPOSITE
    uint8_t ep_in = pdev->tclasslist[pdev->classId].Eps[0].add;
#else
    uint8_t ep_in = 0x81U;
#endif

    (void)USBD_LL_CloseEP(pdev, ep_in);
    pdev->ep_in[ep_in & 0xFU].is_used = 0U;
    pdev->ep_in[ep_in & 0xFU].bInterval = 0U;

    /* Don't free -- it's a static buffer */
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;

    return (uint8_t)USBD_OK;
}

/*---------- Setup (control requests) ----------*/

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
    USBD_StatusTypeDef ret = USBD_OK;
    uint16_t status_info = 0U;

    if (hhid == NULL) {
        return (uint8_t)USBD_FAIL;
    }

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {

    case USB_REQ_TYPE_CLASS:
        switch (req->bRequest) {
        case HID_REQ_SET_PROTOCOL:
            hhid->protocol = (uint8_t)(req->wValue);
            break;

        case HID_REQ_GET_PROTOCOL:
            (void)USBD_CtlSendData(pdev, &hhid->protocol, 1U);
            break;

        case HID_REQ_SET_IDLE:
            hhid->idle = (uint8_t)(req->wValue >> 8);
            break;

        case HID_REQ_GET_IDLE:
            (void)USBD_CtlSendData(pdev, &hhid->idle, 1U);
            break;

        case HID_REQ_SET_REPORT:
            /* Host sends LED state -- just accept and discard */
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    case USB_REQ_TYPE_STANDARD:
        switch (req->bRequest) {
        case USB_REQ_GET_STATUS:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
            } else {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        case USB_REQ_GET_DESCRIPTOR:
            if ((req->wValue >> 8) == HID_REPORT_DESCRIPTOR_TYPE) {
                (void)USBD_CtlSendData(pdev, (uint8_t *)HID_KeyboardReportDesc,
                                       MIN(HID_KEYBOARD_REPORT_DESC_SIZE, req->wLength));
            } else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE) {
                /* Return the 9-byte HID descriptor from the configuration descriptor.
                 * For simplicity, build it on the fly. */
                static USBD_HIDDescTypeDef hidDesc;
                hidDesc.bLength            = sizeof(USBD_HIDDescTypeDef);
                hidDesc.bDescriptorType    = HID_DESCRIPTOR_TYPE;
                hidDesc.bcdHID             = 0x0111U;
                hidDesc.bCountryCode       = 0x00U;
                hidDesc.bNumDescriptors    = 0x01U;
                hidDesc.bHIDDescriptorType = HID_REPORT_DESCRIPTOR_TYPE;
                hidDesc.wItemLength        = HID_KEYBOARD_REPORT_DESC_SIZE;
                (void)USBD_CtlSendData(pdev, (uint8_t *)&hidDesc,
                                       MIN(sizeof(USBD_HIDDescTypeDef), req->wLength));
            } else {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        case USB_REQ_GET_INTERFACE:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                (void)USBD_CtlSendData(pdev, &hhid->altSetting, 1U);
            } else {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        case USB_REQ_SET_INTERFACE:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                hhid->altSetting = (uint8_t)(req->wValue);
            } else {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    default:
        USBD_CtlError(pdev, req);
        ret = USBD_FAIL;
        break;
    }

    return (uint8_t)ret;
}

/*---------- DataIn (interrupt IN transfer complete) ----------*/

static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    UNUSED(epnum);
    USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

    if (hhid != NULL) {
        hhid->state = HID_IDLE;
    }

    return (uint8_t)USBD_OK;
}

/*---------- Public API ----------*/

uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
    USBD_HID_HandleTypeDef *hhid;

#ifdef USE_USBD_COMPOSITE
    /* Find HID class instance */
    for (uint32_t i = 0; i < pdev->NumClasses; i++) {
        if (pdev->tclasslist[i].ClassType == CLASS_TYPE_HID) {
            hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[i];
            if (hhid == NULL) return (uint8_t)USBD_FAIL;

            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                if (hhid->state == HID_IDLE) {
                    hhid->state = HID_BUSY;
                    (void)USBD_LL_Transmit(pdev, pdev->tclasslist[i].Eps[0].add, report, len);
                }
            }
            return (uint8_t)USBD_OK;
        }
    }
    return (uint8_t)USBD_FAIL;
#else
    hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
    if (hhid == NULL) return (uint8_t)USBD_FAIL;

    if (pdev->dev_state == USBD_STATE_CONFIGURED) {
        if (hhid->state == HID_IDLE) {
            hhid->state = HID_BUSY;
            (void)USBD_LL_Transmit(pdev, 0x81U, report, len);
        }
    }
    return (uint8_t)USBD_OK;
#endif
}

/*---------- Non-composite config descriptor (fallback) ----------*/

#ifndef USE_USBD_COMPOSITE
static uint8_t USBD_HID_CfgFSDesc[] = {
    /* Configuration Descriptor */
    0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xC0, 0x32,
    /* Interface Descriptor */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    /* HID Descriptor */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(HID_KEYBOARD_REPORT_DESC_SIZE & 0xFF),
    (uint8_t)((HID_KEYBOARD_REPORT_DESC_SIZE >> 8) & 0xFF),
    /* Endpoint Descriptor */
    0x07, 0x05, 0x81, 0x03, HID_EPIN_SIZE, 0x00, HID_FS_BINTERVAL,
};

static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_HID_CfgFSDesc);
    return USBD_HID_CfgFSDesc;
}
#endif
