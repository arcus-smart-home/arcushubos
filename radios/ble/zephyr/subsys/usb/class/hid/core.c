/*
 * Human Interface Device (HID) USB class core
 *
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_DEVICE_LEVEL
#define SYS_LOG_DOMAIN "usb/hid"
#include <logging/sys_log.h>

#include <usb_device.h>
#include <usb_common.h>

#include <usb_descriptor.h>
#include <class/usb_hid.h>

#ifdef CONFIG_USB_COMPOSITE_DEVICE
#include <composite.h>
#endif

static struct hid_device_info {
	const u8_t *report_desc;
	size_t report_size;

	const struct hid_ops *ops;
} hid_device;

static void hid_status_cb(enum usb_dc_status_code status, u8_t *param)
{
	/* Check the USB status and do needed action if required */
	switch (status) {
	case USB_DC_ERROR:
		SYS_LOG_DBG("USB device error");
		break;
	case USB_DC_RESET:
		SYS_LOG_DBG("USB device reset detected");
		break;
	case USB_DC_CONNECTED:
		SYS_LOG_DBG("USB device connected");
		break;
	case USB_DC_CONFIGURED:
		SYS_LOG_DBG("USB device configured");
		break;
	case USB_DC_DISCONNECTED:
		SYS_LOG_DBG("USB device disconnected");
		break;
	case USB_DC_SUSPEND:
		SYS_LOG_DBG("USB device suspended");
		break;
	case USB_DC_RESUME:
		SYS_LOG_DBG("USB device resumed");
		break;
	case USB_DC_UNKNOWN:
	default:
		SYS_LOG_DBG("USB unknown state");
		break;
	}
}

static int hid_class_handle_req(struct usb_setup_packet *setup,
				s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("Class request: bRequest 0x%x bmRequestType 0x%x len %d",
		    setup->bRequest, setup->bmRequestType, *len);

	if (REQTYPE_GET_DIR(setup->bmRequestType) == REQTYPE_DIR_TO_HOST) {
		switch (setup->bRequest) {
		case HID_GET_REPORT:
			SYS_LOG_DBG("Get Report");
			if (hid_device.ops->get_report) {
				return hid_device.ops->get_report(setup, len,
								  data);
			} else {
				SYS_LOG_ERR("Mandatory request not supported");
				return -EINVAL;
			}
			break;
		default:
			SYS_LOG_ERR("Unhandled request 0x%x", setup->bRequest);
			break;
		}
	} else {
		switch (setup->bRequest) {
		case HID_SET_IDLE:
			SYS_LOG_DBG("Set Idle");
			if (hid_device.ops->set_idle) {
				return hid_device.ops->set_idle(setup, len,
								data);
			}
			break;
		case HID_SET_REPORT:
			if (hid_device.ops->set_report == NULL) {
				SYS_LOG_ERR("set_report not implemented");
				return -EINVAL;
			}
			return hid_device.ops->set_report(setup, len, data);
		default:
			SYS_LOG_ERR("Unhandled request 0x%x", setup->bRequest);
			break;
		}
	}

	return -ENOTSUP;
}

static int hid_custom_handle_req(struct usb_setup_packet *setup,
				s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("Standard request: bRequest 0x%x bmRequestType 0x%x len %d",
		    setup->bRequest, setup->bmRequestType, *len);

	if (REQTYPE_GET_DIR(setup->bmRequestType) == REQTYPE_DIR_TO_HOST &&
	    REQTYPE_GET_RECIP(setup->bmRequestType) ==
					REQTYPE_RECIP_INTERFACE &&
					setup->bRequest == REQ_GET_DESCRIPTOR) {
		switch (setup->wValue) {
		case 0x2200:
			SYS_LOG_DBG("Return Report Descriptor");

			/* Some buggy system may be pass a larger wLength when
			 * it try read HID report descriptor, although we had
			 * already tell it the right descriptor size.
			 * So truncated wLength if it doesn't match. */
			if (*len != hid_device.report_size) {
				SYS_LOG_WRN("len %d doesn't match"
					    "Report Descriptor size", *len);
				*len = min(*len, hid_device.report_size);
			}
			*data = (u8_t *)hid_device.report_desc;
			break;
		default:
			return -ENOTSUP;
		}

		return 0;
	}

	return -ENOTSUP;
}

static void hid_int_in(u8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	if (ep_status != USB_DC_EP_DATA_IN ||
	    hid_device.ops->int_in_ready == NULL) {
		return;
	}
	hid_device.ops->int_in_ready();
}

/* Describe Endpoints configuration */
static struct usb_ep_cfg_data hid_ep_data[] = {
	{
		.ep_cb = hid_int_in,
		.ep_addr = CONFIG_HID_INT_EP_ADDR
	}
};

static struct usb_cfg_data hid_config = {
	.usb_device_description = NULL,
	.cb_usb_status = hid_status_cb,
	.interface = {
		.class_handler = hid_class_handle_req,
		.custom_handler = hid_custom_handle_req,
		.payload_data = NULL,
	},
	.num_endpoints = NUMOF_ENDPOINTS_HID,
	.endpoint = hid_ep_data,
};

#if !defined(CONFIG_USB_COMPOSITE_DEVICE)
static u8_t interface_data[64];
#endif

int usb_hid_init(void)
{
	int ret;

	SYS_LOG_DBG("Iinitializing HID Device");

	/*
	 * Modify Report Descriptor Size
	 */
	usb_set_hid_report_size(hid_device.report_size);

#ifdef CONFIG_USB_COMPOSITE_DEVICE
	ret = composite_add_function(&hid_config, FIRST_IFACE_HID);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to add HID function");
		return ret;
	}
#else
	hid_config.interface.payload_data = interface_data;
	hid_config.usb_device_description = usb_get_device_descriptor();

	/* Initialize the USB driver with the right configuration */
	ret = usb_set_config(&hid_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to config USB");
		return ret;
	}

	/* Enable USB driver */
	ret = usb_enable(&hid_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to enable USB");
		return ret;
	}
#endif

	return 0;
}

void usb_hid_register_device(const u8_t *desc, size_t size,
			     const struct hid_ops *ops)
{
	hid_device.report_desc = desc;
	hid_device.report_size = size;

	hid_device.ops = ops;
}
