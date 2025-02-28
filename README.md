# Tangent Field Editor
This editor allows editing 2D tangent fields and rendering in real-time the anisotropic appearance defined by such fields. This project is the result of my internship in the team [MFX](https://mfx.loria.fr/) on the occasion of my master's [thesis](https://drive.google.com/file/d/1iu_zLZyRdtY2Du09JKbdB2y5B_ygdd7k/view?usp=drive_link).

![preview of the user interface of the software](assets/preview.png)

## Rendering
The rendering algorithm was accepted for [PG2024](https://pg2024.hsu.edu.cn/). The relevant file is located at `src/Render/Shaders/fast.frag`.

For further information refer to the [project page](https://xavierchermain.github.io/publications/aniso-ibl).

## Usage
For further information about the usage please refer to the help menu or alternatively, you can see the same information [here](assets/help.md).

## Dependencies
This project depends on Qt 6.7+ and the Vulkan SDK 1.3+, we also advise using Qt Creator for the building process, nevertheless, this is not strictly required.

### Windows
- Install Qt using the [online installer](https://www.qt.io/download-qt-installer-oss). During the installation process select "Qt6.x for desktop development".
- Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows).

### MacOS
Building on MacOS is not supported as it would require not trivial work as Vulkan is not supported natively and alternatives such as MoltenVK would have to be used.

### Linux
#### Majaro 24.0.4
Running `sudo pacman -S qtcreator make` on a fresh installation is sufficient.

#### Ubuntu 24.04 LTS
- Run `sudo apt install qtcreator clang libxcb-cursor0 libxcb-cursor-dev`.
- Download Qt [online installer](https://www.qt.io/download-qt-installer-oss).
- Change the installer permission to make it executable using `cmod +x qt-online-installer-linux-x64-4.8.1.run`.
- Run the installer using `./qt-online-installer-linux-x64-4.8.1.run`, during the installation process select "Qt6.x for desktop development".
- Install the Vulkan SDK as specified [here](https://vulkan.lunarg.com/doc/view/latest/linux/getting_started_ubuntu.html). For Ubuntu 24.04 LTS you have to run `wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc; sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list; sudo apt update; sudo apt install vulkan-sdk`.

## Building
- Open the project file "Editor.pro" by selecting "File" -> "Open File or Project" inside Qt Creator.
- Go to the "Projects" tab (which can be selected on the left) and click "Configure Project" button (on the bottom-right).
- Build and run the project by selecting "Build" -> "Run".
- To generate a portable installation, please refer to [Qt deployment instruction](https://doc.qt.io/qt-6/deployment.html).