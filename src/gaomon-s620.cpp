#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <libusb-1.0/libusb.h>

#include "gaomon-s620.hpp"

#define DUMP_HEX(var) {	\
	for (int __dump_hex_i = 0; __dump_hex_i < sizeof(var); __dump_hex_i++) 	\
		printf("%02hhx", ((char *) &var)[__dump_hex_i]);		\
}

libusb_context * GAOMON_S620::DeviceInterface::ctx = nullptr;
struct libusb_device_handle * GAOMON_S620::DeviceInterface::dev_handle = nullptr;

Error GAOMON_S620::DeviceInterface::init() {
	int32_t r = libusb_init(&ctx);

	if(r < 0) {
		printf("Init Error: %d\n", r);
		return Error::LIB_USB_INIT_FAILED;
	}

	dev_handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);

	if(dev_handle == NULL) {
		printf("Cannot open device\n");
		return Error::OPEN_DEVICE_FAILED;
	}

	printf("Device opened\n");

	libusb_set_configuration(dev_handle, 1);

	if(libusb_kernel_driver_active(dev_handle, 0) == 1) {
		printf("Kernel driver is active...\n");

		if(libusb_detach_kernel_driver(dev_handle, 0) == 0)
			printf("Kernel driver detached.\n");

	}

	r = libusb_claim_interface(dev_handle, 0);

	if(r < 0) {
		printf("Cannot claim interface.\n");
		return Error::CLAIM_INTERFACE_FAILED;
	}

	printf("Interface claimed!\n");

	return Error::NO_ERROR;
};

int32_t GAOMON_S620::DeviceInterface::read(uint8_t * output) {
	return libusb_bulk_transfer(
		dev_handle, BULK_EP_OUT,
		(uint8_t *) output, PACKET_LENGTH,
		nullptr, 0
	);
};

Error GAOMON_S620::DeviceInterface::stop() {
	const int r = libusb_release_interface(dev_handle, 0);

	if(r != 0) {
		printf("Cannot release the interface.\n");
		return Error::INTERFACE_RELEASE_FAILED;
	}

	printf("Interface released.\n");

	libusb_close(dev_handle);
	libusb_exit(ctx);

	return Error::NO_ERROR;
};


GAOMON_S620::Packet::Packet() {
	this->head = 0;
	this->type = 0;
	this->action = 0;
	this->xcord = 0;
	this->ycord = 0;
	this->pressure = 0;
	this->left_over = 0;
};

bool GAOMON_S620::Packet::isButtonUpdate() {
	return this->action == Action::BUTTON_UPDATE;
};

bool GAOMON_S620::Packet::isPencilUpdate() {
	return this->action == Action::PENCIL_UPDATE;
};

uint8_t GAOMON_S620::Packet::getPressedButton() {
	return this->isButtonUpdate() ? this->ycord : 0;
};

uint8_t GAOMON_S620::Packet::getPencilMode() {
	return this->type;
};

uint16_t GAOMON_S620::Packet::getPencilX() {
	return this->xcord;
};

uint16_t GAOMON_S620::Packet::getPencilY() {
	return this->ycord;
};

uint16_t GAOMON_S620::Packet::getPencilPressure() {
	return this->pressure;
};

void GAOMON_S620::Packet::printPacket() {
	printf("Head: %02x\n", this->head);

	printf("Action: %02x\n", this->action);
	printf("Type: %02x\n", this->type);
	printf("X: %u\n", this->xcord);
	printf("Y: %u\n", this->ycord);
	printf("Pressure: %u\n", this->pressure);

	printf("Left Over: %08x\n", this->left_over);
	printf("\n");
};

const char * GAOMON_S620::UInput::DEVICE_NAME = "Gaomon S620 (pytness)";
int GAOMON_S620::UInput::fileDescriptor = 0;

