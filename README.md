## Low CBF Tools

This repository contains low-level engineering tools used during development of FPGA firmware. Each subdirectory contains code for a different tool. The tools are:
* gemini-viewer (Displays gemini FPGA internal registers)
* lfaa-sim (Generates a 25GBPS data stream, similar to that output by LFAA, that can be ingested & processed by FPGA)

## gemini-viewer
The gemini-viewer is a GUI application which displays/modifies the contents of registers implemented in a Gemini FPGA card. It communicates with a Gemini FPGA card using UDP packets over an IP network. The communication protocol is described here: https://confluence.skatelescope.org/display/SE/Gemini+Protocol+in+brief. The application is intended to be used for debugging a FPGA implemenation at the lowest level.

Usage: The gemini viewer is a GUI application that is started without command line arguments. It appends debug information to a file 'log-file.txt' located in its start directory. The GUI will show a window that lists all the FPGAs on the local network that are emitting 'discovery' packets. Clicking on any of these will allow the internal registers of the FPGA to be viewed. When opening a FPGA, a register address file (.ccfg) for the FPGA is requested, but if none is available an empty .ccfg file can be given. Register address files are output from ARGS (Automatic Register Generation System) which is part of the FPGA build framework.

## lfaa-sim
The LFAA interface simulator reads packetised data that the low-cbf-model produces and plays it over a 40GbE network interface to a Gemini FPGA card. It acts as a data source for testing the Gemini card's FPGA processing blocks. lfaa-sim is a unix command line program.

Usage: *./lfaa-sim -h header\_file -d data\_file -a my.ip.dest.addr -p dest\_port -r repeats -z no\_of\_pkts*
where:
* *-h header\_file* is the packet header file output from the matlab model
* *-d data\_file* is the packet data file output from the matlab model
* *-a my.ip.dest.addr* is the IPv4 address of the interface used to send packets
* *-p dest\_port* is the UDP destination port that packets are sent to
* *-r N* (optional) specifies how many times to repeat the data (0 = send once, no repeat)
* *-z N* (optional) specifies how many packets from the file to send
If no arguments are given to lfaa-sim, it will print this usage information


## Runtime dependencies
* gemini-viewer: Qt5, register address file generated from FPGA build (.ccfg)
* lfaa-sim: Linux/Unix OS, packet data output from the matlab model

## Build dependencies
* gemini-viewer: Qt5, C++14 (or later) compiler
* lfaa-sim: C++14 (or later) compiler, Linux/Unix OS
