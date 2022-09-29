Configuring the device support
==============================

In order to use the device support in an EPICS IOC, the IOC has to include one
of the following DBD files:

- `mrfMmap.dbd` (only when controlling a device over PCI(e))
- `mrfUdpIp.dbd` (only when controlling a device over Ethernet)

The IOC has to be linked against the following libraries:

- `mrfCommon`
- `mrfEpics`
- `mrfEpicsMmap` and `mrfMmap` (only when controlling a device over PCI(e))
- `mrfEpicsUdpIp` and `mrfUdpIp` (only when controlling a device over Ethernet)


### Table of contents
- [IOC startup configuration](#ioc-startup-configuration)
- [Autosave support](#autosave-support)
- [Interrupt handling](#interrupt-handling)
- [Clock generator configuration](#clock-generator-configuration)
- [GUI / OPI panels](#gui--opi-panels)


IOC startup configuration
-------------------------

The IOC startup script has to connect to the device(s) and load the
corresponding database files. Both parts depend on the type of the device.


### VME-EVG-230

```
mrfUdpIpEvgDevice("EVG01", "evg.example.com")

dbLoadRecords("${MRF}/db/vme-evg-230.db", "P=TEST:EVG:,R=01:,DEVICE=EVG01,UNIV_OUT_0_1_INSTALLED=0,UNIV_OUT_2_3_INSTALLED=0,UNIV_IN_0_1_INSTALLED=0,UNIV_IN_2_3_INSTALLED=0,TB_UNIV_IN_0_1_INSTALLED=0,TB_UNIV_IN_2_3_INSTALLED=0,TB_UNIV_IN_4_5_INSTALLED=0,TB_UNIV_IN_6_7_INSTALLED=0,TB_UNIV_IN_8_9_INSTALLED=0,TB_UNIV_IN_10_11_INSTALLED=0,TB_UNIV_IN_12_13_INSTALLED=0,TB_UNIV_IN_14_15_INSTALLED=0")
```

The following parameters have to be passed to `dbLoadRecords`:

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
</tr>
<tr>
<td><code>P</code></td>
<td>First part of the prefix for all EPICS PVs.</td>
</tr>
<tr>
<td><code>R</code></td>
<td>Second part of the prefix for all EPICS PVs (e.g. device number).</td>
</tr>
<tr>
<td><code>DEVICE</code></td>
<td>Name of the device (must be the name that is specified when calling <code>mrfUdpIpEvgDevice</code>).</td>
</tr>
</table>

The following parameters are optional. If they are not specified, the respective
default values are used.

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
<th>Default Value</th>
</tr>
<tr>
<td><code>CLK_GEN_CW_x</code></td>
<td>Configuration word for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
<tr>
<td><code>CLK_GEN_F_x</code></td>
<td>Frequency for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
<tr>
<td><code>UNIV_OUT_INSTALLED</code></td>
<td>Default value for <code>UNIV_OUT_x_y_INSTALLED</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>UNIV_OUT_x_y_INSTALLED</code></td>
<td><code>0</code> if the corresponding universal output module is not installed, <code>1</code> if it is installed.</td>
<td><code>$(UNIV_OUT_INSTALLED)</code></td>
</tr>
<tr>
<td><code>UNIV_IN_INSTALLED</code></td>
<td>Default value for <code>UNIV_IN_INSTALLED</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>UNIV_IN_x_y_INSTALLED</code></td>
<td><code>0</code> if the corresponding universal input module (front panel) is not installed, <code>1</code> if it is installed.</td>
<td><code>$(UNIV_IN_INSTALLED)</code></td>
</tr>
<tr>
<td><code>TB_UNIV_OUT_INSTALLED</code></td>
<td>Default value for <code>TB_UNIV_OUT_x_y_INSTALLED</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>TB_UNIV_IN_x_y_INSTALLED</code></td>
<td><code>0</code> if the corresponding universal input module (transition board) is not installed, <code>1</code> if it is installed.</td>
<td><code>$(TB_UNIV_IN_INSTALLED)</code></td>
</tr>
</table>


### VME-EVR-230 and VME-EVR-230RF

```
mrfUdpIpEvrDevice("EVR01", "evr.example.com")

dbLoadRecords("${MRF}/db/vme-evr-230rf.db","P=TEST:EVR:,R=01:,DEVICE=EVR01,UNIV_OUT_0_1_INSTALLED=0,UNIV_OUT_2_3_INSTALLED=0,UNIV_OUT_0_1_FD_AVAILABLE=0,UNIV_OUT_2_3_FD_AVAILABLE=0,TB_UNIV_OUT_0_1_INSTALLED=0,TB_UNIV_OUT_2_3_INSTALLED=0,TB_UNIV_OUT_4_5_INSTALLED=0,TB_UNIV_OUT_6_7_INSTALLED=0,TB_UNIV_OUT_8_9_INSTALLED=0,TB_UNIV_OUT_10_11_INSTALLED=0,TB_UNIV_OUT_12_13_INSTALLED=0,TB_UNIV_OUT_14_15_INSTALLED=0")
```

For a VME-EVR-230, the file `vme-evr-230.db` has to be loaded instead of
`vme-evr-230rf.db`.

The following parameters have to be passed to `dbLoadRecords`:

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
</tr>
<tr>
<td><code>P</code></td>
<td>First part of the prefix for all EPICS PVs.</td>
</tr>
<tr>
<td><code>R</code></td>
<td>Second part of the prefix for all EPICS PVs (e.g. device number).</td>
</tr>
<tr>
<td><code>DEVICE</code></td>
<td>Name of the device (must be the name that is specified when calling <code>mrfUdpIpEvrDevice</code>).</td>
</tr>
</table>

The following parameters are optional. If they are not specified, the respective
default values are used.

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
<th>Default Value</th>
</tr>
<tr>
<td><code>CLK_GEN_CW_x</code></td>
<td>Configuration word for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
<tr>
<td><code>CLK_GEN_F_x</code></td>
<td>Frequency for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
<tr>
<td><code>UNIV_OUT_INSTALLED</code></td>
<td>Default value for <code>UNIV_OUT_x_y_INSTALLED</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>UNIV_OUT_x_y_INSTALLED</code></td>
<td><code>0</code> if the corresponding universal output module (front panel) is not installed, <code>1</code> if it is installed.</td>
<td><code>$(UNIV_OUT_INSTALLED)</code></td>
</tr>
<tr>
<td><code>UNIV_OUT_FD_AVAILABLE</code></td>
<td>Default value for <code>UNIV_OUT_x_y_FD_AVAILABLE</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>UNIV_OUT_x_y_FD_AVAILABLE</code></td>
<td><code>0</code> if the corresponding universal output module (front panel) is not installed or does not allow specifying a fine delay, <code>1</code> if it is installed and allows specifying a fine delay.</td>
<td><code>$(UNIV_OUT_FD_AVAILABLE)</code></td>
</tr>
<tr>
<td><code>TB_UNIV_OUT_INSTALLED</code></td>
<td>Default value for <code>TB_UNIV_OUT_x_y_INSTALLED</code> (<code>0</code> or <code>1</code>).</td>
<td><code>0</code></td>
</tr>
<tr>
<td><code>TB_UNIV_OUT_x_y_INSTALLED</code></td>
<td><code>0</code> if the corresponding universal output module (transition board) is not installed, <code>1</code> if it is installed.</td>
<td><code>$(TB_UNIV_OUT_INSTALLED)</code></td>
</tr>
</table>


### MTCA-EVR-300

```
mrfMmapMtcaEvr300Device("EVR01", "/dev/era3")

dbLoadRecords("${MRF}/db/mtca-evr-300.db","P=TEST:EVR:,R=01:,DEVICE=EVR01")
```

If there is more than one EVR module in the system, the path to the device node
might have to be changed (e.g. `/dev/erb3`, `/dev/erc3`, etc.).

The following parameters have to be passed to `dbLoadRecords`:

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
</tr>
<tr>
<td><code>P</code></td>
<td>First part of the prefix for all EPICS PVs.</td>
</tr>
<tr>
<td><code>R</code></td>
<td>Second part of the prefix for all EPICS PVs (e.g. device number).</td>
</tr>
<tr>
<td><code>DEVICE</code></td>
<td>Name of the device (must be the name that is specified when calling <code>mrfMmapMtcaEvr300Device</code>).</td>
</tr>
</table>

The following parameters are optional. If they are not specified, the respective
default values are used.

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
<th>Default Value</th>
</tr>
<tr>
<td><code>CLK_GEN_CW_x</code></td>
<td>Configuration word for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
<tr>
<td><code>CLK_GEN_F_x</code></td>
<td>Frequency for the internal clock generator. See <a href="#clock-generator-configuration">Clock generator configuration</a> for details.</td>
<td>Depends on <code>x</code>.</td>
</tr>
</table>


### Supplemental database files

There are two additional database files that can be used for each device,
regardless of the device type:

 - `mrf-autosave-menu.db`
 - `mrf-write-all-settings-with-status.db`

The `mrf-autosave-menu.db` provides integration with
[Autosave](https://github.com/epics-modules/autosave) for managing the device
configuration (see below).

The `mrf-write-all-settings-with-status.db` provides two additional records that
can be used to write the current value of all settings to the hardware. This is
particularly useful with the VME-based hardware that might be restarted without
restarting the EPICS IOC. The basic support for writing all settings is already
provided by the standard database files (it can be triggered by writing to
`$(P)$(R)WriteAll.PROC`). The additional database file adds two more records
that provide make it possible to see whether the operation has finished. When
using this database file, one writes to `$(P)$(R)WriteAll:DoWithStatus.PROC` and
monitors `$(P)$(R)WriteAll:Status`, which is `1` while the operation is in
progress and `0` when it has finished.

These records are not included in the standard database file because they
require the `sseq` record which is not part of EPICS Base, but is provided by
the [calc module](https://github.com/epics-modules/calc).

When loading the database file, the `P` and `R` parameters have to be set to the
same values that are specified for the primary database file of the respective
device.


Autosave support
----------------

The [Autosave](https://github.com/epics-modules/autosave) support is entirely
optional and does not have to be used. For each device type, there is an
Autosave request file that lists all records storing settings for the respective
device type. As Autosave uses the name of the request file for constructing the
names of the save files, it is strongly suggested to create a custom request
file that includes the generic request file.

The `mrf-autosave-menu.db` database file can be used for loading and saving
configuration sets at runtime (see "Supplemental database files" above).


### Example

For saving a VME-EVR-230RF device, one could use the following request file (in
this example, we call it `evr01_preset.req`):

```
file vme-evr-230rf.req P=$(P),R=$(R)
$(P)$(R)Presets:Intrnl:Menu:currDesc
```

The second line is only needed if the support for loading and saving configuration sets (`mrf-autosave-menu.db`) is supposed to be used.

The corresponding IOC startup file would contain the following lines:

```
# Before iocInit
dbLoadRecords("${MRF}/db/mrf-autosave-menu.db", "P=TEST:EVR:,R=01:,CONFIG_PV_PREFIX=Presets:,CONFIG_NAME=evr01_preset")
dbLoadRecords("${MRF}/db/mrf-write-all-settings-with-status.db", "P=TEST:EVR:,R=01:")
set_requestfile_path("${TOP}/db", "")
set_requestfile_path("${MRF}/db", "")
set_savefile_path("${TOP}/iocBoot/${IOC}/autosave")
save_restoreSet_DatedBackupFiles(0)
save_restoreSet_NumSeqFiles(3)
save_restoreSet_SeqPeriodInSeconds(600)
save_restoreSet_CAReconnect(1)
save_restoreSet_IncompleteSetsOk(1)

# After iocInit
create_manual_set("evr02_preset.req", "P=A:TI:EVR:,R=02:,CONFIGMENU=1")
```

If the settings should also be saved automatically, one would use
`create_monitor_set` instead of `create_manual_set` and not specify
`CONFIGMENU=1`. If the settings should only be saved automatically and no manual
configuration management was desired, one would also omit the `dbLoadRecords`
lines.


Interrupt handling
------------------

Interrupts are only supported when using the mmap-based device support (the
UDP/IP-based protocol does not include support for interrupts). In order for an
interrupt to be triggered, it has to be enabled. This can be done through the
following process variables:

 - `$(P)$(R)IRQ:Enabled` (global flag, must be enabled for any of the other
   flags to be effective)
 - `$(P)$(R)IRQ:DataBuffer:Enabled`
 - `$(P)$(R)IRQ:Event:Enabled`
 - `$(P)$(R)IRQ:EventFIFOFull:Enabled`
 - `$(P)$(R)IRQ:Hardware:Enabled`
 - `$(P)$(R)IRQ:Heartbeat:Enabled`
 - `$(P)$(R)IRQ:LinkStateChange:Enabled`
 - `$(P)$(R)IRQ:RXViolation:Enabled`

If a certain interrupt type is not enabled, the corresponding bit in the
interrupt flags will never be set. Even when it is set in the device, it is
filtered by the device support so that only those flags that have been enabled
will actually cause the interrupt handlers to be notified.

Interrupts can be handled in two different ways. They can be mapped to EPICS
events, or processing of a record can be triggered.


### Triggering an EPICS event

A mapping from an interrupt to an EPICS event can be defined with the
`mrfMapInterruptToEvent` IOC shell command:

```
mrfMapInterruptToEvent("EVR01", 5, "0xffffffff")
```

The first parameter is the name of the device, the second the number of the
EPICS event that shall be posted, and the third is the interrupt mask. The
interrupt mask specifies which kind of interrupts trigger the event. Only when
at least one of the bits specified in the mask is also set in the interrupt flag
register, the event is triggered. The meaning of the bits in the interrupt flag
register is documented in the manual. On an EVR, it should typically be as follows:

<table>
<tr>
<th>Bit #</th>
<th>Meaning</th>
</tr>
<tr>
<td>0</td>
<td>Receiver violation flag</td>
</tr>
<tr>
<td>1</td>
<td>Event FIFO full flag</td>
</tr>
<tr>
<td>2</td>
<td>Heartbeat interrupt flag</td>
</tr>
<tr>
<td>3</td>
<td>Event interrupt flag</td>
</tr>
<tr>
<td>4</td>
<td>Hardware interrupt flag (mapped signal)</td>
</tr>
<tr>
<td>5</td>
<td>Data buffer flag</td>
</tr>
<tr>
<td>6</td>	
<td>ink state change interrupt flag</td>
</tr>
<tr>
<td>7 - 31</td>
<td>unused</td>
</tr>
</table>


### Triggering processing of an I/O Intr record

An interrupt can also be used to trigger processing of a record. The interrupt
has to be of type `bi`, `longin`, or `mbbiDirect`. The `DTYP` has to be set to
`MRF Interrupt` and `SCAN` has to be `I/O Intr`. When processed, the record's
`VAL` (`longin`) or `RVAL` (`bi`, `mbbiDirect`) is set to the content of the
interrupt flags register, masked by the specified mask. The mask is specified in
the `INP` field of the record, like in the following example:

```
record(longin, "MyInterrupt") {
  field(DTYP, "MRF Interrupt")
  field(INP,  "@EVR01 interrupt_flags_mask=0x01")
  field(SCAN, "I/O Intr")
}
```

In this example, `EVR01` is the name of the device and the interrupt mask is set
to `0x01`. If no mask is specified, a mask that has all bits set is used.
Processing of the record is only triggered if at least one of the bits specified
in the mask is set in the interrupt flag register.

Such a record can also be used with a different scan mode, but in this case it
is much less useful: When being processed (e.g. periodically or by another
trigger) it will simply get the value that corresponds to the last interrupt.


Clock generator configuration
-----------------------------

Each of the EVGs and EVRs has an internal clock generator. This clock generator
is based on a fixed frequency oscillator combined with a SY87739L fractional
divider. In order to set the frequency of the internal clock, this fractional
divider has to be configured.

Each frequency corresponds to a configuration word for the SY87739L chip. The
configuration word that is needed to get a specific frequency can be calculated
using a Python script (`utils/sy87739l-convert.py`) that is distributed with
this device support.

Please note that the difference between the actual event clock frequency (as
derived by dividing the external reference clock fed to the EVG) and the
frequency of the internal clock generator must be less than 100 ppm. If the
difference is larger, the PLL might not properly lock to the reference, which
can result in EVRs losing their link to the EVG and similar problems.

This frequency of the internal clock generator can be configurated through the
process variable `$(P)$(R)EventClock:Gen:Freq`. This PV is an enum, so that 16
different frequencies can be selected.

Each available frequency is configured by a pair of configuration macros that
have to be passed to `dbLoadRecords` when loading the record file for an EVG or
EVR. When a macro is not specified, its default value according to the following
table is used.

<table>
<tr>
<th>Parameter Name</th>
<th>Description</th>
<th>Default Value</th>
</tr>
<tr>
<td><code>CLK_GEN_CW_0</code></td>
<td>Configuration word corresponding to the 1<sup>st</sup> enum value.</td>
<td><code>0x00de816d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_0</code></td>
<td>Frequency corresponding to the 1<sup>st</sup> enum value.</td>
<td><code>125 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_1</code></td>
<td>Configuration word corresponding to the 2<sup>nd</sup> enum value.</td>
<td><code>0x0c928166</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_1</code></td>
<td>Frequency corresponding to the 2<sup>nd</sup> enum value.</td>
<td><code>124.908 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_2</code></td>
<td>Configuration word corresponding to the 3<sup>rd</sup> enum value.</td>
<td><code>0x018741ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_2</code></td>
<td>Frequency corresponding to the 3<sup>rd</sup> enum value.</td>
<td><code>119 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_3</code></td>
<td>Configuration word corresponding to the 4<sup>th</sup> enum value.</td>
<td><code>0x072f01ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_3</code></td>
<td>Frequency corresponding to the 4<sup>th</sup> enum value.</td>
<td><code>114.24 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_4</code></td>
<td>Configuration word corresponding to the 5<sup>th</sup> enum value.</td>
<td><code>0x049e81ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_4</code></td>
<td>Frequency corresponding to the 5<sup>th</sup> enum value.</td>
<td><code>106.25 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_5</code></td>
<td>Configuration word corresponding to the 6<sup>th</sup> enum value.</td>
<td><code>0x008201ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_5</code></td>
<td>Frequency corresponding to the 6<sup>th</sup> enum value.</td>
<td><code>100 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_6</code></td>
<td>Configuration word corresponding to the 7<sup>th</sup> enum value.</td>
<td><code>0x025b41ed</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_6</code></td>
<td>Frequency corresponding to the 7<sup>th</sup> enum value.</td>
<td><code>99.956 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_7</code></td>
<td>Configuration word corresponding to the 8<sup>th</sup> enum value.</td>
<td><code>0x0187422d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_7</code></td>
<td>Frequency corresponding to the 8<sup>th</sup> enum value.</td>
<td><code>89.25 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_8</code></td>
<td>Configuration word corresponding to the 9<sup>th</sup> enum value.</td>
<td><code>0x0082822d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_8</code></td>
<td>Frequency corresponding to the 9<sup>th</sup> enum value.</td>
<td><code>81 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_9</code></td>
<td>Configuration word corresponding to the 10<sup>th</sup> enum value.</td>
<td><code>0x0106822d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_9</code></td>
<td>Frequency corresponding to the 10<sup>th</sup> enum value.</td>
<td><code>80 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_10</code></td>
<td>Configuration word corresponding to the 11<sup>th</sup> enum value.</td>
<td><code>0x019e822d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_10</code></td>
<td>Frequency corresponding to the 11<sup>th</sup> enum value.</td>
<td><code>78.9 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_11</code></td>
<td>Configuration word corresponding to the 12<sup>th</sup> enum value.</td>
<td><code>0x018742ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_11</code></td>
<td>Frequency corresponding to the 12<sup>th</sup> enum value.</td>
<td><code>71.4 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_12</code></td>
<td>Configuration word corresponding to the 13<sup>th</sup> enum value.</td>
<td><code>0x0c9282a6</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_12</code></td>
<td>Frequency corresponding to the 13<sup>th</sup> enum value.</td>
<td><code>62.454 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_13</code></td>
<td>Configuration word corresponding to the 14<sup>th</sup> enum value.</td>
<td><code>0x009743ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_13</code></td>
<td>Frequency corresponding to the 14<sup>th</sup> enum value.</td>
<td><code>50 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_14</code></td>
<td>Configuration word corresponding to the 15<sup>th</sup> enum value.</td>
<td><code>0xc25b43ad</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_14</code></td>
<td>Frequency corresponding to the 15<sup>th</sup> enum value.</td>
<td><code>49.978 MHz</code></td>
</tr>
<tr>
<td><code>CLK_GEN_CW_15</code></td>
<td>Configuration word corresponding to the 16<sup>th</sup> enum value.</td>
<td><code>0x0176c36d</code></td>
</tr>
<tr>
<td><code>CLK_GEN_F_15</code></td>
<td>Frequency corresponding to the 16<sup>th</sup> enum value.</td>
<td><code>49.965 MHz</code></td>
</tr>
</table>


GUI / OPI panels
----------------

A complete set of panels for controlling the EVG and EVR are distributed with
this device support. This panels are designed to work with
[CSS](http://controlsystemstudio.org/) / BOY and can be found in the `opi/boy`
directory of this device support.

There are four different panels that can be used:


### EVG panel

The main EVG panel is provided by the file `EVG/Main.opi`. When opening this
panel, a couple of macros have to be set:

<table>
<tr>
<th>Macro name</th>
<th>Description</th>
</tr>
<tr>
<td>mrf_evg_name</td>
<td>Human-readable name of the EVG. This is used in the title of the panel.</td>
</tr>
<tr>
<td>mrf_evg_prefix</td>
<td>PV prefix of the EVG. This is the combination of the <code>P</code> and <code>R</code> macros specified in the IOC startup file.</td>
</tr>
<tr>
<td>mrf_evg_type</td>
<td>Type of the EVG. The type is used to determine the features of the EVG and display the respective GUI elements. At the moment, only <code>VME-EVG-230</code> is supported.</td>
</tr>
</table>


### EVR panel

The main EVR panel is provided by the files `EVR/Main_MTCA-EVR-300.opi` and
`EVR/Main_VME-EVR-230RF.opi` (depending on the type of the EVR). When opening
this panel, a couple of macros have to be set:

<table>
<tr>
<th>Macro name</th>
<th>Description</th>
</tr>
<tr>
<td>mrf_evr_name</td>
<td>Human-readable name of the EVR. This is used in the title of the panel.</td>
</tr>
<tr>
<td>mrf_evr_prefix</td>
<td>PV prefix of the EVR. This is the combination of the <code>P</code> and <code>R</code> macros specified in the IOC startup file.</td>
</tr>
</table>


### Autosave configuration management panel

When using [Autosave](#autosave-support), there is a panel for managing
configuration sets saved with Autosave. This panel is provided by the file
`ConfigMenu/MainFull.opi` (or `ConfigMenu/MainLight.opi` for a version that does
not display the description for each saved file).

When opening this panel, a couple of macros have to be set:

<table>
<tr>
<th>Macro name</th>
<th>Description</th>
</tr>
<tr>
<td>mrf_config_menu_device_name</td>
<td>Human-readable name of the EVR. This is used in the title of the panel.</td>
</tr>
<tr>
<td>mrf_config_menu_device_prefix</td>
<td>PV prefix of the EVR. This is the combination of the <code>P</code> and <code>R</code> macros specified in the IOC startup file.</td>
</tr>
<tr>
<td>mrf_config_menu_prefix</td>
<td>PV prefix used for the Autosave PVs. This is the prefix that is specified with the <code>CONFIG_PV_PREFIX</code> macro when loading the <code>mrf-autosave-menu.db</code> file in the IOC startup file.</td>
</tr>
</table>
