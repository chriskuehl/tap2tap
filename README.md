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
authentication), and has a couple design quirks I plan to factor out.


[sigmavpn]: https://github.com/neilalexander/sigmavpn
[quicktun]: http://wiki.ucis.nl/QuickTun
