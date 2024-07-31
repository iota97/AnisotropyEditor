# Tangent Field Editor
This editor allows editing 2D tangent fields and to render in real time the anisotropic appearance defined by such field.

***
***

## View controls
It is possible to move the object to see the anisotropic appearance at various angles.
### Move
Press the Mouse wheel to pan the camera.
### Rotate
Press Ctrl + Mouse wheel to rotate the object. Right mouse button can be used, but only with the Edit tool.
### Zoom
Use the mouse wheel to zoom.

***
***

## Tools
To edit the tangent field it is possible to specify constraints using a vector like graphic editor. The constraints are then propagated to produce the smoothest tangent field satisfying them.
Tools are located in the tools bar on the left side of the screen.
### Edit (Q)
Allows to select a constraint on the screen. Selected constraints can then be moved, rotated and scaled at will using the handles that will appear on them. It is possible to move each point of a line constraint separately and well as add new points by double-clicking on the line.
### Line (W)
Add a new line, left click to place the various points and Right mouse button, Enter or Escape to confirm. The constraint is tangent to the line direction.
### Rectangle (R)
Add a new rectangle. The constraint direction is constant.
### Ellipse (E)
Add a new ellipse. The constraint is tangent to the border.

***
***

## Constraints editor
This menu is placed in the right part of the screen. It allows to edit the selected constraint.
### Position
Specifies the constraint position.
### Rotation
Specifies the constraint rotation.
### Scale
Specifies the constraint scale.
### Skew
Specifies the constraint skew.
### Direction
Rotate the constraint direction with respect to the default one. Constraints are color-encoded based on this parameter.
### Align field to border
If selected the field will be aligned with the constraint border.
### Duplicate (CTRL+D)
Duplicate the selected constraint.
### Remove (Delete)
Remove the selected constraint.

***
***

## Information bar
The information bar is located at the bottom of the screen.
### Resolution and mouse position
On the left side the image resolution and the current mouse position can be read.
### Frame rate and sample count
On the right side, the current frame rate and sample count are displayed.

***
***

## Menu
This section contains a brief overview of the menus.

***

### File

***

#### New project
Creates a new project.
#### New project from reference
Creates a new project with the same dimension of a reference image.
The reference image can be seen using "View -> Reference".
#### Save project
Save the current project.
#### Open project
Load a previously saved project.
#### Load mesh
Load a Wavefront mesh for visualization, note that this would disable the editing operation.
#### Unload mesh
Unload a previously loaded mesh.
#### Load reference image
Load a reference image, does not affect the project state in any way.
The reference image can be seen using "View -> Reference".
#### Import anisotropy angles [0, 180]
Load an image where the luminosity of each pixel encodes the anisotropy angle in [0, 180].
If one wants to change the conversion, without recompiling the project, it is possible to edit "assets/shaders/imageImporter_comp.spv". Its GLSL source code is located in "src/Field/Shaders/imageImporter.comp".
#### Import anisotropy angles [0, 90]
Load an image where the luminosity of each pixel encodes the anisotropy angle in [0, 90].
#### Import anisotropy vectors
Load an image encoding the anisotropy as a 2D vector in the red/green channels.
#### Hot reload anisotropy
If selected the last image loaded with be automatically reimported if is changed on disk. This allows integration with other image editing software.
#### Export anisotropy angles [0, 180]
Export an image where the luminosity of each pixel encodes the anisotropy angle in [0, 180].
If one wants to change the conversion, without recompiling the project, it is possible to edit "assets/shaders/imageExporter_comp.spv". Its GLSL source code is located in "src/Field/Shaders/imageExporter.comp".
#### Export anisotropy vectors
Export an image where the 2D anisotropy direction is encoded in the red/green channels and the phase from the optimizer is encoded in the alpha one.
#### Export sine field
Export a binary image with the evaluated sine field produced by the optimizer.
#### Quit
Exit the application.

***

### Edit

***

#### Undo
Undo the last constraint editing operation.
#### Redo
Redo the last constraint editing operation.
#### Render settings
Open the render settings menu.
#### Optimizer settings
Open the optimizer settings menu.
#### Reset view
Reset the original position of the image.

***

### View

***

#### Fast
Render a realistic appearance of the object. The illumination is specified by an HDR environment map in "assets/textures/cubemap/". It is possible to change this to edit the lighting environment.
#### Accurate
Slower realistic render using importance sampling. Will converge over time.
#### Direction
Noise-based visualization of the tangent field.
#### Twilight
Cyclic color encoding to visualize the angle of the tangent field.
#### Sine Field
Sinusoidal-based visualization of the tangent field produced by the optimizer.
#### Reference
Shows the previously loaded reference image.
#### Legacy
Fast rendering using bent normal.
#### Show constraints
Toggle the rendering of the constraints.
#### Fullscreen
Toggle full-screen view.

***

### Help

***

#### Show help
Shows this help text.

***
***

## Render settings
Allows to edit the appearance of the material.
### Material color
Changes the material albedo.
### Anisotropy
Changes the amount of anisotropy.
### Roughness
Changes the amount of roughness.
### Metalness
Allows specifying if the material is dielectric or metallic.
### Exposure
Allows to change the exposure to better visualize the HDR lighting effects.
### Samples count (fast only)
Change the number of samples used by the "fast" rendering path.
***
***

## Optimizer settings
Allows to edit the parameters of the optimizer.
### Method
Specifies the field representation used for averaging the neighborhood. Vector fields are affected by the bias inducted by the order of traversal, Tensor fields fail with orthogonal direction in a worse way.
"Tensor field - vector fallback" tries to use tensor field and falls back to vector if the directions are too close to be orthogonal.
### Knöppel et al. 2015 alignment method
Uses a different algorithm to align the phases that improve the quality.
### Optimize on move
If enabled, the optimizer is run each time a constraint is moved and not only on mouse release.
### Iteration
Specifies the number of iterations for each level of the multi-resolution grid.
### Iteration on move
Specifies the number of iterations for each level of the multi-resolution grid when a constraint is moved.
### Angle offset
Allows to specify an angle offset for the tangent field.

***
***

## Credits
Developed by Giovanni Cocco.

***
