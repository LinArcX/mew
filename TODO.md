## Features
- [ ] implement workspaces.
  it should be possible to define them(number of them and name of each) through conf file.

- [ ] implement background switcher.
  So, when you click on background on mew, a context-menu should have these two options:
    "change background color"
    "change background image"
  And for image, you need to set for it the directories he can search for images.

- [ ] add persian glyphs to current font.

- [ ] ability to change font size/name for active/inactive windows.
  currently it's hard-coded. finally it should read from conf file.

- [ ] mouse theme
  currently it's hard-coded. finally it should read from conf file.

- [ ] impelemnt windows theme

- [ ] transfer my README file to a professional README. i want it to be:
  - simple
  - understandable
  - attract peoples attention. (at the sime time minimal and not bloated)
  - looks professional
  - link to pateron

## Bugs
- [ ] sometimes, i want to upload files to some website, so normally nemo pops up with image preview panel inside it.
  in this cases, the buttons in bottom(Canel, Ok) are not fully visible. for example just half of OK button is visilbe. fix it.

- [ ] some gtk apps like gedit or meld, they already have frame and close/max/min buttons. in mew, we put an extra frame out of it which doesn't make sense.
  So check if the app, or window already has frame, if it has don't make any frame around it.

- [ ] imagine i open neovim, and also i open mpv on top. i close mpv with close buton, it shows me neovim.
  but cursor in neovim is empty rectangle instead of full rectangle.(maybe because the focus is not on neovim?)
