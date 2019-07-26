# A Qt5 GUI application that allows Gemini registers to be displayed.

The application can be built either via command line or with the QtCreator IDE.
In both cases **the first step is to create a Makefile** using the command-line
qmake tool. (Trying to open the project file with the QtCreator IDE before
creating a Makefile results in errors because the C++14 directive is not
applied.)
  0: qmake (invokes qmake-qt5 to build the toplevel Makefile)

## Linux command line build steps:
  1: make  (recursively builds Makefiles and compiles code)
The build product will be in the bin subdirectory
There will be a library in the lib subdirectory.

## Build steps for QtCreator IDE:
  1: Choose File->'open file or project'
  2: Navigate to and choose the 'gemini-viewer.pro' project file
  3: follow the prompts to configure project build directory, etc
  3: Choose Build->'Build All'
Location of the build products depends on the configuration.

# Notes on running the gemini-viewer application

When run, the application will open a window that shows all the Gemini
broadcast events that it receives from the local network. Gemini devices
send 'discovery' packets after power up, repeated every two seconds.
(Note that at present Gemini devices do nothing for 30 seconds after boot
 before starting regular broadcast of discovery packets.)

Clicking on any of the IP addresses shown in the window will allow that
FPGA instance to be opened and its registers to be explored. When opening
the FPGA, a config file (extension '.ccfg') that specifies fields in the
FPGA is required. Config files can be generated using the ARGS tool
gen`_`c`_`config.py, eg
  python3 tools/args/gen`_`c`_`config.py -f kcu105`_`mace`_`test 

The gemini-viewer application creates a **debug log file 'log-file.txt'** in
the directory from which it was run. The file will grow in size over time
because the application only appends to it and never deletes it. Users can
delete the file at any time they wish, even while the application is running.
