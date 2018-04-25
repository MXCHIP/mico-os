/*
 * This file is part of the libjaylink project.
 *
 * Copyright (C) 2014-2015 Marc Schink <jaylink-dev@marcschink.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "libjaylink.h"
#include "libjaylink-internal.h"

/*
 * libusb.h includes windows.h and therefore must be included after anything
 * that includes winsock2.h.
 */
#include <libusb.h>

/**
 * @file
 *
 * Transport abstraction layer.
 */

/** Timeout of an USB transfer in milliseconds. */
#define USB_TIMEOUT	1000

/**
 * Number of consecutive timeouts before an USB transfer will be treated as
 * timed out.
 */
#define NUM_TIMEOUTS	2

/** Chunk size in bytes in which data is transferred. */
#define CHUNK_SIZE	2048

static int initialize_handle(struct jaylink_device_handle *devh)
{
	int ret;
	struct jaylink_context *ctx;
	struct libusb_config_descriptor *config;
	const struct libusb_interface *interface;
	const struct libusb_interface_descriptor *desc;
	const struct libusb_endpoint_descriptor *ep_desc;
	bool found_interface;
	bool found_endpoint_in;
	bool found_endpoint_out;
	uint8_t i;

	ctx = devh->dev->ctx;
	devh->interface_number = 0;

	/*
	 * Retrieve active configuration descriptor to determine the endpoints
	 * for the interface number of the device.
	 */
	ret = libusb_get_active_config_descriptor(devh->dev->usb_dev, &config);

	if (ret == LIBUSB_ERROR_IO) {
		log_err(ctx, "Failed to get configuration descriptor: "
			"input/output error.");
		return JAYLINK_ERR_IO;
	} else if (ret != LIBUSB_SUCCESS) {
		log_err(ctx, "Failed to get configuration descriptor: %s.",
			libusb_error_name(ret));
		return JAYLINK_ERR;
	}

	found_interface = false;

	for (i = 0; i < config->bNumInterfaces; i++) {
		interface = &config->interface[i];
		desc = &interface->altsetting[0];

		if (desc->bInterfaceClass != LIBUSB_CLASS_VENDOR_SPEC)
			continue;

		if (desc->bInterfaceSubClass != LIBUSB_CLASS_VENDOR_SPEC)
			continue;

		if (desc->bNumEndpoints < 2)
			continue;

		found_interface = true;
		devh->interface_number = i;
		break;
	}

	if (!found_interface) {
		log_err(ctx, "No suitable interface found.");
		libusb_free_config_descriptor(config);
		return JAYLINK_ERR;
	}

	found_endpoint_in = false;
	found_endpoint_out = false;

	for (i = 0; i < desc->bNumEndpoints; i++) {
		ep_desc = &desc->endpoint[i];

		if (ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
			devh->endpoint_in = ep_desc->bEndpointAddress;
			found_endpoint_in = true;
		} else {
			devh->endpoint_out = ep_desc->bEndpointAddress;
			found_endpoint_out = true;
		}
	}

	libusb_free_config_descriptor(config);

	if (!found_endpoint_in) {
		log_err(ctx, "Interface IN endpoint not found.");
		return JAYLINK_ERR;
	}

	if (!found_endpoint_out) {
		log_err(ctx, "Interface OUT endpoint not found.");
		return JAYLINK_ERR;
	}

	log_dbg(ctx, "Using endpoint %02x (IN) and %02x (OUT).",
		devh->endpoint_in, devh->endpoint_out);

	/* Buffer size must be a multiple of CHUNK_SIZE bytes. */
	devh->buffer_size = CHUNK_SIZE;
	devh->buffer = malloc(devh->buffer_size);

	if (!devh->buffer) {
		log_err(ctx, "Transport buffer malloc failed.");
		return JAYLINK_ERR_MALLOC;
	}

	devh->read_length = 0;
	devh->bytes_available = 0;
	devh->read_pos = 0;

	devh->write_length = 0;
	devh->write_pos = 0;

	return JAYLINK_OK;
}

static void cleanup_handle(struct jaylink_device_handle *devh)
{
	free(devh->buffer);
}

/**
 * Open a device.
 *
 * This function must be called before any other function of the transport
 * abstraction layer for the given device handle is called.
 *
 * @param[in,out] devh Device handle.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_IO Input/output error.
 * @retval JAYLINK_ERR Other error conditions.
 */
