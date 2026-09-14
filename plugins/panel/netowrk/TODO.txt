- [ ] clicking on network, makes system freeze.

- [ ] trying to change network interface in networkmanager plugin leads to system freeze.
  Maybe because it needs sudo access under the hood? fix it then.

- [ ] trying to use kill-switch network widget in panel leads to system freeze.
  Does it need root access? (CAP_NET_ADMIN) implement it with polkit-devel or any other similar and minimal/safe way.
