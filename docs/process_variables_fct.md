Process variables for fan-out / concentrator
============================================

When using the database files that are provided as part of the device support,
process variables for controlling and monitoring nearly every aspect of the
fan-out / concentrator (FCT) that is built into each [EVM](
process_variables_evm.md) are provided.

The exact list of process variables depends on the device type (different form
factors or hardware revisions have different features), but most of the process
variables described here should be available for the majority of the devices.

The process variables listed here are specified without any prefix, so the
device specific prefix (the combination of the `P` and `R` macros
[specified in the IOC's startup file](using.md)) and the string `FCT:` have to
be added to the names specified here. For example, if `P` is `EVM:` and `R` is
`01:`, the effective prefix for the FCT is `EVM:01:FCT:`.


### Table of contents

- [Delay compensation](#delay-compensation)
- [Link status](#link-status)
- [SFP modules](#sfp-modules)


## Delay compensation

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>DelayComp:FIFODelay</td>
<td>Receive FIFO delay (read-only).</td>
</tr>
<tr>
<td>DelayComp:InternalDelay</td>
<td>Internal delay (read-only). This is the propagation delay between the upstream port (port U) and the downstream ports.</td>
</tr>
<tr>
<td>DelayComp:TopologyID</td>
<td>Topology ID of this EVM (read-only). The topology ID describes where in the timing-system topology a specific device is located with respect to the master EVM.</td>
</tr>
<tr>
<td>DelayComp:PortXDelay</td>
<td>Delay of the downstream link (read-only). This is half of the loop delay (the time it takes for a beacon event sent by the EVM to be returned by the downstream EVM or EVR).</td>
</tr>
<tr>
<td>DelayComp:UpstreamDelay</td>
<td>Delay measured by the upstream EVM (read-only). This, combined with the internal delay and the delay of the individual downstream links, is used to adjust the delay inside the downstream EVRs.</td>
</tr>
</table>


## Link status

For each of the links to the downstream devices, information about the link
status is provided.

In the following table, all PVs are listed with the generic PV prefix `SFP`,
which has to be replaced with the prefix or a specific link (e.g. `SFP1` for
the first link to a downstream device. The number of available links depends on
the form-factor and hardware revision.

<table>
<tr>
<th>Name</th>
<th>Description</th>
</tr>
<tr>
<td>SFP:Link</td>
<td>SFP link status (read-only). If 0, the link is down. If 1, the link is up. This record is only updated periodically, so it might take a moment for a change in status to be reflected by the process variable.</td>
</tr>
<tr>
<td>SFP:RXViolation</td>
<td>Receiver violation status flag. If there is a bit error or a loss of signal, this flag is set to one.</td>
</tr>
<tr>
<td>SFP:RXViolation:Reset</td>
<td>Write to this record in order to reset the status flag to zero.</td>
</tr>
</table>


## SFP modules

The PVs with diagnostic information for the SFP modules are exactly the same as
[the ones supported for the EVG](process_variables_evg.md#sfp-module). However,
due to there being more than one SFP module, the name `SFP1:`, `SFP2:`, etc. is
used instead of `SFP:`.

Please note that there are no PVs for the upstream SFP module. The information
for this module is provided within the namespace of the [EVG](
process_variables_evg.md) that is part of the same [EVM](
process_variables_evm.md).