JAYLINK_PRIV int transport_open(struct jaylink_device_handle *devh)
{
	int ret;
	struct jaylink_device *dev;
	struct jaylink_context *ctx;
	struct libusb_device_handle *usb_devh;

	dev = devh->dev;
	ctx = dev->ctx;

	log_dbg(ctx, "Trying to open device (bus:address = %03u:%03u).",
		libusb_get_bus_number(dev->usb_dev),
		libusb_get_device_address(dev->usb_dev));

	ret = initialize_handle(devh);

	if (ret != JAYLINK_OK) {
		log_err(ctx, "Failed to initialize device handle.");
		return ret;
	}

	ret = libusb_open(dev->usb_dev, &usb_devh);

	if (ret == LIBUSB_ERROR_IO) {
		log_err(ctx, "Failed to open device: input/output error.");
		cleanup_handle(devh);
		return JAYLINK_ERR_IO;
	} else if (ret != LIBUSB_SUCCESS) {
		log_err(ctx, "Failed to open device: %s.",
			libusb_error_name(ret));
		cleanup_handle(devh);
		return JAYLINK_ERR;
	}

	ret = libusb_claim_interface(usb_devh, devh->interface_number);

	if (ret == LIBUSB_ERROR_IO) {
		log_err(ctx, "Failed to claim interface: input/output error.");
		return JAYLINK_ERR_IO;
	} else if (ret != LIBUSB_SUCCESS) {
		log_err(ctx, "Failed to claim interface: %s.",
			libusb_error_name(ret));
		cleanup_handle(devh);
		libusb_close(usb_devh);
		return JAYLINK_ERR;
	}

	log_dbg(ctx, "Device opened successfully.");

	devh->usb_devh = usb_devh;

	return JAYLINK_OK;
}

/**
 * Close a device.
 *
 * After this function has been called no other function of the transport
 * abstraction layer for the given device handle must be called.
 *
 * @param[in,out] devh Device handle.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR Other error conditions.
 */
JAYLINK_PRIV int transport_close(struct jaylink_device_handle *devh)
{
	int ret;
	struct jaylink_device *dev;
	struct jaylink_context *ctx;

	dev = devh->dev;
	ctx = dev->ctx;

	log_dbg(ctx, "Closing device (bus:address = %03u:%03u).",
		libusb_get_bus_number(dev->usb_dev),
		libusb_get_device_address(dev->usb_dev));

	ret = libusb_release_interface(devh->usb_devh, devh->interface_number);

	libusb_close(devh->usb_devh);
	cleanup_handle(devh);

	if (ret != LIBUSB_SUCCESS) {
		log_err(ctx, "Failed to release interface: %s.",
			libusb_error_name(ret));
		return JAYLINK_ERR;
	}

	log_dbg(ctx, "Device closed successfully.");

	return JAYLINK_OK;
}

/**
 * Start a write operation for a device.
 *
 * The data of a write operation must be written with at least one call of
 * transport_write(). It is required that all data of a write operation is
 * written before an other write and/or read operation is started.
 *
 * @param[in,out] devh Device handle.
 * @param[in] length Number of bytes of the write operation.
 * @param[in] has_command Determines whether the data of the write operation
 *                        contains the protocol command.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_ARG Invalid arguments.
 */
JAYLINK_PRIV int transport_start_write(struct jaylink_device_handle *devh,
		size_t length, bool has_command)
{
	struct jaylink_context *ctx;

	(void)has_command;

	if (!length)
		return JAYLINK_ERR_ARG;

	ctx = devh->dev->ctx;

	log_dbg(ctx, "Starting write operation (length = %zu bytes).", length);

	if (devh->write_pos > 0)
		log_warn(ctx, "Last write operation left %zu bytes in the "
			"buffer.", devh->write_pos);

	if (devh->write_length > 0)
		log_warn(ctx, "Last write operation was not performed.");

	devh->write_length = length;
	devh->write_pos = 0;

	return JAYLINK_OK;
}

/**
 * Start a read operation for a device.
 *
 * The data of a read operation must be read with at least one call of
 * transport_read(). It is required that all data of a read operation is read
 * before an other write and/or read operation is started.
 *
 * @param[in,out] devh Device handle.
 * @param[in] length Number of bytes of the read operation.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_ARG Invalid arguments.
 */
