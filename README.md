# Tangent Field Editor
This editor allows editing 2D tangent fields and rendering in real-time the anisotropic appearance defined by such fields. This project is the result of my internship in the team [MFX](https://mfx.loria.fr/) on the occasion of my master's thesis.

![preview of the user interface of the software](assets/preview.png)

## Usage
For further information about the usage please refer to the help menu or alternatively, you can see the same information [here](assets/help.md).

## Building
To build on both Windows and Linux the following dependencies are required:
- [Qt 6.7+](https://www.qt.io/download-dev)
- [Vulkan SDK](https://vulkan.lunarg.com)

We advise using Qt creator for the building, nevertheless, this is not strictly required. Opening the project file `Editor.pro` and selecting `Build -> Build Project "Editor"` should suffice.

Building on MacOS would require not trivial work as Vulkan is not currently supported on that system natively and alternatives such as MoltenVK would have to be used.