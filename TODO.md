- [ ] keybindings window, should have a EDIT button at bottom. so when user click on it, it can be able to edit text.
  In Edit Mode, There should be a SAVE button at bottm, beside that there should be a VIEW-Mode button.
  So when user click on View-Mode, keybindings window come back to it's initial view mode state.
  When you save, it should really save on the file on disk.
  Please also add lines numbers, when we are in edit mode.

- [ ] implement workspaces.
  it should be possible to define them(number of them and name of each) through conf file.

- [ ] implement background switcher.
  So, when you click on background on mew, a context-menu should have these two options:
    "change background color"
    "change background image"
  And for image, you need to set for it the directories he can search for images.

- [ ] when clicking on keyboard switcher on panel, there should be a pop-up shows to user.
  - this pop-up contains langauges in a list, so yuo can select from the list.
  - we should show the flag of the language also.

- [ ] ability to choose city in weather widget.
  A pop-up window should shows up with list of all major cities in world. (implement it like Apps pop-up with ability to search and navigate)
  When user select it's city, it should reflect the widget in panel and also it should update conf file.

- [ ] sometimes, i want to upload files to some website, so normally nemo pops up with image preview panel inside it.
  in this cases, the buttons in bottom(Canel, Ok) are not fully visible. for example just half of OK button is visilbe. fix it.

- [ ] ability to change font size/name for active/inactive windows.

- [ ] some gtk apps like gedit or meld, they already have frame and close/max/min buttons. in mew, we put an extra frame out of it which doesn't make sense.
  So check if the app, or window already has frame, if it has don't make any frame around it.

- [ ] ability to set panel items color + panel item hover color in config file.

- [ ] network manager (mandatory)
    it should show a plugin in panel. that shows currently using network intreface. (you can access to all netowrk interfaces with: `ip a`)
    then when i click on it, a pop-up window should shows and gives me all of the interfaces.
    i can select one of them. and it can become my main network interface.
    - [ ] there should be another plugin next to network manager plugin, i call it kill-switch for internet. so when i click on it, it should toggle between these states:
    "stop internet")
      sudo ip link set dev INTERFACE up
    "stop internet")
      sudo ip link set dev INTERFACE down
    
- [ ] transfer my README file to a professional README. i want it to be:
  - simple
  - understandable
  - attract peoples attention. (at the sime time minimal and not bloated)
  - looks professional
  - link to pateron
- [ ] add persian glyphs to current font.

- [ ] windows title fonts
  currently it's hard-coded. finally it should read from conf file.

- [ ] mouse theme
  currently it's hard-coded. finally it should read from conf file.

- [ ] impelemnt windows theme

- [ ] power manager (mandatory)
  these items just log out. reboot, doesn't reboot actually. poweroff doesn't poweroff actually!
  - [ ] reboot
  - [ ] poweroff
  - [ ] mew --reconfigure
    not working as expected. need to test it again.

- [ ] imagine i open neovim, and also i open mpv on top. i close mpv with close buton, it shows me neovim.
  but cursor in neovim is empty rectangle instead of full rectangle.(maybe because the focus is not on neovim?)