JAYLINK_PRIV int transport_start_read(struct jaylink_device_handle *devh,
		size_t length)
{
	struct jaylink_context *ctx;

	if (!length)
		return JAYLINK_ERR_ARG;

	ctx = devh->dev->ctx;

	log_dbg(ctx, "Starting read operation (length = %zu bytes).", length);

	if (devh->bytes_available > 0)
		log_dbg(ctx, "Last read operation left %zu bytes in the "
			"buffer.", devh->bytes_available);

	if (devh->read_length > 0)
		log_warn(ctx, "Last read operation left %zu bytes.",
			devh->read_length);

	devh->read_length = length;

	return JAYLINK_OK;
}

/**
 * Start a write and read operation for a device.
 *
 * This function starts a write and read operation as the consecutive call of
 * transport_start_write() and transport_start_read() but has a different
 * meaning from the protocol perspective and can therefore not be replaced by
 * these functions and vice versa.
 *
 * @note The write operation must be completed first before the read operation
 *       must be processed.
 *
 * @param[in,out] devh Device handle.
 * @param[in] write_length Number of bytes of the write operation.
 * @param[in] read_length Number of bytes of the read operation.
 * @param[in] has_command Determines whether the data of the write operation
 *                        contains the protocol command.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_ARG Invalid arguments.
 */
JAYLINK_PRIV int transport_start_write_read(struct jaylink_device_handle *devh,
		size_t write_length, size_t read_length, bool has_command)
{
	struct jaylink_context *ctx;

	(void)has_command;

	if (!read_length || !write_length)
		return JAYLINK_ERR_ARG;

	ctx = devh->dev->ctx;

	log_dbg(ctx, "Starting write / read operation (length = "
		"%zu / %zu bytes).", write_length, read_length);

	if (devh->write_pos > 0)
		log_warn(ctx, "Last write operation left %zu bytes in the "
			"buffer.", devh->write_pos);

	if (devh->write_length > 0)
		log_warn(ctx, "Last write operation was not performed.");

	if (devh->bytes_available > 0)
		log_warn(ctx, "Last read operation left %zu bytes in the "
			"buffer.", devh->bytes_available);

	if (devh->read_length > 0)
		log_warn(ctx, "Last read operation left %zu bytes.",
			devh->read_length);

	devh->write_length = write_length;
	devh->write_pos = 0;

	devh->read_length = read_length;
	devh->bytes_available = 0;
	devh->read_pos = 0;

	return JAYLINK_OK;
}

static bool adjust_buffer(struct jaylink_device_handle *devh, size_t size)
{
	struct jaylink_context *ctx;
	size_t num_chunks;
	uint8_t *buffer;

	ctx = devh->dev->ctx;

	/* Adjust buffer size to a multiple of CHUNK_SIZE bytes. */
	num_chunks = size / CHUNK_SIZE;

	if (size % CHUNK_SIZE > 0)
		num_chunks++;

	size = num_chunks * CHUNK_SIZE;
	buffer = realloc(devh->buffer, size);

	if (!buffer) {
		log_err(ctx, "Failed to adjust buffer size to %zu bytes.",
			size);
		return false;
	}

	devh->buffer = buffer;
	devh->buffer_size = size;

	log_dbg(ctx, "Adjusted buffer size to %zu bytes.", size);

	return true;
}

static int usb_send(struct jaylink_device_handle *devh, const uint8_t *buffer,
		size_t length)
{
	int ret;
	struct jaylink_context *ctx;
	unsigned int tries;
	int transferred;

	ctx = devh->dev->ctx;
	tries = NUM_TIMEOUTS;

	while (tries > 0 && length > 0) {
		/* Send data in chunks of CHUNK_SIZE bytes to the device. */
		ret = libusb_bulk_transfer(devh->usb_devh, devh->endpoint_out,
			(unsigned char *)buffer, MIN(CHUNK_SIZE, length),
			&transferred, USB_TIMEOUT);

		if (ret == LIBUSB_SUCCESS) {
			tries = NUM_TIMEOUTS;
		} else if (ret == LIBUSB_ERROR_TIMEOUT) {
			log_warn(ctx, "Sending data to device timed out, "
				"retrying.");
			tries--;
		} else if (ret == LIBUSB_ERROR_IO) {
			log_err(ctx, "Failed to send data to device: "
				"input/output error.");
			return JAYLINK_ERR_IO;
		} else {
			log_err(ctx, "Failed to send data to device: %s.",
				libusb_error_name(ret));
			return JAYLINK_ERR;
		}

		buffer += transferred;
		length -= transferred;

		log_dbg(ctx, "Sent %i bytes to device.", transferred);
	}

	if (!length)
		return JAYLINK_OK;

	log_err(ctx, "Sending data to device timed out.");

	return JAYLINK_ERR_TIMEOUT;
}

