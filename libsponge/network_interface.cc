#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

using namespace std;

// \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
// \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::send_arp(const uint32_t &target_ip, optional<EthernetAddress> target_ethaddr) {
    // insert a new node if not exist
    auto arpt_findres = _arp_table.find(target_ip);
    bool first_send = false;
    if (arpt_findres == _arp_table.cend()) {
        arpt_findres = _arp_table.insert(arpt_findres, {target_ip, _etb_node_t{EthernetAddress{}, 0, 0, false}});
        first_send = true;
    }
    auto &se = arpt_findres->second;
    // don't send arp if interval is too short
    if (not first_send && _timer - se.ask_time <= 5 * 1000) {
        return;
    }

    EthernetFrame efram{};
    EthernetHeader &ehder = efram.header();
    ARPMessage arpm{};
    uint32_t my_ip = _ip_address.ipv4_numeric();

    ehder.type = EthernetHeader::TYPE_ARP;

    ehder.src = arpm.sender_ethernet_address = _ethernet_address;
    arpm.sender_ip_address = my_ip;
    arpm.target_ip_address = target_ip;

    if (target_ethaddr.has_value()) {
        arpm.target_ethernet_address = target_ethaddr.value();
        ehder.dst = target_ethaddr.value();
    } else {
        ehder.dst = ETHERNET_BROADCAST;
    }

    if (ehder.dst == ETHERNET_BROADCAST) {
        arpm.opcode = ARPMessage::OPCODE_REQUEST;
    } else {
        arpm.opcode = ARPMessage::OPCODE_REPLY;
    }

    efram.payload() = arpm.serialize();
    _frames_out.emplace(efram);

    se.ask_time = _timer;
}

// \param[in] dgram the IPv4 datagram to be sent
// \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may
// also be another host if directly connected to the same network as the destination) (Note: the Address type can be
// converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    uint32_t next_ip = next_hop.ipv4_numeric();
    auto arpt_findres = _arp_table.find(next_ip);
    auto &se = arpt_findres->second;
    if (arpt_findres == _arp_table.end() || se.exist == false || _timer - se.updated_time > 30 * 1000) {
        send_arp(next_ip, nullopt);
        // keep this segment
        //!!! I don't know why I can't use <uint32_t, std::deque<InternetDatagram>> as map
        //!!! each time I want to assign it I will meet with a fatal runtime error.
        _sent_dgram.insert({next_ip, dgram});
    } else {
        send_dgram(dgram, se.eAddr);
    }
}

void NetworkInterface::send_dgram(const InternetDatagram &dgram, const EthernetAddress &address) {
    EthernetFrame efram{};
    auto &ehder = efram.header();
    ehder.type = EthernetHeader::TYPE_IPv4;
    ehder.dst = address;
    ehder.src = _ethernet_address;
    efram.payload() = dgram.serialize();
    _frames_out.emplace(efram);
}

// \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &efram) {
    const EthernetHeader &ehder = efram.header();
    size_t pkg_type = 0;  // 0: not send to me, 1: IP packet, 2: ARP packet
    if (ehder.dst == _ethernet_address) {
        if (ehder.type == EthernetHeader::TYPE_ARP)
            pkg_type = 2;  // arp reply
        if (ehder.type == EthernetHeader::TYPE_IPv4)
            pkg_type = 1;
    } else if (ehder.dst == ETHERNET_BROADCAST && ehder.type == EthernetHeader::TYPE_ARP) {
        pkg_type = 2;  // arp broadcast
    }

    if (pkg_type == 0) {
        return nullopt;
    }
    if (pkg_type == 1) {
        //! need judge target
        InternetDatagram idata{};
        ParseResult err = idata.parse(Buffer(efram.payload()));
        if (not(err == ParseResult::NoError)) {  //! maybe have problems
            cerr << "Parse Internet Packet Error: " << static_cast<underlying_type<ParseResult>::type>(err) << endl;
            return nullopt;
        }
        return idata;
    }
    if (pkg_type > 1) {
        // learn from arp
        ARPMessage arpm{};
        ParseResult err = arpm.parse(Buffer(efram.payload()));
        if (not(err == ParseResult::NoError)) {
            cerr << "Parse ARP Packet Error: " << static_cast<underlying_type<ParseResult>::type>(err) << endl;
            return nullopt;
        }
        // expired update or add --> we can directly emplace it
        if (arpm.target_ip_address != _ip_address.ipv4_numeric() && arpm.sender_ip_address != arpm.target_ip_address) {
            return nullopt;
        }

        if (arpm.opcode == ARPMessage::OPCODE_REQUEST) {  // we reply arp
            send_arp(arpm.sender_ip_address, arpm.sender_ethernet_address);
        } else if (arpm.opcode == ARPMessage::OPCODE_REPLY) {  // try to resend pkg
            for (auto it = _sent_dgram.lower_bound(arpm.sender_ip_address);
                 it != _sent_dgram.upper_bound(arpm.sender_ip_address);) {
                send_dgram(it->second, arpm.sender_ethernet_address);
                it = _sent_dgram.erase(it);
            }
        }
        // update arp
        auto arpt_findres = _arp_table.find(arpm.sender_ip_address);
        if (arpt_findres != _arp_table.cend()) {
            auto &se = arpt_findres->second;
            se.updated_time = _timer;
            se.eAddr = arpm.sender_ethernet_address;
            se.exist = true;
        } else {  // useless code
            _arp_table.insert(arpt_findres,
                              {arpm.sender_ip_address, _etb_node_t{arpm.sender_ethernet_address, _timer, 0, true}});
        }
    }
    return nullopt;
}

// \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _timer += ms_since_last_tick; }
