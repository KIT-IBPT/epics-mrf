List of process variables
=========================

When using the database files that are provided as part of the device support,
process variables for controlling and monitoring nearly every aspect of the EVG
and EVR are provided.

The exact list of process variables depends on the device type (different form
factors or hardware revisions have different features), but most of the process
variables described here should be available for the majority of the devices.

The process variables listed here are specified without any prefix, so the
device specific prefix (the combination of the `P` and `R` macros
[specified in the IOC's startup file](using.md)) has to be added to the names
specified here.


### Table of contents

- [EVG](#evg)
- [EVR](#evr)


EVG
---

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


### AC synchronization

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


### Data buffer

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


### Distributed bus

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


### Event analyzer

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


### Event sequence RAM

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


### Event triggers

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


### Other event settings

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


### Event clock

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


### General status and settings

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
<td>Firmware:FormFactor</td>
<td>Form factor of the device (read-only). The form factor is provided as an enum, so that it contains a human-readable string identifying the hardware platform.</td>
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


### Inputs

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


### Interrupts

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


### Multiplexed counters

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


### Outputs

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


### SFP module

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


EVR
---

- [Data buffer](#data-buffer-1)
- [Delay compensation](#delay-compensation)
- [Distributed bus](#distributed-bus-1)
- [Event FIFO](#event-fifo)
- [Event log](#event-log)
- [Event mapping RAM](#event-mapping-ram)
- [Event clock](#event-clock-1)
- [General status and settings](#general-status-and-settings-1)
- [Inputs](#inputs-1)
- [Interrupts](#interrupts-1)
- [Outputs](#outputs-1)
- [Pulse generators](#pulse-generators)
- [Prescalers](#prescalers)
- [Time stamps](#time-stamps)
- [SFP module](#sfp-module-1)


### Data buffer

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DataBuffer:RX:ChecksumError</td>
<td>Reception checksum error flag (read-only). 1 if there was a checksum error while receiving data, 0 if the data seems to be okay. This record is not updated automatically. Process it (write 0 to the PROC field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:RX:Complete</td>
<td>Reception complete flag (read-only). 1 if the last reception of the data buffer has finished, 0 if it is still running. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:RX:Disable</td>
<td>Write to this record to disable data buffer reception.</td>
</tr>
<tr>
<td>DataBuffer:RX:Enable</td>
<td>Write to this record to enable data buffer reception.</td>
</tr>
<tr>
<td>DataBuffer:RX:ReceivedData</td>
<td>Data received. The data is represented as an array of 32-bit words.</td>
</tr>
<tr>
<td>DataBuffer:RX:ReceivedSize</td>
<td>Size of the data that has been received (in bytes, read-only). Only complete words (32-bit) of data can be received, so this number is always a multiple of 4.</td>
</tr>
<tr>
<td>DataBuffer:RX:Running</td>
<td>Reception running flag (read-only). 1 if a data transmission is current running, 0 if no transmission is running. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:TX:Complete</td>
<td>Transmission complete flag (read-only). 1 if the last transmission of the data buffer has finished, 0 if it is still running. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:TX:Enabled</td>
<td>Transmissions enabled flag. 1 if data buffer transmissions shall be enabled, 0 if they shall be disabled. Data buffer transmissions will only work, if the distributed bus is configured to share its bandwidth with data transmissions.</td>
</tr>
<tr>
<td>DataBuffer:TX:Running</td>
<td>Transmission running flag (read-only). 1 if a data transmission is current running, 0 if no transmission is running. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>DataBuffer:TX:TransferSize</td>
<td>Size of the data that shall be transmitted (in bytes) only complete words (32-bit) of data can be transferred, so this number has to be a multiple of 4.</td>
</tr>
<tr>
<td>DataBuffer:TX:Transmit</td>
<td>Write to this record to start transmission.</td>
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


### Delay compensation

Delay compensation is only available in modern versions of the EVR when used in
combination with a recent firmware version that supports it. For this reason,
the process variables related to delay compensation are not available for the
VME-EVR-230 and VME-EVR-230RF.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DelayComp:Enabled</td>
<td>Delay compensation enabled flag. If enabled (1), the delay compensation is active. This will only work correctly when the EVR is connected to an upstream EVM. If disabled (0), the path delay is discarded and
the event FIFO is adjusted so that the internal delay matches the target delay. The latter mode must be selected when using an EVR in a legacy environment without delay compensation.</td>
</tr>
<tr>
<td>DelayComp:InternalDelay</td>
<td>Delay compensation internal delay (read-only). This is the delay caused by the event FIFO in the EVR. It is adjusted by the delay compensation logic in order to match the target delay.</td>
</tr>
<tr>
<td>DelayComp:Locked</td>
<td>Delay compensation event FIFO locked flag (read-only). This is 1 if the event FIFO has been locked to the specified delay.</td>
</tr>
<tr>
<td>DelayComp:PathDelay</td>
<td>Delay compensation path delay (read-only). This is the delay from the master EVM to the EVR as is has been received from the EVM. It is used for adjusting the internal delay in order to match the target delay.</td>
</tr>
<tr>
<td>DelayComp:PathDelay:Valid</td>
<td>Delay compensation path-delay status information (read-only). This is 0 if no valid path delay is available, 1 if the path delay is valid and has coarse precision (quick acquisition), 2 if the path delay is valid has medium prevision (slow acquisition), and 3 if the path delay is valid and has fine precision (slow acquisition).</td>
</tr>
<tr>
<td>DelayComp:TargetDelay</td>
<td>Delay compensation target delay. The delay compensation logic in the EVR will try to adjust the internal delay so that the total delay (path delay plus internal delay) matches the specified delay.</td>
</tr>
<tr>
<td>DelayComp:TargetDelay:TooLong</td>
<td>Delay compensation target delay too-long flag (read-only). This is 1 if the specified target delay is too long (the path delay is so short that the internal delay cannot be adjusted to match the target delay). Otherwise, this is 0.</td>
</tr>
<tr>
<td>DelayComp:TargetDelay:TooShort</td>
<td>Delay compensation target delay too-short flag (read-only). This is 1 if the specified target delay is too short (the path delay is so long that the internal delay cannot be adjusted to match the target delay, e.g. because the internal would have to be negative in order to match the target delay). Otherwise, this is 0.</td>
</tr>
</table>

### Distributed bus

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DBus:BX:RX:Status</td>
<td>Status of the distributed bus bit X received from an upstream EVG or EVR (read-only). This information is only updated when DBus:Status is processed.</td>
</tr>
<tr>
<td>DBus:SharedRX</td>
<td>Bandwidth share flag for the receiver side. If enabled (1), the upstream distributed bus bandwidth is halfed in order to be able to receive data into the data buffer. If disabled (0), the full distributed bus bandwidth is available and data reception is disabled. Please note that for firmware versions that support delay compensation, this is always enabled and disabling it is not possible (the write operation will fail).</td>
</tr>
<tr>
<td>DBus:SharedTX</td>
<td>Bandwidth share flag for the transmission side. If enabled (1), the downstream distributed bus bandwidth is halfed in order to be able to send data from the data buffer. If disabled (0), the full distributed bus bandwidth is available and data transmissions are disabled. Please note that for firmware versions that support delay compensation, this is always enabled and disabling it is not possible (the write operation will fail).</td>
</tr>
<tr>
<td>DBus:Status</td>
<td>Combined RX/TX status of all distributed bus bits as a bitfield (read-only). Process this record (write 0 to its <code>PROC</code> field) in order to update it by reading from the hardware. This also updates the RX/TX status records.</td>
</tr>
</table>


### Event FIFO

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:FIFO:EventCode</td>
<td>Event code from event FIFO (read-only). Process this record (write 0 to its <code>PROC</code> field) in order to update it by reading from the hardware. This also updates the seconds and time-stamp record.</td>
</tr>
<tr>
<td>Event:FIFO:Seconds</td>
<td>Seconds counter from event FIFO (read-only).</td>
</tr>
<tr>
<td>Event:FIFO:TimeStamp</td>
<td>Time-stamp from event FIFO (read-only).</td>
</tr>
</table>


### Event log

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Event:Log:Disable</td>
<td>Write to this record to disable the event log.</td>
</tr>
<tr>
<td>Event:Log:Enable</td>
<td>Write to this record to enable the event log.</td>
</tr>
<tr>
<td>Event:Log:EventCodes</td>
<td>Event codes from the event log (read-only). This is an array containing the whole event log. This record has to be updated manually by writing to <code>Event:Log:Update</code>.</td>
</tr>
<tr>
<td>Event:Log:EventCounters</td>
<td>Event counters from the event log (read-only). This is an array containing the whole event log. This record has to be updated manually by writing to <code>Event:Log:Update</code>.</td>
</tr>
<tr>
<td>Event:Log:Offset</td>
<td>Offset into the event log. The offset specified the first entry in the event log that shall be used. This record has to be updated manually by writing to <code>Event:Log:Update</code>.</td>
</tr>
<tr>
<td>Event:Log:Reset</td>
<td>Write to this record to reset the event log.</td>
</tr>
<tr>
<td>Event:Log:Seconds</td>
<td>Seconds entries from the event log (read-only). This is an array containing the whole event log. This record has to be updated manually by writing to <code>Event:Log:Update</code>.</td>
</tr>
<tr>
<td>Event:Log:Size</td>
<td>Number of entries in the event log. The size specifies the number of entries (starting from the offset) in the event log that shall be used. This record has to be updated manually by writing to <code>Event:Log:Update</code>.</td>
</tr>
<tr>
<td>Event:Log:StopEventEnabled</td>
<td>Stop event enabled flag. If enabled (1), a stop event will cause the event log to be stopped. If disabled (0), such an event is ignored.</td>
</tr>
<tr>
<td>Event:Log:Update</td>
<td>Write to this record to cause an update of the event-log related records.</td>
</tr>
</table>


### Event mapping RAM

There are two mapping RAMs (mapping RAM 0 and 1). The following list uses the
generic prefix `MapRAM#` for all records that are specific to a certain mapping
RAM. Replace this prefix with `MapRAM0` or `MapRAM1` in order to get the actual
PV name.

Please note that the the last two PVs in the list apply to both mapping RAMs and
thus do not use a mapping RAM specific prefix.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>MapRAM#:InternalFunctions</td>
<td>Mapping of event codes to internal functions. This is an array containing one element for each event code. Each element is a bitset where each bit represents an internal function that can be triggered by an event. If the respective bit is set, the corresponding internal function is triggered when the event code is received. When modifying this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:InternalFunctions:WriteAll</td>
<td>Writing to this record causes all entries in the <code>MapRAM#:InternalFunctions</code> array to be sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:ResetPulseGens</td>
<td>Mapping of event codes to pulse generators that shall be reset. This is an array containing one element for each event code. Each element is a bitset where each bit represents a pulse generator that can be reset by an event. If the respective bit is set, the corresponding pulse generator is reset when the event code is received. Such a mapping is only effective if the corresponding pulse generator allows the mapping of reset events. When modifying this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:ResetPulseGens:WriteAll</td>
<td>Writing to this record causes all entries in the <code>MapRAM#:ResetPulseGens</code> array to be sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:SetPulseGens</td>
<td>Mapping of event codes to pulse generators that shall be set. This is an array containing one element for each event code. Each element is a bitset where each bit represents a pulse generator that can be set by an event. If the respective bit is set, the corresponding pulse generator is set when the event code is received. Such a mapping is only effective if the corresponding pulse generator allows the mapping of set events. When modifying this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:SetPulseGens:WriteAll</td>
<td>Writing to this record causes all entries in the <code>MapRAM#:SetPulseGens</code> array to be sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:TrigPulseGens</td>
<td>Mapping of event codes to pulse generators that shall be triggered. This is an array containing one element for each event code. Each element is a bitset where each bit represents a pulse generator that can be triggered by an event. If the respective bit is set, the corresponding pulse generator is triggered when the event code is received. Such a mapping is only effective if the corresponding pulse generator allows the mapping of trigger events. When modifying this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:TrigPulseGens:WriteAll</td>
<td>Writing to this record causes all entries in the <code>MapRAM#:TrigPulseGens</code> array to be sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM#:WriteAll</td>
<td>Writing to this record causes all enties in the <code>MapRAM#:InternalFunctions</code>, <code>MapRAM#:ResetPulseGens</code>, <code>MapRAM#:SetPulseGens</code>, and <code>MapRAM#:TrigPulseGens</code> arrays to be sent to the hardware.</td>
</tr>
<tr>
<td>MapRAM:Enabled</td>
<td>Mapping RAM enabled flag. If enabled (1), the mapping RAM selected through the <code>MapRAM:Select</code> PV is used for triggering events for received event codes. If disabled (0), received event codes do not trigger actions specified in either of the mapping RAMs.</td>
</tr>
<tr>
<td>MapRAM:Select</td>
<td>Mapping RAM selection. If 0, mapping RAM 0 is used. If 1, mapping RAM 1 is used.</td>
</tr>
</table>


### Event clock

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>EventClock:Freq</td>
<td>Frequency of the event clock. This frequency only has to fit roughly because it is only used for estimating the length of a microsecond in event clock cycles. For example, this information is used by the heartbeat watchdog.</td>
</tr>
<tr>
<td>EventClock:Gen:Freq</td>
<td>Frequency generated by the internal frequency generator (Micrel SY87739L). The frequency is not specified directly. Instead, one has to choose from a list of preconfigured frequencies. The event clock generated by the internal frequency generator must closely match the frequency of the event clock of the upstream EVG, otherwise the SFP received is not able to synchronize on the incoming signal.</td>
</tr>
<tr>
<td>EventClock:Gen:Locked</td>
<td>PLL status of the internal frequency generator (read-only). 1 if the PLL has locked, 0 if it is unlocked. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
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


### General status and settings

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
<td>EVR master enable. 1 if the EVR shall be enabled, 0 if it shall be disabled. This can be used to ensure that the EVR only starts operating after being configured completely.</td>
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
<td>Firmware:RevisionID</td>
<td>Firmware revision ID (read-only). The firmware revision ID as a number.</td>
</tr>
<tr>
<td>Firmware:SubreleaseID</td>
<td>Firmware subrelease ID (read-only). The firmware subrelease ID as a number.</td>
</tr>
<tr>
<td>Firmware:Version</td>
<td>Firmware revision ID (read-only). The firmware revision ID as a number. This is an alias for <code>Firmware:RevisionID</code> and is only provided for backwards compatibility with older versions of this device support. It is going to be removed in version 2.x of this device support.</td>
</tr>
<tr>
<td>Firmware:Version:Complete</td>
<td>Complete firmware version (read-only). The complete content of the firmware version register as a number. This includes the form factor, subrelease ID, firmware ID, and release ID.</td>
</tr>
<tr>
<td>RX:EventFIFOStopped</td>
<td>Event FIFO stopped flag (read-only). If 0, the event FIFO is running. If 1, it has been stopped. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>RX:Loopback</td>
<td>Receiver loopback flag. If 0, the receiver operates normally (receives events from an upstream EVG or EVR). If 1, the data transmitted to a downstream EVG or EVR is looped back into the receiver.</td>
</tr>
<tr>
<td>RX:ResetEventFIFO</td>
<td>Writing to this record resets the event FIFO.</td>
</tr>
<tr>
<td>SFP:Link</td>
<td>SFP link status (read-only). If 0, the link is down. If 1, the link is up. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>SFP:Missing</td>
<td>SFP missing flag (read-only). If 0, the SFP module is installed. If 1, the SFP module is missing. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>TX:Loopback</td>
<td>Transmitter loopback flag. If 0, the transmitter operates normally (sends events and distributed bus bits possibly modified by this EVR). If 1, the signal received from an upstream EVG or EVR is directly looped back into the transmitter, thus transmitting the signal to a downstream EVG or EVR without any modifications.</td>
</tr>
<tr>
<td>WriteAll</td>
<td>Write to this record to force all settings to the hardware. This is useful after a device has been power-cycled (or the FPGA has been reset) in order to ensure that all settings in the EPICS records are actually applied to the hardware.</td>
</tr>
</table>


### Inputs

In the following table, all PVs are listed without the generic PV prefix `In`,
which has to be replaced with an input-specific prefix (e.g. `FPIn0` for
front-panel input 0, `FPIn1` for front-panel input 1, etc.). The number of
available inputs depends on the form-factor and hardware revision.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>In:BackwardEvent:Code</td>
<td>Backward event code. This event code is send over the SFP link (to a downstream EVG or EVR) when one of the trigger conditions (edge or level) is enabled and applies.</td>
</tr>
<tr>
<td>In:BackwardEvent:EdgeEnabled</td>
<td>Backward event edge enabled flag. If enabled (1), the backward event code is sent when an edge matching the specified edge sensitivity is detected on the input. If disabled (0), edges on the input do not trigger a backward event.</td>
</tr>
<tr>
<td>In:BackwardEvent:LevelEnabled</td>
<td>Back event level enabled flag. If enabled (1), the backward event code is sent when the level of the input matches the level sensitivity. If disabled (0), the level of the input does not have an influence on whether a backward event is sent.</td>
</tr>
<tr>
<td>In:Description</td>
<td>Description for the input. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific input.</td>
</tr>
<tr>
<td>In:EdgeSensitivity</td>
<td>Input edge sensitivity. If 0, the input is sensitive to a rising edge. If 1, the input is sensitive to a falling edge.</td>
</tr>
<tr>
<td>In:ExternalEvent:Code</td>
<td>External event code. This event code is resolved using the active mapping RAM when one of the trigger conditions (edge or level) is enabled and applies.</td>
</tr>
<tr>
<td>In:ExternalEvent:EdgeEnabled</td>
<td>External event edge enabled flag. If enabled (1), the external event code is resolved using the active mapping RAM when an edge matching the specified edge sensitivity is detected on the input. If disabled (0), edges on the input do not trigger an external event.</td>
</tr>
<tr>
<td>In:ExternalEvent:LevelEnabled</td>
<td>External event level enabled flag. If enabled (1), the external event code is resolved using the active mapping RAM when the level of the input matches the level sensitivity. If disabled (0), the level of the input does not have an influence on whether an external event is triggered.</td>
</tr>
<tr>
<td>In:LevelSensitivity</td>
<td>Input level sensitivity. If 0, the input is sensitive to a high level. If 1, the input is sensitive to a low level.</td>
</tr>
<tr>
<td>In:MapTo:DBusB#</td>
<td>Input to distributed bus bit mapping. If enabled (1), the input is mapped to distributed bus bit #. If disabled (0), the distributed bus bit is not influenced by signals on the input.</td>
</tr>
</table>


### Interrupts

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
<td>Data buffer interrupt enabled flag. If 1, the completion of a data buffer transmission or reception shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
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
<td>IRQ:Event:Enabled</td>
<td>Event interrupt enabled flag. If 1, the reception of an event shall trigger an interrupt. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:Event:Status</td>
<td>Event interrupt status flag. If the event interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:Event:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
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
<td>IRQ:Hardware:Enabled</td>
<td>Hardware interrupt enabled flag. If 1, the source specified with IRQ:Hardware:Map triggers an interrupt. If 0, no interrupt is triggered by that source.</td>
</tr>
<tr>
<td>IRQ:Hardware:Map</td>
<td>Hardware interrupt mapping. A hardware interrupt is triggered when the signal source specified with this mapping changes from low to high. The allowed values for the mapping are the same ones that can be specified for outputs (see Out:Map).</td>
</tr>
<tr>
<td>IRQ:Hardware:Status</td>
<td>Hardware interrupt status flag. If the external interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:Hardware:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:Heartbeat:Enabled</td>
<td>Heartbeat interrupt enabled flag. If 1, an interrupt is triggered by the heartbeat watchdog when it detects that too much time (approx. 1.6 seconds) has passed since the last heartbeat event. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:Heartbeat:Status</td>
<td>Heartbeat interrupt status flag. If the heartbeat interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:Heartbeat:Status:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
<tr>
<td>IRQ:LinkStateChange:Enabled</td>
<td>Link state change interrupt enabled flag. If 1, an interrupt is triggered when the status of the SFP link changes. If 0, no interrupt is triggered by such an event.</td>
</tr>
<tr>
<td>IRQ:LinkStateChange:Status</td>
<td>Link state change interrupt status. If the link state change interrupt is enabled and a corresponding interrupt occurs, the status is set to 1.</td>
</tr>
<tr>
<td>IRQ:LinkStateChange:Status:Reset</td>
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
<td>IRQ:Status</td>
<td>Combined interrupt status. This process variable represents a bitfield where each interrupt source is represented by a bit. This process variable is updated every time the interrupt status register is read, so it only has those bits set that occurred recently. When a device is accessed using the UDP/IP protocol, the contents of this process variable are updated periodically. When a device is accessed using the Linux device driver, it is only updated when an interrupt occurs. Only those interrupts which are actually enabled can have their bits set to one. Due to the volatile nature of this process variable, the process variables representing individual interrupt flags should be preferred.</td>
</tr>
<tr>
<td>IRQ:Status:Scan</td>
<td>Triggers periodical polling of the interrupt status register. This process variable only exists when using the UDP/IP protocol. Its <code>SCAN</code> field can be changed in order to change the rate at which the interrupt status register is polled.</td>
</tr>
<tr>
<td>IRQ:Status:Update</td>
<td>Writing to this record triggers a single poll of the interrupt status register. This process variable only exists when using the UDP/IP protocol.</td>
</tr>
</table>


### Outputs

Typically, there are three types of outputs: Front-panel outputs, front-panel
universal outputs, and transition board outputs. Which types of outputs are
available and the number of outputs depends on the exact hardware model.

Front-panel outputs use the PV prefix `FPOut#`, where # is the number of the
output (starting at zero). Front-panel universal outputs use the PV prefix
`UnivOut#`, where # is the number of the output (starting at zero).
Transition-board universal outputs use the PV prefix `TBOut#`, where # is the
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
<td>Mapping used for the output. A value of 0 to 31 specifies that the corresponding pulse generator shall be used. A value of 32 to 39 specifies that one of the distributed bus bits (32 meaning distributed bus bit 0 and 39 meaning distributed bus bit 7) shall be used. A value of 40 to 42 specifies that one of the prescalers (40 meaning prescaler 0 and 42 meaning prescaler 2) shall be used. A value of 62 specified that the output shall be set to constant hight. A value of 63 specifies that the output shall be set to constant low.</td>
</tr>
</table>

For CML outputs, there is a number of additional PVs that configure the CML
operation:

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Out:Enabled</td>
<td>Output enable flag. If 1, the output is enabled. If 0, the output is disabled.</td>
</tr>
<tr>
<td>Out:FreqMode:HighPeriod</td>
<td>High period for the frequency mode. This specifies the number of CML clock cycles (event clock mutiplied by twenty) for which the output is pulled high. Must be between 20 and 65535.</td>
</tr>
<tr>
<td>Out:FreqMode:LowPeriod</td>
<td>Low period for the frequency mode. This specifies the number of CML clock cycles (event clock mutiplied by twenty) for which the output is pulled low. Must be between 20 and 65535.</td>
</tr>
<tr>
<td>Out:FreqMode:TrigLevel</td>
<td>Output level when output is triggered. If 0, the output starts in the low state when triggered and later switches to the high state. If 1, the output starts in the high state when triggered and later switches to the low state.</td>
</tr>
<tr>
<td>Out:FreqMode:TrigPosition</td>
<td>Offset period when output is triggered (in CML clock cycles). The counter measuring the time the output has been in a certain state is initialized to this value when the output is triggered. This effectively changes the phase of the generated signal, because the switch to the oppositve output state will happen earlier when this value is not zero.</td>
</tr>
<tr>
<td>Out:Mode</td>
<td>Output mode select. 0 for pulse mode, 1 for frequency mode, and 2 for pattern mode.</td>
</tr>
<tr>
<td>Out:PatternMode:NumberOfSamples</td>
<td>Number of samples that shall be used in pattern mode. Only the specified number of entries (max. 2048) from the <code>Out:PatternMode:Samples</code> array are used for generating the output signal.</td>
</tr>
<tr>
<td>Out:PatternMode:Recycle</td>
<td>Pattern recycle flag. If enabled (1), the output again starts with the first sample when the specified number of samples has been used. If disabled (0), the output stays in the state specified by the last sample until the output is triggered again.</td>
</tr>
<tr>
<td>Out:PatternMode:Samples</td>
<td>Samples for the pattern mode. This is an array of 2048 elements, each element representing a bitfield. Only the lower 20 bits are used, the upper 12 bits are unused. Each of the 20 bits represents one sample (1 for high and 0 for low) that is used by the CML output. The CML output uses the bits at a rate of 20 times the event clock, thus resulting in one sample (up to 20 different states) being output with each event clock cycle. When modifying this array, only changed elements are actually sent to the hardware.</td>
</tr>
<tr>
<td>Out:PatternMode:Samples:WriteAll</td>
<td>Writing to this record causes all elements of the <code>Out:PatternMode:Samples</code> array to be written to the hardware.</td>
</tr>
<tr>
<td>Out:PowerDown</td>
<td>Output power down flag. If 0, the output is powered up. If 1, the output is powered down (and thus disabled).</td>
</tr>
<tr>
<td>Out:PulseMode:Falling</td>
<td>Pattern used in pulse mode for the falling edge state. This is a bit-set where only the lower 20 bits are used. Those bits represent the 20 states that are used for the output at the CML clock rate (20 times the event clock rate) when a falling edge condition is detected. A falling edge condition is detected when the signal mapped to the output was high during the last event clock cycle and is low during the current event clock cycle.</td>
</tr>
<tr>
<td>Out:PulseMode:High</td>
<td>Pattern used in pulse mode for the high state. This is a bit-set where only the lower 20 bits are used. Those bits represent the 20 states that are used for the output at the CML clock rate (20 times the event clock rate) when a high condition is detected. A high condition is detected when the signal mapped to the output was high during the last event clock cycle and is also high during the current event clock cycle.</td>
</tr>
<tr>
<td>Out:PulseMode:Low</td>
<td>Pattern used in pulse mode for the lowd state. This is a bit-set where only the lower 20 bits are used. Those bits represent the 20 states that are used for the output at the CML clock rate (20 times the event clock rate) when a low condition is detected. A low condition is detected when the signal mapped to the output was low during the last event clock cycle and is also low during the current event clock cycle.</td>
</tr>
<tr>
<td>Out:PulseMode:Rising</td>
<td>Pattern used in pulse mode for the rising edge state. This is a bit-set where only the lower 20 bits are used. Those bits represent the 20 states that are used for the output at the CML clock rate (20 times the event clock rate) when a rising edge condition is detected. A rising edge condition is detected when the signal mapped to the output was low during the last event clock cycle and is high during the current event clock cycle.</td>
</tr>
<tr>
<td>Out:Reset</td>
<td>Output reset flag. If 1, the output is forced to be reset. If 0, the output shall operate normally.</td>
</tr>
</table>


### Pulse generators

The number of available pulse generators depends on the form-factor and hardware
revision. Even within one device, different pulse generators might have
different features (e.g. prescaler available / not available, allowed range of
the pulse delay and width). Please refer to the EVR manual for details regarding
the features of a specific device.

In the following table, the generic prefix `PulseGen#` is used for PVs that are
specific to a certain pulse generator. # has to be replaced by the number of the
pulse generator (starting at zero) in order to get the actual PV name.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>NumPulseGen</td>
<td>Number of pulse generators supported by the device.</td>
</tr>
<tr>
<td>PulseGen#:Delay</td>
<td>Pulse delay (in event clock cycles).</td>
</tr>
<tr>
<td>PulseGen#:Description</td>
<td>Description for the pulse generator.This PV has no equivalent register in the device. It is just intended to document the purpose of a specific pulse generator.</td>
</tr>
<tr>
<td>PulseGen#:Enabled</td>
<td>Enabled flag. If 0, the pulse generator is disabled. If 1, it is enabled.</td>
</tr>
<tr>
<td>PulseGen#:MapResetEnabled</td>
<td>Allow mapping of reset event flag. If enabled (1), the mapping RAM may cause the pulse generator to be reset. If disabled (0), such a mapping is ignored.</td>
</tr>
<tr>
<td>PulseGen#:MapSetEnabled</td>
<td>Allow mapping of set event flag. If enabled (1), the mapping RAM may cause the pulse generator to be set. If disabled (0), such a mapping is ignored.</td>
</tr>
<tr>
<td>PulseGen#:MapTrigEnabled</td>
<td>Allow mapping of trigger event flag. If enabled (1), the mapping RAM may cause the pulse generator to be triggered. If disabled (0), such a mapping is ignored.</td>
</tr>
<tr>
<td>PulseGen#:Polarity</td>
<td>Polarity. If 0, the pulse generator has normal polarity. This means that the outputs using the pulse generator are set to the same state as the pulse generator. If 1, the pulse generator has inverse polarity. This means that the outputs using the pulse generator use the inverse state of the pulse generator. A low state of the pulse generator causes a high state of the outputs and a high state of the pulse generator causes a low state of the outputs.</td>
</tr>
<tr>
<td>PulseGen#:Prescaler</td>
<td>Prescaler divider. This record is typically only available for a few of the pulse generators. The prescaler divider scales down the clock used for the delay and width counters. Instead of using the event clock, the specified fraction of the event clock is used. This effectively increases the range of the pulse delay and width.</td>
</tr>
<tr>
<td>PulseGen#:Reset</td>
<td>Writing to this record cause the pulse generator to be reset (put into the low state).</td>
</tr>
<tr>
<td>PulseGen#:Set</td>
<td>Writing to this record cause the pulse generator to be set (put into the low state).</td>
</tr>
<tr>
<td>PulseGen#:Status</td>
<td>Current state of the pulse generator (read-only). A value of 0 means low and a value of 1 means high. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>PulseGen#:Width</td>
<td>Pulse width (in event clock cycles). After being triggered, the pulse generator stays in the high state for the specified number of event clock cycles and then switches to the low state.</td>
</tr>
</table>


### Prescalers

The number of prescalers and their width may depend on the form factor and
hardware revision. Most devices have three prescalers, each having a width of at
least 16-bits.

In the following table, the generic prefix `Prescaler#` is used for PVs that are
specific to a certain prescaler. # has to be replaced by the number of the
prescaler (starting at zero) in order to get the actual PV name.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>Prescaler#</td>
<td>Divider used by prescaler #. The divider specifies the rate at which prescaler # runs. The rate is derived from the event clock rate. For example, at an event clock rate of 125 MHz, a divider of 5 would result in a prescaler that runs at 25 MHz.</td>
</tr>
<tr>
<td>Prescaler#:Description</td>
<td>Description for prescaler #. This PV has no equivalent register in the device. It is just intended to document the purpose of a specific prescaler.</td>
</tr>
</table>


### Time stamps

The time-stamp mechanism allows for a high-precision time-stamp being
distributed from an EVG to all EVRs. The details of this mechanism are tricky,
so it is strongly suggested that you refer to the EVG and EVR manuals for a
detailed description.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>TimeStamp:EventCounter</td>
<td>Time-stamp event counter (read-only). The event counter is incremented depending on the event counter prescaler and event counter source. If the event counter prescaler is non-zero, the event clock is divided by the prescaler factor and the resulting clock increments the event counter with each cycle. If the event counter prescaler is zero, the behavior depends on the event counter source. If the event counter source is set to distributed bus bit 4, the event counter is incremented with each rising edge of that bus bit. If the event counter source is set to event code 0x7C, the event counter is incremented each time that event (or another event mapped to the time-stamp clock event internal function) is received. The event counter is reset to zero when the reset time-stamp event (event code 0x7D) or another event mapped to the respective internal function is received. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>TimeStamp:EventCounterPrescaler</td>
<td>Event counter clock prescaler. If non-zero, the event counter is incremented with each n<sup>th</sup> even clock cycle where n is the number specified for the prescaler.</td>
</tr>
<tr>
<td>TimeStamp:EventCounterSource</td>
<td>Event counter source. This setting is only effective if the the event counter prescaler is set to zero. In this case, a setting of 0 specifies distributed bus bit as the trigger source for the event counter and a setting of 1 specifies event code 0x7C (or any other event mapped to the time-stamp clock event internal function) as the trigger source.</td>
</tr>
<tr>
<td>TimeStamp:EventLatch</td>
<td>Time-stamp event latch (read-only). The latch is updated with the current value of the time-stamp event counter (the current value in the hardware, not in EPICS) when an event that is mapped to the latch time-stamp internal function is received. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>TimeStamp:Latch</td>
<td>Writing to this record has the same effect as if an event mapped to the latch time-stamp internal function was received.</td>
</tr>
<tr>
<td>TimeStamp:Reset</td>
<td>Writing to this record has the same effect as if the reset time-stamp event (event code 0x7D) or another event mapped to the respective internal function was received.</td>
</tr>
<tr>
<td>TimeStamp:SecondsCounter</td>
<td>Time-stamp seconds counter (read-only). The time-stamp seconds counter contains the value that the time-stamp seconds shift register had when the last time-stamp reset event occurred. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>TimeStamp:SecondsLatch</td>
<td>Time-stamp seconds latch (read-only). The latch is updated with the current value of the time-stamp seconds counter (the current value in the hardware, not in EPICS) when an event that is mapped to the latch time-stamp internal function is received. This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
<tr>
<td>TimeStamp:SecondsShift</td>
<td>Time-stamp seconds shift register (read-only). The time-stamp seconds shift register is updated when a "seconds shift register 0" event (event code 0x70) or a "seconds shift register 1" event (event code 0x71) is received. The value of this register is the source used when updating the time-stamp seconds counter.  This record is not updated automatically. Process it (write 0 to the <code>PROC</code> field) in order to get the latest status.</td>
</tr>
</table>


### SFP module

The PVs with diagnostic information for the SFP module are exactly the same as
[the ones supported for the EVG](#sfp-module).
