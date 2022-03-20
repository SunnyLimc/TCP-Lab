#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

// \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the
// route_prefix will need to match the corresponding bits of the datagram's destination address? \param[in] next_hop The
// IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next
// hop address should be the datagram's final destination). \param[in] interface_num The index of the interface to send
// the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    //! my code
    uint8_t shift = prefix_length == 0 ? 0 : 32 - prefix_length;
    uint32_t start_index = route_prefix & (UINT32_MAX << shift);
    uint32_t end_index = start_index + static_cast<uint32_t>(pow(2, 32 - prefix_length) - 1);
    uint32_t next_hop_fmd = next_hop.has_value() ? next_hop.value().ipv4_numeric() : 0;

    auto insert =
        _router_table.insert(_router_table.cbegin(),
                             make_pair(make_pair(route_prefix, end_index),
                                       make_tuple(_router_table.cbegin(), interface_num, next_hop_fmd, prefix_length)));
    auto it = insert;
    // if meet with a duplicate key, it cloud be treat as the neatest_prefix
    while (it != _router_table.cbegin()) {
        --it;
        // geq does not make any sense, if a previous prefix already included dst, dst either choose it or skip it
        if (it->first.second > end_index) {
            // if a *it can include this prefix, that means it->nearest_prefix prefix is the nearest prefix can included
            // this prefix
            get<0>(insert->second) = it;
            break;
        }
    }
    //! NOTES
    // merge the subprefix from [route_prefix, end_index]
    // skip merge the subpreifx's subpreifx
    //! HOW TO
    // if the subprefix's length is small, we use iterator search, or we use lower_bound to search
    it = insert;
    ++it;
    auto log_length = static_cast<uint32_t>(log(end_index - route_prefix + 1));
    while (it != _router_table.cend()) {
        auto it_end = it->first.second;
        // skip the prefix geq than this
        if (insert->first.second >= it->first.second) {
            break;
        }
        get<0>(it->second) = insert;
        // use different strategy by prefix's length
        // short
        if (it->first.second - it->first.first + 1 < log_length) {
            while (++it != _router_table.cend() && it->first.first < it_end) {
            };
        }
        // (you can use binary number to verify it)
        else {  // long (avoid the situation that there are many subprefix in this prefix to slow down traversal)
            // if meet a same route_prefix then match the larger
            it = _router_table.lower_bound(make_pair(it_end, UINT32_MAX));
        }
        //! WHY THIS STRATEGY WORK ?
        //! you need to know that long prefix match will not overlap
        // (NOTE: "overlap" only refers to the situation that list below)
        //! that means |____|__|____|  ← that's not going to happen
        //!           1s   2s  1e   2e
        //! 192.168.1.1 use 255.255.0.0 -> 192.168.0.0 - 192.168.255.255
        //! and -.-.255.255 is a fix end and not crossover with a smaller prefix_length
        //! same thing for all the other prefix lens
    }
}

// \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl != 0) {
        --dgram.header().ttl;
    }
    if (dgram.header().ttl == 0) {
        return;
    }

    auto dst_ip = dgram.header().dst;
    // if there are to much segment large than dst_ip, this algorithm time complexity will degenerate to nearly o(n)
    // if have same, match the shortest, to ensure all prefix can be matched
    auto it = _router_table.lower_bound(make_pair(dst_ip, 0));
    size_t save_interf = 0;
    uint32_t tar_plen = 0;
    uint32_t tar_nexthop = 0;
    bool saved = false;
    bool fast_search = false;
    while (it != _router_table.cbegin()) {
        //! long prefix match only ensure overlap will not heppen between prefixs
        //! it can not ensure where is the dst_ip position (you can treat dst_ip as a end_index)
        //! so we need addition step to ensure fast_search work
        //! but, because of that (not overlap), the addition work will not take long
        if (not fast_search) {
            // bind the first element that small than dst_ip
            --it;
            if (it->first.second <= dst_ip) {
                fast_search = true;
            }
        } else {  // use fast_search
            it = any_cast<std::map<_rt_k, _rt_v, cmp_map>::iterator>(get<0>(it->second));
        }
        auto cur_pfl = get<3>(it->second);
        //!! cur_pfl geq is needed. we also need to use this prefix for send packet when prefix_len == 0.
        if (it->first.second >= dst_ip && cur_pfl >= tar_plen) {
            tar_plen = cur_pfl;
            save_interf = get<1>(it->second);
            tar_nexthop = get<2>(it->second);
            saved = true;
        }
    }
    if (saved) {
        interface(save_interf)
            .send_datagram(
                dgram, tar_nexthop == 0 ? Address::from_ipv4_numeric(dst_ip) : Address::from_ipv4_numeric(tar_nexthop));
    }
    return;
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
