## Low CBF Tools

This repository contains low-level engineering tools used during development of FPGA firmware. Each subdirectory contains code for a different tool. The tools are:
* gemini-viewer (Displays gemini FPGA internal registers)
* lfaa-sim (Generates a 25GBPS data stream, similar to that output by LFAA, that can be ingested & processed by FPGA)

## gemini-viewer
The gemini-viewer is a GUI application which displays/modifies the contents of registers implemented in a Gemini FPGA card. Used for debugging a FPGA implemenation at the lowest level. The application has no dependencies except the Qt5 framework used to implement the GUI. This allows it to be used even if Tango is not running.

## lfaa-sim
The LFAA interface simulator reads packetised data that the low-cbf-model produces and plays it over a 40GbE network interface to a Gemini FPGA card. It acts as a data source for testing the Gemini card's FPGA processing blocks. 
