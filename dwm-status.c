#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#if HAVE_NVIDIA_GPU
#include <nvml.h>
#endif

#include "config.h"

static const char* prog_name;
static int volume;
static const char* volume_sym;

static void error(const char* msg, ...) {
   va_list ap;
   va_start(ap, msg);

   fprintf(stderr, "%s: ", prog_name);
   vfprintf(stderr, msg, ap);
   fprintf(stderr, ": %s\n", strerror(errno));

   va_end(ap);
}

static bool xsetroot(const char* s) {
   pid_t pid = fork();
   if (pid < 0) {
      error("failed to fork() for xsetroot");
      exit(1);
   }
   if (pid == 0) {
      execlp("xsetroot", "xsetroot", "-name", s, NULL);
      error("failed to invoke xsetroot");
      _exit(1);
   } else {
      int wstatus;
      waitpid(pid, &wstatus, 0);
      if (!WIFEXITED(wstatus) || wstatus != 0) {
         error("xsetroot failed");
         return false;
      }
      return true;
   }
}

static long double vldsum(size_t num, long double a[]) {
   long double sum = 0.0l;
   for (size_t i = 0; i < num; ++i)
      sum += a[i];
   return sum;
}
static int pscanf(const char* path, const char* fmt, ...) {
   FILE* file = fopen(path, "r");
   if (!file) {
      error("failed to open '%s'", path);
      return -1;
   }

   va_list ap;
   va_start(ap, fmt);

   const int n = vfscanf(file, fmt, ap);


   va_end(ap);
   fclose(file);
   return (n == EOF) ? -1 : n;
}

static int cpu_perc(void) {
   static long double a[7];
   long double b[7];

   memcpy(b, a, sizeof(b));
   if (pscanf("/proc/stat", "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf",
            &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]) != 7) return -1;
   if (b[0] == 0) return 0;

   const long double sum = vldsum(7, b) - vldsum(7, a);
   return sum ? (int)(100 * ((b[0] + b[1] + b[2] + b[5] + b[6]) - (a[0] + a[1] + a[2] + a[5] + a[6])) / sum) : 0;
}
static int ram_perc(void) {
   uintmax_t total, free, buffers, cached;
   if (pscanf("/proc/meminfo",
            "MemTotal: %ju kB\n"
            "MemFree: %ju kB\n"
            "MemAvailable: %ju kB\n"
            "Buffers: %ju kB\n"
            "Cached: %ju kB\n",
            &total, &free, &buffers, &buffers, &cached) != 5)
      return -1;
   if (!total) return 0;
   return 100 * ((total - free) - (buffers + cached)) / total;
}
static int vol_perc(void) {
   FILE* file = popen("pulsemixer --get-volume", "r");
   if (!file) return error("failed to invoke pulsemixer"), -1;
   int left, right;
   if (fscanf(file, "%d %d\n", &left, &right) != 2) return error("failed to read volume"), 0;
   pclose(file);
   return (left + right) / 2;
}
static int bat_perc(void) {
   int perc;
   if (pscanf("/sys/class/power_supply/" BAT_NAME "/capacity", "%d\n", &perc) != 1) return -1;
   return perc;
}
static const char* vol_sym(int volume) {
   int muted;
   FILE* file = popen("pulsemixer --get-mute", "r");
   if (!file) muted = 0;
   else {
      if (fscanf(file, "%d\n", &muted) != 1) muted = 0;
      pclose(file);
   }

   if (muted) return "";
   else if (volume < 25) return "";
   else if (volume < 50) return "";
   else return "";
}
static const char* bat_sym(int bat) {
   char status[32];
   if (pscanf("/sys/class/power_supply/" BAT_NAME "/status", "%s\n", &status) == 1
         && strcmp(status, "Charging") == 0) return "\uf5e7";

   if (bat < 20) return "\uf244";
   else if (bat < 40) return "\uf243";
   else if (bat < 60) return "\uf242";
   else if (bat < 80) return "\uf241";
   else return "\uf240";
}
static const char* net_sym(void) {
   pid_t pid = fork();
   if (pid < 0) return error("failed to fork"), "-";
   if (pid == 0) {
      close(1);
      open("/dev/null", O_WRONLY);
      execlp("ping", "ping", "-c", "1", "1.1.1.1", NULL);
      error("failed to execlp");
      _exit(1);
   }
   int wstatus;
   waitpid(pid, &wstatus, 0);
   if (WIFEXITED(wstatus)) {
      if (wstatus) return "-";
      else return "\uf1eb";
   } else return "-";
}
static void* update_ns(void* ns) {
   while (1) {
      sleep(network_delay);
      *(const char**)ns = net_sym();
   }
   return NULL;
}

