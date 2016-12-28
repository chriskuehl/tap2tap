tap2tap
========

`tap2tap` is a simple layer-2 point-to-point VPN written in C.

It establishes a `tap` device on both hosts, and shuffles ethernet frames
back-and-forth between them over UDP.

The only supported mode of operation is one host to one other host.  Think of
it like a virtual ethernet cable connecting the two hosts.

I use `tap2tap` to connect my home network to my VPS (my home router runs
`tap2tap` in client mode, since it doesn't have a static IP address, but you
can also use it between two servers with static IPs).


## Why tap2tap?

I used OpenVPN for this purpose for many years, but it's a pain to deal with,
and I'm not convinced that I'm smart enough to configure OpenVPN securely.
There's about a thousand options, but I *don't want* to use various OpenVPN
options to try to configure bridging, packet forwarding, etc. OpenVPN even has
options to mangle DHCP packets!

All I want my VPN software to do is to give me a network interface. I'd much
rather use the standard Linux toolset to configure routes, bridges, maybe
change DNS, etc.

`tap2tap` was inspired by [SigmaVPN][sigmavpn] and [QuickTun][quicktun], which
I've used and enjoyed in the past.


## Current status

It works, and I'm using it, but it's missing key features (like encryption and
authentication).


### Usage

For full usage, run `tap2tap --help`.

*  on your client:
   ```
   $ tap2tap --remote 1.2.3.4
   ```

*  on your server:
   ```
   $ tap2tap
   ```

You'll probably want to add an IP to the interface (e.g. `ip addr add
10.0.0.1/24 dev tap0`). You can specify a script to run via `--up` to do this
automatically.

If both of your peers use static IPs, it's totally fine to specify `--remote`
on each.


#### MTU selection

tap2tap has a simple one-to-one correspondence between packets sent to the
`tap` interface and what goes over the wire. Each packet sent to the tap device
gets embedded in a UDP datagram and when it is sent over the tunnel. This adds
some overhead:

* 14 bytes for the ethernet headers
* 20 bytes for the standard IPv4 header
* 8 bytes for the UDP header

The above overhead is on *tunneled packets*, sent between the two peers. To
avoid fragmentation, you want your tap device's MTU, plus those 42 bytes of
overhead, to fit in one path MTU between the two peers.

Path MTU is frequently 1500 bytes, especially between well-connected peers. The
default MTU is thus `1500 - 42 = 1458` bytes. Many connections have lower MTUs,
though (e.g. DSL with PPP has a 1492 byte MTU, so you want to lower it by
another 8 bytes). You can use `ping -M do -s 1472 <peer>` (to test MTU 1500).
Lower the `-s` until it works, then you'll know the `-s` value (plus 28) is
your path MTU.

Fragmentation affects performance only, not correctness. You can use Wireshark
on the interface used by the peers (*not* the tunnel's tap interface) to see if
packets are fragmenting.


[sigmavpn]: https://github.com/neilalexander/sigmavpn
[quicktun]: http://wiki.ucis.nl/QuickTun
