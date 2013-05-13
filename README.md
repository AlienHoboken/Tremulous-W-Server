Tremulous W Server
==================

This repository contains the source code for the popular W server gameplay mod for tremulous

Following is information on running this mod.

Extra Dependencies
-----------------------
The W QVM depends on MySQL/MySQL development libraries. Ensure these are installed before attempting to build.

Building
-----------------------
You can build the W QVM the same way you build normally.

Make sure that the `MAKE_GAME_QVM` flag is set in the Makefile before building.

As normal there are shell scripts for building on Windows and Mac OSX. Windows requires MingW be used.

Disable MySQL before building if you do not wish to use the MySQL offerings of the W mod.

Configuration
-----------------------
If you are using the MySQL offerings, you will also need to specify your credentials in the server's config file.

Running
-----------------------
The W QVM requires the xserverx customized tremulous dedicated server to run. You can find the code for this server here: https://github.com/AlienHoboken/Tremulous-xserverx-tremded

Furthermore, when running the W mod will attempt to communicate with a MySQL database. Make sure you disable MySQL if you do cannot support this behavior.
