CC   = gcc
CXX  = g++
CFLAGS   = -Wall -O2 -std=c11 -pthread -Wno-stringop-truncation -Wno-format-truncation -Wno-format-extra-args
CXXFLAGS = -Wall -O2 -std=c++11 -pthread -Wno-stringop-truncation
LDFLAGS  = -lm

C_SRCS = main.c menu.c adb.c adb_native.c adb_ext.c adb_raw.c adb_sync.c predictor.c bypass_exploit.c pattern_unlock.c recovery_tools.c brute_adv.c detect.c detect_fast.c force.c autorun.c network.c sysfs.c database.c service.c hardware.c account.c telephony.c brute.c usb.c util.c safe.c extra.c devices.c debug.c bypass.c pincrack.c chip.c panel.c cam.c snd.c pwr.c radio.c mem.c sec.c sens.c allprop.c filesys.c multi_device.c platform.c cpu.c fastboot.c usb_raw.c usb_mode.c deep_scan.c engine_connect.c input_precision.c screen_watch.c perf_log.c connection_mgr.c recovery_exploit.c usb_hid_brute.c usb_gadget_exploit.c adb_auth_bypass.c fastboot_exploit.c usb_stack_exploit.c lock_screen_bypass.c kernel_direct.c usb_device_raw.c touch_fb_exploit.c gpio_button_exploit.c i2c_bus_exploit.c spmi_exploit.c devmem_exploit.c sysfs_hw_exploit.c pinctrl_exploit.c
CPP_SRCS = bruteforce_turbo.cpp db_parser.cpp pattern_ai.cpp device_fingerprint.cpp pin_ml.cpp pin_ai.cpp usb_attack.cpp strategy_engine.cpp adaptive_timer.cpp device_profile.cpp progress_engine.cpp hardware_probe.cpp data_forensics.cpp exploit_chain.cpp neural_cracker.cpp security_bypass.cpp engine_bridge.cpp kernel_exploit_orchestrator.cpp hw_control_expert.cpp gesture_synthesis.cpp pin_entry_expert.cpp soc_exploit_engine.cpp trustzone_pwn.cpp hardware_scanner_deep.cpp auto_pwn_chain.cpp
C_OBJS   = $(C_SRCS:.c=.o)
CPP_OBJS = $(CPP_SRCS:.cpp=.o)
OBJS     = $(C_OBJS) $(CPP_OBJS)
TARGET   = brutepin

# Cross-compilation targets
#   make            -> Linux (default)
#   make windows    -> Windows (mingw-w64 cross)
#   make mac        -> macOS (osxcross)
#   make all        -> all three

linux: CFLAGS   = -Wall -O2 -std=c11 -pthread -Wno-stringop-truncation -Wno-format-truncation -Wno-format-extra-args
linux: CXXFLAGS = -Wall -O2 -std=c++11 -pthread -Wno-stringop-truncation
linux: LDFLAGS  = -lm
linux: $(TARGET)

windows: CC   = x86_64-w64-mingw32-gcc
windows: CXX  = x86_64-w64-mingw32-g++
windows: CFLAGS   = -Wall -O2 -std=c11 -pthread -D_WIN32_WINNT=0x0601
windows: CXXFLAGS = -Wall -O2 -std=c++11 -pthread -Wno-stringop-truncation
windows: LDFLAGS  = -lm -lwinusb -lsetupapi -lws2_32
windows: TARGET   = brutepin.exe
windows: $(TARGET)

mac: CC   = o64-clang
mac: CXX  = o64-clang++
mac: CFLAGS   = -Wall -O2 -std=c11 -pthread -Wno-stringop-truncation
mac: CXXFLAGS = -Wall -O2 -std=c++11 -pthread -Wno-stringop-truncation
mac: LDFLAGS  = -lm -framework IOKit -framework CoreFoundation
mac: $(TARGET)

all: linux windows mac

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) brutepin.exe

install: $(TARGET)
	cp $(TARGET) ../

.PHONY: all clean install linux windows mac
