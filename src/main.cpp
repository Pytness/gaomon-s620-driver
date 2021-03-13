#include <cstdio>
#include <cstdint>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        // For mode constants
#include <fcntl.h>           // For O_* constants
#include <libusb-1.0/libusb.h>
#include <errno.h>

#include "gaomon-s620.hpp"

const char * SHARED_MEMORY_NAME = "gaomon-s620-driver::packet";

GAOMON_S620::Packet::Packet * create_shared_packet();
void destroy_shared_packet(GAOMON_S620::Packet::Packet * shared_packet);

int main() {

	if (geteuid() != 0) {
		printf("Must be executed as root.\n");
		return EACCES;
	}

	auto shared_packet = create_shared_packet();
	GAOMON_S620::Packet::Packet packet = GAOMON_S620::Packet::Packet();

	GAOMON_S620::init();

	while (true) {
		const int result = GAOMON_S620::DeviceInterface::read((uint8_t *) &packet);

		if (result != 0) {
			printf("Reading error: %d\n", result);
			break;
		}

		*shared_packet = packet;

		if (packet.isPencilUpdate()) {

			uint8_t mode = packet.getPencilMode();
			uint16_t x = packet.getPencilX();
			uint16_t y = packet.getPencilY();
			uint16_t pressure = packet.getPencilPressure();

			GAOMON_S620::UInput::setPencilMode(mode);
			GAOMON_S620::UInput::moveTo(x, y);
			GAOMON_S620::UInput::setPressure(pressure);

		} else if (packet.isButtonUpdate()) {
			printf("Button:\n");
			printf("\nPressed Button:%x\n", packet.getPressedButton());
		}

		GAOMON_S620::UInput::sync_input();
	}

	GAOMON_S620::stop();
	destroy_shared_packet(shared_packet);


	return 0;
}


GAOMON_S620::Packet::Packet * create_shared_packet() {

	// Create shared memory space
	int shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);

	if (shm_fd == -1) {
		shm_unlink(SHARED_MEMORY_NAME);
		shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
	}

	ftruncate(shm_fd, sizeof(GAOMON_S620::Packet::Packet));

	return (GAOMON_S620::Packet::Packet *) mmap(
		nullptr,
		sizeof(GAOMON_S620::Packet::Packet),
		PROT_WRITE, MAP_SHARED,
		shm_fd, 0
	);
}

void destroy_shared_packet(GAOMON_S620::Packet::Packet * shared_packet) {
	shm_unlink(SHARED_MEMORY_NAME);
	munmap(shared_packet, sizeof(GAOMON_S620::Packet::Packet));
}
