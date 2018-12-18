#include "../device.h"
#include "../utility.h"

#include <hidapi.h>
#include <string.h>

enum hs70_battery_flags {
    HS70_BATTERY_MICUP = 128
};

static struct device device_hs70;

static int hs70_send_sidetone(hid_device *device_handle, uint8_t num);
static int hs70_request_battery(hid_device *device_handle);
static int hs70_notification_sound(hid_device *hid_device, uint8_t soundid);

void hs70_init(struct device** device)
{
    device_hs70.idVendor = VENDOR_CORSAIR;
    device_hs70.idProduct = 0x0a38;

    strcpy(device_hs70.device_name, "Corsair HS70");

    device_hs70.capabilities = CAP_SIDETONE | CAP_BATTERY_STATUS | CAP_NOTIFICATION_SOUND;
    device_hs70.send_sidetone = &hs70_send_sidetone;
    device_hs70.request_battery = &hs70_request_battery;
    device_hs70.notifcation_sound = &hs70_notification_sound;

    *device = &device_hs70;
}

static int hs70_send_sidetone(hid_device *device_handle, uint8_t num)
{
    // the range of the hs70 seems to be from 200 to 255
    num = map(num, 0, 128, 200, 255);

    unsigned char data[12] = {0xFF, 0x0B, 0, 0xFF, 0x04, 0x0E, 0xFF, 0x05, 0x01, 0x04, 0x00, num};

    return hid_send_feature_report(device_handle, data, 12);
}

static int hs70_request_battery(hid_device *device_handle)
{
    // Packet Description
    // Answer of battery status
    // Index    0   1   2       3       4
    // Data     100 0   Loaded% 177     5 when loading, 0 when loading and off, 1 otherwise
    //
    // Loaded% has bitflag HS70_BATTERY_MICUP set when mic is in upper position

    int r = 0;

    // request battery status
    unsigned char data_request[2] = {0xC9, 0x64};

    r = hid_write(device_handle, data_request, 2);

    if (r < 0) return r;

    // read battery status
    unsigned char data_read[5];

    r = hid_read(device_handle, data_read, 5);

    if (r < 0) return r;

    if (data_read[4] == 0 || data_read[4] == 4 || data_read[4] == 5)
    {
        return BATTERY_LOADING;
    }
    else if (data_read[4] == 1)
    {
        // Discard HS70_BATTERY_MICUP when it's set
        // see https://github.com/Sapd/HeadsetControl/issues/13
        if (data_read[2] & HS70_BATTERY_MICUP)
            return data_read[2] &~ HS70_BATTERY_MICUP;
        else
            return (int)data_read[2]; // battery status from 0 - 100
    }
    else
    {
        return -100;
    }
}

static int hs70_notification_sound(hid_device* device_handle, uint8_t soundid)
{
    // soundid can be 0 or 1
    unsigned char data[5] = {0xCA, 0x02, soundid};

    return hid_write(device_handle, data, 3);
}
