- [ ] network manager (mandatory)
    it should show a plugin in panel. that shows currently using network intreface. (you can access to all netowrk interfaces with: `ip a`)
    then when i click on it, a pop-up window should shows and gives me all of the interfaces.
    i can select one of them. and it can become my main network interface.
    - [ ] there should be another plugin next to network manager plugin, i call it kill-switch for internet. so when i click on it, it should toggle between these states:
    "stop internet")
      sudo ip link set dev INTERFACE up
    "stop internet")
      sudo ip link set dev INTERFACE down
 
- [ ] clicking on network, makes system freeze.

- [ ] trying to change network interface in networkmanager plugin leads to system freeze.
  Maybe because it needs sudo access under the hood? fix it then.

- [ ] trying to use kill-switch network plugin in panel leads to system freeze.
  Does it need root access? (CAP_NET_ADMIN) implement it with polkit-devel or any other similar and minimal/safe way.
