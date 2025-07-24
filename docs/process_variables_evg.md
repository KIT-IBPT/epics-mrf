Process variables for EVG
=========================

When using the database files that are provided as part of the device support,
process variables for controlling and monitoring nearly every aspect of the EVG
are provided.

The exact list of process variables depends on the device type (different form
factors or hardware revisions have different features), but most of the process
variables described here should be available for the majority of the devices.

The process variables listed here are specified without any prefix, so the
device specific prefix (the combination of the `P` and `R` macros
[specified in the IOC's startup file](using.md)) has to be added to the names
specified here. An exception is the EVG that is embedded into an
[EVM](process_variables_evm.md). In this case, there is an additional middle
part, so the effective prefix is `$(P)$(R)G:`.


### Table of contents

- [AC synchronization](#ac-synchronization)
- [Data buffer](#data-buffer)
- [Distributed bus](#distributed-bus)
- [Event analyzer](#event-analyzer)
- [Event sequence RAM](#event-sequence-ram)
- [Event triggers](#event-triggers)
- [Other event settings](#other-event-settings)
- [Event clock](#event-clock)
- [General status and settings](#general-status-and-settings)
- [Inputs](#inputs)
- [Interrupts](#interrupts)
- [Multiplexed counter](#multiplexed-counters)
- [Outputs](#outputs)
- [SFP module](#sfp-module)


## AC synchronization

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>ACSync:ByPassDividerAndShift</td>
<td>Divider and phase-shifter by-pass flag. If enabled (1), the phase shifter and divider are bypassed. This means that the respective settings do not have any effect.</td>
</tr>
<tr>
<td>ACSync:Divider</td>
<td>Divider for the AC sync logic. The AC frequency (usually 50 Hz) is divided by this number, reducing the frequency of the AC sync trigger. For example, a divider of 50 with an input of 50 Hz results in one AC sync event every second.</td>
</tr>
<tr>
<td>ACSync:MapTo:EvTrig#</td>
<td>Mapping of AC sync logic to event trigger #. If 1, the AC sync event is mapped to the respective event trigger (sending the event code associated with that trigger). If 0, the respective mapping is disable. # usually has to be a number between 0 and 7.</td>
</tr>
<tr>
<td>ACSync:PhaseShift</td>
<td>Phase shift for the AC sync logic. This can be used to shift the AC sync event w.r.t. to the phase of the AC sync input.</td>
</tr>
<tr>
<td>ACSync:SyncSource</td>
<td>Synchronization source of the AC sync logic. If 0, the AC sync event is generated at the event cycle when the AC sync logic triggers. If 1, a trigger of the AC sync logic is delayed until multiplexed counter 7 has counted down. This can be used to ensure that the AC sync event is coincidental with some other clock (which should typically be much larger than the AC input frequency).</td>
</tr>
</table>


## Data buffer

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DataBuffer:TX:Complete</td>
<td>Transmission complete flag (read-only). 1 if the last transmission of the data buffer has finished, 0 if it is still running. This record is not updated automatically. Process it (write 0 to the PROC field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:TX:Enabled</td>
<td>Transmissions enabled flag. 1 if data buffer transmissions shall be enabled, 0 if they shall be disabled. Data buffer transmissions will only work, if the distributed bus is configured to share its bandwidth with data transmissions.</td>
</tr>
<tr>
<td>DataBuffer:TX:Running</td>
<td>Transmission running flag (read-only). 1 if a data transmission is current running, 0 if no transmission is running. This record is not updated automatically. Process it (write 0 to the PROC field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:TX:TransferSize</td>
<td>Size of the data that shall be transmitted (in bytes) only complete words (32-bit) of data can be transferred, so this number has to be a multiple of 4.</td>
</tr>
<tr>
<td>DataBuffer:TX:TransmitData</td>
<td>Data to be transmitted. The data is represented as an array of 32-bit words. When changing this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>DataBuffer:TX:TransmitData:WriteAll</td>
<td>Writing to this record forces all elements of the DataBuffer:TX:TransmitData array to be written to the hardware.</td>
</tr>
</table>


## Distributed bus

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DBus:B#:Description</td>
<td>Description for distributed bus bit #. # must be a number between 0 and 7. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific distributed bus bit.</td>
</tr>
<tr>
<td>DBus:B#:Map</td>
<td>Mapping of distributed bus bit #. # must be a number between 0 and 7. The mapping can be 0 to disable this distributed bus bit, 1 to use external input #, 2 to use mutiplexed counter #, or 3 in order to forward the distributed bus bit state received from an upstream EVG or EVR.</td>
</tr>
<tr>
<td>DBus:B#:RX:Status</td>
<td>Status of the distributed bus bit # received from an upstream EVG or EVR (read-only). This information is only updated when DBus:Status is processed.</td>
</tr>
<tr>
<td>DBus:B#:TX:Status</td>
<td>Status of the distributed bus bit # send to a downstream EVG or EVR (read-only). This information is only updated when <code>DBus:Status</code> is processed.</td>
</tr>
<tr>
<td>DBus:SharedTX</td>
<td>Bandwidth share flag. If enabled (1), the distributed bus bandwidth is halfed in order to be able to send data from the data buffer. If disabled (0), the full distributed bus bandwidth is available and data transmissions are disabled.</td>
</tr>
<tr>
<td>DBus:Status</td>
<td>Combined RX/TX status of all distributed bus bits as a bitfield (read-only). Process this record (write 0 to its <code>PROC</code> field) in order to update it by reading from the hardware. This also updates the RX/TX status records.</td>
</tr>
</table>


## Event analyzer

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:Analyzer:Counter:High</td>
<td>High word of the event analyzer’s counter (read-only). This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>Event:Analyzer:Counter:Low</td>
<td>Low word of the event analyzer’s counter (read-only). This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>Event:Analyzer:Counter:Reset</td>
<td>Counter reset flag (read-only). 1 if the event analyzer’s reset flag is set, 0 otherwise. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status. </td>
</tr>
<tr>
<td>Event:Analyzer:Enabled</td>
<td>Event analyzer enable flag. 1 if the event analyzer shall be enabled, 0 if it shall be disabled.</td>
</tr>
<tr>
<td>Event:Analyzer:EventCode</td>
<td>Event code from the event analyzer’s FIFO (read-only). This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>Event:Analyzer:FIFONotEmpty</td>
<td>FIFO not empty flag (read-only). 1 if the event analyzer’s FIFO contains elements, 0 if it is empty. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>Event:Analyzer:FIFOOverflow</td>
<td>FIFO overflow flag (read-only). 1 if the event analyzer’s FIFO has overflown, 0 otherwise.  This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>Event:Analyzer:Reset</td>
<td>Reset flag (read-only). 1 if the event anaylzer’s reset flag is set, 0 otherwise. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
</table>


## Event sequence RAM

There are two sequence RAMs (sequence RAM 0 and 1). In this list, the generic
prefix SeqRAM# is used for all PVs that exist for both sequence RAM 0 and 1.
Replace # with 0 or 1 depending on the target sequence RAM.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:SeqRAM#:Disable</td>
<td>Writing to this record disables the sequence RAM.</td>
</tr>
<tr>
<td>Event:SeqRAM#:Enable</td>
<td>Writing to this record enabled the sequence RAM.</td>
</tr>
<tr>
<td>Event:SeqRAM#:Enabled</td>
<td>Enabled status of the sequence RAM (read-only). 1 if it is currently enabled, 0 if it is currently disabled. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>Event:SeqRAM#:EventCodes</td>
<td>List of event codes used by the sequence RAM. This list together with the time-stamps represents the contents of the sequence RAM. The sequence RAM holds 2048 entries. When changing this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>Event:SeqRAM#:EventCodes:WriteAll</td>
<td>Writing to this record forces all elements of the Event:SeqRAM#:EventCodes array to be written to the hardware.</td>
</tr>
<tr>
<td>Event:SeqRAM#:Recycle</td>
<td>Recycle flag. If 1, the sequence RAM automatically starts again after having reached the end of sequence. If 0, this is disabled and the sequence RAM waits for the next trigger.</td>
</tr>
<tr>
<td>Event:SeqRAM#:Reset</td>
<td>Writing to this record resets the sequence RAM. In particular, this will force the sequence RAM to begin back at the start.</td>
</tr>
<tr>
<td>Event:SeqRAM#:Running</td>
<td>Running status of the sequence RAM (read-only). 1 if it is currently running, 0 if it is currently not running. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>Event:SeqRAM#:SingleSeq</td>
<td>Single sequence flag. If 1, the sequence RAM is automatically disabled after reaching the end of sequence. If 0, the mapping RAM is not disabled. Whether it starts again right away or waits for the next trigger depends on the recycle flag.</td>
</tr>
<tr>
<td>Event:SeqRAM#:SoftTrig</td>
<td>Write to this record in order to start the sequence RAM: This only has an effect if the sequence RAM is configured to use the respective soft trigger as its trigger source.</td>
</tr>
<tr>
<td>Event:SeqRAM#:TimeStamps</td>
<td>List of time-stamps used by the sequence RAM. This list together with the event codes represents the contents of the sequence RAM. The sequence RAM holds 2048 entries. When changing this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>Event:SeqRAM#:TimeStamps:WriteAll</td>
<td>Writing to this record forces all elements of the Event:SeqRAMX:TimeStamps array to be written to the hardware.</td>
</tr>
<tr>
<td>Event:SeqRAM#:TrigSource</td>
<td>Trigger source for the sequence RAM. A value of 0 to 7 selects the corresponding mutiplexed counter. 8 selects the AC sync logic. 9 selects soft trigger 0 and 10 selects soft trigger 1.</td>
</tr>
<tr>
<td>Event:SeqRAM#:WriteAll</td>
<td>Writing to this record forces all elements of the Event:SeqRAMX:EventCodes and Event:SeqRAMX:TimeStamps arrays to be written to the hardware.</td>
</tr>
</table>


## Event triggers

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:EvTrig#:Description</td>
<td>Description for event trigger #. # must be a number between 0 and 7. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific event trigger.</td>
</tr>
<tr>
<td>Event:EvTrig#:Enabled</td>
<td>Event trigger # enable flag. 1 if the event trigger # shall be enabled, 0 if it shall be disabled. # must typically be a number between 0 and 7. If the event trigger is enabled and some other logic is mapped to that event trigger, the corresponding event code is send to a downstream EVG or EVR whenever the mapped logic triggers.</td>
</tr>
<tr>
<td>Event:EvTrig#:EventCode</td>
<td>Event code sent by event trigger #. This is the event code that is sent to a downstream EVG or EVR when the respective event trigger is enabled and is triggered.</td>
</tr>
</table>


## Other event settings

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:MapDBusExtInput5ToTSReset</td>
<td>External input to time-stamp reset mapping. If enabled (1), the external input usually used for distributed bus bit 5 causes event code 0x7D (reset time-stamp) to be sent. If disabled (0), this mapping is disabled.</td>
</tr>
<tr>
<td>MapDBusExtInput6ToSec0</td>
<td>External input to seconds register 0 input mapping. If enabled (1), the external input usually used for distributed bus bit 6 causes event code 0x70 (time-stamp seconds 0 input) to be sent. If disabled (0), this mapping is disabled.</td>
</tr>
<tr>
<td>Event:MapDBusExtInput7ToSec1</td>
<td>External input to seconds register 1 input mapping. If enabled (1), the external input usually used for distributed bus bit 7 causes event code 0x71 (time-stamp seconds 1 input) to be sent. If disabled (0), this mapping is disabled.</td>
</tr>
<tr>
<td>Event:Soft:Enabled</td>
<td>Soft events enabled flag. If 1, writing to Event:Soft:EventCode causes the corresponding event code to be written. If zero, soft events are not sent.</td>
</tr>
<tr>
<td>Event:Soft:EventCode</td>
<td>Event code to be sent. Writing to this record sends the respective event code, if soft events have been enabled.</td>
</tr>
<tr>
<td>Event:Soft:Pending</td>
<td>Soft event pending flag (read-only). If 1, a soft event is currently pending to be sent, if 0 no event is pending. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
</table>


## Event clock

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>EventClock:Divider</td>
<td>RF clock to event clock divider. This divider is used when deriving the event clock from the external RF input. A value of 13 disables the RF clock input.</td>
</tr>
<tr>
<td>EventClock:Freq</td>
<td>Frequency of the event clock. This frequency only has to fit roughly because it is only used for estimating the length of a microsecond in event clock cycles. For example, this information is used by the heartbeat watchdog.</td>
</tr>
<tr>
<td>EventClock:Gen:Freq</td>
<td>Frequency generated by the internal frequency generator (Micrel SY87739L). The frequency is not specified directly. Instead, one has to choose from a list of preconfigured frequencies.</td>
</tr>
<tr>
<td>EventClock:Gen:Locked</td>
<td>PLL status of the internal frequency generator (read-only). 1 if the PLL has locked, 0 if it is unlocked. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>EventClock:RefSelect</td>
<td>Reference clock used for the event clock. If 0, the internal frequency generator is used. If 1, the external RF reference clock is used.</td>
</tr>
</table>

For some hardware revisions, there are additional PVs directly representing the status of the FPGA’s DCMs. These PVs are read-only and periodically poll the hardware:

- EventClock:EvDCM:Locked
- EventClock:EvDCM:PSDec
- EventClock:EvDCM:PSDone
- EventClock:EvDCM:PSInc
- EventClock:EvDCM:RESFlag
- EventClock:EvDCM:SRESFlag
- EventClock:EvDCM:SRUNFlag
- EventClock:EvDCM:Stopped
- EventClock:RecDCM:InitDone
- EventClock:RecDCM:PSDec
- EventClock:RecDCM:PSDone
- EventClock:RecDCM:PSInc
- EventClock:RecDCM:RESFlag
- EventClock:RecDCM:RUNFlag


## General status and settings

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Cmd:Reset</td>
<td>Writing to this record results in the FPGA being reset.</td>
</tr>
<tr>
<td>Enabled</td>
<td>EVG master enable. 1 if the EVG shall be enabled, 0 if it shall be disabled. This can be used to ensure that the EVG only starts operating after being configured completely.</td>
</tr>
<tr>
<td>Firmware:FirmwareID</td>
<td>Firmware ID (read-only). The firmware ID as a number.</td>
</tr>
<tr>
<td>Firmware:FormFactor</td>
<td>Form factor of the device (read-only). The form factor is provided as an enum, so that it contains a human-readable string identifying the hardware platform.</td>
</tr>
<tr>
<td>Firmware:SubreleaseID</td>
<td>Firmware subrelease ID (read-only). The firmware subrelease ID as a number.</td>
</tr>
<tr>
<td>Firmware:Version</td>
<td>Firmware version (read-only). The firmware revision as a number.</td>
</tr>
<tr>
<td>Firmware:Version:Complete</td>
<td>Complete firmware version (read-only). The complete content of the firmware version register as a number. This does not just include the actual version number, but also includes the form factor and potentially other information.</td>
</tr>
<tr>
<td>RX:Disabled</td>
<td>SFP receiver disable. If 1, the SFP receiver is disabled and no data is received from an upstream EVG or EVR. If 0, the SFP receiver is not disabled.</td>
</tr>
<tr>
<td>RX:PowerDown</td>
<td>SFP receiver power down. If 1, the SFP receiver is power down (also disabling it). If 0, the SFP receiver is powered up.</td>
</tr>
<tr>
<td>RX:ResetEventFIFO</td>
<td>Writing to this record resets the event FIFO.</td>
</tr>
<tr>
<td>WriteAll</td>
<td>Write to this record to force all settings to the hardware. This is useful after a device has been power-cycled (or the FPGA has been reset) in order to ensure that all settings in the EPICS records are actually applied to the hardware.</td>
</tr>
</table>


## Inputs

Typically, there are three types of inputs: Front-panel inputs, front-panel
universal inputs, and transition-board universal inputs. Which types of inputs
are available and the number of inputs depends on the exact hardware model.
Front-panel inputs use the PV prefix `FPIn#`, where # is the number of the input
(starting at zero). Front-panel universal inputs use the PV prefix `UnivIn#`,
where # is the number of the input (starting at zero). Transition-board
universal inputs use the PV prefix `TBIn#`, where # is the number of the input
(starting at zero).

In the following table, all PVs are listed with the generic PV prefix `In`,
which has to be replaced with the input-specific prefix.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>In:Description</td>
<td>Description for the input. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific input.</td>
</tr>
<tr>
<td>In:Installed</td>
<td>Installed flag (read only). This flag is only available for universal inputs (both front-panel and transition board). The flag is 1 if the respective universal input module is installed and 0 if it is not installed. Please note that this information is not read from the hardware, but specified in the IOC startup configuration.</td>
</tr>
<tr>
<td>In:MapTo:DBusB$</td>
<td>Distributed bus mapping flag. If enabled (1), a signal on the respective input is mapped to distributed bus bit $. If disabled (0) no such mapping happens.</td>
</tr>
<tr>
<td>In:MapTo:EvTrig$</td>
<td>Event trigger mapping flag. If enabled (1), a trigger on the respective input triggers event trigger $. If disabled (0) no such mapping happens.</td>
</tr>
<tr>
<td>In:MapTo:ExtIRQ</td>
<td>IRQ mapping flag. If enabled (1), a trigger on the respective input causes an “external” interrupt. If disabled (0) no interrupt is generated.</td>
</tr>
<tr>
<td>In:MapTo:SeqRAMTrig$</td>
<td>SequenceRAM trigger flag. If enabled (1), a trigger on the respective input causes the trigger of sequence RAM trigger $. If disabled (0) no such trigger is generated.</td>
</tr>
</table>


## Interrupts

The details of the interrupt handling depend on the hardware type. For hardware
that is accessed using the Linux device driver, interrupts are supported
directly and should thus usually occur with minimal delay. When accessing the
hardware over the UDP/IP protocol, on the other hand, the interrupt status has
to be polled and thus there can be significant delays.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>IRQ:DataBuffer:Enabled</td>
<td>Data buffer interrupt enabled flag. If 1, the completion of a data buffer transmission shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:DataBuffer:Status</td>
<td>Data buffer interrupt status flag. If the data buffer interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:DataBuffer:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:Enabled</td>
<td>Interrupt master enable flag. If 1, interrupts are enabled. If 0, all interrupts are disabled the other interrupt enable flags do not have any effect.</td>
</tr>
<tr>
<td>IRQ:EventFIFOFull:Enabled</td>
<td>Event FIFO full interrupt enabled flag. If 1, the completion of a data buffer transmission shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:EventFIFOFull:Status</td>
<td>Event FIFO full status flag. If the event FIFO full interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:EventFIFOFull:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:External:Enabled</td>
<td>“External” interrupt enabled flag. If 1, the a trigger on an input that is mapped to the external interrupt shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:External:Status</td>
<td>“External” interrupt status flag. If the external interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:External:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:RXViolation:Enabled</td>
<td>Receiver violation interrupt enabled flag. If 1, a receiver violation shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:RXViolation:Status</td>
<td>Receiver violation status flag. If the receiver violation interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:RXViolation:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Start:Enabled</td>
<td>Sequence RAM 0 started interrupt enabled flag. If 1, sequence RAM 0 starting shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Start:Status</td>
<td>Sequence RAM 0 started status flag. If the sequence RAM 0 started interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Start:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Stop:Enabled</td>
<td>Sequence RAM 0 stopped interrupt enabled flag. If 1, sequence RAM 0 stopping shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Stop:Status</td>
<td>Sequence RAM 0 stopped status flag. If the sequence RAM 0 stopped interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:SeqRAM0Stop:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Start:Enabled</td>
<td>Sequence RAM 1 started interrupt enabled flag. If 1, sequence RAM 1 starting shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Start:Status</td>
<td>Sequence RAM 1 started status flag. If the sequence RAM 1 started interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Start:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Stop:Enabled</td>
<td>Sequence RAM 1 stopped interrupt enabled flag. If 1, sequence RAM 1 stopping shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Stop:Status</td>
<td>Sequence RAM 1 stopped status flag. If the sequence RAM 1 stopped interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:SeqRAM1Stop:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:Status</td>
<td>Combined interrupt status. This process variable represents a bitfield where each interrupt source is represented by a bit. This process variable is updated every time the interrupt status register is read, so it only has those bits set that occurred recently. When a device is accessed using the UDP/IP protocol, the contents of this process variable are updated periodically. When a device is accessed using the Linux device driver, it is only updated when an interrupt occurs. Only those interrupts which are actually enabled can have their bits set to one. Due to the volatile nature of this process variable, the process variables representing individual interrupt flags should be preferred.</td>
</tr>
<tr>
<td>IRQ:Status:Scan</td>
<td>Triggers periodical polling of the interrupt status register. This process variable only exists when using the UDP/IP protocol. Its SCAN field can be changed in order to change the rate at which the interrupt status register is polled.</td>
</tr>
<tr>
<td>IRQ:Status:Update</td>
<td>Writing to this record triggers a single poll of the interrupt status register. This process variable only exists when using the UDP/IP protocol.</td>
</tr>
</table>


## Multiplexed counters

Typically, there are eight mutiplexed counters and each counter is identified by
a number in the range from 0 to 7.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>MXC#:Description</td>
<td>Description for the multiplexed counter. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific multiplexed counter.</td>
</tr>
<tr>
<td>MXC#:MapTo:EvTrig$</td>
<td>Mutiplexed counter to event trigger mapping. If enabled (1), mutiplexed counter # triggers event trigger $. If disabled (0), there is no such mapping.</td>
</tr>
<tr>
<td>MXC#:Prescaler</td>
<td>Mutiplexed counter prescaler. Defines the frequency generated by mutiplexed counter #. The frequency of the counter is the frequency of the event clock divided by the prescaler. For example, with a event clock rate of 125 MHz, a prescaler of 2 results in a counter frequency of 62.5 MHz.</td>
</tr>
<tr>
<td>MXC#:ResetState</td>
<td>Mutiplexed counter reset state. If 0, mutiplexed counter # starts in the low state after being reset. If 1, it starts in the high state.</td>
</tr>
<tr>
<td>MXC#:Status</td>
<td>Mutiplexed counter state (read-only). This process variable represents the current state (0 for low and 1 for high) of mutiplexed counter #. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>MXC:Reset</td>
<td>Writing to this record resets all multiplexed counters. This is usually done after configuring all mutiplexed counters in order to ensure that they have a fixed phase w.r.t. to each other.</td>
</tr>
</table>


## Outputs

Typically, there are two types of outputs: Front-panel outputs and front-panel
universal outputs. Which types of outputs are available and the number of
outputs depends on the exact hardware model. Front-panel outputs use the PV
prefix `FPOut#`, where # is the number of the output (starting at zero).
Front-panel universal outputs use the PV prefix `UnivOut#`, where # is the
number of the output (starting at zero).

In the following table, all PVs are listed with the generic PV prefix `Out`,
which has to be replaced with the output-specific prefix.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Out:Description</td>
<td>Description for the output. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific output.</td>
</tr>
<tr>
<td>Out:Installed</td>
<td>Installed flag (read only). This flag is only available for universal outputs. The flag is 1 if the respective universal output module is installed and 0 if it is not installed. Please note that this information is not read from the hardware, but specified in the IOC startup configuration.</td>
</tr>
<tr>
<td>Out:Map</td>
<td>Mapping of the output. A value of 0 to 7 corresponds to the respective distributed bus bit. A value of 8 sets the output constant high and a value of 9 sets it constant low.</td>
</tr>
</table>


## SFP module

The following PVs provide diagnostics information about the SFP module that is
installed inside the EVG. Please note that some of the information is optional
and not all SFP modules might provide it. In that case, the respective PVs will
not have valid values (but they won’t report that through an INVALID alarm). In
case of doubt, refer to the documentation of the SFP module in order to find out
which information is provided.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>SFP:NominalBitRate</td>
<td>Nominal bit rate reported by the SFP module (in Gbps).</td>
</tr>
<tr>
<td>SFP:RXPower</td>
<td>Real-time receive power reported by the SFP module (in µW). This PV also provides the alarm state signaled by the SFP module.</td>
</tr>
<tr>
<td>SFP:ScanDiag</td>
<td>Triggers periodic polling of the SFP registers that provide diagnostics data (analog values, corresponding alarm flags, and the status register). Its <code>SCAN</code> field can be changed in order to change the rate at which these registers are polled.</td>
</tr>
<tr>
<td>SFP:ScanInfo</td>
<td>Triggers periodic polling of the SFP registers that provide static information about the SFP module (alarm limits for analog values, vendor information, and nominal bit rate). Its <code>SCAN</code> field can be changed in order to change the rate at which these registers are polled. By default, these registers are only read once at IOC startup.</td>
</tr>
<tr>
<td>SFP:Status:DataNotReady</td>
<td>Indicates that the SFP module reports that the analog monitoring data is not yet available (valid).</td>
</tr>
<tr>
<td>SFP:Status:RXLossOfSignal</td>
<td>State of the RX loss-of-signal pin as reported by the SFP module.</td>
</tr>
<tr>
<td>SFP:Status:TXDisabled</td>
<td>State of the TX disable pin as reported by the SFP module.</td>
</tr>
<tr>
<td>SFP:Status:TXFault</td>
<td>State of the TX fault pin as reported by the SFP module.</td>
</tr>
<tr>
<td>SFP:Temperature</td>
<td>Real-time temperature reported by the SFP module (in °C). This PV also provides the alarm state signaled by the SFP module.</td>
</tr>
<tr>
<td>SFP:TXBiasCurrent</td>
<td>Real-time transmit laser-diode bias current reported by the SFP module (in mA). This PV also provides the alarm state signaled by the SFP module.</td>
</tr>
<tr>
<td>SFP:TXPower</td>
<td>Real-time transmit power reported by the SFP module (in µW). This PV also provides the alarm state signaled by the SFP module.</td>
</tr>
<tr>
<td>SFP:VCC</td>
<td>Real-time power-supply voltage reported by the SFP module (in V). This PV also provides the alarm state signaled by the SFP module.</td>
</tr>
<tr>
<td>SFP:Vendor:DateCode</td>
<td>Vendor-provided manufacturing date code reported by the SFP module (as an ASCII string).</td>
</tr>
<tr>
<td>SFP:Vendor:Id</td>
<td>IEEE company ID of the SFP module’s vendor. This is a 24-bit unsigned integer that is usually represented as a hexadecimal number.</td>
</tr>
<tr>
<td>SFP:Vendor:Name</td>
<td>Vendor name reported by the SFP module.</td>
</tr>
<tr>
<td>SFP:Vendor:PartNumber</td>
<td>Vendor-provided part number reported by the SFP module (as an ASCII string).</td>
</tr>
<tr>
<td>SFP:Vendor:PartNumberRevision</td>
<td>Vendor-provided revision of the part number reported by the SFP module (as an ASCII string).</td>
</tr>
<tr>
<td>SFP:Vendor:SerialNumber</td>
<td>Vendor-provided serial number reported by the SFP module (as an ASCII string).</td>
</tr>
</table>
