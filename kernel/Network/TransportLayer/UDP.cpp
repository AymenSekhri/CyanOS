#include "UDP.h"
#include "Network/Network.h"
#include "Network/NetworkLayer/IPv4.h"
#include "Tasking/ScopedLock.h"
#include <Algorithms.h>
#include <Buffer.h>
#include <Endianess.h>

// TODO: refactor network structures so one function will handle convertions to/from big endian.
UDP::UDP(Network& network) : m_network{network} {}

void UDP::send(const IPv4Address& dest_ip, u16 dest_port, u16 src_port, const BufferView& data)
{
	send_segment(dest_ip, dest_port, src_port, data);
}

UDP::ConnectionInformation UDP::receive(u16 dest_port, Buffer& buffer)
{
	ScopedLock local_lock{m_lock};

	ASSERT(!m_connections_list.contains([dest_port](const Connection& i) { return dest_port == i.dest_port; }));

	auto& connection = m_connections_list.emplace_back(dest_port, buffer);

	connection.wait_queue.wait(local_lock);

	ConnectionInformation connection_info;
	connection_info.data_size = connection.data_size;
	connection_info.src_port = connection.src_port;
	connection_info.src_ip = connection.src_ip;

	m_connections_list.remove_if([&connection](const Connection& i) { return connection.dest_port == i.dest_port; });

	return connection_info;
}

void UDP::handle(const IPv4Address& src_ip, const BufferView& data)
{
	ScopedLock local_lock{m_lock};

	warn() << "UDP Packet received.";

	auto& udp_segment = data.const_convert_to<UDPHeader>();

	u16 dest_port = to_big_endian<u16>(udp_segment.destination_port);
	auto connection = m_connections_list.find_if([dest_port](const Connection& connection) {
		return connection.dest_port == dest_port;
		info() << connection.dest_port;
	});

	if (connection != m_connections_list.end()) {
		connection->src_port = to_big_endian<u16>(udp_segment.source_port);
		connection->src_ip = src_ip;
		connection->data_size = data.size() - sizeof(UDPHeader);
		// FIXME: check if it fits first;
		connection->buffer->fill_from(data.ptr() + sizeof(UDPHeader), 0, data.size() - sizeof(UDPHeader));

		connection->wait_queue.wake_up_all();
	} else {
		err() << "Received UDP packet with no known source port.";
	}
}

void UDP::send_segment(const IPv4Address& dest_ip, u16 dest_port, u16 src_port, const BufferView& data)
{
	Buffer udp_raw_segment{sizeof(UDPHeader) + data.size()};
	auto& udp_segment = udp_raw_segment.convert_to<UDPHeader>();

	udp_segment.source_port = to_big_endian<u16>(src_port);
	udp_segment.destination_port = to_big_endian<u16>(dest_port);
	udp_segment.total_length = to_big_endian<u16>(sizeof(UDPHeader) + data.size());
	udp_segment.checksum = 0;

	udp_raw_segment.fill_from(data.ptr(), sizeof(UDPHeader), data.size());

	m_network.ipv4_provider().send_ip_packet(dest_ip, IPv4Protocols::UDP, udp_raw_segment);
}

u16 UDP::calculate_checksum(const BufferView& data)
{
	auto* u16_array = reinterpret_cast<const u16*>(data.ptr());
	size_t u16_array_size = number_of_words<u16>(data.size());

	u32 sum = 0;
	for (size_t i = 0; i < u16_array_size; i++) {
		sum += to_big_endian<u16>(u16_array[i]);
	}

	while (sum > 0xFFFF) {
		sum = (sum & 0xFFFF) + ((sum & 0xFFFF0000) >> 16);
	}

	return to_big_endian<u16>(~u16(sum));
}