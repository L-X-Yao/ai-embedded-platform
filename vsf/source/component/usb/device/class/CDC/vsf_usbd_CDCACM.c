/*****************************************************************************
 *   Copyright(C)2009-2022 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/

/*============================ INCLUDES ======================================*/

#include "component/usb/vsf_usb_cfg.h"

#if VSF_USE_USB_DEVICE == ENABLED && VSF_USBD_USE_CDCACM == ENABLED && VSF_USBD_CFG_RAW_MODE != ENABLED

#define __VSF_USBD_CLASS_INHERIT__
#define __VSF_USBD_CDC_CLASS_INHERIT__
#define __VSF_USBD_CDCACM_CLASS_IMPLEMENT

#include "kernel/vsf_kernel.h"
#include "../../vsf_usbd.h"
#include "./vsf_usbd_CDCACM.h"

/*============================ MACROS ========================================*/
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ PROTOTYPES ====================================*/

static vsf_err_t __vk_usbd_cdcacm_data_init(vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs);
static vsf_err_t __vk_usbd_cdcacm_control_request_prepare(
        vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs);
static vsf_err_t __vk_usbd_cdcacm_control_request_process(
        vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs);

/*============================ GLOBAL VARIABLES ==============================*/

const vk_usbd_class_op_t vk_usbd_cdcacm_control =
{
    .request_prepare = __vk_usbd_cdcacm_control_request_prepare,
    .request_process = __vk_usbd_cdcacm_control_request_process,
};

const vk_usbd_class_op_t vk_usbd_cdcacm_data =
{
    .init = __vk_usbd_cdcacm_data_init,
};

/*============================ LOCAL VARIABLES ===============================*/
/*============================ IMPLEMENTATION ================================*/

static vsf_err_t __vk_usbd_cdcacm_data_init(vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs)
{
    vk_usbd_cdcacm_t *acm = ifs->class_param;

    acm->control_line = 0;
    if (    (acm->callback.set_line_coding != NULL)
        &&  (VSF_ERR_NONE != acm->callback.set_line_coding(&acm->line_coding))) {
        return VSF_ERR_FAIL;
    }
    return vk_usbd_cdc_data.init(dev, ifs);
}

static vsf_err_t __vk_usbd_cdcacm_control_request_prepare(
        vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs)
{
    vk_usbd_cdcacm_t *acm = ifs->class_param;
    vk_usbd_ctrl_handler_t *ctrl_handler = &dev->ctrl_handler;
    struct usb_ctrlrequest_t *request = &ctrl_handler->request;
    usb_cdcacm_line_coding_t *line_coding = &acm->line_coding;
    uint8_t *buffer = NULL;
    uint_fast32_t size = 0;

    switch (request->bRequest) {
    case USB_CDCACM_REQ_SET_LINE_CODING:
        if ((request->wLength != 7) || (request->wValue != 0)) {
            return VSF_ERR_FAIL;
        }

        buffer = (uint8_t *)line_coding;
        size = 7;
        break;
    case USB_CDCACM_REQ_GET_LINE_CODING:
        if ((request->wLength != 7) || (request->wValue != 0)) {
            return VSF_ERR_FAIL;
        }

        cpu_to_le32s(&acm->line_coding.bitrate);
        buffer = (uint8_t *)line_coding;
        size = 7;
        break;
    case USB_CDCACM_REQ_SET_CONTROL_LINE_STATE:
        if (    (request->wLength != 0)
            ||  (request->wValue & ~USB_CDCACM_CONTROLLINE_MASK)) {
            return VSF_ERR_FAIL;
        }

        acm->control_line = (uint8_t)request->wValue;
        if (    (acm->callback.set_control_line != NULL)
            &&  (VSF_ERR_NONE != acm->callback.set_control_line(acm->control_line))) {
            return VSF_ERR_FAIL;
        }
        break;
    case USB_CDCACM_REQ_SEND_BREAK:
        if (    (request->wLength != 0)
            ||  (   (acm->callback.send_break != NULL)
                &&  acm->callback.send_break())) {
            return VSF_ERR_FAIL;
        }
        break;
    default:
        return vk_usbd_cdc_control.request_prepare(dev, ifs);
    }

    ctrl_handler->trans.use_as__vsf_mem_t.buffer = buffer;
    ctrl_handler->trans.use_as__vsf_mem_t.size = size;
    return VSF_ERR_NONE;
}

static vsf_err_t __vk_usbd_cdcacm_control_request_process(
        vk_usbd_dev_t *dev, vk_usbd_ifs_t *ifs)
{
    vk_usbd_cdcacm_t *acm = ifs->class_param;
    vk_usbd_ctrl_handler_t *ctrl_handler = &dev->ctrl_handler;
    struct usb_ctrlrequest_t *request = &ctrl_handler->request;

    switch (request->bRequest) {
    case USB_CDCACM_REQ_SET_LINE_CODING:
        le32_to_cpus(&acm->line_coding.bitrate);
        if (    (acm->callback.set_line_coding != NULL)
            &&  (VSF_ERR_NONE != acm->callback.set_line_coding(&acm->line_coding))) {
            return VSF_ERR_FAIL;
        }
        vk_usbd_cdc_data_connect(&acm->use_as__vk_usbd_cdc_t);
        break;
    case USB_CDCACM_REQ_GET_LINE_CODING:
        le32_to_cpus(&acm->line_coding.bitrate);
    case USB_CDCACM_REQ_SET_CONTROL_LINE_STATE:
    case USB_CDCACM_REQ_SEND_BREAK:
        break;
    default:
        return vk_usbd_cdc_control.request_process(dev, ifs);
    }
    return VSF_ERR_NONE;
}

#endif  // VSF_USE_USB_DEVICE && VSF_USBD_USE_CDCACM
