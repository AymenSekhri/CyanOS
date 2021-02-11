#include "ARP.h"
#include "Devices/DebugPort/Logger.h"
#include "Endianess.h"
#include "IPv4.h"
#include "IPv4Address.h"

Vector<ARP::ARPEntry> ARP::arp_table{};
Spinlock ARP::lock{};

const MACAddress& ARP::mac_address_lookup(const IPv4Address& lookup_ip)
{
	ScopedLock local_lock{lock};

	auto dest_mac = arp_table.find_if([&lookup_ip](const ARPEntry& arp_entry) {
		if (arp_entry.ip == lookup_ip) {
			return true;
		} else {
			return false;
		}
	});

	if (dest_mac != arp_table.end()) {
		return dest_mac->mac;
	} else {
		send_arp_request(lookup_ip);

		auto& new_arp_entry = arp_table.emplace_front(lookup_ip);
		new_arp_entry.wait_queue.wait(local_lock);

		return new_arp_entry.mac;
	}
}

void ARP::send_arp_request(const IPv4Address& lookup_ip)
{
	auto& adapter = *NetworkAdapter::default_network_adapter;

	ARPHeader arp{.hardware_type = to_big_endian(static_cast<u16>(HardwareType::Ethernet)),
	              .protocol_type = to_big_endian(static_cast<u16>(ProtocolType::IPv4)),
	              .hardware_addr_len = MAC_ADDRESS_LENGTH,
	              .protocol_addr_len = IP4_ADDRESS_LENGTH,
	              .opcode = to_big_endian(static_cast<u16>(ARPCode::Request)),
	              .source_hw_addr = {},
	              .source_protocol_addr = {10, 0, 2, 15}, // static address for us until we get one from DHCP.
	              .destination_hw_addr = {},
	              .destination_protocol_addr = {}};

	adapter.MAC().copy(arp.source_hw_addr);

	MACAddress::Zero.copy(arp.destination_hw_addr);
	lookup_ip.copy(arp.destination_protocol_addr);

	adapter.send_frame(ProtocolType::ARP, MACAddress::Broadcast, BufferView{&arp, sizeof(ARPHeader)});

	info() << "ARP Request: Who owns " << IPv4Address{arp.destination_protocol_addr} << " ? Tell "
	       << IPv4Address{arp.source_protocol_addr};
}

void ARP::handle_arp_packet(const BufferView& data)
{
	if (data.size() < sizeof(ARPHeader)) {
		warn() << "Insufficient ARP packet size!";
	}

	const ARPHeader& arp = data.const_convert_to<ARPHeader>();

	switch (static_cast<ARPCode>(to_big_endian(arp.opcode))) {
		case ARPCode::Request: {
			info() << "ARP Request: Who owns " << IPv4Address{arp.destination_protocol_addr} << " ? Tell "
			       << IPv4Address{arp.source_protocol_addr} << ".";

			if (IPv4Address{arp.destination_protocol_addr} == IPv4::IP()) {
				answer_arp_request(IPv4Address{arp.source_protocol_addr}, MACAddress{arp.source_hw_addr});
			}

			break;
		}
		case ARPCode::Reply: {
			info() << "ARP Reply: Device " << MACAddress{arp.source_hw_addr} << " Owns "
			       << IPv4Address{arp.source_protocol_addr} << ".";

			add_arp_entry(IPv4Address{arp.source_protocol_addr}, MACAddress{arp.source_hw_addr});

			break;
		}
		default:
			warn() << "Unkown ARP code " << to_big_endian(arp.opcode) << "!";
			break;
	}
}

void ARP::add_arp_entry(const IPv4Address& ip, const MACAddress& mac)
{
	ScopedLock local_lock{lock};

	auto arp_entry = arp_table.find_if([&ip](const ARPEntry& arp_entry) {
		if (arp_entry.ip == ip) {
			return true;
		} else {
			return false;
		}
	});

	if (arp_entry == arp_table.end()) {
		// We got a MAC we didn't ask for, add it to the table anyway.
		arp_table.emplace_back(ip, mac);
	} else {
		// Update the MAC address to the entry and wake up the waiting thread.
		arp_entry->mac = mac;
		arp_entry->wait_queue.wake_up_all();
	}
}

void ARP::answer_arp_request(const IPv4Address& dest_ip, const MACAddress& dest_mac)
{
	auto& adapter = *NetworkAdapter::default_network_adapter;

	ARPHeader arp{.hardware_type = to_big_endian(static_cast<u16>(HardwareType::Ethernet)),
	              .protocol_type = to_big_endian(static_cast<u16>(ProtocolType::IPv4)),
	              .hardware_addr_len = MAC_ADDRESS_LENGTH,
	              .protocol_addr_len = IP4_ADDRESS_LENGTH,
	              .opcode = to_big_endian(static_cast<u16>(ARPCode::Reply)),
	              .source_hw_addr = {},
	              .source_protocol_addr = {},
	              .destination_hw_addr = {},
	              .destination_protocol_addr = {}};

	adapter.MAC().copy(arp.source_hw_addr);
	IPv4::IP().copy(arp.source_protocol_addr);

	dest_mac.copy(arp.destination_hw_addr);
	dest_ip.copy(arp.destination_protocol_addr);

	adapter.send_frame(ProtocolType::ARP, dest_mac, BufferView{&arp, sizeof(ARPHeader)});

	info() << "I'm " << adapter.MAC() << " and I own " << IPv4::IP() << ", Telling " << dest_ip << ".";
}