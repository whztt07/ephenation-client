IDEAS
=====

Oculus Rift
-----------
* Information text drawn by vsfl does not show properly in stereo mode
* Drawing monster stats should use gProjectionView
* Use bold font for better readability
* Dialogs are usually made for high resolution screens, so they become very big
* The mouse pointer should be hidden when not needed
* The map should only be computed once in OVR mode, not once for each eye
* Text in options windows doesn't always show
* Tilting the head should also tilt the head of the avatar, for other players to see
* Enlarge the compass a little, it is hard to find.
* A better frame is needed for OVR mode, to make it more obvious what the moving direction is.

Graphical effects
-----------------
* Lamps and spherical fogs are cut off too hard at the edge
* Upgrade to GLFW 3, to support alternative displays (wait for Ubuntu to support it?)
* Implement LOD (scale blocks with a factor of 2)
* Enable blurred shadows again
* Use fading surface material effects at block border, where needed
* Surface effects: Moss, rust, dust, dirt
* Update textures with higher resolution (512x512)
* libRocket Dialogs projected on blocks (a special case of the action block)
* For underground, use automatic green tints for positive up normals
* Make three sets of skyboxes, for different time of day
* Make water splash when a player or monsters falls into water
* Move all shaders to the Shaders namespace
* Use particle effects to draw grass
* Use atmospheric based realistic lighting

Sound
-----
* Rewrite sound management using [OpenAL Soft](http://kcat.strangesoft.net/openal.html) and [Alure](http://kcat.strangesoft.net/alure.html).
* Implement environment depending echo using delayed emitters at a number of directions
* Sound from walking is wrong (generating too many steps)

Other
-----
* Fix the issue with low initial lagging FPS
* Timers should also be drawn live on the screen
* Pre-load chunks of teleport destination
* Update to newer version of libRocket
* Teleport shall use a jump parabola instead of instant transfer. If inside rock, black will have to be used.
* Create About-dialog

Refactoring
-----------
* The ui/options.cpp and Options.cpp has redundant use of name for options. That should be centralized at one place.
* The transformation of a chunk into printable data should be managed by a Streaming class
* glsw should be moved to a separate github repository, and included as a submodule
* Remove glfw 2.7.9 shared library from debian installer, as soon as it is standard in Ubuntu.

Installation
------------
* Uninstalling the game should also remove the cache.

UI
--
* When logging in, pressing ESC of "Cancel" shall quit the game.
