#include <stdio.h>
#include <Windows.h>
#include "libusb.h"

static void start_timer();
static void log_timer();
static int query_devices(libusb_context *ctx);

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
  
  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);

  start_timer();

  if(!query_devices(ctx))
  {
    ret = 2;
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

static int query_devices(libusb_context *ctx)
{
  libusb_device **devs;
  ssize_t num = libusb_get_device_list(ctx, &devs);

  if(num < 0)
    return 0;

  for(ssize_t i = 0; i < num; i++)
  {
    libusb_device *dev = devs[i];

    struct libusb_device_descriptor desc;
    if(libusb_get_device_descriptor(dev, &desc) != 0)
      printf("%04X %04X\n", desc.idVendor, desc.idProduct);
  }

  libusb_free_device_list(devs, 1);

  return 1;
}

