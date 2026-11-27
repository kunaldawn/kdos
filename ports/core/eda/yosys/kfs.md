## About
Yosys synthesises Verilog into a netlist: the first stage of the open FPGA flow, ahead of
place-and-route, and the engine under formal verification with SBY.

## Notes
[python3-click](#port-python3-click) is in `depends` for its run time: `yosys-witness`, the tool
that converts and slices the witness traces SBY and `sim` read and write, is a Python program
built on click and imports it on every run.