static void signal_handler(int signum) {
   (void)signum;
   puts("Interrupted!");
   volume = vol_perc();
   volume_sym = vol_sym(volume);
}

#if HAVE_NVIDIA_GPU
static nvmlDevice_t nvidia_gpu;
static bool init_nvml(void) {
   nvmlReturn_t result;
   unsigned dev_count;

   result = nvmlInit();
   if (result != NVML_SUCCESS) {
      fprintf(stderr, "dwm-status: failed to initialize NVML: %s\n", nvmlErrorString(result));
      return false;
   }
   result = nvmlDeviceGetCount(&dev_count);
   if (result != NVML_SUCCESS) {
      fprintf(stderr, "dwm-status: failed to get device count: %s\n", nvmlErrorString(result));
      nvmlShutdown();
      return false;
   }
   if (dev_count < 1) return nvmlShutdown(), false;
   result = nvmlDeviceGetHandleByIndex(0, &nvidia_gpu);
   if (result != NVML_SUCCESS) {
      fprintf(stderr, "dwm-status: failed to get GPU 0: %s\n", nvmlErrorString(result));
      nvmlShutdown();
      return false;
   }
   char device_name[NVML_DEVICE_NAME_BUFFER_SIZE];
   result = nvmlDeviceGetName(nvidia_gpu, device_name, NVML_DEVICE_NAME_BUFFER_SIZE);
   if (result == NVML_SUCCESS) {
      printf("GPU 0: %s\n", device_name);
   } else fprintf(stderr, "dwm-status: failed to get GPU 0 name: %s\n", nvmlErrorString(result));
   return true;
}
static bool nvidia_gpu_usage(int* gpu, int* mem) {
   nvmlUtilization_t dev_util;
   nvmlReturn_t result = nvmlDeviceGetUtilizationRates(nvidia_gpu, &dev_util);
   if (result != NVML_SUCCESS) {
      fprintf(stderr, "dwm-status: failed to get GPU utilization: %s\n", nvmlErrorString(result));
      return false;
   }
   *gpu = dev_util.gpu;
   *mem = dev_util.memory;
   return true;
}
#endif

int main(int argc, char* argv[]) {
   (void)argc;
   const struct timespec sleep_time = { 0, (long)((long double)refresh_delay * 1000000000.0l) };
   unsigned num_fails = 0;
   char buffer[256];
   signal(SIGUSR1, signal_handler);
   signal(SIGHUP,  signal_handler);

   prog_name = strrchr(argv[0], '/');
   if (prog_name) ++prog_name;
   else prog_name = __FILE__;
  
#if HAVE_NVIDIA_GPU
   const bool has_gpu = init_nvml();
#endif

   const char* ns = net_sym();
   pthread_t thr_net;
   pthread_create(&thr_net, NULL, &update_ns, (void*)&ns);
   volume = vol_perc();
   volume_sym = vol_sym(volume);
   while (1) {
      const time_t now = time(NULL);
      const int bat = bat_perc();
      size_t n;
#if HAVE_NVIDIA_GPU
      int gpu_perc, gpu_mem;
      if (has_gpu && nvidia_gpu_usage(&gpu_perc, &gpu_mem)) {
         n = (size_t)snprintf(buffer, sizeof(buffer), "[CPU \uf2db %d%%] [GPU \uf2db %d%%/%d%%] [RAM \uf538 %d%%] [VOL %s %d%%] [BAT %s %d%%] %s ",
               cpu_perc(), gpu_perc, gpu_mem, ram_perc(), volume_sym, volume, bat_sym(bat), bat, ns);
      } else
#endif
      n = (size_t)snprintf(buffer, sizeof(buffer), "[CPU \uf2db %d%%] [RAM \uf538 %d%%] [VOL %s %d%%] [BAT %s %d%%] %s ",
            cpu_perc(), ram_perc(), volume_sym, volume, bat_sym(bat), bat, ns);
      strftime(buffer + n, sizeof(buffer) - n, date_format, localtime(&now));

      if (xsetroot(buffer)) num_fails = 0;
      else {
         if (num_fails >= max_fails) return 1;
         else ++num_fails;
      }
      nanosleep(&sleep_time, NULL);
   }
}
