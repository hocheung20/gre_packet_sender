#include <sys/socket.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/if_ether.h>

#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    std::string kInterfaceName = "wlp9s0";
}

std::string address(const sockaddr & sock) {
    std::string address;
    address.resize(17);
    sprintf(address.data(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", sock.sa_data[0], sock.sa_data[1], sock.sa_data[2], sock.sa_data[3], sock.sa_data[4], sock.sa_data[5]);
    return address;
}

int main()  {
    auto packet_socket = socket(AF_PACKET , SOCK_RAW , htons(ETH_P_ALL));
    if (packet_socket == -1) {
        throw std::runtime_error(strerror(errno));
    }

    // Get interface index
    ifreq ir;
    memcpy(ir.ifr_name, kInterfaceName.c_str(), kInterfaceName.size() + 1);
    if (ioctl(packet_socket, SIOCGIFINDEX, &ir) == -1) {
        throw std::runtime_error(strerror(errno));
    }
    const int interface_index = ir.ifr_ifru.ifru_ivalue;

    // Get interface hw (mac address)
    if (ioctl(packet_socket, SIOCGIFHWADDR, &ir) == -1) {
        throw std::runtime_error(strerror(errno));
    }
    const std::string mac_address = address(ir.ifr_ifru.ifru_hwaddr);

    // Build the message
    std::vector<char> buf;
    char c_buf[65536];

    ethhdr eth_header;
    {
        // Fill in eth_header
        memset(&eth_header, 0, sizeof(eth_header));
        memcpy(&eth_header.h_source, &(ether_aton(mac_address.c_str())->ether_addr_octet), sizeof(eth_header.h_source));
        memcpy(&eth_header.h_dest, &(ether_aton(mac_address.c_str())->ether_addr_octet), sizeof(eth_header.h_dest));
        eth_header.h_proto = 0x0800;
    }
    buf.insert(buf.end(), reinterpret_cast<char *>(&eth_header), reinterpret_cast<char *>(&eth_header) + sizeof(eth_header));

    iphdr ip_header;
    {
        memset(&ip_header, 0, sizeof(ip_header));
        ip_header.protocol = 47; // GRE
    }
    buf.insert(buf.end(), reinterpret_cast<char *>(&ip_header), reinterpret_cast<char *>(&ip_header) + sizeof(ip_header));

    // Send the buffer as link layer message
    struct sockaddr_ll dest;
    dest.sll_family = AF_PACKET;
    dest.sll_protocol = htons(ETH_P_IP);
    memcpy(&dest.sll_addr, &(ether_aton(mac_address.c_str())->ether_addr_octet), 6);
    dest.sll_halen = 6;
    dest.sll_ifindex = interface_index;

    // Only used for receiving, set to 0
    dest.sll_hatype = 0;
    dest.sll_pkttype = 0;

    ssize_t bytes_sent = sendto(packet_socket, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
    if (bytes_sent == -1) {
        throw std::runtime_error(strerror(errno));
    }

    std::cout << "Bytes sent: " << bytes_sent << std::endl;
}