/**
 * Write data to a device.
 *
 * Before this function is used transport_start_write() or
 * transport_start_write_read() must be called to start a write operation. The
 * total number of written bytes must not exceed the number of bytes of the
 * write operation.
 *
 * @note A write operation will be performed and the data will be sent to the
 *       device when the number of written bytes reaches the number of bytes of
 *       the write operation. Before that the data will be written into a
 *       buffer.
 *
 * @param[in,out] devh Device handle.
 * @param[in] buffer Buffer to write data from.
 * @param[in] length Number of bytes to write.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_ARG Invalid arguments.
 * @retval JAYLINK_ERR_TIMEOUT A timeout occurred.
 * @retval JAYLINK_ERR_IO Input/output error.
 * @retval JAYLINK_ERR Other error conditions.
 */
JAYLINK_PRIV int transport_write(struct jaylink_device_handle *devh,
		const uint8_t *buffer, size_t length)
{
	int ret;
	struct jaylink_context *ctx;
	size_t num_chunks;
	size_t fill_bytes;
	size_t tmp;

	ctx = devh->dev->ctx;

	if (length > devh->write_length) {
		log_err(ctx, "Requested to write %zu bytes but only %zu bytes "
			"are expected for the write operation.", length,
			devh->write_length);
		return JAYLINK_ERR_ARG;
	}

	/*
	 * Store data in the buffer if the expected number of bytes for the
	 * write operation is not reached.
	 */
	if (length < devh->write_length) {
		if (devh->write_pos + length > devh->buffer_size) {
			if (!adjust_buffer(devh, devh->write_pos + length))
				return JAYLINK_ERR_MALLOC;
		}

		memcpy(devh->buffer + devh->write_pos, buffer, length);

		devh->write_length -= length;
		devh->write_pos += length;

		log_dbg(ctx, "Wrote %zu bytes into buffer.", length);
		return JAYLINK_OK;
	}

	/*
	 * Expected number of bytes for this write operation is reached and
	 * therefore the write operation will be performed.
	 */
	devh->write_length = 0;

	/* Send data directly to the device if the buffer is empty. */
	if (!devh->write_pos)
		return usb_send(devh, buffer, length);

	/*
	 * Calculate the number of bytes to fill up the buffer to reach a
	 * multiple of CHUNK_SIZE bytes. This ensures that the data from the
	 * buffer will be sent to the device in chunks of CHUNK_SIZE bytes.
	 * Note that this is why the buffer size must be a multiple of
	 * CHUNK_SIZE bytes.
	 */
	num_chunks = devh->write_pos / CHUNK_SIZE;

	if (devh->write_pos % CHUNK_SIZE)
		num_chunks++;

	fill_bytes = (num_chunks * CHUNK_SIZE) - devh->write_pos;
	tmp = MIN(length, fill_bytes);

	if (tmp > 0) {
		memcpy(devh->buffer + devh->write_pos, buffer, tmp);

		length -= tmp;
		buffer += tmp;

		log_dbg(ctx, "Buffer filled up with %zu bytes.", tmp);
	}

	/* Send buffered data to the device. */
	ret = usb_send(devh, devh->buffer, devh->write_pos + tmp);
	devh->write_pos = 0;

	if (ret != JAYLINK_OK)
		return ret;

	if (!length)
		return JAYLINK_OK;

	/* Send remaining data to the device. */
	return usb_send(devh, buffer, length);
}

