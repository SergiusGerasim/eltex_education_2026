#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>

#include "netlink_exchange.h"

#define MODULE_NAME "netlink_exchange"

static struct sock *exchange_socket; // сокет ядра

static int send_response(u32 port_id, u32 sequence_number,
			 const void *message, size_t message_length)
{
	static const char response_prefix[] =
		NETLINK_EXCHANGE_RESPONSE_PREFIX;
	struct sk_buff *response_buffer;
	struct nlmsghdr *response_header;
	char *response_data;
	size_t response_length;

	response_length = sizeof(response_prefix) - 1 + message_length + 1;
	// выделяем буфер sk_buff емкости response_length
	response_buffer = nlmsg_new(response_length, GFP_KERNEL);
	if (!response_buffer)
		return -ENOMEM;
	// тут добовляется заголовок Netlink 
	response_header = nlmsg_put(response_buffer, 0, sequence_number,
				    NETLINK_EXCHANGE_RESPONSE,
				    response_length, 0);
	if (!response_header) {
		kfree_skb(response_buffer);
		return -EMSGSIZE;
	}
	// далее заполняем содержимое
	response_data = nlmsg_data(response_header);
	memcpy(response_data, response_prefix, sizeof(response_prefix) - 1);
	memcpy(response_data + sizeof(response_prefix) - 1, message,
	       message_length);
	response_data[response_length - 1] = '\0';

	return netlink_unicast(exchange_socket, response_buffer, port_id,
			       MSG_DONTWAIT);
}

// обработка одного подготовленного заголовка
static void process_request(const struct nlmsghdr *request_header, u32 port_id)
{
	size_t message_length;
	int error;

	if (request_header->nlmsg_type != NETLINK_EXCHANGE_REQUEST) {
		pr_warn_ratelimited(MODULE_NAME
				    ": ignored message with unknown type %u\n",
				    request_header->nlmsg_type);
		return;
	}

	message_length = nlmsg_len(request_header);
	if (message_length > NETLINK_EXCHANGE_MAX_MESSAGE_SIZE) {
		pr_warn_ratelimited(MODULE_NAME
				    ": message from port %u is too long: %zu bytes\n",
				    port_id, message_length);
		return;
	}

	pr_info(MODULE_NAME ": received %zu bytes from port %u\n",
		message_length, port_id);
	error = send_response(port_id, request_header->nlmsg_seq,
			      nlmsg_data(request_header), message_length);
	if (error < 0)
		pr_warn_ratelimited(MODULE_NAME
				    ": failed to reply to port %u: %d\n",
				    port_id, error);
}

static void receive_messages(struct sk_buff *socket_buffer)
{
	struct nlmsghdr *request_header;
	u32 port_id;
	int remaining;

	port_id = NETLINK_CB(socket_buffer).portid;
	nlmsg_for_each_msg(request_header, nlmsg_hdr(socket_buffer),
			   socket_buffer->len, remaining)
		process_request(request_header, port_id);

	if (remaining > 0)
		pr_warn_ratelimited(MODULE_NAME ": ignored malformed message from port %u\n",port_id);
}

static struct netlink_kernel_cfg exchange_socket_config = {
	.flags = NL_CFG_F_NONROOT_SEND,
	.input = receive_messages,
};

static int __init netlink_exchange_init(void)
{
	exchange_socket = netlink_kernel_create(&init_net,
						NETLINK_EXCHANGE_PROTOCOL,
						&exchange_socket_config);
	if (!exchange_socket) {
		pr_err(MODULE_NAME ": failed to create Netlink socket\n");
		return -ENOMEM;
	}

	pr_info(MODULE_NAME ": listening on Netlink protocol %d\n",
		NETLINK_EXCHANGE_PROTOCOL);
	return 0;
}

static void __exit netlink_exchange_exit(void)
{
	netlink_kernel_release(exchange_socket);
	exchange_socket = NULL;
	pr_info(MODULE_NAME ": unloaded\n");
}

module_init(netlink_exchange_init);
module_exit(netlink_exchange_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerasimov Sergei");
MODULE_DESCRIPTION("Kernel module for exchanging data through Netlink");
MODULE_VERSION("1.0");
