#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "assert.h"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    
    EthernetFrame eth;
    eth.payload() = dgram.serialize();
    eth.header().src = this->_ethernet_address;
    eth.header().type = EthernetHeader::TYPE_IPv4;

    auto it = this->_arp_cache.find(next_hop_ip);
    if(it == this->_arp_cache.end())
    {
        auto waitq = this->_wait_for_arp.find(next_hop_ip);
        if(waitq == this->_wait_for_arp.end())
        {
            this->_wait_for_arp[next_hop_ip] = make_pair(queue<EthernetFrame>(), this->_ticks);
            this->_wait_for_arp[next_hop_ip].first.push(eth);
            this->send_arp_request(next_hop_ip);
        }
        else
        {
            waitq->second.first.push(eth);
            if(this->_ticks - waitq->second.second < 5000)
                return;
            else
            {
                this->send_arp_request(next_hop_ip);
                waitq->second.second = this->_ticks;   
            }
        }
    }   
    else
    {
        eth.header().dst = it->second.first;
        this->_frames_out.push(eth);
    }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != this->_ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
    {   
        // cerr<<frame.header().to_string()<<endl;
        return {};
    }
    if(frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        InternetDatagram ip;
        auto res = ip.parse(frame.payload());
        if(res == ParseResult::NoError)
            return ip;
    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP)
    {
        ARPMessage arp;
        auto res = arp.parse(frame.payload());
        if(res == ParseResult::NoError)
        {
            if(arp.opcode == ARPMessage::OPCODE_REPLY)
            {
                this->_arp_cache[arp.sender_ip_address] = make_pair(arp.sender_ethernet_address, this->_ticks);
                auto it = this->_wait_for_arp.find(arp.sender_ip_address);
                if(it != this->_wait_for_arp.end())
                {
                    while(it->second.first.empty() == false)
                    {
                        auto waitframe = it->second.first.front();
                        waitframe.header().dst = arp.sender_ethernet_address;
                        this->_frames_out.push(waitframe);
                        it->second.first.pop();
                    }
                    this->_wait_for_arp.erase(it);
                }
            
            }
            else if(arp.opcode == ARPMessage::OPCODE_REQUEST)
            {
                if(arp.target_ip_address != this->_ip_address.ipv4_numeric())
                    return {};
                this->_arp_cache[arp.sender_ip_address] = make_pair(arp.sender_ethernet_address, this->_ticks);
                this->send_arp_reply(arp.sender_ip_address, arp.sender_ethernet_address);
            }
        }
        return {};
    }
    assert(0);
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    this->_ticks += ms_since_last_tick;
    for(auto it = this->_arp_cache.begin(); it != this->_arp_cache.end(); )
    {
        if(this->_ticks - it->second.second >= 30000)
            it = this->_arp_cache.erase(it);
        else
            ++it;
    }
}

void NetworkInterface::send_arp_request(uint32_t dstip)
{
    EthernetFrame eth;
    eth.header().src = this->_ethernet_address;
    eth.header().dst = ETHERNET_BROADCAST;
    eth.header().type = EthernetHeader::TYPE_ARP;

    ARPMessage arp_payload;
    arp_payload.opcode = ARPMessage::OPCODE_REQUEST;
    arp_payload.sender_ethernet_address = this->_ethernet_address;
    arp_payload.sender_ip_address = this->_ip_address.ipv4_numeric();
    arp_payload.target_ip_address = dstip;

    eth.payload() = arp_payload.serialize();

    this->_frames_out.push(eth);
}

void NetworkInterface::send_arp_reply(uint32_t dstip, EthernetAddress dstmac)
{
    EthernetFrame eth;
    eth.header().src = this->_ethernet_address;
    eth.header().dst = dstmac;
    eth.header().type = EthernetHeader::TYPE_ARP;

    ARPMessage arp_payload;
    arp_payload.opcode = ARPMessage::OPCODE_REPLY;
    arp_payload.sender_ethernet_address = this->_ethernet_address;
    arp_payload.sender_ip_address = this->_ip_address.ipv4_numeric();
    arp_payload.target_ip_address = dstip;
    arp_payload.target_ethernet_address = dstmac;

    eth.payload() = arp_payload.serialize();

    this->_frames_out.push(eth);

}
