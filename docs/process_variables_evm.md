Process variables for EVM
=========================

The EVM is internally made up of four (virtual) devices:

- one [EVG](process_variables_evg.md), using the PV prefix `$(P)$(R)G:`,
- one [FCT](process_variables_fct.md), using the PV prefix `$(P)$(R)FCT:`, and
- two [EVRs](process_variables_evr.md), using the PV prefix `$(P)$(R)RD:` and
  `$(P)$(R)RU:`.

The only “global” PV that is provided for the whole EVM is
`$(P)$(R)WriteAll`. Writing to this PV has the effect of triggering the
`WriteAll` PVs of all four internal devices.
