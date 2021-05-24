#include <stdio.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <Usbiodef.h>
#include "libusb.h"

static void start_timer();
static void log_timer();
static int query_devices_libusb(libusb_context *ctx);
static int query_devices_win();

static LARGE_INTEGER start_time;

int main(int argc, const char *argv[])
{
  int ret = 0;
  libusb_context *ctx = NULL;

  printf("Hello\n");

  if(libusb_init(&ctx) != 0)
  {
    ret = 1;
    goto end;
  }
  
  //libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_ERROR);

  start_timer();
  if(!query_devices_libusb(ctx))
  {
    ret = 2;
    goto end;
  }
  log_timer();

  start_timer();
  if(!query_devices_win(ctx))
  {
    ret = 3;
    goto end;
  }
  log_timer();

end:
  if(ctx)
  {
    libusb_exit(ctx);
    ctx = NULL;
  }

  printf("exit %d\n", ret);

  return 0;
}


static void start_timer()
{
  QueryPerformanceCounter(&start_time);
  printf("start\n");
}


static void log_timer()
{
  LARGE_INTEGER current_time;
  QueryPerformanceCounter(&current_time);

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);

  LARGE_INTEGER diff_time;
  diff_time.QuadPart = current_time.QuadPart - start_time.QuadPart;
  diff_time.QuadPart *= 1000000;
  diff_time.QuadPart /= freq.QuadPart;
  diff_time.QuadPart /= 1000;

  printf("time %lldms\n", diff_time.QuadPart);
}

static int query_devices_libusb(libusb_context *ctx)
{
  libusb_device **devs;
  ssize_t num = libusb_get_device_list(ctx, &devs);

  if(num < 0)
    return 0;

  for(ssize_t i = 0; i < num; i++)
  {
    libusb_device *dev = devs[i];

    struct libusb_device_descriptor desc;
    if(libusb_get_device_descriptor(dev, &desc) == 0)
      printf("%04X %04X %d-%d\n", desc.idVendor, desc.idProduct, libusb_get_bus_number(dev), libusb_get_device_address(dev));
  }

  libusb_free_device_list(devs, 1);

  return 1;
}





static int get_usb_device_address(
  HDEVINFO DeviceInfoSet,
  PSP_DEVINFO_DATA DeviceInfoData,
  const GUID *iface_guid,
  unsigned index
)
{
  SP_DEVICE_INTERFACE_DATA devIfaceData;
  memset(&devIfaceData, 0, sizeof(devIfaceData));
  devIfaceData.cbSize = sizeof(devIfaceData);

  if(!SetupDiEnumDeviceInterfaces(
        DeviceInfoSet,
        NULL,
        iface_guid,
        index,
        &devIfaceData))
    return -1;
  
  SP_DEVICE_INTERFACE_DETAIL_DATA_A *ifaceDetail = NULL;
  DWORD size = 0;
  SetupDiGetDeviceInterfaceDetailA(
    DeviceInfoSet, &devIfaceData,
    NULL, 0, &size, NULL);
  
  if(!size);
    return -2;
  
  ifaceDetail = malloc(size);
  memset(ifaceDetail, 0, size);
  ifaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  if(!SetupDiGetDeviceInterfaceDetailA(
        DeviceInfoSet, &devIfaceData,
        ifaceDetail, size, NULL, NULL))
  {
    free(ifaceDetail);
    return -3;
  }


  return 0;
}



/*  Wrapper around SetupDiGetDeviceRegistryPropertyA
    Returns a string which needs to be free()'d
    or NULL on error
    mutli_arch can be NULL or a match to the start of a string.
    If REG_MULTI_SZ is returned, then multi_arch is used to find the right string. */
static char *get_setup_reg_property_str(
  HDEVINFO DeviceInfoSet,
  PSP_DEVINFO_DATA DeviceInfoData,
  DWORD Property,
  const char *multi_match)
{
  BOOL res;
  DWORD size = 0;
  DWORD type = 0;
  char *str = NULL;

  SetupDiGetDeviceRegistryPropertyA(
    DeviceInfoSet, DeviceInfoData, Property,
    &type, NULL, 0, &size);

  if(!size || (type != REG_SZ && type != REG_MULTI_SZ))
    return NULL;

  str = malloc(size);
  memset(str, 0, size);
  
  res = SetupDiGetDeviceRegistryPropertyA(
          DeviceInfoSet, DeviceInfoData, Property,
          NULL, str, size, NULL);
  
  if(!res)
  {
    free(str);
    return NULL;
  }

  if(type == REG_SZ && (!multi_match || strstr(str, multi_match) == str))
    return str;
  
  if(type == REG_MULTI_SZ)
  {
    if(!multi_match)
      return str;
    
    for(unsigned i = 0; str[i] != '\0'; i += strlen(&(str[i]))+1)
    {
      char *s = &(str[i]);
      if(strstr(s, multi_match) == s)
      {
        s = strdup(s);
        free(str);
        return s;
      }
    }
  }

  free(str);
  return NULL;
}


