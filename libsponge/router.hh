#ifndef SPONGE_LIBSPONGE_ROUTER_HH
#define SPONGE_LIBSPONGE_ROUTER_HH

#include "network_interface.hh"

#include <any>
#include <optional>
#include <queue>
#include <set>
#include <tuple>

// \brief A wrapper for NetworkInterface that makes the host-side
// interface asynchronous: instead of returning received datagrams
// immediately (from the `recv_frame` method), it stores them for
// later retrieval. Otherwise, behaves identically to the underlying
// implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface {
    std::queue<InternetDatagram> _datagrams_out{};

  public:
    using NetworkInterface::NetworkInterface;

    // Construct from a NetworkInterface
    AsyncNetworkInterface(NetworkInterface &&interface) : NetworkInterface(interface) {}

    // \brief Receives and Ethernet frame and responds appropriately.

    // - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
    // - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    // - If type is ARP reply, learn a mapping from the "target" fields.
    //
    // \param[in] frame the incoming Ethernet frame
    void recv_frame(const EthernetFrame &frame) {
        auto optional_dgram = NetworkInterface::recv_frame(frame);
        if (optional_dgram.has_value()) {
            _datagrams_out.push(std::move(optional_dgram.value()));
        }
    };

    // Access queue of Internet datagrams that have been received
    std::queue<InternetDatagram> &datagrams_out() { return _datagrams_out; }
};

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router {
    // The router's collection of network interfaces
    std::vector<AsyncNetworkInterface> _interfaces{};

    //// std::map<route_prefix, std::tuple<prefix_length, end_index, neatest_prefix, interface_index, next_hop>>
    //! set or map can not set value for key (because of hash algorithm)
    //! so we don't use set
    //! we need end_index to discriminate different prefix (multimap doesn't help this)
    //! so we use map with pair<> for key
    // first -> route_prefix
    // second -> end_index
    // get<0>() -> nearest_prefix
    // get<1>() -> interface_index
    // get<2>() -> next_hop
    // get<3>() -> prefix_length
    //! get list ↑
    // nearest_prefix = the nearest route_prefix include this prefix_seg (prefix_seg = a calculated period of seg, from
    // prefix_length to it's end_index)
    typedef std::pair<uint32_t, uint32_t> _rt_k;
    typedef std::tuple<std::any, size_t, uint32_t, uint8_t> _rt_v;
    // use (0.0.0.0) -> 0 as a placeholder to indicate null next_hop and use dgram's dst ip address
    struct cmp_map {
        bool operator()(const _rt_k &ts, const _rt_k &tb) const {
            if (ts.first == tb.first) {
                // put the member which have larger range at front
                // it's effectively to get the shorter as the correct prefix
                return ts.second > tb.second;
            } else {
                return ts.first < tb.first;
            }
        };
    };
    std::map<_rt_k, _rt_v, cmp_map> _router_table{};

    // Send a single datagram from the appropriate outbound interface to the next hop,
    // as specified by the route with the longest prefix_length that matches the
    // datagram's destination address.
    void route_one_datagram(InternetDatagram &dgram);

  public:
    // Add an interface to the router
    // \param[in] interface an already-constructed network interface
    // \returns The index of the interface after it has been added to the router

    size_t add_interface(AsyncNetworkInterface &&interface) {
        _interfaces.push_back(std::move(interface));
        return _interfaces.size() - 1;
    }

    // Access an interface by index
    AsyncNetworkInterface &interface(const size_t N) { return _interfaces.at(N); }

    // Add a route (a forwarding rule)
    void add_route(const uint32_t route_prefix,
                   const uint8_t prefix_length,
                   const std::optional<Address> next_hop,
                   const size_t interface_num);

    // Route packets between the interfaces
    void route();
};

#endif  // SPONGE_LIBSPONGE_ROUTER_HH
