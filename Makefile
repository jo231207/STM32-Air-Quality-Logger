BUILD_PRESET ?= Debug
PROJECT_NAME ?= PMS7003_OLED_I2C
ELF := build/$(BUILD_PRESET)/$(PROJECT_NAME).elf
PROGRAMMER ?= STM32_Programmer_CLI.exe
PROGRAMMER_FLAGS ?= -c port=SWD -d "$(ELF)" -v -rst

.PHONY: all configure build clean flash

all: build

configure:
	cmake --preset $(BUILD_PRESET)

build: configure
	cmake --build --preset $(BUILD_PRESET)

clean:
	cmake --build --preset $(BUILD_PRESET) --target clean

flash: build
	$(PROGRAMMER) $(PROGRAMMER_FLAGS)