Error GAOMON_S620::UInput::init() {
	fileDescriptor = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	// enable left click
	ioctl(fileDescriptor, UI_SET_EVBIT, EV_KEY);
	ioctl(fileDescriptor, UI_SET_KEYBIT, BTN_LEFT);

	// enable absolute x & y axes + pressure
	ioctl(fileDescriptor, UI_SET_EVBIT, EV_ABS);
	ioctl(fileDescriptor, UI_SET_ABSBIT, ABS_X);
	ioctl(fileDescriptor, UI_SET_ABSBIT, ABS_Y);
	ioctl(fileDescriptor, UI_SET_ABSBIT, ABS_PRESSURE);

	// enable pencil
	ioctl(fileDescriptor, UI_SET_EVBIT, EV_KEY);
	ioctl(fileDescriptor, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(fileDescriptor, UI_SET_KEYBIT, BTN_TOOL_PEN);
	ioctl(fileDescriptor, UI_SET_KEYBIT, BTN_STYLUS);
	ioctl(fileDescriptor, UI_SET_KEYBIT, BTN_STYLUS2);

	struct uinput_abs_setup abs_setup;
	struct uinput_setup setup;


	// Set x axis info
	{
		memset(&abs_setup, 0, sizeof(abs_setup));

		abs_setup.code = ABS_X;
		abs_setup.absinfo.maximum = Resolution::WIDTH;
		abs_setup.absinfo.resolution = 400;

		ioctl(fileDescriptor, UI_ABS_SETUP, &abs_setup);
	}

	// Set y axis info
	{
		memset(&abs_setup, 0, sizeof(abs_setup));

		abs_setup.code = ABS_Y;
		abs_setup.absinfo.maximum = Resolution::HEIGHT;
		abs_setup.absinfo.resolution = 400;

		ioctl(fileDescriptor, UI_ABS_SETUP, &abs_setup);
	}

	// Set pressure axis info
	{
		memset(&abs_setup, 0, sizeof(abs_setup));

		abs_setup.code = ABS_PRESSURE;
		abs_setup.absinfo.maximum = Resolution::MAX_PRESSURE;

		ioctl(fileDescriptor, UI_ABS_SETUP, &abs_setup);
	}

	// Set device info
	{
		memset(&setup, 0, sizeof(setup));
		strncpy(setup.name, DEVICE_NAME, UINPUT_MAX_NAME_SIZE);

		setup.id = {
			.bustype = BUS_VIRTUAL,
			.vendor  = VENDOR_ID,
			.product = PRODUCT_ID,
			.version = 2
		};

		ioctl(fileDescriptor, UI_DEV_SETUP, &setup);
	}

	// Create device
	ioctl(fileDescriptor, UI_DEV_CREATE);

	return Error::NO_ERROR;
};

void GAOMON_S620::UInput::stop() {
	ioctl(fileDescriptor, UI_DEV_DESTROY);
	close(fileDescriptor);
};

void GAOMON_S620::UInput::sendEvent(uint16_t type, uint16_t code, uint32_t value) {
	struct input_event event;
	memset(&event, 0, sizeof(struct input_event));

	event.type = type;
        event.code = code;
	event.value = value;

	write(fileDescriptor, &event, sizeof(struct input_event));
}

void GAOMON_S620::UInput::moveTo(const uint16_t x, const uint16_t y) {
	sendEvent(EV_ABS, ABS_X, x);
	sendEvent(EV_ABS, ABS_Y, y);
};

void GAOMON_S620::UInput::setPressure(const uint16_t pressure) {
	sendEvent(EV_ABS, ABS_PRESSURE, pressure);
};

void GAOMON_S620::UInput::setPencilMode(const uint8_t mode) {
	// if ((mode & Packet::PencilMode::TOUCHING) == 0) // Hovering
	// 	sendEvent(EV_KEY, BTN_TOOL_PEN, 1);
	// else
	// 	sendEvent(EV_KEY, BTN_TOOL_PEN, 0);
	//
	//
	// if ((mode & Packet::PencilMode::TOUCHING) == 1) // Touching
	// 	sendEvent(EV_KEY, BTN_TOUCH, 1);
	// else
	// 	sendEvent(EV_KEY, BTN_TOUCH, 0);


	if (mode == Packet::PencilMode::BOTTOM_BUTTON_PRESSED)
		sendEvent(EV_KEY, BTN_STYLUS, 1);
	else
		sendEvent(EV_KEY, BTN_STYLUS, 0);


	if (mode == Packet::PencilMode::TOP_BUTTON_PRESSED)
		sendEvent(EV_KEY, BTN_STYLUS2, 1);
	else
		sendEvent(EV_KEY, BTN_STYLUS2, 0);
};

void GAOMON_S620::UInput::sync() {
	struct input_event event = {
		.type = EV_SYN
	};

	write(fileDescriptor, &event, sizeof(struct input_event));
};

Error GAOMON_S620::init() {
	Error device_init = DeviceInterface::init();

	if (device_init != Error::NO_ERROR) {
		return device_init;
	}

	return UInput::init();
};

Error GAOMON_S620::stop() {
	UInput::stop();
	return DeviceInterface::stop();
};
