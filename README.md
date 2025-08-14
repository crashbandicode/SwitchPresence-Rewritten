# SwitchPresence-Rewritten
Change your Discord rich presence to your currently playing Nintendo Switch game! Concept taken from [SwitchPresence](https://github.com/Random0666/SwitchPresence) by [Random](https://github.com/Random0666)<br>

## Install
General homebrew installing information can be found [here](https://switch.homebrew.guide/)<br>

You will also need to install [SwitchPresence-CSharpClient](https://github.com/crashbandicode/SwitchPresence-CSharpClient)

## Building from Source

To build this project from the source code, you will need to set up a development environment for Nintendo Switch homebrew.

1.  **Install DevKitPro:** Follow the instructions on the [DevKitPro Getting Started page](https://devkitpro.org/wiki/Getting_Started). Make sure to select "Switch Development" during the installation.

2.  **Install Dependencies:**  Open the MSYS2 terminal that comes with DevKitPro and run the following command:
    ```bash
    pacman -S switch-libgd zip
    ```

3.  **Build the Project:** Once the environment is set up, you can build the entire project by running `make` in the root directory.

