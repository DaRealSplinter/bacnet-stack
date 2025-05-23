/**
 * @file
 * @brief BACnet stack initialization and task processing
 * @author Steve Karg
 * @date 2011
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdbool.h>
/* hardware layer includes */
#include "bacnet/basic/sys/mstimer.h"
#include "rs485.h"
#include "program-ubasic.h"
/* BACnet Stack includes */
#include "bacnet/datalink/datalink.h"
#include "bacnet/npdu.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datetime.h"
#include "bacnet/dcc.h"
#include "bacnet/iam.h"
/* BACnet objects */
#include "bacnet/basic/object/device.h"
/* me */
#include "bacnet.h"

/* timer for device communications control */
static struct mstimer DCC_Timer;
#define DCC_CYCLE_SECONDS 1
/* Device ID to track changes */
static uint32_t Device_ID = 0xFFFFFFFF;

/**
 * @brief Initialize the BACnet device object, the service handlers, and timers
 */
void bacnet_init(void)
{
    /* initialize objects */
    Device_Init(NULL);
    /* set up our confirmed service unrecognized service handler - required! */
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_REINITIALIZE_DEVICE, handler_reinitialize_device);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    /* local time and date */
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION, handler_timesync);
    handler_timesync_set_callback_set(datetime_timesync);
    datetime_init();
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* start the cyclic 1 second timer for DCC */
    mstimer_set(&DCC_Timer, DCC_CYCLE_SECONDS * 1000);
}

/* local buffer for incoming PDUs to process */
static uint8_t PDUBuffer[MAX_MPDU];

/**
 * @brief non-blocking BACnet task
 */
void bacnet_task(void)
{
    bool hello_world = false;
    uint16_t pdu_len = 0;
    BACNET_ADDRESS src = { 0 };

    /* hello, World! */
    if (Device_ID != Device_Object_Instance_Number()) {
        Device_ID = Device_Object_Instance_Number();
        hello_world = true;
    }
    if (hello_world) {
        Send_I_Am(&Handler_Transmit_Buffer[0]);
    }
    /* handle the communication timer */
    if (mstimer_expired(&DCC_Timer)) {
        mstimer_reset(&DCC_Timer);
        dcc_timer_seconds(DCC_CYCLE_SECONDS);
    }
    /* handle the messaging */
    pdu_len = datalink_receive(&src, &PDUBuffer[0], sizeof(PDUBuffer), 0);
    if (pdu_len) {
        npdu_handler(&src, &PDUBuffer[0], pdu_len);
    }
}
