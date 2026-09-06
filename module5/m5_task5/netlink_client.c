#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "netlink_exchange.h"

#define RESPONSE_BUFFER_SIZE                                                \
	NLMSG_SPACE(sizeof(NETLINK_EXCHANGE_RESPONSE_PREFIX) - 1 +           \
		    NETLINK_EXCHANGE_MAX_MESSAGE_SIZE + 1)

// привязать дескриптор к лок адрессу Netlink
static int bind_client_socket(int socket_descriptor, struct sockaddr_nl *address)
{
	socklen_t address_length = sizeof(*address);

	memset(address, 0, sizeof(*address));
	address->nl_family = AF_NETLINK;
	if (bind(socket_descriptor, (struct sockaddr *)address,
		 sizeof(*address)) < 0)
		return -1;

	if (getsockname(socket_descriptor, (struct sockaddr *)address,
			&address_length) < 0)
		return -1;
	if (address_length != sizeof(*address) ||
	    address->nl_family != AF_NETLINK) {
		errno = EPROTO;
		return -1;
	}

	return 0;
}

static int send_request(int socket_descriptor,
			const struct sockaddr_nl *client_address,
			const char *message, size_t message_length)
{
	struct {
		struct nlmsghdr header;
		char payload[NETLINK_EXCHANGE_MAX_MESSAGE_SIZE];
	} request;
	struct sockaddr_nl kernel_address = {
		.nl_family = AF_NETLINK,
	};
	struct nlmsghdr *request_header = &request.header;
	struct iovec io_vector; // непрерывный пакет
	struct msghdr socket_message; 
	ssize_t sent_bytes;

	// обнулили -> заполнили
	memset(&request, 0, sizeof(request));
	request_header->nlmsg_len = NLMSG_LENGTH(message_length);
	request_header->nlmsg_type = NETLINK_EXCHANGE_REQUEST;
	request_header->nlmsg_flags = NLM_F_REQUEST;
	request_header->nlmsg_seq = 1;
	request_header->nlmsg_pid = client_address->nl_pid;
	memcpy(NLMSG_DATA(request_header), message, message_length);
	// описывается для ядра адресс назначения и дипазон памяти пакета
	io_vector.iov_base = request_header;
	io_vector.iov_len = request_header->nlmsg_len;
	memset(&socket_message, 0, sizeof(socket_message));
	socket_message.msg_name = &kernel_address;
	socket_message.msg_namelen = sizeof(kernel_address);
	socket_message.msg_iov = &io_vector;
	socket_message.msg_iovlen = 1;

	sent_bytes = sendmsg(socket_descriptor, &socket_message, 0);
	if (sent_bytes < 0)
		return -1;
	if ((size_t)sent_bytes != request_header->nlmsg_len) {
		errno = EIO;
		return -1;
	}

	return 0;
}

static int receive_response(int socket_descriptor)
{
	union {
		struct nlmsghdr alignment;
		char data[RESPONSE_BUFFER_SIZE];
	} response_buffer;

	struct sockaddr_nl sender_address = { 0 };

	struct iovec io_vector = {
		.iov_base = response_buffer.data,
		.iov_len = sizeof(response_buffer),
	};
	struct msghdr socket_message = {
		.msg_name = &sender_address,
		.msg_namelen = sizeof(sender_address),
		.msg_iov = &io_vector,
		.msg_iovlen = 1,
	};
	struct nlmsghdr *response_header;
	const char *response_data;
	const char *terminator;
	size_t response_length;
	ssize_t received_bytes;
	int remaining;

	received_bytes = recvmsg(socket_descriptor, &socket_message, 0);
	if (received_bytes < 0)
		return -1;
	if (socket_message.msg_flags & MSG_TRUNC) {
		errno = EMSGSIZE;
		return -1;
	}
	if (sender_address.nl_family != AF_NETLINK ||
	    sender_address.nl_pid != 0) {
		errno = EPROTO;
		return -1;
	}

	remaining = received_bytes;
	response_header = &response_buffer.alignment;
	if (!NLMSG_OK(response_header, remaining) ||
	    response_header->nlmsg_type != NETLINK_EXCHANGE_RESPONSE ||
	    response_header->nlmsg_seq != 1) {
		errno = EPROTO;
		return -1;
	}

	response_data = NLMSG_DATA(response_header);
	response_length = NLMSG_PAYLOAD(response_header, 0);
	terminator = memchr(response_data, '\0', response_length);
	if (!terminator) {
		errno = EPROTO;
		return -1;
	}

	printf("%.*s\n", (int)(terminator - response_data), response_data);
	return 0;
}

int main(int argument_count, char *argument_values[])
{
	struct sockaddr_nl client_address;
	size_t message_length;
	int socket_descriptor;
	int result = EXIT_FAILURE;

	if (argument_count != 2) {
		fprintf(stderr, "Usage: %s \"message\"\n", argument_values[0]);
		return EXIT_FAILURE;
	}

	message_length = strlen(argument_values[1]);
	if (message_length > NETLINK_EXCHANGE_MAX_MESSAGE_SIZE) {
		fprintf(stderr, "Message must not exceed %d bytes\n",
			NETLINK_EXCHANGE_MAX_MESSAGE_SIZE);
		return EXIT_FAILURE;
	}

	socket_descriptor = socket(AF_NETLINK, SOCK_RAW,
				   NETLINK_EXCHANGE_PROTOCOL);
	if (socket_descriptor < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	if (bind_client_socket(socket_descriptor, &client_address) < 0) {
		perror("bind Netlink socket");
		goto close_socket;
	}
	if (send_request(socket_descriptor, &client_address, argument_values[1],
			 message_length) < 0) {
		perror("send Netlink request");
		goto close_socket;
	}
	if (receive_response(socket_descriptor) < 0) {
		perror("receive Netlink response");
		goto close_socket;
	}

	result = EXIT_SUCCESS;

close_socket:
	close(socket_descriptor);
	return result;
}