static int usb_recv(struct jaylink_device_handle *devh, uint8_t *buffer,
		size_t *length)
{
	int ret;
	struct jaylink_context *ctx;
	unsigned int tries;
	int transferred;

	ctx = devh->dev->ctx;
	tries = NUM_TIMEOUTS;
	transferred = 0;

	while (tries > 0 && !transferred) {
		/* Always request CHUNK_SIZE bytes from the device. */
		ret = libusb_bulk_transfer(devh->usb_devh, devh->endpoint_in,
			(unsigned char *)buffer, CHUNK_SIZE, &transferred,
			USB_TIMEOUT);

		if (ret == LIBUSB_ERROR_TIMEOUT) {
			log_warn(ctx, "Receiving data from device timed out, "
				"retrying.");
			tries--;
			continue;
		} else if (ret == LIBUSB_ERROR_IO) {
			log_err(ctx, "Failed to receive data from device: "
				"input/output error.");
			return JAYLINK_ERR_IO;
		} else if (ret != LIBUSB_SUCCESS) {
			log_err(ctx, "Failed to receive data from device: %s.",
				libusb_error_name(ret));
			return JAYLINK_ERR;
		}

		log_dbg(ctx, "Received %i bytes from device.", transferred);
	}

	/* Ignore a possible timeout if at least one byte was received. */
	if (transferred > 0) {
		*length = transferred;
		return JAYLINK_OK;
	}

	log_err(ctx, "Receiving data from device timed out.");

	return JAYLINK_ERR_TIMEOUT;
}

/**
 * Read data from a device.
 *
 * Before this function is used transport_start_read() or
 * transport_start_write_read() must be called to start a read operation. The
 * total number of read bytes must not exceed the number of bytes of the read
 * operation.
 *
 * @param[in,out] devh Device handle.
 * @param[out] buffer Buffer to read data into on success. Its content is
 *                    undefined on failure.
 * @param[in] length Number of bytes to read.
 *
 * @retval JAYLINK_OK Success.
 * @retval JAYLINK_ERR_ARG Invalid arguments.
 * @retval JAYLINK_ERR_TIMEOUT A timeout occurred.
 * @retval JAYLINK_ERR_IO Input/output error.
 * @retval JAYLINK_ERR Other error conditions.
 */
JAYLINK_PRIV int transport_read(struct jaylink_device_handle *devh,
		uint8_t *buffer, size_t length)
{
	int ret;
	struct jaylink_context *ctx;
	size_t bytes_received;
	size_t tmp;

	ctx = devh->dev->ctx;

	if (length > devh->read_length) {
		log_err(ctx, "Requested to read %zu bytes but only %zu bytes "
			"are expected for the read operation.", length,
			devh->read_length);
		return JAYLINK_ERR_ARG;
	}

	if (length <= devh->bytes_available) {
		memcpy(buffer, devh->buffer + devh->read_pos, length);

		devh->read_length -= length;
		devh->bytes_available -= length;
		devh->read_pos += length;

		log_dbg(ctx, "Read %zu bytes from buffer.", length);
		return JAYLINK_OK;
	}

	if (devh->bytes_available > 0) {
		memcpy(buffer, devh->buffer + devh->read_pos,
			devh->bytes_available);

		buffer += devh->bytes_available;
		length -= devh->bytes_available;
		devh->read_length -= devh->bytes_available;

		log_dbg(ctx, "Read %zu bytes from buffer to flush it.",
			devh->bytes_available);

		devh->bytes_available = 0;
		devh->read_pos = 0;
	}

	while (length > 0) {
		/*
		 * If less than CHUNK_SIZE bytes are requested from the device,
		 * store the received data into the internal buffer instead of
		 * directly into the user provided buffer. This is necessary to
		 * prevent a possible buffer overflow because the number of
		 * requested bytes from the device is always CHUNK_SIZE and
		 * therefore up to CHUNK_SIZE bytes may be received.
		 * Note that this is why the internal buffer size must be at
		 * least CHUNK_SIZE bytes.
		 */
		if (length < CHUNK_SIZE) {
			ret = usb_recv(devh, devh->buffer, &bytes_received);

			if (ret != JAYLINK_OK)
				return ret;

			tmp = MIN(bytes_received, length);
			memcpy(buffer, devh->buffer, tmp);

			/*
			 * Setup the buffer for the remaining data if more data
			 * was received from the device than was requested.
			 */
			if (bytes_received > length) {
				devh->bytes_available = bytes_received - tmp;
				devh->read_pos = tmp;
			}

			buffer += tmp;
			length -= tmp;
			devh->read_length -= tmp;

			log_dbg(ctx, "Read %zu bytes from buffer.", tmp);
		} else {
			ret = usb_recv(devh, buffer, &bytes_received);

			if (ret != JAYLINK_OK)
				return ret;

			buffer += bytes_received;
			length -= bytes_received;
			devh->read_length -= bytes_received;

			log_dbg(ctx, "Read %zu bytes from device.",
				bytes_received);
		}
	}

	return JAYLINK_OK;
}
