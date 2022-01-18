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


GAOMON_S620::Packet * create_shared_packet(const char * name);

int main() {

	if (geteuid() != 0) {
		printf("Must be executed as root.\n");
		return EACCES;
	}

	auto shared_packet_data = create_shared_packet(SHARED_MEMORY_NAME);

	GAOMON_S620::Packet * packet = new GAOMON_S620::Packet();

	GAOMON_S620::init();

	while (true) {
		const int r = GAOMON_S620::DeviceInterface::read((uint8_t *) packet);

		if (r != 0) {
			printf("Reading error: %d\n", r);
			break;
		}

		*shared_packet_data = *packet;

		if (packet->isPencilUpdate()) {

			GAOMON_S620::UInput::moveTo(packet->getPencilX(), packet->getPencilY());
			GAOMON_S620::UInput::setPencilMode(packet->getPencilMode());
			GAOMON_S620::UInput::setPressure(packet->getPencilPressure());

		} else if (packet->isButtonUpdate()) {
			printf("Button:\n");
			printf("\nPressed Button:%x\n", packet->getPressedButton());
		}

		GAOMON_S620::UInput::sync();

		// printf("\n");
	}

	GAOMON_S620::stop();
	shm_unlink(SHARED_MEMORY_NAME);
	delete[] packet;

	return 0;
}


GAOMON_S620::Packet * create_shared_packet(const char * name) {

	constexpr int SHARED_MEM_FLAGS = O_CREAT | O_EXCL | O_RDWR;
	// Create shared memory space
	int shm_fd = shm_open(name, SHARED_MEM_FLAGS, 0666);

	// Failed to open, may be because it already exists
	if (shm_fd == -1) {
		shm_unlink(name);
		shm_fd = shm_open(name, SHARED_MEM_FLAGS, 0666);
	}

	ftruncate(shm_fd, sizeof(GAOMON_S620::Packet));

	auto shared_packet_data = (GAOMON_S620::Packet *) mmap(
			0, sizeof(GAOMON_S620::Packet),
			PROT_WRITE, MAP_SHARED,
			shm_fd, 0
		);
	
	return shared_packet_data;
}