struct usb_hcd {
  int bus_number;
  char *location_path;
};


DEFINE_GUID(GUID_IFACE_HCD, 0x3ABF6F2D, 0x71C4, 0x462A, 0x8A, 0x92, 0x1E, 0x68, 0x61, 0xE6, 0xAF, 0x27);
DEFINE_GUID(GUID_IFACE_USB, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);
DEFINE_GUID(GUID_IFACE_WINUSB, 0xdee824ef, 0x729b, 0x4a0e, 0x9c, 0x14, 0xb7, 0x11, 0x7d, 0x33, 0xa8, 0x17);
DEFINE_GUID(GUID_IFACE_SHOWXPRESS, 0x9ef5175d, 0x990d, 0x48be, 0xa5, 0x84, 0x57, 0x32, 0x0b, 0x3d, 0xc2, 0xe8);

static int query_devices_win()
{
  int ok = 1;
  HDEVINFO devInfo = NULL;
  SP_DEVINFO_DATA devData;
#define NUM_HCDS 10
  struct usb_hcd hcds[NUM_HCDS];

  memset(hcds, 0, sizeof(hcds));

  // Build list of host controllers
  {
    devInfo = SetupDiGetClassDevs(&GUID_IFACE_HCD, NULL, NULL, DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);

    if(devInfo == INVALID_HANDLE_VALUE)
    {
      ok = 0;
      goto end;
    }
    
    for(int i = 0; i < NUM_HCDS; i++)
    {
      memset(&devData, 0, sizeof(devData));
      devData.cbSize = sizeof(devData);

      if(!SetupDiEnumDeviceInfo(devInfo, i, &devData))
      {
        if(GetLastError() == ERROR_NO_MORE_ITEMS)
          break;
        
        continue;
      }

      // location_path will look something like "PCIROOT(0)#PCI(1D00)"
      char *location_path = get_setup_reg_property_str(devInfo, &devData, SPDRP_LOCATION_PATHS, "PCIROOT");
      if(!location_path)
      {
        free(location_path);
        ok = 0;
        goto end;
      }

      struct usb_hcd hcd = {
        .bus_number = i + 1,
        .location_path = strdup(location_path)
      };

      printf("Bus %d %s\n", hcd.bus_number, hcd.location_path);

      hcds[i] = hcd;
    }
    
    SetupDiDestroyDeviceInfoList(devInfo);
    devInfo = NULL;
  }

  // Build list of USB devices
  {
    const GUID *iface_guid = &GUID_IFACE_USB;
    devInfo = SetupDiGetClassDevs(iface_guid, NULL, NULL, DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
    //devInfo = SetupDiGetClassDevsA(NULL, "USB", NULL, DIGCF_ALLCLASSES|DIGCF_PRESENT);

    if(devInfo == INVALID_HANDLE_VALUE)
    {
      ok = 0;
      goto end;
    }

    for(int i = 0; ; i++)
    {
      memset(&devData, 0, sizeof(devData));
      devData.cbSize = sizeof(devData);

      if(!SetupDiEnumDeviceInfo(devInfo, i, &devData))
      {
        if(GetLastError() == ERROR_NO_MORE_ITEMS)
          break;
        
        continue;
      }

      // location_path will look like "PCIROOT(0)#PCI(1D00)#USBROOT(0)#USB(1)#USB(1)"
      // Find out which bus number by matching the start of the string to a HCD
      char *location_path = get_setup_reg_property_str(devInfo, &devData, SPDRP_LOCATION_PATHS, "PCIROOT");

      int bus_number = -1;
      for(int j = 0; location_path && (j < NUM_HCDS); j++)
      {
        if(strstr(location_path, hcds[j].location_path) == location_path)
        {
          bus_number = hcds[j].bus_number;
          break;
        }
      }

      free(location_path);
      location_path = NULL;

      if(bus_number < 0)
        continue;

      char *hardware_id = get_setup_reg_property_str(devInfo, &devData, SPDRP_HARDWAREID, "USB\\VID_");
      unsigned idVendor = 0;
      unsigned idProduct = 0;
      unsigned bcdDevice = 0;
      int n = 0;

      if(hardware_id)
         n = sscanf(hardware_id, "USB\\VID_%x&PID_%x&REV_%x", &idVendor, &idProduct, &bcdDevice);
      
      free(hardware_id);
      hardware_id = NULL;

      if(n != 3 || !idVendor || !idProduct)
        continue;
      
      int device_address = get_usb_device_address(devInfo, &devData, iface_guid, i);

      printf("%04X %04X %d-%d\n", idVendor, idProduct, bus_number, device_address);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    devInfo = NULL;
  }

end:
  if(devInfo != NULL && devInfo != INVALID_HANDLE_VALUE)
    SetupDiDestroyDeviceInfoList(devInfo);
  
  for(int i = 0; i < sizeof(hcds)/sizeof(hcds[0]); i++)
    if(hcds[i].location_path)
      free(hcds[i].location_path);
  
  return ok;
}

